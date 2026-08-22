#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <cstring>

namespace
{
    constexpr size_t InvalidLabelOffset = static_cast<size_t>(-1);

    uint8 RegisterIndex(EX64Register Register)
    {
        return static_cast<uint8>(ToUnderlying(Register) & 0x7);
    }

    bool RegisterNeedsExtension(EX64Register Register)
    {
        return ToUnderlying(Register) >= 8;
    }
}

void FX64Emitter::Reset()
{
    Bytes.clear();
    Labels.clear();
    Fixups.clear();
}

size_t FX64Emitter::GetSize() const
{
    return Bytes.size();
}

const std::vector<uint8>& FX64Emitter::GetBytes() const
{
    return Bytes;
}

FX64Emitter::FLabelId FX64Emitter::CreateLabel()
{
    Labels.push_back(InvalidLabelOffset);
    return Labels.size() - 1;
}

void FX64Emitter::BindLabel(FLabelId Label)
{
    Labels[Label] = Bytes.size();
}

void FX64Emitter::EmitBytes(std::initializer_list<uint8> InBytes)
{
    Bytes.insert(Bytes.end(), InBytes.begin(), InBytes.end());
}

void FX64Emitter::EmitRaw(const uint8* InBytes, size_t Count)
{
    Bytes.insert(Bytes.end(), InBytes, InBytes + Count);
}

void FX64Emitter::EmitU8(uint8 Value)
{
    Bytes.push_back(Value);
}

void FX64Emitter::EmitU32(uint32 Value)
{
    for (int Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<uint8>((Value >> Shift) & 0xFF));
    }
}

void FX64Emitter::EmitU64(uint64 Value)
{
    for (int Shift = 0; Shift < 64; Shift += 8)
    {
        Bytes.push_back(static_cast<uint8>((Value >> Shift) & 0xFF));
    }
}

void FX64Emitter::PushRegister(EX64Register Register)
{
    if (RegisterNeedsExtension(Register))
    {
        EmitU8(0x41);
    }

    EmitU8(static_cast<uint8>(0x50 + RegisterIndex(Register)));
}

void FX64Emitter::PopRegister(EX64Register Register)
{
    if (RegisterNeedsExtension(Register))
    {
        EmitU8(0x41);
    }

    EmitU8(static_cast<uint8>(0x58 + RegisterIndex(Register)));
}

void FX64Emitter::MoveRegisterImmediate(EX64Register Destination, uint64 Value)
{
    EmitU8(static_cast<uint8>(0x48 | (RegisterNeedsExtension(Destination) ? 0x1 : 0x0)));
    EmitU8(static_cast<uint8>(0xB8 + RegisterIndex(Destination)));
    EmitU64(Value);
}

void FX64Emitter::MoveRegisterRegister(EX64Register Destination, EX64Register Source)
{
    uint8 Rex = 0x48;
    if (RegisterNeedsExtension(Source))
    {
        Rex |= 0x4;
    }

    if (RegisterNeedsExtension(Destination))
    {
        Rex |= 0x1;
    }

    EmitU8(Rex);
    EmitU8(0x89);
    EmitU8(static_cast<uint8>(0xC0 | (RegisterIndex(Source) << 3) | RegisterIndex(Destination)));
}

void FX64Emitter::EmitRexForRegisterMemory(EX64Register Register, EX64Register Base)
{
    uint8 Rex = 0x48;
    if (RegisterNeedsExtension(Register))
    {
        Rex |= 0x4;
    }

    if (RegisterNeedsExtension(Base))
    {
        Rex |= 0x1;
    }

    EmitU8(Rex);
}

void FX64Emitter::EmitModRmWithDisplacement(EX64Register Register, EX64Register Base, int32 Displacement)
{
    const uint8 BaseIndex = RegisterIndex(Base);
    const uint8 RegisterField = static_cast<uint8>(RegisterIndex(Register) << 3);

    uint8 Mode = 0x80;
    if (Displacement == 0 && BaseIndex != 0x5)
    {
        Mode = 0x00;
    }
    else if (Displacement >= -128 && Displacement <= 127)
    {
        Mode = 0x40;
    }

    EmitU8(static_cast<uint8>(Mode | RegisterField | BaseIndex));

    if (BaseIndex == 0x4)
    {
        EmitU8(0x24);
    }

    if (Mode == 0x40)
    {
        EmitU8(static_cast<uint8>(static_cast<int8>(Displacement)));
    }
    else if (Mode == 0x80)
    {
        EmitU32(static_cast<uint32>(Displacement));
    }
}

void FX64Emitter::LoadRegisterFromMemory(EX64Register Destination, EX64Register Base, int32 Displacement)
{
    EmitRexForRegisterMemory(Destination, Base);
    EmitU8(0x8B);
    EmitModRmWithDisplacement(Destination, Base, Displacement);
}

void FX64Emitter::StoreRegisterToMemory(EX64Register Base, int32 Displacement, EX64Register Source)
{
    EmitRexForRegisterMemory(Source, Base);
    EmitU8(0x89);
    EmitModRmWithDisplacement(Source, Base, Displacement);
}

void FX64Emitter::StoreImmediateToMemory(EX64Register Base, int32 Displacement, uint32 Value)
{
    EmitRexForRegisterMemory(EX64Register::Rax, Base);
    EmitU8(0xC7);
    EmitModRmWithDisplacement(EX64Register::Rax, Base, Displacement);
    EmitU32(Value);
}

void FX64Emitter::AddRegisterImmediate(EX64Register Destination, int32 Value)
{
    EmitU8(static_cast<uint8>(0x48 | (RegisterNeedsExtension(Destination) ? 0x1 : 0x0)));
    EmitU8(0x81);
    EmitU8(static_cast<uint8>(0xC0 | RegisterIndex(Destination)));
    EmitU32(static_cast<uint32>(Value));
}

void FX64Emitter::SubRegisterImmediate(EX64Register Destination, int32 Value)
{
    EmitU8(static_cast<uint8>(0x48 | (RegisterNeedsExtension(Destination) ? 0x1 : 0x0)));
    EmitU8(0x81);
    EmitU8(static_cast<uint8>(0xE8 | RegisterIndex(Destination)));
    EmitU32(static_cast<uint32>(Value));
}

void FX64Emitter::CompareMemoryImmediate(EX64Register Base, int32 Displacement, uint32 Value)
{
    EmitRexForRegisterMemory(EX64Register::Rax, Base);
    EmitU8(0x81);
    EmitModRmWithDisplacement(EX64Register::Rdi, Base, Displacement);
    EmitU32(Value);
}

void FX64Emitter::TestRegisterRegister(EX64Register Left, EX64Register Right)
{
    uint8 Rex = 0x48;
    if (RegisterNeedsExtension(Right))
    {
        Rex |= 0x4;
    }

    if (RegisterNeedsExtension(Left))
    {
        Rex |= 0x1;
    }

    EmitU8(Rex);
    EmitU8(0x85);
    EmitU8(static_cast<uint8>(0xC0 | (RegisterIndex(Right) << 3) | RegisterIndex(Left)));
}

void FX64Emitter::CompareRegisterRegister(EX64Register Left, EX64Register Right)
{
    uint8 Rex = 0x48;
    if (RegisterNeedsExtension(Right))
    {
        Rex |= 0x4;
    }

    if (RegisterNeedsExtension(Left))
    {
        Rex |= 0x1;
    }

    EmitU8(Rex);
    EmitU8(0x39);
    EmitU8(static_cast<uint8>(0xC0 | (RegisterIndex(Right) << 3) | RegisterIndex(Left)));
}

void FX64Emitter::SaveXmmToMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement)
{
    EmitBytes({ 0x0F, 0x11 });
    EmitModRmWithDisplacement(static_cast<EX64Register>(XmmIndex), Base, Displacement);
}

void FX64Emitter::LoadXmmFromMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement)
{
    EmitBytes({ 0x0F, 0x10 });
    EmitModRmWithDisplacement(static_cast<EX64Register>(XmmIndex), Base, Displacement);
}

void FX64Emitter::LoadXmmQwordFromMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement)
{
    EmitBytes({ 0xF3, 0x0F, 0x7E });
    EmitModRmWithDisplacement(static_cast<EX64Register>(XmmIndex), Base, Displacement);
}

void FX64Emitter::StoreXmmQwordToMemory(EX64Register Base, int32 Displacement, uint8 XmmIndex)
{
    EmitBytes({ 0x66, 0x0F, 0xD6 });
    EmitModRmWithDisplacement(static_cast<EX64Register>(XmmIndex), Base, Displacement);
}

void FX64Emitter::XorRegisterRegister(EX64Register Destination, EX64Register Source)
{
    uint8 Rex = 0x48;
    if (RegisterNeedsExtension(Source))
    {
        Rex |= 0x4;
    }

    if (RegisterNeedsExtension(Destination))
    {
        Rex |= 0x1;
    }

    EmitU8(Rex);
    EmitU8(0x31);
    EmitU8(static_cast<uint8>(0xC0 | (RegisterIndex(Source) << 3) | RegisterIndex(Destination)));
}

void FX64Emitter::IncrementMemory(EX64Register Base, int32 Displacement)
{
    EmitRexForRegisterMemory(EX64Register::Rax, Base);
    EmitU8(0xFF);
    EmitModRmWithDisplacement(EX64Register::Rax, Base, Displacement);
}

void FX64Emitter::CallRegister(EX64Register Register)
{
    if (RegisterNeedsExtension(Register))
    {
        EmitU8(0x41);
    }

    EmitU8(0xFF);
    EmitU8(static_cast<uint8>(0xD0 | RegisterIndex(Register)));
}

void FX64Emitter::JumpToLabel(FLabelId Label)
{
    EmitU8(0xE9);

    FLabelFixup Fixup;
    Fixup.Label = Label;
    Fixup.PatchOffset = Bytes.size();
    Fixup.InstructionEnd = Bytes.size() + sizeof(int32);

    EmitU32(0);
    Fixups.push_back(Fixup);
}

void FX64Emitter::JumpConditionalToLabel(EX64Condition Condition, FLabelId Label)
{
    EmitU8(0x0F);
    EmitU8(ToUnderlying(Condition));

    FLabelFixup Fixup;
    Fixup.Label = Label;
    Fixup.PatchOffset = Bytes.size();
    Fixup.InstructionEnd = Bytes.size() + sizeof(int32);

    EmitU32(0);
    Fixups.push_back(Fixup);
}

void FX64Emitter::JumpAbsolute(uint64 TargetAddress)
{
    EmitBytes({ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });
    EmitU64(TargetAddress);
}

void FX64Emitter::EmitPause()
{
    EmitBytes({ 0xF3, 0x90 });
}

void FX64Emitter::EmitReturn()
{
    EmitU8(0xC3);
}

void FX64Emitter::EmitBreakpoint()
{
    EmitU8(0xCC);
}

bool FX64Emitter::Finalize()
{
    for (const FLabelFixup& Fixup : Fixups)
    {
        const size_t TargetOffset = Labels[Fixup.Label];
        if (TargetOffset == InvalidLabelOffset)
        {
            return false;
        }

        const int32 Displacement = static_cast<int32>(static_cast<int64>(TargetOffset) - static_cast<int64>(Fixup.InstructionEnd));
        std::memcpy(Bytes.data() + Fixup.PatchOffset, &Displacement, sizeof(Displacement));
    }

    Fixups.clear();
    return true;
}

void FX64Emitter::WriteRelativeJump(uint8* Destination, int32 Displacement)
{
    Destination[0] = 0xE9;
    std::memcpy(Destination + 1, &Displacement, sizeof(Displacement));
}

size_t FX64Emitter::GetAbsoluteJumpSize()
{
    return 14;
}

void FX64Emitter::WriteAbsoluteJump(std::vector<uint8>& Destination, uint64 TargetAddress)
{
    Destination.push_back(0xFF);
    Destination.push_back(0x25);
    Destination.push_back(0x00);
    Destination.push_back(0x00);
    Destination.push_back(0x00);
    Destination.push_back(0x00);

    for (int Shift = 0; Shift < 64; Shift += 8)
    {
        Destination.push_back(static_cast<uint8>((TargetAddress >> Shift) & 0xFF));
    }
}
