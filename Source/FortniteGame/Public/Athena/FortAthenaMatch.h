#pragma once

#include "FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Public/Gameplay/FortAircraft.h"
#include "FortniteGame/Public/Gameplay/FortSafeZone.h"
#include "FortniteGame/Public/Player/FortPlayerBootstrap.h"
#include "FortniteGame/Public/Player/FortTeamRoster.h"
#include "Runtime/Engine/Public/Engine/NetworkHooks.h"

#include <map>

struct FAthenaMatchSettings
{
    std::string PlaylistPath = std::string(FAthenaPaths::DefaultSoloPlaylist);
    std::string MapName = std::string(FAthenaPaths::AthenaMap);
    std::string GameModeClassPath = std::string(FAthenaPaths::AthenaGameMode);

    int32 MaxPlayers = 100;
    int32 TeamSize = 1;
    int32 MinimumPlayersToStart = 2;

    float WarmupCountdownSeconds = 120.0f;
    float AircraftFlightSeconds = 45.0f;
    float SafeZoneStartDelaySeconds = 30.0f;
    float EndOfMatchDelaySeconds = 15.0f;

    bool bAllowJoinInProgress = false;
    bool bAllowSpectateAfterDeath = true;
    bool bFriendlyFireEnabled = false;
    bool bUseGameSessions = false;

    FPlayerBootstrapSettings PlayerSettings;
};

struct FTrackedPlayer
{
    FRemoteAddress ControllerAddress = InvalidRemoteAddress;
    FRemoteAddress ConnectionAddress = InvalidRemoteAddress;
    std::string PlayerName;
    int32 TeamIndex = InvalidIndex;
    bool bInitialized = false;
    bool bEliminated = false;
    uint64 JoinTimeMilliseconds = 0;
};

class FFortAthenaMatch
{
public:
    bool Initialize(FEngineRuntime& InEngineRuntime, FNetworkHooks& InNetworkHooks, const FAthenaMatchSettings& InSettings);

    void Shutdown();

    bool TravelToAthena();

    bool PrepareMatch();

    void Tick();

    EAthenaGamePhase GetGamePhase() const;

    int32 GetConnectedPlayerCount() const;

    int32 GetAlivePlayerCount() const;

    const FAthenaMatchSettings& GetSettings() const;

    void SetSettings(const FAthenaMatchSettings& InSettings);

    bool HasMatchStarted() const;

    bool HasMatchEnded() const;

    std::vector<FTrackedPlayer> GetTrackedPlayers() const;

    void ForceStartMatch();

    void ForceEndMatch();

private:
    void ScanForNewConnections();

    void InitializeJoiningPlayer(const AFortPlayerControllerAthena& Controller, FRemoteAddress ConnectionAddress);

    void RemoveStalePlayers();

    void UpdateWarmupPhase(float CurrentTime);

    void UpdateAircraftPhase(float CurrentTime);

    void UpdateSafeZonePhase(float CurrentTime);

    void UpdateEndGamePhase(float CurrentTime);

    void UpdateEliminationState();

    void TransitionToPhase(EAthenaGamePhase Phase, float CurrentTime);

    void DropPlayerFromAircraft(const AFortPlayerControllerAthena& Controller, float CurrentTime) const;

    void AnnounceVictory(int32 WinningTeam);

    float GetMatchTimeSeconds() const;

    AFortGameModeAthena GetGameMode() const;

    AFortGameStateAthena GetGameState() const;

    FEngineRuntime* EngineRuntime = nullptr;
    FNetworkHooks* NetworkHooks = nullptr;

    FAthenaMatchSettings Settings;
    FFortPlayerBootstrap PlayerBootstrap;
    FFortTeamRoster TeamRoster;
    FFortSafeZoneDirector SafeZoneDirector;
    FFortAircraftDirector AircraftDirector;

    std::map<FRemoteAddress, FTrackedPlayer> TrackedPlayers;

    EAthenaGamePhase CurrentPhase = EAthenaGamePhase::None;
    float PhaseStartTime = 0.0f;
    float MatchStartTimeMilliseconds = 0.0f;
    uint64 MatchStartTimestamp = 0;

    bool bInitialized = false;
    bool bMatchPrepared = false;
    bool bMatchStarted = false;
    bool bMatchEnded = false;
    int32 WinningTeam = InvalidIndex;
};
