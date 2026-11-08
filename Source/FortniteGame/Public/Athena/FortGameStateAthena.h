#pragma once

#include "FortniteGame/Public/Athena/FortPlaylistAthena.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class AFortGameStateAthena : public AActor
{
public:
    using AActor::AActor;

    EAthenaGamePhase GetGamePhase() const;

    void SetGamePhase(EAthenaGamePhase Phase);

    AFortPlaylistAthena GetCurrentPlaylist() const;

    void SetCurrentPlaylist(const AFortPlaylistAthena& Playlist);

    int32 GetPlayersLeft() const;

    void SetPlayersLeft(int32 Value);

    int32 GetTotalPlayers() const;

    void SetTotalPlayers(int32 Value);

    int32 GetTeamCount() const;

    void SetTeamCount(int32 Value);

    int32 GetTeamSize() const;

    void SetTeamSize(int32 Value);

    void SetWarmupCountdownEndTime(float Value);

    void SetWarmupCountdownStartTime(float Value);

    void SetAircraftStartTime(float Value);

    void SetSkipAircraft(bool bSkip);

    void SetSafeZonePhase(uint8 Phase);

    void SetSafeZoneStartTime(float Value);

    void SetSafeZoneCenter(const FVector& Center);

    void SetSafeZoneRadius(float Radius);

    FVector GetSafeZoneCenter() const;

    float GetSafeZoneRadius() const;

    float GetServerWorldTimeSeconds() const;

    void SetReplicatedHasBegunPlay(bool bValue);

    FScriptArrayView GetPlayerArray() const;

    FObjectHandle GetPlayerStateAt(int32 Index) const;

    FObjectHandle GetMapInfo() const;
};
