#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <cstring>

namespace
{
    constexpr int32 StubFrameSize = 0x88;
    constexpr uint64 BusyReportIntervalMilliseconds = 10000;
    constexpr int32 StubSavedRcx = 0x20;
    constexpr int32 StubSavedRdx = 0x28;
    constexpr int32 StubSavedR8 = 0x30;
    constexpr int32 StubSavedR9 = 0x38;
    constexpr int32 StubSavedXmm0 = 0x40;
    constexpr int32 StubSavedXmm1 = 0x50;
    constexpr int32 StubSavedXmm2 = 0x60;
    constexpr int32 StubSavedXmm3 = 0x70;

    constexpr int32 PumpFrameSize = 0x48;
    constexpr int32 PumpStackArgument0 = 0x20;
    constexpr int32 PumpSavedTicket = 0x40;

    constexpr EX64Register IntegerArgumentRegisters[FBridgeLayout::IntegerArgumentCount] = { EX64Register::Rcx, EX64Register::Rdx, EX64Register::R8,
        EX64Register::R9 };

    void EmitSaveArgumentRegisters(FX64Emitter& Emitter)
    {
        Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedRcx, EX64Register::Rcx);
        Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedRdx, EX64Register::Rdx);
        Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedR8, EX64Register::R8);
        Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedR9, EX64Register::R9);
        Emitter.SaveXmmToMemory(0, EX64Register::Rsp, StubSavedXmm0);
        Emitter.SaveXmmToMemory(1, EX64Register::Rsp, StubSavedXmm1);
        Emitter.SaveXmmToMemory(2, EX64Register::Rsp, StubSavedXmm2);
        Emitter.SaveXmmToMemory(3, EX64Register::Rsp, StubSavedXmm3);
    }

    void EmitRestoreFloatArgumentRegisters(FX64Emitter& Emitter)
    {
        Emitter.LoadXmmFromMemory(0, EX64Register::Rsp, StubSavedXmm0);
        Emitter.LoadXmmFromMemory(1, EX64Register::Rsp, StubSavedXmm1);
        Emitter.LoadXmmFromMemory(2, EX64Register::Rsp, StubSavedXmm2);
        Emitter.LoadXmmFromMemory(3, EX64Register::Rsp, StubSavedXmm3);
    }
}

float FHookInvocation::GetFloatArgument(int32 Index) const
{
    const uint64 Bits = FloatArgumentBits[Index];
    float Value = 0.0f;
    std::memcpy(&Value, &Bits, sizeof(Value));
    return Value;
}

FHookResponse FHookResponse::ContinueToOriginal()
{
    return FHookResponse();
}

FHookResponse FHookResponse::OverrideWith(uint64 Value)
{
    FHookResponse Response;
    Response.bSkipOriginal = true;
    Response.ReturnValue = Value;
    return Response;
}

bool FGameThreadBridge::Initialize(const FRemoteMemory& InMemory, FRemoteAllocation& InArena, uint32 InGameThreadId)
{
    Shutdown();

    if (InGameThreadId == 0)
    {
        UE_LOG_ERROR("Bridge", "The game thread id is unknown, remote calls cannot be pinned to the game thread");
        return false;
    }

    Memory = &InMemory;
    Arena = &InArena;

    ControlBlockAddress = Arena->AllocateZeroed(FBridgeLayout::ControlBlockSize, 64);
    ChannelArrayAddress = Arena->AllocateZeroed(static_cast<size_t>(FBridgeLayout::ChannelSize) * FBridgeLayout::MaximumChannels, 64);

    if (ControlBlockAddress == InvalidRemoteAddress || ChannelArrayAddress == InvalidRemoteAddress)
    {
        return false;
    }

    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::GameThreadId, InGameThreadId);

    if (!EmitPumpRoutine())
    {
        UE_LOG_ERROR("Bridge", "Failed to emit the game thread pump routine");
        return false;
    }

    Hooks.resize(FBridgeLayout::MaximumChannels);

    UE_LOG_DISPLAY("Bridge", "Control block at " + FStringConv::ToHex(ControlBlockAddress) + ", pump routine at " + FStringConv::ToHex(PumpRoutineAddress) +
        ", game thread " + std::to_string(InGameThreadId));
    return true;
}

void FGameThreadBridge::Shutdown()
{
    if (Memory != nullptr)
    {
        RemoveAllHooks();
    }

    Hooks.clear();
    ControlBlockAddress = InvalidRemoteAddress;
    ChannelArrayAddress = InvalidRemoteAddress;
    PumpRoutineAddress = InvalidRemoteAddress;
    NextCommandTicket = 0;
    HoldDepth = 0;
    Memory = nullptr;
    Arena = nullptr;
}

bool FGameThreadBridge::IsInitialized() const
{
    return Memory != nullptr && Arena != nullptr && PumpRoutineAddress != InvalidRemoteAddress;
}

FRemoteAllocation& FGameThreadBridge::GetArena() const
{
    return *Arena;
}

const FRemoteMemory& FGameThreadBridge::GetMemory() const
{
    return *Memory;
}

FRemoteAddress FGameThreadBridge::GetChannelAddress(int32 ChannelIndex) const
{
    return ChannelArrayAddress + static_cast<uint64>(ChannelIndex) * FBridgeLayout::ChannelSize;
}

void FGameThreadBridge::EmitCommandDispatch(FX64Emitter& Emitter, size_t LoopLabel) const
{
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::R10, EX64Register::Rax, FBridgeLayout::CommandTarget);
    Emitter.LoadRegisterFromMemory(EX64Register::R11, EX64Register::Rax, FBridgeLayout::CommandTicket);
    Emitter.StoreRegisterToMemory(EX64Register::Rsp, PumpSavedTicket, EX64Register::R11);

    for (int32 Index = 0; Index < FBridgeLayout::IntegerArgumentCount; ++Index)
    {
        const int32 Displacement = FBridgeLayout::CommandArgument0 + Index * 8;
        Emitter.LoadXmmQwordFromMemory(static_cast<uint8>(Index), EX64Register::Rax, Displacement);
        Emitter.LoadRegisterFromMemory(IntegerArgumentRegisters[Index], EX64Register::Rax, Displacement);
    }

    for (int32 Index = FBridgeLayout::IntegerArgumentCount; Index < FBridgeLayout::MaximumCommandArguments; ++Index)
    {
        Emitter.LoadRegisterFromMemory(EX64Register::R11, EX64Register::Rax, FBridgeLayout::CommandArgument0 + Index * 8);
        Emitter.StoreRegisterToMemory(EX64Register::Rsp, PumpStackArgument0 + (Index - FBridgeLayout::IntegerArgumentCount) * 8, EX64Register::R11);
    }

    Emitter.StoreImmediateToMemory(EX64Register::Rax, FBridgeLayout::CommandDoorbell, 0);
    Emitter.CallRegister(EX64Register::R10);

    Emitter.MoveRegisterImmediate(EX64Register::R10, ControlBlockAddress);
    Emitter.StoreRegisterToMemory(EX64Register::R10, FBridgeLayout::CommandResult, EX64Register::Rax);
    Emitter.StoreXmmQwordToMemory(EX64Register::R10, FBridgeLayout::CommandResultFloat, 0);
    Emitter.LoadRegisterFromMemory(EX64Register::R11, EX64Register::Rsp, PumpSavedTicket);
    Emitter.StoreRegisterToMemory(EX64Register::R10, FBridgeLayout::CompletedTicket, EX64Register::R11);
    Emitter.IncrementMemory(EX64Register::R10, FBridgeLayout::CommandCompletion);
    Emitter.JumpToLabel(LoopLabel);
}

bool FGameThreadBridge::EmitPumpRoutine()
{
    FX64Emitter Emitter;

    const FX64Emitter::FLabelId LabelLoop = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelCheckHold = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelExit = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelExitNested = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelForeignThread = Emitter.CreateLabel();

    Emitter.LoadCurrentThreadId(EX64Register::Rax);
    Emitter.MoveRegisterImmediate(EX64Register::R10, ControlBlockAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::R11, EX64Register::R10, FBridgeLayout::GameThreadId);
    Emitter.CompareRegisterRegister(EX64Register::Rax, EX64Register::R11);
    Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, LabelForeignThread);

    Emitter.SubRegisterImmediate(EX64Register::Rsp, PumpFrameSize);
    Emitter.IncrementMemory(EX64Register::R10, FBridgeLayout::PumpDepth);

    Emitter.BindLabel(LabelLoop);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::CommandDoorbell);
    Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, LabelCheckHold);

    EmitCommandDispatch(Emitter, LabelLoop);

    Emitter.BindLabel(LabelCheckHold);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.CompareMemoryImmediate(EX64Register::Rax, FBridgeLayout::PumpDepth, 1);
    Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, LabelExitNested);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::PumpHoldRequest);
    Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, LabelExit);
    Emitter.StoreImmediateToMemory(EX64Register::Rax, FBridgeLayout::PumpHoldEngaged, 1);
    Emitter.EmitPause();
    Emitter.JumpToLabel(LabelLoop);

    Emitter.BindLabel(LabelExit);
    Emitter.StoreImmediateToMemory(EX64Register::Rax, FBridgeLayout::PumpHoldEngaged, 0);

    Emitter.BindLabel(LabelExitNested);
    Emitter.DecrementMemory(EX64Register::Rax, FBridgeLayout::PumpDepth);
    Emitter.IncrementMemory(EX64Register::Rax, FBridgeLayout::PumpSequence);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, PumpFrameSize);

    Emitter.BindLabel(LabelForeignThread);
    Emitter.EmitReturn();

    if (!Emitter.Finalize())
    {
        return false;
    }

    PumpRoutineAddress = Arena->AllocateAndWrite(Emitter.GetBytes().data(), Emitter.GetBytes().size(), 16);
    return PumpRoutineAddress != InvalidRemoteAddress;
}

bool FGameThreadBridge::EmitHookStub(int32 ChannelIndex, EHookDispatchMode Mode, FRemoteAddress StubAddress, FRemoteAddress TrampolineAddress)
{
    const FRemoteAddress ChannelAddress = GetChannelAddress(ChannelIndex);

    FX64Emitter Emitter;

    const FX64Emitter::FLabelId LabelWait = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelOverride = Emitter.CreateLabel();

    Emitter.SubRegisterImmediate(EX64Register::Rsp, StubFrameSize);
    EmitSaveArgumentRegisters(Emitter);

    Emitter.MoveRegisterImmediate(EX64Register::Rax, ChannelAddress);
    Emitter.StoreRegisterToMemory(EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 0, EX64Register::Rcx);
    Emitter.StoreRegisterToMemory(EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 8, EX64Register::Rdx);
    Emitter.StoreRegisterToMemory(EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 16, EX64Register::R8);
    Emitter.StoreRegisterToMemory(EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 24, EX64Register::R9);
    Emitter.StoreXmmQwordToMemory(EX64Register::Rax, FBridgeLayout::ChannelFloatArgument1, 1);
    Emitter.StoreXmmQwordToMemory(EX64Register::Rax, FBridgeLayout::ChannelFloatArgument2, 2);

    Emitter.XorRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.StoreRegisterToMemory(EX64Register::Rax, FBridgeLayout::ChannelVerdict, EX64Register::Rcx);
    Emitter.IncrementMemory(EX64Register::Rax, FBridgeLayout::ChannelRequestSequence);

    Emitter.BindLabel(LabelWait);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, PumpRoutineAddress);
    Emitter.CallRegister(EX64Register::Rax);

    if (Mode == EHookDispatchMode::Blocking)
    {
        Emitter.MoveRegisterImmediate(EX64Register::Rax, ChannelAddress);
        Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::ChannelRequestSequence);
        Emitter.LoadRegisterFromMemory(EX64Register::Rdx, EX64Register::Rax, FBridgeLayout::ChannelAckSequence);
        Emitter.CompareRegisterRegister(EX64Register::Rcx, EX64Register::Rdx);
        Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, LabelWait);

        Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::ChannelVerdict);
        Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
        Emitter.JumpConditionalToLabel(EX64Condition::NotZero, LabelOverride);
    }

    Emitter.MoveRegisterImmediate(EX64Register::Rax, ChannelAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 0);
    Emitter.LoadRegisterFromMemory(EX64Register::Rdx, EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 8);
    Emitter.LoadRegisterFromMemory(EX64Register::R8, EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 16);
    Emitter.LoadRegisterFromMemory(EX64Register::R9, EX64Register::Rax, FBridgeLayout::ChannelIntegerArgument0 + 24);
    EmitRestoreFloatArgumentRegisters(Emitter);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, StubFrameSize);
    Emitter.JumpAbsolute(TrampolineAddress);

    if (Mode == EHookDispatchMode::Blocking)
    {
        Emitter.BindLabel(LabelOverride);
        Emitter.MoveRegisterImmediate(EX64Register::Rcx, ChannelAddress);
        Emitter.LoadXmmQwordFromMemory(0, EX64Register::Rcx, FBridgeLayout::ChannelReturnValue);
        Emitter.LoadRegisterFromMemory(EX64Register::Rax, EX64Register::Rcx, FBridgeLayout::ChannelReturnValue);
        Emitter.AddRegisterImmediate(EX64Register::Rsp, StubFrameSize);
        Emitter.EmitReturn();
    }

    if (!Emitter.Finalize())
    {
        return false;
    }

    if (Emitter.GetSize() > FBridgeLayout::ReservedStubSize)
    {
        UE_LOG_ERROR("Bridge", "Hook stub exceeds the reserved slot size");
        return false;
    }

    return Memory->WriteRaw(StubAddress, Emitter.GetBytes().data(), Emitter.GetBytes().size());
}

bool FGameThreadBridge::EmitPrologueCallStub(FRemoteAddress StubAddress, FRemoteAddress RoutineAddress, FRemoteAddress TrampolineAddress)
{
    FX64Emitter Emitter;

    Emitter.SubRegisterImmediate(EX64Register::Rsp, StubFrameSize);
    EmitSaveArgumentRegisters(Emitter);

    Emitter.MoveRegisterImmediate(EX64Register::Rax, RoutineAddress);
    Emitter.CallRegister(EX64Register::Rax);

    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rsp, StubSavedRcx);
    Emitter.LoadRegisterFromMemory(EX64Register::Rdx, EX64Register::Rsp, StubSavedRdx);
    Emitter.LoadRegisterFromMemory(EX64Register::R8, EX64Register::Rsp, StubSavedR8);
    Emitter.LoadRegisterFromMemory(EX64Register::R9, EX64Register::Rsp, StubSavedR9);
    EmitRestoreFloatArgumentRegisters(Emitter);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, StubFrameSize);
    Emitter.JumpAbsolute(TrampolineAddress);

    if (!Emitter.Finalize() || Emitter.GetSize() > FBridgeLayout::ReservedStubSize)
    {
        return false;
    }

    return Memory->WriteRaw(StubAddress, Emitter.GetBytes().data(), Emitter.GetBytes().size());
}

int32 FGameThreadBridge::FindVirtualIndex(FRemoteAddress Object, FRemoteAddress Function) const
{
    if (Memory == nullptr || Object == InvalidRemoteAddress || Function == InvalidRemoteAddress)
    {
        return InvalidIndex;
    }

    const FRemoteAddress Table = Memory->ReadPointer(Object);
    if (Table == InvalidRemoteAddress || Table == 0)
    {
        return InvalidIndex;
    }

    std::vector<uint64> Entries(FBridgeLayout::ShadowVirtualTableEntries);
    if (!Memory->ReadRaw(Table, Entries.data(), Entries.size() * sizeof(uint64)))
    {
        return InvalidIndex;
    }

    for (int32 Index = 0; Index < static_cast<int32>(Entries.size()); ++Index)
    {
        if (Entries[static_cast<size_t>(Index)] == Function)
        {
            return Index;
        }
    }

    return InvalidIndex;
}

FRemoteAddress FGameThreadBridge::AcquireShadowVirtualTable(FRemoteAddress Object)
{
    for (const FShadowVirtualTable& Shadow : ShadowVirtualTables)
    {
        if (Shadow.Object == Object)
        {
            return Shadow.ShadowTable;
        }
    }

    const FRemoteAddress OriginalTable = Memory->ReadPointer(Object);
    if (OriginalTable == InvalidRemoteAddress || OriginalTable == 0)
    {
        return InvalidRemoteAddress;
    }

    const size_t CopiedEntries = static_cast<size_t>(FBridgeLayout::ShadowVirtualTableEntries) + 1;
    std::vector<uint64> Entries(CopiedEntries);
    if (!Memory->ReadRaw(OriginalTable - sizeof(uint64), Entries.data(), Entries.size() * sizeof(uint64)))
    {
        return InvalidRemoteAddress;
    }

    const FRemoteAddress Copy = Arena->AllocateAndWrite(Entries.data(), Entries.size() * sizeof(uint64), 16);
    if (Copy == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const FRemoteAddress ShadowTable = Copy + sizeof(uint64);
    if (!Memory->Write<uint64>(Object, ShadowTable))
    {
        return InvalidRemoteAddress;
    }

    FShadowVirtualTable Shadow;
    Shadow.Object = Object;
    Shadow.OriginalTable = OriginalTable;
    Shadow.ShadowTable = ShadowTable;
    ShadowVirtualTables.push_back(Shadow);
    return ShadowTable;
}

void FGameThreadBridge::RestoreShadowVirtualTables()
{
    for (const FShadowVirtualTable& Shadow : ShadowVirtualTables)
    {
        if (Memory->ReadPointer(Shadow.Object) == Shadow.ShadowTable)
        {
            Memory->Write<uint64>(Shadow.Object, Shadow.OriginalTable);
        }
    }

    ShadowVirtualTables.clear();
}

bool FGameThreadBridge::InstallVirtualPrologueCall(FRemoteAddress Object, int32 VirtualIndex, FRemoteAddress RoutineAddress)
{
    if (!IsInitialized() || Object == InvalidRemoteAddress || RoutineAddress == InvalidRemoteAddress || VirtualIndex < 0 ||
        VirtualIndex >= FBridgeLayout::ShadowVirtualTableEntries)
    {
        return false;
    }

    const FRemoteAddress OriginalTable = Memory->ReadPointer(Object);
    const FRemoteAddress SlotOffset = static_cast<FRemoteAddress>(VirtualIndex) * sizeof(uint64);
    const FRemoteAddress OriginalFunction = Memory->ReadPointer(OriginalTable + SlotOffset);
    if (OriginalFunction == InvalidRemoteAddress || OriginalFunction == 0)
    {
        UE_LOG_ERROR("Bridge", "Virtual slot " + std::to_string(VirtualIndex) + " of " + FStringConv::ToHex(Object) + " holds no function");
        return false;
    }

    const FRemoteAddress StubAddress = Arena->AllocateZeroed(FBridgeLayout::ReservedStubSize, 16);
    if (StubAddress == InvalidRemoteAddress || !EmitPrologueCallStub(StubAddress, RoutineAddress, OriginalFunction))
    {
        UE_LOG_ERROR("Bridge", "Failed to emit a prologue call for virtual slot " + std::to_string(VirtualIndex));
        return false;
    }

    Memory->FlushInstructionCacheRange(StubAddress, FBridgeLayout::ReservedStubSize);

    const FRemoteAddress ShadowTable = AcquireShadowVirtualTable(Object);
    if (ShadowTable == InvalidRemoteAddress || !Memory->Write<uint64>(ShadowTable + SlotOffset, StubAddress))
    {
        UE_LOG_ERROR("Bridge", "Failed to redirect virtual slot " + std::to_string(VirtualIndex) + " of " + FStringConv::ToHex(Object));
        return false;
    }

    UE_LOG_DISPLAY("Bridge", "Redirected virtual slot " + std::to_string(VirtualIndex) + " of " + FStringConv::ToHex(Object));
    return true;
}

bool FGameThreadBridge::InstallVirtualOverride(FRemoteAddress Object, int32 VirtualIndex, FRemoteAddress RoutineAddress)
{
    if (!IsInitialized() || Object == InvalidRemoteAddress || RoutineAddress == InvalidRemoteAddress || VirtualIndex < 0 ||
        VirtualIndex >= FBridgeLayout::ShadowVirtualTableEntries)
    {
        return false;
    }

    const FRemoteAddress SlotOffset = static_cast<FRemoteAddress>(VirtualIndex) * sizeof(uint64);
    const FRemoteAddress ShadowTable = AcquireShadowVirtualTable(Object);
    if (ShadowTable == InvalidRemoteAddress || !Memory->Write<uint64>(ShadowTable + SlotOffset, RoutineAddress))
    {
        UE_LOG_ERROR("Bridge", "Failed to override virtual slot " + std::to_string(VirtualIndex) + " of " + FStringConv::ToHex(Object));
        return false;
    }

    UE_LOG_DISPLAY("Bridge", "Overrode virtual slot " + std::to_string(VirtualIndex) + " of " + FStringConv::ToHex(Object));
    return true;
}

bool FGameThreadBridge::InstallGameThreadPump(FRemoteAddress Object, int32 VirtualIndex)
{
    if (bGameThreadPumpInstalled || !InstallVirtualPrologueCall(Object, VirtualIndex, PumpRoutineAddress))
    {
        return false;
    }

    bGameThreadPumpInstalled = true;
    return true;
}

FRemoteAddress FGameThreadBridge::EmitRoutine(FX64Emitter& Emitter)
{
    if (!IsInitialized() || !Emitter.Finalize())
    {
        return InvalidRemoteAddress;
    }

    const FRemoteAddress RoutineAddress = Arena->AllocateZeroed(Emitter.GetSize(), 16);
    if (RoutineAddress == InvalidRemoteAddress || !Memory->WriteRaw(RoutineAddress, Emitter.GetBytes().data(), Emitter.GetSize()))
    {
        return InvalidRemoteAddress;
    }

    Memory->FlushInstructionCacheRange(RoutineAddress, Emitter.GetSize());
    return RoutineAddress;
}

int32 FGameThreadBridge::AcquireFreeChannel() const
{
    for (int32 Index = 0; Index < static_cast<int32>(Hooks.size()); ++Index)
    {
        if (!Hooks[static_cast<size_t>(Index)].bActive)
        {
            return Index;
        }
    }

    UE_LOG_ERROR("Bridge", "No free hook channels remain");
    return InvalidIndex;
}

void FGameThreadBridge::ActivateRegistration(int32 ChannelIndex, EHookDispatchMode Mode, FHookHandler Handler, FRemoteAddress StubAddress)
{
    const FRemoteAddress ChannelAddress = GetChannelAddress(ChannelIndex);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelRequestSequence, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelAckSequence, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelVerdict, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelEnabled, 1);

    FHookRegistration& Registration = Hooks[static_cast<size_t>(ChannelIndex)];
    Registration.bActive = true;
    Registration.bDispatching = false;
    Registration.Mode = Mode;
    Registration.Handler = std::move(Handler);
    Registration.ChannelAddress = ChannelAddress;
    Registration.StubAddress = StubAddress;
    Registration.LastHandledSequence = 0;
}

int32 FGameThreadBridge::InstallPointerHook(FRemoteAddress PointerSlot, EHookDispatchMode Mode, FHookHandler Handler)
{
    if (!IsInitialized() || PointerSlot == InvalidRemoteAddress)
    {
        return InvalidIndex;
    }

    const FRemoteAddress OriginalPointer = Memory->ReadPointer(PointerSlot);
    if (OriginalPointer == InvalidRemoteAddress || OriginalPointer == 0)
    {
        UE_LOG_ERROR("Bridge", "Pointer slot " + FStringConv::ToHex(PointerSlot) + " holds no function");
        return InvalidIndex;
    }

    const int32 ChannelIndex = AcquireFreeChannel();
    if (ChannelIndex == InvalidIndex)
    {
        return InvalidIndex;
    }

    const FRemoteAddress StubAddress = Arena->AllocateZeroed(FBridgeLayout::ReservedStubSize, 16);
    if (StubAddress == InvalidRemoteAddress || !EmitHookStub(ChannelIndex, Mode, StubAddress, OriginalPointer))
    {
        return InvalidIndex;
    }

    Memory->FlushInstructionCacheRange(StubAddress, FBridgeLayout::ReservedStubSize);
    ActivateRegistration(ChannelIndex, Mode, std::move(Handler), StubAddress);

    FHookRegistration& Registration = Hooks[static_cast<size_t>(ChannelIndex)];
    Registration.PointerSlot = PointerSlot;
    Registration.OriginalPointer = OriginalPointer;

    if (!Memory->Write<uint64>(PointerSlot, StubAddress))
    {
        UE_LOG_ERROR("Bridge", "Failed to redirect pointer slot " + FStringConv::ToHex(PointerSlot));
        Registration.bActive = false;
        Registration.Handler = nullptr;
        return InvalidIndex;
    }

    return ChannelIndex;
}

bool FGameThreadBridge::RemoveHook(int32 ChannelIndex)
{
    if (ChannelIndex < 0 || ChannelIndex >= static_cast<int32>(Hooks.size()))
    {
        return false;
    }

    FHookRegistration& Registration = Hooks[static_cast<size_t>(ChannelIndex)];
    if (!Registration.bActive)
    {
        return false;
    }

    if (Registration.PointerSlot != InvalidRemoteAddress)
    {
        Memory->Write<uint64>(Registration.PointerSlot, Registration.OriginalPointer);
    }

    Memory->Write<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelEnabled, 0);

    Registration.bActive = false;
    Registration.Handler = nullptr;
    Registration.ChannelAddress = InvalidRemoteAddress;
    Registration.StubAddress = InvalidRemoteAddress;
    Registration.PointerSlot = InvalidRemoteAddress;
    Registration.OriginalPointer = InvalidRemoteAddress;
    return true;
}

void FGameThreadBridge::RemoveAllHooks()
{
    for (int32 Index = 0; Index < static_cast<int32>(Hooks.size()); ++Index)
    {
        RemoveHook(Index);
    }

    RestoreShadowVirtualTables();
    bGameThreadPumpInstalled = false;
}

bool FGameThreadBridge::RewriteHookArgument(int32 ChannelIndex, int32 ArgumentIndex, uint64 Value) const
{
    if (!IsInitialized() || ChannelIndex < 0 || ChannelIndex >= static_cast<int32>(Hooks.size()))
    {
        return false;
    }

    if (ArgumentIndex < 0 || ArgumentIndex >= FBridgeLayout::IntegerArgumentCount)
    {
        return false;
    }

    return Memory->Write<uint64>(GetChannelAddress(ChannelIndex) + FBridgeLayout::ChannelIntegerArgument0 + ArgumentIndex * 8, Value);
}

uint64 FGameThreadBridge::GetPumpSequence() const
{
    if (!IsInitialized())
    {
        return 0;
    }

    return Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::PumpSequence);
}

bool FGameThreadBridge::WaitForGameThreadPump(uint64 TimeoutMilliseconds)
{
    const uint64 StartSequence = GetPumpSequence();
    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        ServicePendingHooks();

        if (GetPumpSequence() != StartSequence)
        {
            return true;
        }

        FWindowsPlatform::SleepMilliseconds(1);
    }

    return false;
}

void FGameThreadBridge::ServicePendingHooks()
{
    if (!IsInitialized())
    {
        return;
    }

    for (size_t Index = 0; Index < Hooks.size(); ++Index)
    {
        FHookRegistration& Registration = Hooks[Index];
        if (!Registration.bActive || !Registration.Handler || Registration.bDispatching)
        {
            continue;
        }

        const uint64 RequestSequence = Memory->Read<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelRequestSequence);
        if (RequestSequence == Registration.LastHandledSequence)
        {
            continue;
        }

        FHookInvocation Invocation;
        Invocation.ChannelIndex = static_cast<int32>(Index);

        for (int32 ArgumentIndex = 0; ArgumentIndex < FBridgeLayout::IntegerArgumentCount; ++ArgumentIndex)
        {
            Invocation.IntegerArguments[ArgumentIndex] = Memory->Read<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelIntegerArgument0 + ArgumentIndex * 8);
        }

        Invocation.FloatArgumentBits[0] = Memory->Read<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelFloatArgument1);
        Invocation.FloatArgumentBits[1] = Memory->Read<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelFloatArgument2);

        Registration.LastHandledSequence = RequestSequence;
        Registration.bDispatching = true;

        const FHookHandler Handler = Registration.Handler;
        const FHookResponse Response = Handler(Invocation);

        FHookRegistration& Current = Hooks[Index];
        Current.bDispatching = false;

        if (Current.bActive && Current.Mode == EHookDispatchMode::Blocking)
        {
            Memory->Write<uint64>(Current.ChannelAddress + FBridgeLayout::ChannelReturnValue, Response.ReturnValue);
            Memory->Write<uint64>(Current.ChannelAddress + FBridgeLayout::ChannelVerdict, Response.bSkipOriginal ? 1 : 0);
            Memory->Write<uint64>(Current.ChannelAddress + FBridgeLayout::ChannelAckSequence, RequestSequence);
        }
    }
}

bool FGameThreadBridge::BeginGameThreadHold(uint64 TimeoutMilliseconds)
{
    if (!IsInitialized())
    {
        return false;
    }

    if (HoldDepth > 0)
    {
        ++HoldDepth;
        return true;
    }

    HoldDepth = 1;
    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::PumpHoldRequest, 1);

    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::PumpHoldEngaged) != 0)
        {
            return true;
        }

        ServicePendingHooks();
        FWindowsPlatform::YieldThread();
    }

    return false;
}

void FGameThreadBridge::EndGameThreadHold()
{
    if (!IsInitialized() || HoldDepth == 0)
    {
        return;
    }

    --HoldDepth;

    if (HoldDepth == 0)
    {
        Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::PumpHoldRequest, 0);
    }
}

FGameThreadHoldScope::FGameThreadHoldScope(FGameThreadBridge& InBridge, uint64 TimeoutMilliseconds)
    : Bridge(InBridge)
{
    bHeld = Bridge.BeginGameThreadHold(TimeoutMilliseconds);
}

FGameThreadHoldScope::~FGameThreadHoldScope()
{
    Bridge.EndGameThreadHold();
}

bool FGameThreadHoldScope::IsHeld() const
{
    return bHeld;
}

bool FGameThreadBridge::WaitForGameThread(FRemoteAddress Function, const std::function<bool()>& IsComplete, bool bServiceHooks)
{
    uint64 NextReport = FWindowsPlatform::GetTimeMilliseconds() + BusyReportIntervalMilliseconds;

    while (!IsComplete())
    {
        const FProcessAttachment* Process = Memory->GetProcess();
        if (Process == nullptr || !Process->IsAlive())
        {
            return false;
        }

        const uint64 Now = FWindowsPlatform::GetTimeMilliseconds();
        if (Now >= NextReport)
        {
            NextReport = Now + BusyReportIntervalMilliseconds;
            UE_LOG_WARNING("Bridge", "The game thread is still busy, waiting to run " + FStringConv::ToHex(Function));
        }

        if (bServiceHooks)
        {
            ServicePendingHooks();
        }

        FWindowsPlatform::YieldThread();
    }

    return true;
}

uint64 FGameThreadBridge::CallFunction(FRemoteAddress Function, const std::vector<uint64>& Arguments)
{
    if (!IsInitialized() || Function == InvalidRemoteAddress)
    {
        return 0;
    }

    if (Arguments.size() > static_cast<size_t>(FBridgeLayout::MaximumCommandArguments))
    {
        UE_LOG_ERROR("Bridge", "Remote call exceeds the supported argument count");
        return 0;
    }

    const auto IsDoorbellReleased = [this]() { return Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CommandDoorbell) == 0; };

    if (!WaitForGameThread(Function, IsDoorbellReleased, true))
    {
        return 0;
    }

    const uint64 Ticket = ++NextCommandTicket;

    for (int32 Index = 0; Index < FBridgeLayout::MaximumCommandArguments; ++Index)
    {
        const uint64 Value = static_cast<size_t>(Index) < Arguments.size() ? Arguments[static_cast<size_t>(Index)] : 0;
        Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandArgument0 + Index * 8, Value);
    }

    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandTarget, Function);
    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandTicket, Ticket);
    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandDoorbell, 1);

    const auto IsTicketCompleted = [this, Ticket]() { return Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CompletedTicket) == Ticket; };

    if (!WaitForGameThread(Function, IsTicketCompleted, false))
    {
        UE_LOG_ERROR("Bridge", "The game process exited during the remote call to " + FStringConv::ToHex(Function));
        return 0;
    }

    return Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CommandResult);
}
