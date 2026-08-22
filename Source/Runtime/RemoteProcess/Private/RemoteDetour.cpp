#include "Runtime/RemoteProcess/Public/RemoteDetour.h"
#include "Runtime/RemoteProcess/Public/InstructionDecoder.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <cstring>

namespace
{
    constexpr size_t RelativeJumpSize = 5;
    constexpr size_t MaximumPrologueSize = 32;
}

bool FRemoteDetour::RelocatePrologue(std::vector<uint8>& PrologueBytes, FRemoteAddress OriginalAddress, FRemoteAddress RelocatedAddress)
{
    size_t Cursor = 0;

    while (Cursor < PrologueBytes.size())
    {
        const FDecodedInstruction Instruction = FInstructionDecoder::Decode(PrologueBytes.data() + Cursor, PrologueBytes.size() - Cursor);
        if (!Instruction.bValid)
        {
            return false;
        }

        if (Instruction.bHasRelativeBranch)
        {
            if (Instruction.RelativeOperandSize != sizeof(int32))
            {
                return false;
            }

            int32 Displacement = 0;
            std::memcpy(&Displacement, PrologueBytes.data() + Cursor + Instruction.RelativeOperandOffset, sizeof(Displacement));

            const FRemoteAddress BranchTarget = OriginalAddress + Cursor + Instruction.Length + static_cast<int64>(Displacement);
            const int64 NewDisplacement = static_cast<int64>(BranchTarget) - static_cast<int64>(RelocatedAddress + Cursor + Instruction.Length);

            if (NewDisplacement < INT32_MIN || NewDisplacement > INT32_MAX)
            {
                return false;
            }

            const int32 NarrowedDisplacement = static_cast<int32>(NewDisplacement);
            std::memcpy(PrologueBytes.data() + Cursor + Instruction.RelativeOperandOffset, &NarrowedDisplacement, sizeof(NarrowedDisplacement));
        }

        if (Instruction.bHasRipRelativeOperand)
        {
            int32 Displacement = 0;
            std::memcpy(&Displacement, PrologueBytes.data() + Cursor + Instruction.RipDisplacementOffset, sizeof(Displacement));

            const FRemoteAddress OperandTarget = OriginalAddress + Cursor + Instruction.Length + static_cast<int64>(Displacement);
            const int64 NewDisplacement = static_cast<int64>(OperandTarget) - static_cast<int64>(RelocatedAddress + Cursor + Instruction.Length);

            if (NewDisplacement < INT32_MIN || NewDisplacement > INT32_MAX)
            {
                return false;
            }

            const int32 NarrowedDisplacement = static_cast<int32>(NewDisplacement);
            std::memcpy(PrologueBytes.data() + Cursor + Instruction.RipDisplacementOffset, &NarrowedDisplacement, sizeof(NarrowedDisplacement));
        }

        Cursor += Instruction.Length;
    }

    return true;
}

bool FRemoteDetour::Install(const FRemoteMemory& Memory, FRemoteAllocation& Arena, FRemoteAddress TargetFunction, FRemoteAddress DetourFunction)
{
    if (IsInstalled() || TargetFunction == InvalidRemoteAddress || DetourFunction == InvalidRemoteAddress)
    {
        return false;
    }

    if (!Arena.IsWithinBranchRange(TargetFunction))
    {
        UE_LOG_ERROR("RemoteDetour", "Code arena is not within branch range of " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    std::vector<uint8> Prologue(MaximumPrologueSize);
    if (!Memory.ReadRaw(TargetFunction, Prologue.data(), Prologue.size()))
    {
        UE_LOG_ERROR("RemoteDetour", "Failed to read prologue at " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    const uint32 PrologueLength = FInstructionDecoder::MeasurePrologue(Prologue.data(), Prologue.size(), static_cast<uint32>(RelativeJumpSize));
    if (PrologueLength == 0)
    {
        UE_LOG_ERROR("RemoteDetour", "Unable to measure a safe prologue at " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    Prologue.resize(PrologueLength);

    const size_t TrampolineSize = PrologueLength + FX64Emitter::GetAbsoluteJumpSize();
    const FRemoteAddress Trampoline = Arena.Allocate(TrampolineSize, 16);
    if (Trampoline == InvalidRemoteAddress)
    {
        return false;
    }

    std::vector<uint8> RelocatedPrologue = Prologue;
    if (!RelocatePrologue(RelocatedPrologue, TargetFunction, Trampoline))
    {
        UE_LOG_ERROR("RemoteDetour", "Prologue at " + FStringConv::ToHex(TargetFunction) + " cannot be relocated");
        return false;
    }

    std::vector<uint8> TrampolineBytes = RelocatedPrologue;
    FX64Emitter::WriteAbsoluteJump(TrampolineBytes, TargetFunction + PrologueLength);

    if (!Memory.WriteRaw(Trampoline, TrampolineBytes.data(), TrampolineBytes.size()))
    {
        UE_LOG_ERROR("RemoteDetour", "Failed to write trampoline for " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    std::vector<uint8> PatchBytes(PrologueLength, 0x90);
    const int64 Displacement = static_cast<int64>(DetourFunction) - static_cast<int64>(TargetFunction + RelativeJumpSize);

    if (Displacement < INT32_MIN || Displacement > INT32_MAX)
    {
        UE_LOG_ERROR("RemoteDetour", "Detour target is out of relative branch range for " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    FX64Emitter::WriteRelativeJump(PatchBytes.data(), static_cast<int32>(Displacement));

    if (!Memory.WriteRaw(TargetFunction, PatchBytes.data(), PatchBytes.size()))
    {
        UE_LOG_ERROR("RemoteDetour", "Failed to patch entry point at " + FStringConv::ToHex(TargetFunction));
        return false;
    }

    Memory.FlushInstructionCacheRange(TargetFunction, PatchBytes.size());
    Memory.FlushInstructionCacheRange(Trampoline, TrampolineBytes.size());

    TargetAddress = TargetFunction;
    TrampolineAddress = Trampoline;
    OriginalBytes = std::move(Prologue);

    UE_LOG_VERBOSE("RemoteDetour", "Hooked " + FStringConv::ToHex(TargetFunction) + " with trampoline at " + FStringConv::ToHex(Trampoline));
    return true;
}

bool FRemoteDetour::Uninstall(const FRemoteMemory& Memory)
{
    if (!IsInstalled())
    {
        return false;
    }

    const bool bRestored = Memory.WriteRaw(TargetAddress, OriginalBytes.data(), OriginalBytes.size());
    Memory.FlushInstructionCacheRange(TargetAddress, OriginalBytes.size());

    TargetAddress = InvalidRemoteAddress;
    TrampolineAddress = InvalidRemoteAddress;
    OriginalBytes.clear();

    return bRestored;
}

bool FRemoteDetour::IsInstalled() const
{
    return TargetAddress != InvalidRemoteAddress;
}

FRemoteAddress FRemoteDetour::GetTargetAddress() const
{
    return TargetAddress;
}

FRemoteAddress FRemoteDetour::GetTrampolineAddress() const
{
    return TrampolineAddress;
}
