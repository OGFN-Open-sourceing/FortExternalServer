#include "Runtime/Engine/Public/Engine/NetworkHooks.h"

namespace
{
    constexpr int32 LoginBunchOverflowSlot = 7;
    constexpr int64 LoginBunchOverflowBytes = 16 * 1024 * 1024;
    constexpr int32 LoginNetSpeed = 30000;
}

bool FNetworkHooks::Install(FEngineRuntime& InEngineRuntime)
{
    if (bInstalled)
    {
        return true;
    }

    EngineRuntime = &InEngineRuntime;

    if (!InstallConstantOverrides())
    {
        return false;
    }

    if (!InstallDispatchHooks())
    {
        return false;
    }

    bInstalled = true;
    UE_LOG_DISPLAY("NetworkHooks", "Listen server hooks are active");
    return true;
}

void FNetworkHooks::Uninstall()
{
    if (!bInstalled || EngineRuntime == nullptr)
    {
        return;
    }

    EngineRuntime->GetUnrealRuntime().GetBridge().RemoveAllHooks();

    TickFlushChannel = InvalidIndex;
    WorldControlMessageChannel = InvalidIndex;
    WelcomePlayerChannel = InvalidIndex;
    BeaconControlMessageChannel = InvalidIndex;
    LocalPlayerSpawnChannel = InvalidIndex;
    bInstalled = false;
}

bool FNetworkHooks::IsInstalled() const
{
    return bInstalled;
}

void FNetworkHooks::SetFrameTickDelegate(FFrameTickDelegate Delegate)
{
    FrameTickDelegate = std::move(Delegate);
}

void FNetworkHooks::SetControlMessageDelegate(FControlMessageDelegate Delegate)
{
    ControlMessageDelegate = std::move(Delegate);
}

void FNetworkHooks::SetTravelCompleted(bool bCompleted)
{
    bTravelCompleted = bCompleted;
}

bool FNetworkHooks::HasTravelCompleted() const
{
    return bTravelCompleted;
}

uint64 FNetworkHooks::GetFrameCounter() const
{
    return FrameCounter;
}

void FNetworkHooks::Service()
{
    if (EngineRuntime != nullptr)
    {
        EngineRuntime->GetUnrealRuntime().GetBridge().ServicePendingHooks();
    }
}

bool FNetworkHooks::InstallConstantOverrides()
{
    FGameThreadBridge& Bridge = EngineRuntime->GetUnrealRuntime().GetBridge();
    const FEngineFunctions& Functions = EngineRuntime->GetFunctions();

    if (!Bridge.InstallConstantHook(Functions.ActorGetNetMode, static_cast<uint64>(ENetMode::ListenServer)))
    {
        UE_LOG_ERROR("NetworkHooks", "Failed to force the net mode to listen server");
        return false;
    }

    if (Functions.OnlineSessionKickPlayer != InvalidRemoteAddress)
    {
        Bridge.InstallConstantHook(Functions.OnlineSessionKickPlayer, 0);
    }

    if (Functions.CollectGarbage != InvalidRemoteAddress)
    {
        Bridge.InstallConstantHook(Functions.CollectGarbage, 0);
    }

    if (Functions.NetDriverNetDebug != InvalidRemoteAddress)
    {
        Bridge.InstallConstantHook(Functions.NetDriverNetDebug, 0);
    }

    return true;
}

bool FNetworkHooks::InstallDispatchHooks()
{
    FGameThreadBridge& Bridge = EngineRuntime->GetUnrealRuntime().GetBridge();
    const FEngineFunctions& Functions = EngineRuntime->GetFunctions();

    TickFlushChannel = Bridge.InstallHook(Functions.NetDriverTickFlush, EHookDispatchMode::Blocking,
        [this](const FHookInvocation& Invocation) { return HandleTickFlush(Invocation); });

    if (TickFlushChannel == InvalidIndex)
    {
        UE_LOG_ERROR("NetworkHooks", "Failed to hook the net driver tick, the game thread pump would never run");
        return false;
    }

    WorldControlMessageChannel = Bridge.InstallHook(Functions.WorldNotifyControlMessage, EHookDispatchMode::Blocking,
        [this](const FHookInvocation& Invocation) { return HandleWorldNotifyControlMessage(Invocation); });

    WelcomePlayerChannel = Bridge.InstallHook(Functions.WorldWelcomePlayer, EHookDispatchMode::Blocking,
        [this](const FHookInvocation& Invocation) { return HandleWelcomePlayer(Invocation); });

    if (Functions.BeaconHostNotifyControlMessage != InvalidRemoteAddress)
    {
        BeaconControlMessageChannel = Bridge.InstallHook(Functions.BeaconHostNotifyControlMessage, EHookDispatchMode::Blocking,
            [this](const FHookInvocation& Invocation) { return HandleBeaconNotifyControlMessage(Invocation); });
    }

    if (Functions.LocalPlayerSpawnPlayActor != InvalidRemoteAddress)
    {
        LocalPlayerSpawnChannel = Bridge.InstallHook(Functions.LocalPlayerSpawnPlayActor, EHookDispatchMode::Blocking,
            [this](const FHookInvocation& Invocation) { return HandleLocalPlayerSpawnPlayActor(Invocation); });
    }

    return true;
}

FHookResponse FNetworkHooks::HandleTickFlush(const FHookInvocation& Invocation)
{
    ++FrameCounter;

    const uint64 Now = FPlatformMisc::GetTimeMilliseconds();

    if (FrameTickDelegate && Now - LastFrameTickTimestamp >= FrameTickIntervalMilliseconds)
    {
        LastFrameTickTimestamp = Now;
        FrameTickDelegate();
    }

    return FHookResponse::ContinueToOriginal();
}

void FNetworkHooks::SetFrameTickInterval(uint64 IntervalMilliseconds)
{
    FrameTickIntervalMilliseconds = IntervalMilliseconds;
}

FHookResponse FNetworkHooks::HandleWorldNotifyControlMessage(const FHookInvocation& Invocation)
{
    const UWorld World = EngineRuntime->GetWorld();
    if (World)
    {
        EngineRuntime->GetUnrealRuntime().GetBridge().RewriteHookArgument(Invocation.ChannelIndex, 0, World.GetAddress());
    }

    if (ControlMessageDelegate)
    {
        const UNetConnection Connection(EngineRuntime->GetUnrealRuntime().MakeHandle(Invocation.IntegerArguments[1]));
        const ENetworkMessageType MessageType = static_cast<ENetworkMessageType>(Invocation.IntegerArguments[2] & 0xFF);

        if (ControlMessageDelegate(Connection, MessageType, Invocation.IntegerArguments[3]))
        {
            return FHookResponse::OverrideWith(0);
        }
    }

    return FHookResponse::ContinueToOriginal();
}

FHookResponse FNetworkHooks::HandleWelcomePlayer(const FHookInvocation& Invocation)
{
    const UWorld World = EngineRuntime->GetWorld();
    if (World)
    {
        EngineRuntime->GetUnrealRuntime().GetBridge().RewriteHookArgument(Invocation.ChannelIndex, 0, World.GetAddress());
    }

    return FHookResponse::ContinueToOriginal();
}

FHookResponse FNetworkHooks::HandleBeaconNotifyControlMessage(const FHookInvocation& Invocation)
{
    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    const UNetConnection Connection(Runtime.MakeHandle(Invocation.IntegerArguments[1]));
    const ENetworkMessageType MessageType = static_cast<ENetworkMessageType>(Invocation.IntegerArguments[2] & 0xFF);
    const FRemoteAddress BunchAddress = Invocation.IntegerArguments[3];

    if (MessageType == ENetworkMessageType::Netspeed)
    {
        UNetConnection MutableConnection = Connection;
        MutableConnection.SetCurrentNetSpeed(LoginNetSpeed);
        return FHookResponse::OverrideWith(0);
    }

    if (MessageType == ENetworkMessageType::Login)
    {
        if (!ReceiveLoginPayload(Connection, BunchAddress))
        {
            return FHookResponse::OverrideWith(0);
        }

        WelcomePlayer(Connection);
        return FHookResponse::OverrideWith(0);
    }

    if (MessageType == ENetworkMessageType::PCSwap)
    {
        return FHookResponse::OverrideWith(0);
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (World)
    {
        Runtime.GetBridge().RewriteHookArgument(Invocation.ChannelIndex, 0, World.GetAddress());
    }

    return FHookResponse::ContinueToOriginal();
}

FHookResponse FNetworkHooks::HandleLocalPlayerSpawnPlayActor(const FHookInvocation& Invocation)
{
    if (bTravelCompleted)
    {
        return FHookResponse::OverrideWith(1);
    }

    return FHookResponse::ContinueToOriginal();
}

bool FNetworkHooks::WelcomePlayer(const UNetConnection& Connection) const
{
    const FEngineFunctions& Functions = EngineRuntime->GetFunctions();
    const UWorld World = EngineRuntime->GetWorld();

    if (!World || !Connection || Functions.WorldWelcomePlayer == InvalidRemoteAddress)
    {
        return false;
    }

    EngineRuntime->GetUnrealRuntime().GetBridge().CallFunction(Functions.WorldWelcomePlayer, { World.GetAddress(), Connection.GetAddress() });
    return true;
}

bool FNetworkHooks::ReceiveLoginPayload(const UNetConnection& Connection, FRemoteAddress BunchAddress) const
{
    const FEngineFunctions& Functions = EngineRuntime->GetFunctions();
    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    if (BunchAddress == InvalidRemoteAddress || Functions.NetConnectionReceiveFString == InvalidRemoteAddress ||
        Functions.NetConnectionReceiveUniqueIdRepl == InvalidRemoteAddress)
    {
        return false;
    }

    const FRemoteAddress OverflowSlot = BunchAddress + static_cast<uint64>(LoginBunchOverflowSlot) * sizeof(int64);
    const int64 OriginalLimit = Runtime.GetMemory().Read<int64>(OverflowSlot);
    Runtime.GetMemory().Write<int64>(OverflowSlot, OriginalLimit + LoginBunchOverflowBytes);

    const FRemoteAddress ClientResponse = Connection.GetClientResponseAddress();
    const FRemoteAddress RequestUrl = Connection.GetRequestUrlAddress();
    const FRemoteAddress PlayerId = Connection.GetPlayerIdAddress();
    const FRemoteAddress PlatformName = Runtime.AcquireScratch(FUnrealLayout::FScriptArray_Size);

    FGameThreadBridge& Bridge = Runtime.GetBridge();

    if (ClientResponse != InvalidRemoteAddress)
    {
        Bridge.CallFunction(Functions.NetConnectionReceiveFString, { BunchAddress, ClientResponse });
    }

    if (RequestUrl != InvalidRemoteAddress)
    {
        Bridge.CallFunction(Functions.NetConnectionReceiveFString, { BunchAddress, RequestUrl });
    }

    if (PlayerId != InvalidRemoteAddress)
    {
        Bridge.CallFunction(Functions.NetConnectionReceiveUniqueIdRepl, { BunchAddress, PlayerId });
    }

    if (PlatformName != InvalidRemoteAddress)
    {
        Bridge.CallFunction(Functions.NetConnectionReceiveFString, { BunchAddress, PlatformName });
    }

    Runtime.GetMemory().Write<int64>(OverflowSlot, OriginalLimit);

    UE_LOG_DISPLAY("NetworkHooks", "Accepted login request " + Connection.GetRequestUrl());
    return true;
}
