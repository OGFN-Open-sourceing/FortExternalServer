#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

#include <functional>

class FX64Emitter;

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
    inline constexpr int32 PumpHoldEngaged = 0x78;
    inline constexpr int32 CommandTicket = 0x80;
    inline constexpr int32 CompletedTicket = 0x88;
    inline constexpr int32 PumpDepth = 0x90;
    inline constexpr int32 GameThreadId = 0x98;
    inline constexpr int32 ControlBlockSize = 0xC0;

    inline constexpr int32 ChannelRequestSequence = 0x00;
    inline constexpr int32 ChannelAckSequence = 0x08;
    inline constexpr int32 ChannelIntegerArgument0 = 0x10;
    inline constexpr int32 ChannelFloatArgument1 = 0x30;
    inline constexpr int32 ChannelFloatArgument2 = 0x38;
    inline constexpr int32 ChannelReturnValue = 0x40;
    inline constexpr int32 ChannelVerdict = 0x48;
    inline constexpr int32 ChannelEnabled = 0x50;
    inline constexpr int32 ChannelSize = 0x60;

    inline constexpr int32 MaximumChannels = 64;
    inline constexpr int32 MaximumCommandArguments = 8;
    inline constexpr int32 IntegerArgumentCount = 4;
    inline constexpr size_t ReservedStubSize = 512;
    inline constexpr int32 ShadowVirtualTableEntries = 0x400;
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
    bool Initialize(const FRemoteMemory& InMemory, FRemoteAllocation& InArena, uint32 InGameThreadId);

    void Shutdown();

    bool IsInitialized() const;

    int32 FindVirtualIndex(FRemoteAddress Object, FRemoteAddress Function) const;

    bool InstallGameThreadPump(FRemoteAddress Object, int32 VirtualIndex);

    bool InstallVirtualPrologueCall(FRemoteAddress Object, int32 VirtualIndex, FRemoteAddress RoutineAddress);

    bool InstallVirtualOverride(FRemoteAddress Object, int32 VirtualIndex, FRemoteAddress RoutineAddress);

    int32 InstallPointerHook(FRemoteAddress PointerSlot, EHookDispatchMode Mode, FHookHandler Handler);

    FRemoteAddress EmitRoutine(FX64Emitter& Emitter);

    bool RewriteHookArgument(int32 ChannelIndex, int32 ArgumentIndex, uint64 Value) const;

    bool RemoveHook(int32 ChannelIndex);

    void RemoveAllHooks();

    bool WaitForGameThreadPump(uint64 TimeoutMilliseconds);

    uint64 GetPumpSequence() const;

    void ServicePendingHooks();

    bool BeginGameThreadHold(uint64 TimeoutMilliseconds);

    void EndGameThreadHold();

    uint64 CallFunction(FRemoteAddress Function, const std::vector<uint64>& Arguments);

    FRemoteAllocation& GetArena() const;

    const FRemoteMemory& GetMemory() const;

private:
    struct FHookRegistration
    {
        bool bActive = false;
        bool bDispatching = false;
        EHookDispatchMode Mode = EHookDispatchMode::PumpOnly;
        FHookHandler Handler;
        FRemoteAddress ChannelAddress = InvalidRemoteAddress;
        FRemoteAddress StubAddress = InvalidRemoteAddress;
        FRemoteAddress PointerSlot = InvalidRemoteAddress;
        FRemoteAddress OriginalPointer = InvalidRemoteAddress;
        uint64 LastHandledSequence = 0;
    };

    struct FShadowVirtualTable
    {
        FRemoteAddress Object = InvalidRemoteAddress;
        FRemoteAddress OriginalTable = InvalidRemoteAddress;
        FRemoteAddress ShadowTable = InvalidRemoteAddress;
    };

    FRemoteAddress AcquireShadowVirtualTable(FRemoteAddress Object);

    void RestoreShadowVirtualTables();

    int32 AcquireFreeChannel() const;

    void ActivateRegistration(int32 ChannelIndex, EHookDispatchMode Mode, FHookHandler Handler, FRemoteAddress StubAddress);

    bool EmitPumpRoutine();

    bool EmitHookStub(int32 ChannelIndex, EHookDispatchMode Mode, FRemoteAddress StubAddress, FRemoteAddress TrampolineAddress);

    bool EmitPrologueCallStub(FRemoteAddress StubAddress, FRemoteAddress RoutineAddress, FRemoteAddress TrampolineAddress);

    void EmitCommandDispatch(FX64Emitter& Emitter, size_t LoopLabel) const;

    FRemoteAddress GetChannelAddress(int32 ChannelIndex) const;

    bool WaitForGameThread(FRemoteAddress Function, const std::function<bool()>& IsComplete, bool bServiceHooks);

    const FRemoteMemory* Memory = nullptr;
    FRemoteAllocation* Arena = nullptr;
    FRemoteAddress ControlBlockAddress = InvalidRemoteAddress;
    FRemoteAddress ChannelArrayAddress = InvalidRemoteAddress;
    FRemoteAddress PumpRoutineAddress = InvalidRemoteAddress;
    std::vector<FHookRegistration> Hooks;
    std::vector<FShadowVirtualTable> ShadowVirtualTables;
    bool bGameThreadPumpInstalled = false;
    uint64 NextCommandTicket = 0;
    uint64 HoldDepth = 0;
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
