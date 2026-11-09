#pragma once

#include "FortniteGame/Public/Athena/FortGameStateAthena.h"
#include "FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"
#include "Runtime/CoreUObject/Public/UObject/NativeFunctionOverrides.h"

class AFortGameModeAthena : public AActor
{
public:
    using AActor::AActor;

    AFortGameStateAthena GetGameState() const;

    FObjectHandle GetGameSession() const;

    std::string GetMatchState() const;

    void SetMatchState(std::string_view MatchState);

    void StartPlay();

    void StartMatch();

    void EndMatch();

    void SetDisableGarbageCollectionDuringMatch(bool bValue);

    void SetAllowSpectateAfterDeath(bool bValue);

    void SetEnableReplicationGraph(bool bValue);

    void SetMinRespawnDelay(float Value);

    void SetUseSeamlessTravel(bool bValue);

    int32 GetNumberOfPlayers() const;

    bool SetupPlaylist(const AFortPlaylistAthena& Playlist);

    void ChangeGameSessionMaxPlayers(int32 MaxPlayers);

    bool IsWorldReady() const;

    void SetWorldIsReady(bool bValue);

    void SetWarmupRequiredPlayerCount(int32 Value);

    FObjectHandle GetDefaultPawnClassForController(const AActor& Controller) const;

    static ENativeOverrideResult execReadyToStartMatch(const FNativeFunctionCall& Call);

    static ENativeOverrideResult execSpawnDefaultPawnFor(const FNativeFunctionCall& Call);

    static void EnsurePoiVolumeBrushes(const FUnrealRuntime& Runtime);

    static ENativeOverrideResult execHandleStartingNewPlayer(const FNativeFunctionCall& Call);
};

namespace FMatchStateNames
{
    inline constexpr std::string_view EnteringMap = "EnteringMap";
    inline constexpr std::string_view WaitingToStart = "WaitingToStart";
    inline constexpr std::string_view InProgress = "InProgress";
    inline constexpr std::string_view WaitingPostMatch = "WaitingPostMatch";
    inline constexpr std::string_view LeavingMap = "LeavingMap";
}
