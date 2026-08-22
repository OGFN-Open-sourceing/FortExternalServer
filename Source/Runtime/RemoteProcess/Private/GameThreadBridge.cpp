#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <cstring>

namespace
{
    constexpr int32 StubFrameSize = 0x88;
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

    constexpr EX64Register IntegerArgumentRegisters[FBridgeLayout::IntegerArgumentCount] = { EX64Register::Rcx, EX64Register::Rdx, EX64Register::R8,
        EX64Register::R9 };
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

bool FGameThreadBridge::Initialize(const FRemoteMemory& InMemory, FRemoteAllocation& InArena)
{
    Shutdown();

    Memory = &InMemory;
    Arena = &InArena;

    ControlBlockAddress = Arena->AllocateZeroed(FBridgeLayout::ControlBlockSize, 64);
    ChannelArrayAddress = Arena->AllocateZeroed(static_cast<size_t>(FBridgeLayout::ChannelSize) * FBridgeLayout::MaximumChannels, 64);

    if (ControlBlockAddress == InvalidRemoteAddress || ChannelArrayAddress == InvalidRemoteAddress)
    {
        return false;
    }

    if (!EmitPumpRoutine())
    {
        UE_LOG_ERROR("Bridge", "Failed to emit the game thread pump routine");
        return false;
    }

    Hooks.resize(FBridgeLayout::MaximumChannels);

    UE_LOG_DISPLAY("Bridge", "Control block at " + FStringConv::ToHex(ControlBlockAddress) + ", pump routine at " + FStringConv::ToHex(PumpRoutineAddress));
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

    Emitter.CallRegister(EX64Register::R10);

    Emitter.MoveRegisterImmediate(EX64Register::R10, ControlBlockAddress);
    Emitter.StoreRegisterToMemory(EX64Register::R10, FBridgeLayout::CommandResult, EX64Register::Rax);
    Emitter.StoreXmmQwordToMemory(EX64Register::R10, FBridgeLayout::CommandResultFloat, 0);
    Emitter.XorRegisterRegister(EX64Register::Rax, EX64Register::Rax);
    Emitter.StoreRegisterToMemory(EX64Register::R10, FBridgeLayout::CommandDoorbell, EX64Register::Rax);
    Emitter.IncrementMemory(EX64Register::R10, FBridgeLayout::CommandCompletion);
    Emitter.JumpToLabel(LoopLabel);
}

bool FGameThreadBridge::EmitPumpRoutine()
{
    FX64Emitter Emitter;

    const FX64Emitter::FLabelId LabelLoop = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelCheckHold = Emitter.CreateLabel();
    const FX64Emitter::FLabelId LabelExit = Emitter.CreateLabel();

    Emitter.SubRegisterImmediate(EX64Register::Rsp, PumpFrameSize);

    Emitter.BindLabel(LabelLoop);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::CommandDoorbell);
    Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, LabelCheckHold);

    EmitCommandDispatch(Emitter, LabelLoop);

    Emitter.BindLabel(LabelCheckHold);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rax, FBridgeLayout::PumpHoldRequest);
    Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, LabelExit);
    Emitter.EmitPause();
    Emitter.JumpToLabel(LabelLoop);

    Emitter.BindLabel(LabelExit);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ControlBlockAddress);
    Emitter.IncrementMemory(EX64Register::Rax, FBridgeLayout::PumpSequence);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, PumpFrameSize);
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

    Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedRcx, EX64Register::Rcx);
    Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedRdx, EX64Register::Rdx);
    Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedR8, EX64Register::R8);
    Emitter.StoreRegisterToMemory(EX64Register::Rsp, StubSavedR9, EX64Register::R9);
    Emitter.SaveXmmToMemory(0, EX64Register::Rsp, StubSavedXmm0);
    Emitter.SaveXmmToMemory(1, EX64Register::Rsp, StubSavedXmm1);
    Emitter.SaveXmmToMemory(2, EX64Register::Rsp, StubSavedXmm2);
    Emitter.SaveXmmToMemory(3, EX64Register::Rsp, StubSavedXmm3);

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
    Emitter.LoadXmmFromMemory(0, EX64Register::Rsp, StubSavedXmm0);
    Emitter.LoadXmmFromMemory(1, EX64Register::Rsp, StubSavedXmm1);
    Emitter.LoadXmmFromMemory(2, EX64Register::Rsp, StubSavedXmm2);
    Emitter.LoadXmmFromMemory(3, EX64Register::Rsp, StubSavedXmm3);
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

bool FGameThreadBridge::InstallConstantHook(FRemoteAddress TargetFunction, uint64 ReturnValue)
{
    if (!IsInitialized() || TargetFunction == InvalidRemoteAddress)
    {
        return false;
    }

    const FRemoteAddress StubAddress = Arena->AllocateZeroed(FBridgeLayout::ReservedStubSize, 16);
    if (StubAddress == InvalidRemoteAddress)
    {
        return false;
    }

    FX64Emitter Emitter;
    Emitter.MoveRegisterImmediate(EX64Register::Rax, ReturnValue);
    Emitter.EmitReturn();

    if (!Emitter.Finalize() || !Memory->WriteRaw(StubAddress, Emitter.GetBytes().data(), Emitter.GetBytes().size()))
    {
        return false;
    }

    FRemoteDetour Detour;
    if (!Detour.Install(*Memory, *Arena, TargetFunction, StubAddress))
    {
        UE_LOG_ERROR("Bridge", "Failed to detour " + FStringConv::ToHex(TargetFunction) + " with a constant return");
        return false;
    }

    Memory->FlushInstructionCacheRange(StubAddress, Emitter.GetBytes().size());
    ConstantHooks.push_back(std::move(Detour));

    UE_LOG_DISPLAY("Bridge", "Installed constant return hook on " + FStringConv::ToHex(TargetFunction));
    return true;
}

int32 FGameThreadBridge::InstallHook(FRemoteAddress TargetFunction, EHookDispatchMode Mode, FHookHandler Handler)
{
    if (!IsInitialized() || TargetFunction == InvalidRemoteAddress)
    {
        return InvalidIndex;
    }

    int32 ChannelIndex = InvalidIndex;
    for (int32 Index = 0; Index < static_cast<int32>(Hooks.size()); ++Index)
    {
        if (!Hooks[static_cast<size_t>(Index)].bActive)
        {
            ChannelIndex = Index;
            break;
        }
    }

    if (ChannelIndex == InvalidIndex)
    {
        UE_LOG_ERROR("Bridge", "No free hook channels remain");
        return InvalidIndex;
    }

    const FRemoteAddress StubAddress = Arena->AllocateZeroed(FBridgeLayout::ReservedStubSize, 16);
    if (StubAddress == InvalidRemoteAddress)
    {
        return InvalidIndex;
    }

    FHookRegistration& Registration = Hooks[static_cast<size_t>(ChannelIndex)];

    if (!Registration.Detour.Install(*Memory, *Arena, TargetFunction, StubAddress))
    {
        UE_LOG_ERROR("Bridge", "Failed to detour " + FStringConv::ToHex(TargetFunction));
        return InvalidIndex;
    }

    if (!EmitHookStub(ChannelIndex, Mode, StubAddress, Registration.Detour.GetTrampolineAddress()))
    {
        Registration.Detour.Uninstall(*Memory);
        return InvalidIndex;
    }

    Memory->FlushInstructionCacheRange(StubAddress, FBridgeLayout::ReservedStubSize);

    const FRemoteAddress ChannelAddress = GetChannelAddress(ChannelIndex);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelRequestSequence, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelAckSequence, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelVerdict, 0);
    Memory->Write<uint64>(ChannelAddress + FBridgeLayout::ChannelEnabled, 1);

    Registration.bActive = true;
    Registration.Mode = Mode;
    Registration.Handler = std::move(Handler);
    Registration.ChannelAddress = ChannelAddress;
    Registration.StubAddress = StubAddress;
    Registration.LastHandledSequence = 0;

    UE_LOG_DISPLAY("Bridge", "Installed hook channel " + std::to_string(ChannelIndex) + " on " + FStringConv::ToHex(TargetFunction));
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

    Registration.Detour.Uninstall(*Memory);
    Memory->Write<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelEnabled, 0);

    Registration.bActive = false;
    Registration.Handler = nullptr;
    Registration.ChannelAddress = InvalidRemoteAddress;
    Registration.StubAddress = InvalidRemoteAddress;
    return true;
}

void FGameThreadBridge::RemoveAllHooks()
{
    for (int32 Index = 0; Index < static_cast<int32>(Hooks.size()); ++Index)
    {
        RemoveHook(Index);
    }

    for (FRemoteDetour& Detour : ConstantHooks)
    {
        Detour.Uninstall(*Memory);
    }

    ConstantHooks.clear();
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
    if (!IsInitialized() || bServicingHooks)
    {
        return;
    }

    bServicingHooks = true;

    for (size_t Index = 0; Index < Hooks.size(); ++Index)
    {
        FHookRegistration& Registration = Hooks[Index];
        if (!Registration.bActive || !Registration.Handler)
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

        const FHookResponse Response = Registration.Handler(Invocation);

        if (Registration.Mode == EHookDispatchMode::Blocking)
        {
            Memory->Write<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelReturnValue, Response.ReturnValue);
            Memory->Write<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelVerdict, Response.bSkipOriginal ? 1 : 0);
            Memory->Write<uint64>(Registration.ChannelAddress + FBridgeLayout::ChannelAckSequence, RequestSequence);
        }
    }

    bServicingHooks = false;
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

    const bool bGameThreadDrivingPump = WaitForGameThreadPump(TimeoutMilliseconds);

    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::PumpHoldRequest, 1);
    HoldDepth = 1;

    return bGameThreadDrivingPump;
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

uint64 FGameThreadBridge::CallFunction(FRemoteAddress Function, const std::vector<uint64>& Arguments, uint64 TimeoutMilliseconds)
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

    for (int32 Index = 0; Index < FBridgeLayout::MaximumCommandArguments; ++Index)
    {
        const uint64 Value = static_cast<size_t>(Index) < Arguments.size() ? Arguments[static_cast<size_t>(Index)] : 0;
        Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandArgument0 + Index * 8, Value);
    }

    const uint64 StartCompletion = Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CommandCompletion);

    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandTarget, Function);
    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandDoorbell, 1);

    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CommandCompletion) != StartCompletion)
        {
            return Memory->Read<uint64>(ControlBlockAddress + FBridgeLayout::CommandResult);
        }

        ServicePendingHooks();
        FWindowsPlatform::YieldThread();
    }

    Memory->Write<uint64>(ControlBlockAddress + FBridgeLayout::CommandDoorbell, 0);
    UE_LOG_ERROR("Bridge", "Remote call to " + FStringConv::ToHex(Function) + " timed out");
    return 0;
}
