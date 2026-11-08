#include "FortniteGame/Public/FortniteGameModule.h"
#include "FortniteGame/Public/Athena/FortGameModeAthena.h"

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

    const FObjectHandle DefaultGameMode = Runtime.GetClassDefaultObject(GameModeClass.GetName());
    if (!DefaultGameMode)
    {
        UE_LOG_ERROR("FortniteGame", "Game mode " + GameModeClass.GetName() + " has no default object");
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

    const std::string TravelUrl = MatchSettings.MapName + "?game=" + MatchSettings.GameModeClassPath + "?listen";

    AFortPlayerControllerAthena Controller(LocalController);
    Controller.ClientTravelToMap(TravelUrl);

    UE_LOG_DISPLAY("FortniteGame", "Travelling to " + TravelUrl);
    return true;
}
