#pragma once

#include "FortniteGame/Public/Athena/FortGameStateAthena.h"
#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"

class AFortGameModeAthena : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

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
};

namespace FMatchStateNames
{
    inline constexpr std::string_view EnteringMap = "EnteringMap";
    inline constexpr std::string_view WaitingToStart = "WaitingToStart";
    inline constexpr std::string_view InProgress = "InProgress";
    inline constexpr std::string_view WaitingPostMatch = "WaitingPostMatch";
    inline constexpr std::string_view LeavingMap = "LeavingMap";
}
