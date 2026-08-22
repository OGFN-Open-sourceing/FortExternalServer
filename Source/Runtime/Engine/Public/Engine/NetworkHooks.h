#pragma once

#include "Runtime/Engine/Public/Engine/EngineRuntime.h"
#include "Runtime/Engine/Public/Engine/World.h"

#include <functional>

using FFrameTickDelegate = std::function<void()>;
using FControlMessageDelegate = std::function<bool(const UNetConnection&, ENetworkMessageType, FRemoteAddress)>;

class FNetworkHooks
{
public:
    bool Install(FEngineRuntime& InEngineRuntime);

    void Uninstall();

    bool IsInstalled() const;

    void SetFrameTickDelegate(FFrameTickDelegate Delegate);

    void SetFrameTickInterval(uint64 IntervalMilliseconds);

    void SetControlMessageDelegate(FControlMessageDelegate Delegate);

    void SetTravelCompleted(bool bCompleted);

    bool HasTravelCompleted() const;

    uint64 GetFrameCounter() const;

    void Service();

    bool WelcomePlayer(const UNetConnection& Connection) const;

    bool ReceiveLoginPayload(const UNetConnection& Connection, FRemoteAddress BunchAddress) const;

private:
    bool InstallConstantOverrides();

    bool InstallDispatchHooks();

    FHookResponse HandleTickFlush(const FHookInvocation& Invocation);

    FHookResponse HandleWorldNotifyControlMessage(const FHookInvocation& Invocation);

    FHookResponse HandleWelcomePlayer(const FHookInvocation& Invocation);

    FHookResponse HandleBeaconNotifyControlMessage(const FHookInvocation& Invocation);

    FHookResponse HandleLocalPlayerSpawnPlayActor(const FHookInvocation& Invocation);

    FEngineRuntime* EngineRuntime = nullptr;
    FFrameTickDelegate FrameTickDelegate;
    FControlMessageDelegate ControlMessageDelegate;

    int32 TickFlushChannel = InvalidIndex;
    int32 WorldControlMessageChannel = InvalidIndex;
    int32 WelcomePlayerChannel = InvalidIndex;
    int32 BeaconControlMessageChannel = InvalidIndex;
    int32 LocalPlayerSpawnChannel = InvalidIndex;

    bool bInstalled = false;
    bool bTravelCompleted = false;
    uint64 FrameCounter = 0;
    uint64 FrameTickIntervalMilliseconds = 100;
    uint64 LastFrameTickTimestamp = 0;
};
