#include "FortniteGame/Public/FortniteGameModule.h"
#include "FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

FFortniteGameModule& FFortniteGameModule::Get()
{
    static FFortniteGameModule Module;
    return Module;
}

bool FFortniteGameModule::StartupModule(FEngineRuntime& InEngineRuntime, const FAthenaMatchSettings& InMatchSettings)
{
    EngineRuntime = &InEngineRuntime;
    MatchSettings = InMatchSettings;

    NativeOverrides.Initialize(EngineRuntime->GetUnrealRuntime());

    if (!RegisterNativeOverrides())
    {
        return false;
    }

    UE_LOG_DISPLAY("FortniteGame", "Module started with " + std::to_string(NativeOverrides.GetOverrideCount()) + " native overrides");
    return true;
}

void FFortniteGameModule::ShutdownModule()
{
    NativeOverrides.RemoveAll();
    EngineRuntime = nullptr;
}

FEngineRuntime& FFortniteGameModule::GetEngineRuntime() const
{
    return *EngineRuntime;
}

const FAthenaMatchSettings& FFortniteGameModule::GetMatchSettings() const
{
    return MatchSettings;
}

bool FFortniteGameModule::RegisterNativeOverrides()
{
    bool bRegistered = true;

    bRegistered &= NativeOverrides.Override("FortGameModeAthena", "ReadyToStartMatch", &AFortGameModeAthena::execReadyToStartMatch);
    bRegistered &= NativeOverrides.Override("FortGameModeAthena", "SpawnDefaultPawnFor", &AFortGameModeAthena::execSpawnDefaultPawnFor);
    bRegistered &= NativeOverrides.Override("FortGameModeAthena", "HandleStartingNewPlayer", &AFortGameModeAthena::execHandleStartingNewPlayer);
    bRegistered &= NativeOverrides.Override("FortPlayerControllerAthena", "ServerAcknowledgePossession",
        &AFortPlayerControllerAthena::execServerAcknowledgePossession);
    bRegistered &= NativeOverrides.Override("FortPlayerControllerAthena", "ServerExecuteInventoryItem",
        &AFortPlayerControllerAthena::execServerExecuteInventoryItem);

    return bRegistered;
}

bool FFortniteGameModule::PrepareGameModeDefaults() const
{
    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    const FObjectHandle GameModeClass = Runtime.FindOrLoadObject(MatchSettings.GameModeClassPath);
    if (!GameModeClass)
    {
        UE_LOG_ERROR("FortniteGame", "Game mode " + MatchSettings.GameModeClassPath + " could not be loaded");
        return false;
    }

    const FObjectHandle DefaultGameMode = Runtime.GetDefaultObject(GameModeClass);
    if (!DefaultGameMode)
    {
        UE_LOG_ERROR("FortniteGame", "Game mode " + GameModeClass.GetFullName() + " has no default object");
        return false;
    }

    DefaultGameMode.SetBoolProperty("bEnableReplicationGraph", true);
    return true;
}

bool FFortniteGameModule::TravelToAthena() const
{
    if (EngineRuntime == nullptr || !PrepareGameModeDefaults())
    {
        return false;
    }

    const FObjectHandle LocalPlayer = EngineRuntime->GetLocalPlayer(0);
    const FObjectHandle LocalController = LocalPlayer ? LocalPlayer.GetObjectProperty("PlayerController") : FObjectHandle();

    if (!LocalController)
    {
        UE_LOG_ERROR("FortniteGame", "The local player has no player controller to travel with");
        return false;
    }

    const std::string TravelUrl = MatchSettings.MapName + "?game=" + MatchSettings.GameModeClassPath;

    if (!EngineRuntime->RemoveLocalPlayers())
    {
        UE_LOG_ERROR("FortniteGame", "The local player could not be removed before travel");
        return false;
    }

    AFortPlayerControllerAthena Controller(LocalController);
    Controller.ClientTravelToMap(TravelUrl);

    UE_LOG_DISPLAY("FortniteGame", "Travelling to " + TravelUrl);
    return true;
}

bool FFortniteGameModule::PrepareMatch() const
{
    const UWorld World = EngineRuntime->GetWorld();
    const FObjectHandle GameMode = World ? World.GetAuthorityGameMode() : FObjectHandle();
    const FObjectHandle GameSession = GameMode ? GameMode.GetObjectProperty("GameSession") : FObjectHandle();

    if (!GameSession)
    {
        UE_LOG_ERROR("FortniteGame", "The Athena game mode has no game session");
        return false;
    }

    return OverrideGameSessionKickPlayer(GameSession);
}

bool FFortniteGameModule::OverrideGameSessionKickPlayer(const FObjectHandle& GameSession) const
{
    FGameThreadBridge& Bridge = EngineRuntime->GetUnrealRuntime().GetBridge();

    const int32 KickPlayerIndex = Bridge.FindVirtualIndex(GameSession.GetAddress(), EngineRuntime->GetFunctions().GameSessionKickPlayer);
    if (KickPlayerIndex == InvalidIndex)
    {
        UE_LOG_ERROR("FortniteGame", GameSession.GetName() + " does not implement AFortGameSession::KickPlayer through its virtual table");
        return false;
    }

    FX64Emitter Emitter;
    Emitter.MoveRegisterImmediate(EX64Register::Rax, 1);
    Emitter.EmitReturn();

    const FRemoteAddress Routine = Bridge.EmitRoutine(Emitter);
    if (Routine == InvalidRemoteAddress || !Bridge.InstallVirtualOverride(GameSession.GetAddress(), KickPlayerIndex, Routine))
    {
        return false;
    }

    UE_LOG_DISPLAY("FortniteGame", GameSession.GetName() + " no longer kicks players without a reservation");
    return true;
}
