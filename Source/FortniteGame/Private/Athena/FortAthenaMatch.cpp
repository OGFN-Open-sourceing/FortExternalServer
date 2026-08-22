#include "FortniteGame/Public/Athena/FortAthenaMatch.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

#include <algorithm>

bool FFortAthenaMatch::Initialize(FEngineRuntime& InEngineRuntime, FNetworkHooks& InNetworkHooks, const FAthenaMatchSettings& InSettings)
{
    EngineRuntime = &InEngineRuntime;
    NetworkHooks = &InNetworkHooks;
    Settings = InSettings;

    PlayerBootstrap.Initialize(InEngineRuntime, Settings.PlayerSettings);
    AIDirector.Initialize(InEngineRuntime, Settings.PlayerSettings);
    AIDirector.SetSpawnLocationProvider([this]() { return PlayerBootstrap.ChooseWarmupSpawnLocation(); });
    TeamRoster.Initialize(Settings.TeamSize, std::max(Settings.MaxPlayers / std::max(Settings.TeamSize, 1), 1), Settings.bUseGameSessions);

    bInitialized = true;
    return true;
}

void FFortAthenaMatch::Shutdown()
{
    TrackedPlayers.clear();
    TeamRoster.Reset();
    AIDirector.Reset();
    SafeZoneDirector.Reset();
    AircraftDirector.Reset();

    bInitialized = false;
    bMatchPrepared = false;
    bMatchStarted = false;
    bMatchEnded = false;
    CurrentPhase = EAthenaGamePhase::None;
}

const FAthenaMatchSettings& FFortAthenaMatch::GetSettings() const
{
    return Settings;
}

void FFortAthenaMatch::SetSettings(const FAthenaMatchSettings& InSettings)
{
    Settings = InSettings;
    PlayerBootstrap.SetSettings(Settings.PlayerSettings);
}

EAthenaGamePhase FFortAthenaMatch::GetGamePhase() const
{
    return CurrentPhase;
}

bool FFortAthenaMatch::HasMatchStarted() const
{
    return bMatchStarted;
}

bool FFortAthenaMatch::HasMatchEnded() const
{
    return bMatchEnded;
}

int32 FFortAthenaMatch::GetConnectedPlayerCount() const
{
    return static_cast<int32>(TrackedPlayers.size());
}

int32 FFortAthenaMatch::GetAliveBotCount() const
{
    return AIDirector.GetAliveCount();
}

const FFortAIDirector& FFortAthenaMatch::GetAIDirector() const
{
    return AIDirector;
}

int32 FFortAthenaMatch::GetAlivePlayerCount() const
{
    int32 Alive = 0;

    for (const auto& Entry : TrackedPlayers)
    {
        if (!Entry.second.bEliminated)
        {
            ++Alive;
        }
    }

    return Alive;
}

std::vector<FTrackedPlayer> FFortAthenaMatch::GetTrackedPlayers() const
{
    std::vector<FTrackedPlayer> Players;
    Players.reserve(TrackedPlayers.size());

    for (const auto& Entry : TrackedPlayers)
    {
        Players.push_back(Entry.second);
    }

    return Players;
}

AFortGameModeAthena FFortAthenaMatch::GetGameMode() const
{
    if (EngineRuntime == nullptr)
    {
        return AFortGameModeAthena();
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return AFortGameModeAthena();
    }

    return AFortGameModeAthena(World.GetAuthorityGameMode());
}

AFortGameStateAthena FFortAthenaMatch::GetGameState() const
{
    if (EngineRuntime == nullptr)
    {
        return AFortGameStateAthena();
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return AFortGameStateAthena();
    }

    return AFortGameStateAthena(World.GetGameState());
}

float FFortAthenaMatch::GetMatchTimeSeconds() const
{
    if (MatchStartTimestamp == 0)
    {
        return 0.0f;
    }

    return static_cast<float>(FPlatformMisc::GetTimeMilliseconds() - MatchStartTimestamp) / 1000.0f;
}

bool FFortAthenaMatch::TravelToAthena()
{
    if (EngineRuntime == nullptr)
    {
        return false;
    }

    const FObjectHandle LocalPlayer = EngineRuntime->GetLocalPlayer(0);
    if (!LocalPlayer)
    {
        UE_LOG_ERROR("Match", "No local player is available to issue the level travel");
        return false;
    }

    const FObjectHandle LocalController = LocalPlayer.GetObjectProperty("PlayerController");
    if (!LocalController)
    {
        UE_LOG_ERROR("Match", "The local player has no player controller yet");
        return false;
    }

    const std::string TravelUrl = Settings.MapName + "?game=" + Settings.GameModeClassPath;

    AFortPlayerControllerAthena Controller(LocalController);
    Controller.ClientTravelToMap(TravelUrl);

    UE_LOG_DISPLAY("Match", "Travelling to " + TravelUrl);
    return true;
}

bool FFortAthenaMatch::PrepareMatch()
{
    if (EngineRuntime == nullptr || bMatchPrepared)
    {
        return bMatchPrepared;
    }

    AFortGameModeAthena GameMode = GetGameMode();
    AFortGameStateAthena GameState = GetGameState();

    if (!GameMode || !GameState)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    AFortPlaylistAthena Playlist(Runtime.FindOrLoadObject(Settings.PlaylistPath));
    if (Playlist)
    {
        Playlist.SetMaxSquadSize(Settings.TeamSize);
        Playlist.SetMaxTeamCount(std::max(Settings.MaxPlayers / std::max(Settings.TeamSize, 1), 1));
        Playlist.SetFriendlyFireType(Settings.bFriendlyFireEnabled ? EFriendlyFireType::On : EFriendlyFireType::Off);
        Playlist.SetDownedButNotOutEnabled(Settings.TeamSize > 1);

        GameMode.SetupPlaylist(Playlist);
    }
    else
    {
        UE_LOG_WARNING("Match", "Playlist " + Settings.PlaylistPath + " could not be resolved");
    }

    GameState.SetSkipAircraft(true);
    GameState.SetAircraftStartTime(99999.0f);
    GameState.SetWarmupCountdownEndTime(99999.0f);
    GameState.SetTeamSize(Settings.TeamSize);

    GameMode.SetDisableGarbageCollectionDuringMatch(true);
    GameMode.SetAllowSpectateAfterDeath(Settings.bAllowSpectateAfterDeath);
    GameMode.SetEnableReplicationGraph(true);
    GameMode.SetMinRespawnDelay(5.0f);
    GameMode.SetUseSeamlessTravel(false);
    GameMode.ChangeGameSessionMaxPlayers(Settings.MaxPlayers);

    GameMode.SetMatchState(FMatchStateNames::InProgress);
    GameMode.StartPlay();

    GameState.SetReplicatedHasBegunPlay(true);
    GameMode.StartMatch();

    SafeZoneDirector.Initialize(GameState, FVector(0.0f, 0.0f, 0.0f));
    AircraftDirector.Initialize(*EngineRuntime, GameState);

    MatchStartTimestamp = FPlatformMisc::GetTimeMilliseconds();

    TransitionToPhase(EAthenaGamePhase::Warmup, GetMatchTimeSeconds());

    bMatchPrepared = true;
    NetworkHooks->SetTravelCompleted(true);

    UE_LOG_DISPLAY("Match", "Match prepared and waiting for players");
    return true;
}

void FFortAthenaMatch::TransitionToPhase(EAthenaGamePhase Phase, float CurrentTime)
{
    CurrentPhase = Phase;
    PhaseStartTime = CurrentTime;

    AFortGameStateAthena GameState = GetGameState();
    if (GameState)
    {
        GameState.SetGamePhase(Phase);
    }

    switch (Phase)
    {
    case EAthenaGamePhase::Warmup:
        UE_LOG_DISPLAY("Match", "Phase changed to Warmup");
        break;
    case EAthenaGamePhase::Aircraft:
        UE_LOG_DISPLAY("Match", "Phase changed to Aircraft");
        break;
    case EAthenaGamePhase::SafeZones:
        UE_LOG_DISPLAY("Match", "Phase changed to SafeZones");
        break;
    case EAthenaGamePhase::EndGame:
        UE_LOG_DISPLAY("Match", "Phase changed to EndGame");
        break;
    default:
        break;
    }
}

void FFortAthenaMatch::Tick()
{
    if (!bInitialized || EngineRuntime == nullptr)
    {
        return;
    }

    if (!bMatchPrepared)
    {
        PrepareMatch();
        return;
    }

    const float CurrentTime = GetMatchTimeSeconds();

    ScanForNewConnections();
    RemoveStalePlayers();
    UpdateEliminationState();
    AIDirector.UpdateEliminationState();

    switch (CurrentPhase)
    {
    case EAthenaGamePhase::Warmup:
        UpdateWarmupPhase(CurrentTime);
        break;
    case EAthenaGamePhase::Aircraft:
        UpdateAircraftPhase(CurrentTime);
        break;
    case EAthenaGamePhase::SafeZones:
        UpdateSafeZonePhase(CurrentTime);
        break;
    case EAthenaGamePhase::EndGame:
        UpdateEndGamePhase(CurrentTime);
        break;
    default:
        break;
    }

    AFortGameStateAthena GameState = GetGameState();
    if (GameState)
    {
        GameState.SetPlayersLeft(GetAlivePlayerCount() + GetAliveBotCount());
        GameState.SetTotalPlayers(GetConnectedPlayerCount() + AIDirector.GetSpawnedCount());
    }
}

void FFortAthenaMatch::ScanForNewConnections()
{
    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return;
    }

    const UNetDriver NetDriver = World.GetNetDriver();
    if (!NetDriver)
    {
        return;
    }

    const int32 ConnectionCount = NetDriver.GetClientConnectionCount();

    for (int32 Index = 0; Index < ConnectionCount; ++Index)
    {
        const UNetConnection Connection = NetDriver.GetClientConnection(Index);
        if (!Connection)
        {
            continue;
        }

        const FObjectHandle ControllerHandle = Connection.GetPlayerController();
        if (!ControllerHandle)
        {
            continue;
        }

        if (TrackedPlayers.find(ControllerHandle.GetAddress()) != TrackedPlayers.end())
        {
            continue;
        }

        InitializeJoiningPlayer(AFortPlayerControllerAthena(ControllerHandle), Connection.GetAddress());
    }
}

void FFortAthenaMatch::InitializeJoiningPlayer(const AFortPlayerControllerAthena& Controller, FRemoteAddress ConnectionAddress)
{
    if (!Controller)
    {
        return;
    }

    if (bMatchStarted && !Settings.bAllowJoinInProgress)
    {
        UE_LOG_WARNING("Match", "Rejecting a player because the match has already started");
        AFortPlayerControllerAthena MutableController = Controller;
        MutableController.ServerReturnToMainMenu();
        return;
    }

    FTrackedPlayer Player;
    Player.ControllerAddress = Controller.GetAddress();
    Player.ConnectionAddress = ConnectionAddress;
    Player.JoinTimeMilliseconds = FPlatformMisc::GetTimeMilliseconds();

    const AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
    Player.PlayerName = PlayerState ? PlayerState.GetPlayerName() : std::string("Player");

    Player.TeamIndex = TeamRoster.AssignPlayer(Controller);

    PlayerBootstrap.ApplyCosmetics(Controller);

    const FVector SpawnLocation = PlayerBootstrap.ChooseWarmupSpawnLocation();
    Player.bInitialized = PlayerBootstrap.SpawnAndPossess(Controller, SpawnLocation);

    if (Player.bInitialized)
    {
        PlayerBootstrap.ApplyStartingLoadout(Controller);

        AFortPlayerPawnAthena Pawn = Controller.GetPawn();
        if (Pawn)
        {
            Pawn.SetCanBeDamaged(bMatchStarted);
        }
    }

    TrackedPlayers[Player.ControllerAddress] = Player;

    UE_LOG_DISPLAY("Match", Player.PlayerName + " joined, team " + std::to_string(Player.TeamIndex) + ", " + std::to_string(GetConnectedPlayerCount()) + " connected");
}

void FFortAthenaMatch::RemoveStalePlayers()
{
    std::vector<FRemoteAddress> ToRemove;

    for (const auto& Entry : TrackedPlayers)
    {
        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.first));

        if (!Controller || Controller.IsDisconnecting())
        {
            ToRemove.push_back(Entry.first);
        }
    }

    for (const FRemoteAddress Address : ToRemove)
    {
        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Address));

        TeamRoster.RemovePlayer(Controller);

        const auto Entry = TrackedPlayers.find(Address);
        if (Entry != TrackedPlayers.end())
        {
            UE_LOG_DISPLAY("Match", Entry->second.PlayerName + " left the match");
            TrackedPlayers.erase(Entry);
        }
    }
}

void FFortAthenaMatch::UpdateEliminationState()
{
    for (auto& Entry : TrackedPlayers)
    {
        if (Entry.second.bEliminated)
        {
            continue;
        }

        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.first));
        if (!Controller)
        {
            continue;
        }

        const AFortPlayerPawnAthena Pawn = Controller.GetPawn();
        if (!Pawn)
        {
            continue;
        }

        if (Pawn.GetHealth() > 0.0f)
        {
            continue;
        }

        Entry.second.bEliminated = true;
        TeamRoster.MarkPlayerEliminated(Controller);

        AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
        if (PlayerState)
        {
            PlayerState.SetPlace(GetAlivePlayerCount() + 1);
        }

        AFortPlayerControllerAthena MutableController = Controller;
        MutableController.SendEndOfMatch(false);

        UE_LOG_DISPLAY("Match", Entry.second.PlayerName + " was eliminated, " + std::to_string(GetAlivePlayerCount()) + " players remain");
    }
}

void FFortAthenaMatch::UpdateWarmupPhase(float CurrentTime)
{
    const int32 ConnectedPlayers = GetConnectedPlayerCount();

    if (ConnectedPlayers < Settings.MinimumPlayersToStart)
    {
        return;
    }

    AFortGameStateAthena GameState = GetGameState();
    if (GameState)
    {
        GameState.SetWarmupCountdownStartTime(PhaseStartTime);
        GameState.SetWarmupCountdownEndTime(PhaseStartTime + Settings.WarmupCountdownSeconds);
    }

    if (CurrentTime - PhaseStartTime < Settings.WarmupCountdownSeconds)
    {
        return;
    }

    bMatchStarted = true;

    if (Settings.bPlayerBotsEnabled && Settings.PlayerBotCount > 0)
    {
        const int32 FirstBotTeam = 2 + std::max(TeamRoster.GetAssignedPlayerCount(), 0);
        AIDirector.SpawnPlayerBots(Settings.PlayerBotCount, FirstBotTeam);
    }

    if (Settings.bBossesEnabled)
    {
        AIDirector.SpawnBosses();
    }

    AircraftDirector.SpawnFlightPath(FVector(0.0f, 0.0f, 0.0f), GetActiveBuildProfile().MapRadius);
    AircraftDirector.Start(Settings.AircraftFlightSeconds, CurrentTime);

    for (const auto& Entry : TrackedPlayers)
    {
        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.first));
        AFortPlayerPawnAthena Pawn = Controller.GetPawn();

        if (Pawn)
        {
            Pawn.SetCanBeDamaged(true);
        }
    }

    TransitionToPhase(EAthenaGamePhase::Aircraft, CurrentTime);
}

void FFortAthenaMatch::UpdateAircraftPhase(float CurrentTime)
{
    AircraftDirector.Tick(CurrentTime);

    if (AircraftDirector.IsFlying())
    {
        return;
    }

    for (const auto& Entry : TrackedPlayers)
    {
        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.first));
        DropPlayerFromAircraft(Controller, CurrentTime);
    }

    SafeZoneDirector.Start(CurrentTime + Settings.SafeZoneStartDelaySeconds);
    TransitionToPhase(EAthenaGamePhase::SafeZones, CurrentTime);
}

void FFortAthenaMatch::DropPlayerFromAircraft(const AFortPlayerControllerAthena& Controller, float CurrentTime) const
{
    if (!Controller)
    {
        return;
    }

    AFortPlayerPawnAthena Pawn = Controller.GetPawn();
    if (!Pawn)
    {
        return;
    }

    FVector DropLocation = AircraftDirector.GetPositionAtTime(CurrentTime);
    DropLocation.Z = GetActiveBuildProfile().SkydiveDropHeight;

    Pawn.TeleportTo(DropLocation, FRotator());
    Pawn.SetMovementMode(EMovementMode::Falling, 0);
}

void FFortAthenaMatch::UpdateSafeZonePhase(float CurrentTime)
{
    SafeZoneDirector.Tick(CurrentTime);

    if ((TeamRoster.GetAliveTeamCount() <= 1 && GetAliveBotCount() == 0) || (GetAlivePlayerCount() <= 1 && GetAliveBotCount() == 0))
    {
        WinningTeam = TeamRoster.GetLastStandingTeam();
        AnnounceVictory(WinningTeam);
        TransitionToPhase(EAthenaGamePhase::EndGame, CurrentTime);
    }
}

void FFortAthenaMatch::UpdateEndGamePhase(float CurrentTime)
{
    if (bMatchEnded)
    {
        return;
    }

    if (CurrentTime - PhaseStartTime < Settings.EndOfMatchDelaySeconds)
    {
        return;
    }

    AFortGameModeAthena GameMode = GetGameMode();
    if (GameMode)
    {
        GameMode.SetMatchState(FMatchStateNames::WaitingPostMatch);
        GameMode.EndMatch();
    }

    bMatchEnded = true;
    UE_LOG_DISPLAY("Match", "Match finished");
}

void FFortAthenaMatch::AnnounceVictory(int32 InWinningTeam)
{
    for (const auto& Entry : TrackedPlayers)
    {
        if (Entry.second.bEliminated)
        {
            continue;
        }

        const AFortPlayerControllerAthena Controller(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.first));
        if (!Controller)
        {
            continue;
        }

        AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
        if (PlayerState)
        {
            PlayerState.SetPlace(1);
        }

        AFortPlayerControllerAthena MutableController = Controller;
        MutableController.SendEndOfMatch(true);

        UE_LOG_DISPLAY("Match", Entry.second.PlayerName + " won the match on team " + std::to_string(InWinningTeam));
    }
}

void FFortAthenaMatch::ForceStartMatch()
{
    if (CurrentPhase != EAthenaGamePhase::Warmup)
    {
        return;
    }

    PhaseStartTime = GetMatchTimeSeconds() - Settings.WarmupCountdownSeconds;
    UE_LOG_DISPLAY("Match", "Warmup skipped by request");
}

void FFortAthenaMatch::ForceEndMatch()
{
    WinningTeam = TeamRoster.GetLastStandingTeam();
    AnnounceVictory(WinningTeam);

    PhaseStartTime = GetMatchTimeSeconds() - Settings.EndOfMatchDelaySeconds;
    TransitionToPhase(EAthenaGamePhase::EndGame, GetMatchTimeSeconds());
}
