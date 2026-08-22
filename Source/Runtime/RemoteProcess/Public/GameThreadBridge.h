#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Runtime/RemoteProcess/Public/RemoteDetour.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <functional>

namespace FBridgeLayout
{
    inline constexpr int32 PumpSequence = 0x00;
    inline constexpr int32 CommandDoorbell = 0x08;
    inline constexpr int32 CommandTarget = 0x10;
    inline constexpr int32 CommandArgument0 = 0x18;
    inline constexpr int32 CommandResult = 0x58;
    inline constexpr int32 CommandResultFloat = 0x60;
    inline constexpr int32 CommandCompletion = 0x68;
    inline constexpr int32 PumpHoldRequest = 0x70;
    inline constexpr int32 ControlBlockSize = 0x80;

    inline constexpr int32 ChannelRequestSequence = 0x00;
    inline constexpr int32 ChannelAckSequence = 0x08;
    inline constexpr int32 ChannelIntegerArgument0 = 0x10;
    inline constexpr int32 ChannelFloatArgument1 = 0x30;
    inline constexpr int32 ChannelFloatArgument2 = 0x38;
    inline constexpr int32 ChannelReturnValue = 0x40;
    inline constexpr int32 ChannelVerdict = 0x48;
    inline constexpr int32 ChannelEnabled = 0x50;
    inline constexpr int32 ChannelSize = 0x60;

    inline constexpr int32 MaximumChannels = 32;
    inline constexpr int32 MaximumCommandArguments = 8;
    inline constexpr int32 IntegerArgumentCount = 4;
    inline constexpr size_t ReservedStubSize = 512;
}

enum class EHookDispatchMode : uint8
{
    PumpOnly,
    Blocking
};

struct FHookInvocation
{
    int32 ChannelIndex = InvalidIndex;
    uint64 IntegerArguments[FBridgeLayout::IntegerArgumentCount] = {};
    uint64 FloatArgumentBits[2] = {};

    float GetFloatArgument(int32 Index) const;
};

struct FHookResponse
{
    bool bSkipOriginal = false;
    uint64 ReturnValue = 0;

    static FHookResponse ContinueToOriginal();

    static FHookResponse OverrideWith(uint64 Value);
};

using FHookHandler = std::function<FHookResponse(const FHookInvocation&)>;

class FGameThreadBridge
{
public:
    bool Initialize(const FRemoteMemory& InMemory, FRemoteAllocation& InArena);

    void Shutdown();

    bool IsInitialized() const;

    int32 InstallHook(FRemoteAddress TargetFunction, EHookDispatchMode Mode, FHookHandler Handler);

    bool InstallConstantHook(FRemoteAddress TargetFunction, uint64 ReturnValue);

    bool RewriteHookArgument(int32 ChannelIndex, int32 ArgumentIndex, uint64 Value) const;

    bool RemoveHook(int32 ChannelIndex);

    void RemoveAllHooks();

    bool WaitForGameThreadPump(uint64 TimeoutMilliseconds);

    uint64 GetPumpSequence() const;

    void ServicePendingHooks();

    bool BeginGameThreadHold(uint64 TimeoutMilliseconds);

    void EndGameThreadHold();

    uint64 CallFunction(FRemoteAddress Function, const std::vector<uint64>& Arguments, uint64 TimeoutMilliseconds = 10000);

    FRemoteAllocation& GetArena() const;

    const FRemoteMemory& GetMemory() const;

private:
    struct FHookRegistration
    {
        bool bActive = false;
        EHookDispatchMode Mode = EHookDispatchMode::PumpOnly;
        FHookHandler Handler;
        FRemoteDetour Detour;
        FRemoteAddress ChannelAddress = InvalidRemoteAddress;
        FRemoteAddress StubAddress = InvalidRemoteAddress;
        uint64 LastHandledSequence = 0;
    };

    bool EmitPumpRoutine();

    bool EmitHookStub(int32 ChannelIndex, EHookDispatchMode Mode, FRemoteAddress StubAddress, FRemoteAddress TrampolineAddress);

    void EmitCommandDispatch(FX64Emitter& Emitter, size_t LoopLabel) const;

    FRemoteAddress GetChannelAddress(int32 ChannelIndex) const;

    const FRemoteMemory* Memory = nullptr;
    FRemoteAllocation* Arena = nullptr;
    FRemoteAddress ControlBlockAddress = InvalidRemoteAddress;
    FRemoteAddress ChannelArrayAddress = InvalidRemoteAddress;
    FRemoteAddress PumpRoutineAddress = InvalidRemoteAddress;
    std::vector<FHookRegistration> Hooks;
    FX64AbiLayout Abi;
    std::vector<FRemoteDetour> ConstantHooks;
    uint64 HoldDepth = 0;
    bool bServicingHooks = false;
};

class FGameThreadHoldScope
{
public:
    explicit FGameThreadHoldScope(FGameThreadBridge& InBridge, uint64 TimeoutMilliseconds = 5000);

    ~FGameThreadHoldScope();

    FGameThreadHoldScope(const FGameThreadHoldScope&) = delete;
    FGameThreadHoldScope& operator=(const FGameThreadHoldScope&) = delete;

    bool IsHeld() const;

private:
    FGameThreadBridge& Bridge;
    bool bHeld = false;
};
