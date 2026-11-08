#include "FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Public/FortniteGameModule.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

AFortGameStateAthena AFortGameModeAthena::GetGameState() const
{
    return AFortGameStateAthena(Object.GetObjectProperty("GameState"));
}

FObjectHandle AFortGameModeAthena::GetGameSession() const
{
    return Object.GetObjectProperty("GameSession");
}

std::string AFortGameModeAthena::GetMatchState() const
{
    return Object.GetNameProperty("MatchState");
}

void AFortGameModeAthena::SetMatchState(std::string_view MatchState)
{
    const FName StateName = Object.GetRuntime().CreateName(MatchState);

    const FRemoteAddress MatchStateAddress = Object.GetPropertyAddress("MatchState");
    if (MatchStateAddress == InvalidRemoteAddress)
    {
        return;
    }

    Object.GetRuntime().GetMemory().Write<FName>(MatchStateAddress, StateName);

    struct FOnSetMatchStateParameters
    {
        FName NewState;
        uint8 Padding[8] = {};
    } Parameters;

    Parameters.NewState = StateName;
    Object.InvokeFunction("K2_OnSetMatchState", &Parameters, sizeof(Parameters));
}

void AFortGameModeAthena::StartPlay()
{
    Object.InvokeFunction("StartPlay");
}

void AFortGameModeAthena::StartMatch()
{
    Object.InvokeFunction("StartMatch");
}

void AFortGameModeAthena::EndMatch()
{
    Object.InvokeFunction("EndMatch");
}

void AFortGameModeAthena::SetDisableGarbageCollectionDuringMatch(bool bValue)
{
    Object.SetBoolProperty("bDisableGCOnServerDuringMatch", bValue);
}

void AFortGameModeAthena::SetAllowSpectateAfterDeath(bool bValue)
{
    Object.SetBoolProperty("bAllowSpectateAfterDeath", bValue);
}

void AFortGameModeAthena::SetEnableReplicationGraph(bool bValue)
{
    Object.SetBoolProperty("bEnableReplicationGraph", bValue);
}

void AFortGameModeAthena::SetMinRespawnDelay(float Value)
{
    Object.SetProperty<float>("MinRespawnDelay", Value);
}

void AFortGameModeAthena::SetUseSeamlessTravel(bool bValue)
{
    Object.SetBoolProperty("bUseSeamlessTravel", bValue);
}

int32 AFortGameModeAthena::GetNumberOfPlayers() const
{
    return Object.GetProperty<int32>("NumPlayers", 0);
}

bool AFortGameModeAthena::SetupPlaylist(const AFortPlaylistAthena& Playlist)
{
    if (!Playlist)
    {
        return false;
    }

    Object.SetProperty<int32>("CurrentPlaylistId", Playlist.GetPlaylistId());

    AFortGameStateAthena GameState = GetGameState();
    if (!GameState)
    {
        return false;
    }

    GameState.SetCurrentPlaylist(Playlist);
    GameState.SetTeamSize(Playlist.GetMaxSquadSize());
    GameState.SetTeamCount(Playlist.GetMaxTeamCount());

    return true;
}

void AFortGameModeAthena::ChangeGameSessionMaxPlayers(int32 MaxPlayers)
{
    const FObjectHandle GameSession = GetGameSession();
    if (!GameSession)
    {
        return;
    }

    GameSession.SetProperty<int32>("MaxPlayers", MaxPlayers);
    GameSession.SetProperty<int32>("MaxSpectators", MaxPlayers);
}

bool AFortGameModeAthena::IsWorldReady() const
{
    return Object.GetBoolProperty("bWorldIsReady");
}

void AFortGameModeAthena::SetWorldIsReady(bool bValue)
{
    Object.SetBoolProperty("bWorldIsReady", bValue);
}

void AFortGameModeAthena::SetWarmupRequiredPlayerCount(int32 Value)
{
    Object.SetProperty<int32>("WarmupRequiredPlayerCount", Value);
}

FObjectHandle AFortGameModeAthena::GetDefaultPawnClassForController(const AActor& Controller) const
{
    struct FGetDefaultPawnClassForControllerParameters
    {
        FRemoteAddress InController = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    Parameters.InController = Controller.GetAddress();

    if (!Object.InvokeFunction("GetDefaultPawnClassForController", &Parameters, sizeof(Parameters)))
    {
        return FObjectHandle();
    }

    return Object.GetRuntime().MakeHandle(Parameters.ReturnValue);
}

ENativeOverrideResult AFortGameModeAthena::execReadyToStartMatch(const FNativeFunctionCall& Call)
{
    static const FObjectHandle AthenaGameModeClass = Call.GetRuntime().FindClass("FortGameModeAthena");

    AFortGameModeAthena GameMode(Call.GetContext());

    if (!GameMode.GetObject().IsA(AthenaGameModeClass) || GameMode.IsWorldReady())
    {
        return ENativeOverrideResult::ContinueToOriginal;
    }

    const AFortGameStateAthena GameState = GameMode.GetGameState();
    if (!GameState || !GameState.GetMapInfo())
    {
        Call.SetResult<bool>(false);
        return ENativeOverrideResult::Handled;
    }

    const FAthenaMatchSettings& Settings = FFortniteGameModule::Get().GetMatchSettings();

    const AFortPlaylistAthena Playlist(Call.GetRuntime().FindOrLoadObject(Settings.PlaylistPath));
    if (!GameMode.SetupPlaylist(Playlist))
    {
        UE_LOG_WARNING("Athena", "Playlist " + Settings.PlaylistPath + " could not be applied");
    }

    GameMode.SetWarmupRequiredPlayerCount(Settings.MinimumPlayersToStart);
    GameMode.SetDisableGarbageCollectionDuringMatch(true);
    GameMode.ChangeGameSessionMaxPlayers(Settings.MaxPlayers);
    GameMode.SetWorldIsReady(true);

    FFortniteGameModule::Get().GetNetworkHooks().DisableGarbageCollection();

    UE_LOG_DISPLAY("Athena", "World is ready, waiting for " + std::to_string(Settings.MinimumPlayersToStart) + " players");

    Call.SetResult<bool>(false);
    return ENativeOverrideResult::Handled;
}

ENativeOverrideResult AFortGameModeAthena::execSpawnDefaultPawnFor(const FNativeFunctionCall& Call)
{
    struct FSpawnDefaultPawnForParameters
    {
        FRemoteAddress NewPlayer = InvalidRemoteAddress;
        FRemoteAddress StartSpot = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    };

    const FUnrealRuntime& Runtime = Call.GetRuntime();
    const FSpawnDefaultPawnForParameters Parameters = Call.GetParameters<FSpawnDefaultPawnForParameters>();

    const AFortGameModeAthena GameMode(Call.GetContext());
    const AFortPlayerControllerAthena Controller(Runtime.MakeHandle(Parameters.NewPlayer));
    const AActor StartSpot(Runtime.MakeHandle(Parameters.StartSpot));

    Call.SetResult<FRemoteAddress>(0);

    if (!Controller || !StartSpot || Controller.IsLocalController())
    {
        return ENativeOverrideResult::Handled;
    }

    const FObjectHandle PawnClass = GameMode.GetDefaultPawnClassForController(Controller);
    if (!PawnClass)
    {
        UE_LOG_ERROR("Athena", "No pawn class for " + Controller.GetName());
        return ENativeOverrideResult::Handled;
    }

    const FRotator SpotRotation = StartSpot.GetActorRotation();
    const FTransform SpawnTransform = FTransform::FromLocationAndRotation(StartSpot.GetActorLocation(), FRotator(0.0f, SpotRotation.Yaw, 0.0f));

    const AFortPlayerPawnAthena Pawn(
        UGameplayStatics::SpawnActor(GameMode, PawnClass, SpawnTransform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, Controller).GetObject());

    if (!Pawn)
    {
        UE_LOG_ERROR("Athena", "Failed to spawn a pawn for " + Controller.GetName());
        return ENativeOverrideResult::Handled;
    }

    Call.SetResult<FRemoteAddress>(Pawn.GetAddress());
    Controller.SetupAthenaPawn(Pawn);

    UE_LOG_DISPLAY("Athena", Controller.GetName() + " spawned as " + Pawn.GetName() + " at " + StartSpot.GetName());
    return ENativeOverrideResult::Handled;
}

ENativeOverrideResult AFortGameModeAthena::execHandleStartingNewPlayer(const FNativeFunctionCall& Call)
{
    struct FHandleStartingNewPlayerParameters
    {
        FRemoteAddress NewPlayer = InvalidRemoteAddress;
    };

    const FHandleStartingNewPlayerParameters Parameters = Call.GetParameters<FHandleStartingNewPlayerParameters>();
    const AFortPlayerControllerAthena Controller(Call.GetRuntime().MakeHandle(Parameters.NewPlayer));

    if (Controller && !Controller.IsLocalController())
    {
        Controller.EnsureWorldInventory();
    }

    return ENativeOverrideResult::ContinueToOriginal;
}
