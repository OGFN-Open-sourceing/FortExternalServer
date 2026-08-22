#include "Runtime/Engine/Public/Engine/EngineRuntime.h"
#include "Runtime/Engine/Public/Engine/EngineSignatures.h"
#include "Runtime/Engine/Public/Engine/World.h"

bool FEngineFunctions::HasNetworkEssentials() const
{
    return NetDriverTickFlush != InvalidRemoteAddress && WorldNotifyControlMessage != InvalidRemoteAddress && WorldWelcomePlayer != InvalidRemoteAddress &&
        WorldSpawnPlayActor != InvalidRemoteAddress && ActorGetNetMode != InvalidRemoteAddress;
}

namespace
{
    const std::vector<std::string>& GetEngineFunctionNames()
    {
        static const std::vector<std::string> Names = { "NetDriver::TickFlush", "NetDriver::InitListen", "NetDriver::NetDebug", "World::NotifyControlMessage",
            "World::NotifyAcceptingConnection", "World::WelcomePlayer", "World::SpawnPlayActor", "Actor::GetNetMode", "OnlineSession::KickPlayer",
            "OnlineBeacon::PauseBeaconRequests", "OnlineBeaconHost::InitHost", "OnlineBeacon::NotifyAcceptingConnection", "OnlineBeaconHost::NotifyControlMessage",
            "NetConnection::ReceiveFString", "NetConnection::ReceiveUniqueIdRepl", "CollectGarbage", "PlayerController::GetPlayerViewPoint",
            "LocalPlayer::SpawnPlayActor", "AbilitySystemComponent::GiveAbility", "AbilitySystemComponent::InternalTryActivateAbility",
            "AbilitySystemComponent::MarkAbilitySpecDirty" };

        return Names;
    }
}

std::vector<FRemoteAddress> FEngineFunctions::GetAllEntries() const
{
    return { NetDriverTickFlush, NetDriverInitListen, NetDriverNetDebug, WorldNotifyControlMessage, WorldNotifyAcceptingConnection, WorldWelcomePlayer,
        WorldSpawnPlayActor, ActorGetNetMode, OnlineSessionKickPlayer, BeaconPauseRequests, BeaconHostInitHost, BeaconNotifyAcceptingConnection,
        BeaconHostNotifyControlMessage, NetConnectionReceiveFString, NetConnectionReceiveUniqueIdRepl, CollectGarbage, PlayerControllerGetPlayerViewPoint,
        LocalPlayerSpawnPlayActor, AbilitySystemGiveAbility, AbilitySystemInternalTryActivateAbility, AbilitySystemMarkAbilitySpecDirty };
}

int32 FEngineFunctions::CountResolved() const
{
    const std::vector<FRemoteAddress> Entries = GetAllEntries();

    int32 Resolved = 0;
    for (const FRemoteAddress Entry : Entries)
    {
        if (Entry != InvalidRemoteAddress)
        {
            ++Resolved;
        }
    }

    return Resolved;
}

int32 FEngineFunctions::CountTotal() const
{
    return static_cast<int32>(GetAllEntries().size());
}

std::vector<std::string> FEngineFunctions::GetUnresolvedNames() const
{
    const std::vector<FRemoteAddress> Entries = GetAllEntries();
    const std::vector<std::string>& Names = GetEngineFunctionNames();

    std::vector<std::string> Unresolved;

    for (size_t Index = 0; Index < Entries.size() && Index < Names.size(); ++Index)
    {
        if (Entries[Index] == InvalidRemoteAddress)
        {
            Unresolved.push_back(Names[Index]);
        }
    }

    return Unresolved;
}

bool FEngineRuntime::Initialize(FUnrealRuntime& InUnrealRuntime)
{
    UnrealRuntime = &InUnrealRuntime;
    CachedEngineAddress = InvalidRemoteAddress;

    if (!ResolveFunctions())
    {
        return false;
    }

    UE_LOG_DISPLAY("Engine", "Resolved " + std::to_string(Functions.CountResolved()) + " of " + std::to_string(Functions.CountTotal()) + " engine functions");

    for (const std::string& Unresolved : Functions.GetUnresolvedNames())
    {
        UE_LOG_WARNING("Engine", "Unresolved engine function " + Unresolved);
    }

    if (!Functions.HasNetworkEssentials())
    {
        UE_LOG_ERROR("Engine", "The networking functions required to host a match were not all resolved");
        return false;
    }

    return true;
}

bool FEngineRuntime::IsInitialized() const
{
    return UnrealRuntime != nullptr && Functions.HasNetworkEssentials();
}

const FEngineFunctions& FEngineRuntime::GetFunctions() const
{
    return Functions;
}

FUnrealRuntime& FEngineRuntime::GetUnrealRuntime() const
{
    return *UnrealRuntime;
}

bool FEngineRuntime::ResolveFunctions()
{
    const FSignatureScanner Scanner(UnrealRuntime->GetImage());

    Functions.NetDriverTickFlush = Scanner.FindPattern(FEngineSignatures::NetDriverTickFlush);
    Functions.NetDriverInitListen = Scanner.FindPattern(FEngineSignatures::NetDriverInitListen);
    Functions.NetDriverNetDebug = Scanner.FindPattern(FEngineSignatures::NetDriverNetDebug);
    Functions.WorldNotifyControlMessage = Scanner.FindPattern(FEngineSignatures::WorldNotifyControlMessage);
    Functions.WorldNotifyAcceptingConnection = Scanner.FindPattern(FEngineSignatures::WorldNotifyAcceptingConnection);
    Functions.WorldWelcomePlayer = Scanner.FindPattern(FEngineSignatures::WorldWelcomePlayer);
    Functions.WorldSpawnPlayActor = Scanner.FindPattern(FEngineSignatures::WorldSpawnPlayActor);
    Functions.ActorGetNetMode = Scanner.FindPattern(FEngineSignatures::ActorGetNetMode);
    Functions.OnlineSessionKickPlayer = Scanner.FindPattern(FEngineSignatures::OnlineSessionKickPlayer);
    Functions.BeaconPauseRequests = Scanner.FindPattern(FEngineSignatures::BeaconPauseRequests);
    Functions.BeaconHostInitHost = Scanner.FindPattern(FEngineSignatures::BeaconHostInitHost);
    Functions.BeaconNotifyAcceptingConnection = Scanner.FindPattern(FEngineSignatures::BeaconNotifyAcceptingConnection);
    Functions.BeaconHostNotifyControlMessage = Scanner.FindPattern(FEngineSignatures::BeaconHostNotifyControlMessage);
    Functions.NetConnectionReceiveFString = Scanner.FindPattern(FEngineSignatures::NetConnectionReceiveFString);
    Functions.NetConnectionReceiveUniqueIdRepl = Scanner.FindPattern(FEngineSignatures::NetConnectionReceiveUniqueIdRepl);
    Functions.PlayerControllerGetPlayerViewPoint = Scanner.FindPattern(FEngineSignatures::PlayerControllerGetPlayerViewPoint);
    Functions.LocalPlayerSpawnPlayActor = Scanner.FindPattern(FEngineSignatures::LocalPlayerSpawnPlayActor);
    Functions.AbilitySystemGiveAbility = Scanner.FindPattern(FEngineSignatures::AbilitySystemGiveAbility);
    Functions.AbilitySystemInternalTryActivateAbility = Scanner.FindPattern(FEngineSignatures::AbilitySystemInternalTryActivateAbility);
    Functions.AbilitySystemMarkAbilitySpecDirty = Scanner.FindPattern(FEngineSignatures::AbilitySystemMarkAbilitySpecDirty);

    ResolveAnchoredFallbacks(Scanner);

    const FRemoteAddress CollectGarbageCall = Scanner.FindPattern(FEngineSignatures::CollectGarbageCall);
    if (CollectGarbageCall != InvalidRemoteAddress)
    {
        Functions.CollectGarbage =
            Scanner.ResolveRelativeOperand(CollectGarbageCall, FEngineSignatures::CollectGarbageOperandOffset, FEngineSignatures::CollectGarbageInstructionLength);
    }

    return true;
}

void FEngineRuntime::ResolveAnchoredFallbacks(const FSignatureScanner& Scanner)
{
    const auto ResolveFromAnchor = [&Scanner](FRemoteAddress& Target, std::wstring_view Anchor) {
        if (Target != InvalidRemoteAddress)
        {
            return;
        }

        const FRemoteAddress AnchorAddress = Scanner.FindWideStringReference(Anchor);
        if (AnchorAddress == InvalidRemoteAddress)
        {
            return;
        }

        Target = Scanner.FindFunctionStart(AnchorAddress, FEngineSignatures::AnchorBacktrack);
    };

    ResolveFromAnchor(Functions.WorldWelcomePlayer, FEngineSignatures::WelcomePlayerAnchor);
    ResolveFromAnchor(Functions.WorldSpawnPlayActor, FEngineSignatures::SpawnPlayActorAnchor);
    ResolveFromAnchor(Functions.OnlineSessionKickPlayer, FEngineSignatures::KickPlayerAnchor);
}

FObjectHandle FEngineRuntime::GetEngine() const
{
    if (CachedEngineAddress != InvalidRemoteAddress)
    {
        const FObjectHandle Cached = UnrealRuntime->MakeHandle(CachedEngineAddress);
        if (Cached)
        {
            return Cached;
        }
    }

    const FObjectHandle EngineClass = UnrealRuntime->FindClass("FortniteGame.FortEngine");
    if (EngineClass)
    {
        const std::vector<FObjectHandle> Instances = UnrealRuntime->FindObjectsOfClass(EngineClass, 1);
        if (!Instances.empty())
        {
            CachedEngineAddress = Instances.front().GetAddress();
            return Instances.front();
        }
    }

    const std::vector<FObjectHandle> ByName = UnrealRuntime->FindObjectsByNamePrefix("FortEngine_", 1);
    if (!ByName.empty())
    {
        CachedEngineAddress = ByName.front().GetAddress();
        return ByName.front();
    }

    return FObjectHandle();
}

void FEngineRuntime::InvalidateCachedEngine()
{
    CachedEngineAddress = InvalidRemoteAddress;
}

FObjectHandle FEngineRuntime::GetGameViewportClient() const
{
    const FObjectHandle Engine = GetEngine();
    if (!Engine)
    {
        return FObjectHandle();
    }

    return Engine.GetObjectProperty("GameViewport");
}

UWorld FEngineRuntime::GetWorld() const
{
    const FObjectHandle Viewport = GetGameViewportClient();
    if (Viewport)
    {
        const FObjectHandle World = Viewport.GetObjectProperty("World");
        if (World)
        {
            return UWorld(World);
        }
    }

    const FObjectHandle GameInstance = GetGameInstance();
    if (GameInstance)
    {
        const FObjectHandle World = GameInstance.GetObjectProperty("WorldContext");
        if (World)
        {
            return UWorld(World);
        }
    }

    return UWorld();
}

FObjectHandle FEngineRuntime::GetGameInstance() const
{
    const FObjectHandle Engine = GetEngine();
    if (!Engine)
    {
        return FObjectHandle();
    }

    return Engine.GetObjectProperty("GameInstance");
}

FObjectHandle FEngineRuntime::GetLocalPlayer(int32 Index) const
{
    const FObjectHandle GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return FObjectHandle();
    }

    return GameInstance.GetArrayElementAsObject("LocalPlayers", Index);
}

FObjectHandle FEngineRuntime::SpawnObject(const FObjectHandle& ClassHandle, const FObjectHandle& Outer) const
{
    const FObjectHandle GameplayStatics = UnrealRuntime->GetClassDefaultObject("GameplayStatics");
    if (!GameplayStatics || !ClassHandle)
    {
        return FObjectHandle();
    }

    struct FSpawnObjectParameters
    {
        FRemoteAddress ObjectClass = InvalidRemoteAddress;
        FRemoteAddress Outer = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    Parameters.ObjectClass = ClassHandle.GetAddress();
    Parameters.Outer = Outer.GetAddress();

    if (!GameplayStatics.InvokeFunction("SpawnObject", &Parameters, sizeof(Parameters)))
    {
        return FObjectHandle();
    }

    return UnrealRuntime->MakeHandle(Parameters.ReturnValue);
}
