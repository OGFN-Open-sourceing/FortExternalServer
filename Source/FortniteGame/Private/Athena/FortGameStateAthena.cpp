#include "FortniteGame/Public/Athena/FortGameStateAthena.h"

int32 AFortPlaylistAthena::GetPlaylistId() const
{
    return Object.GetProperty<int32>("PlaylistId", 0);
}

int32 AFortPlaylistAthena::GetMaxSquadSize() const
{
    return Object.GetProperty<int32>("MaxSquadSize", 1);
}

int32 AFortPlaylistAthena::GetMaxTeamCount() const
{
    return Object.GetProperty<int32>("MaxTeamCount", 100);
}

void AFortPlaylistAthena::SetMaxSquadSize(int32 Value)
{
    Object.SetProperty<int32>("MaxSquadSize", Value);
}

void AFortPlaylistAthena::SetMaxTeamCount(int32 Value)
{
    Object.SetProperty<int32>("MaxTeamCount", Value);
}

void AFortPlaylistAthena::SetRespawnType(EAthenaRespawnType Type)
{
    Object.SetProperty<uint8>("RespawnType", ToUnderlying(Type));
}

void AFortPlaylistAthena::SetRespawnLocation(EAthenaRespawnLocation Location)
{
    Object.SetProperty<uint8>("RespawnLocation", ToUnderlying(Location));
}

void AFortPlaylistAthena::SetFriendlyFireType(EFriendlyFireType Type)
{
    Object.SetProperty<uint8>("FriendlyFireType", ToUnderlying(Type));
}

void AFortPlaylistAthena::SetDownedButNotOutEnabled(bool bEnabled)
{
    Object.SetBoolProperty("bNoDBNO", !bEnabled);
}

EAthenaGamePhase AFortGameStateAthena::GetGamePhase() const
{
    return static_cast<EAthenaGamePhase>(Object.GetProperty<uint8>("GamePhase", 0));
}

void AFortGameStateAthena::SetGamePhase(EAthenaGamePhase Phase)
{
    const uint8 PreviousPhase = Object.GetProperty<uint8>("GamePhase", 0);
    Object.SetProperty<uint8>("GamePhase", ToUnderlying(Phase));

    struct FOnRepGamePhaseParameters
    {
        uint8 OldGamePhase = 0;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.OldGamePhase = PreviousPhase;
    Object.InvokeFunction("OnRep_GamePhase", &Parameters, sizeof(Parameters));

    ForceNetUpdate();
}

AFortPlaylistAthena AFortGameStateAthena::GetCurrentPlaylist() const
{
    return AFortPlaylistAthena(Object.GetObjectProperty("CurrentPlaylistData"));
}

void AFortGameStateAthena::SetCurrentPlaylist(const AFortPlaylistAthena& Playlist)
{
    if (!Playlist)
    {
        return;
    }

    Object.SetObjectProperty("CurrentPlaylistData", Playlist.GetObject());
    Object.SetProperty<int32>("CurrentPlaylistId", Playlist.GetPlaylistId());

    Object.InvokeFunction("OnRep_CurrentPlaylistData");
    Object.InvokeFunction("OnRep_CurrentPlaylistId");

    ForceNetUpdate();
}

int32 AFortGameStateAthena::GetPlayersLeft() const
{
    return Object.GetProperty<int32>("PlayersLeft", 0);
}

void AFortGameStateAthena::SetPlayersLeft(int32 Value)
{
    Object.SetProperty<int32>("PlayersLeft", Value);
}

int32 AFortGameStateAthena::GetTotalPlayers() const
{
    return Object.GetProperty<int32>("TotalPlayers", 0);
}

void AFortGameStateAthena::SetTotalPlayers(int32 Value)
{
    Object.SetProperty<int32>("TotalPlayers", Value);
}

int32 AFortGameStateAthena::GetTeamCount() const
{
    return Object.GetProperty<int32>("TeamCount", 0);
}

void AFortGameStateAthena::SetTeamCount(int32 Value)
{
    Object.SetProperty<int32>("TeamCount", Value);
}

int32 AFortGameStateAthena::GetTeamSize() const
{
    return Object.GetProperty<int32>("TeamSize", 1);
}

void AFortGameStateAthena::SetTeamSize(int32 Value)
{
    Object.SetProperty<int32>("TeamSize", Value);
}

void AFortGameStateAthena::SetWarmupCountdownEndTime(float Value)
{
    Object.SetProperty<float>("WarmupCountdownEndTime", Value);
}

void AFortGameStateAthena::SetWarmupCountdownStartTime(float Value)
{
    Object.SetProperty<float>("WarmupCountdownStartTime", Value);
}

void AFortGameStateAthena::SetAircraftStartTime(float Value)
{
    Object.SetProperty<float>("AircraftStartTime", Value);
}

void AFortGameStateAthena::SetSkipAircraft(bool bSkip)
{
    Object.SetBoolProperty("bGameModeWillSkipAircraft", bSkip);
}

void AFortGameStateAthena::SetSafeZonePhase(uint8 Phase)
{
    Object.SetProperty<uint8>("SafeZonePhase", Phase);
    Object.InvokeFunction("OnRep_SafeZonePhase");
    ForceNetUpdate();
}

void AFortGameStateAthena::SetSafeZoneStartTime(float Value)
{
    Object.SetProperty<float>("SafeZoneStartShrinkTime", Value);
}

void AFortGameStateAthena::SetSafeZoneCenter(const FVector& Center)
{
    Object.SetProperty<FVector>("SafeZoneLocation", Center);
}

void AFortGameStateAthena::SetSafeZoneRadius(float Radius)
{
    Object.SetProperty<float>("SafeZoneRadius", Radius);
}

FVector AFortGameStateAthena::GetSafeZoneCenter() const
{
    return Object.GetProperty<FVector>("SafeZoneLocation", FVector());
}

float AFortGameStateAthena::GetSafeZoneRadius() const
{
    return Object.GetProperty<float>("SafeZoneRadius", 0.0f);
}

float AFortGameStateAthena::GetServerWorldTimeSeconds() const
{
    return Object.GetProperty<float>("ServerWorldTimeSecondsDelta", 0.0f) + Object.GetProperty<float>("ReplicatedWorldTimeSeconds", 0.0f);
}

void AFortGameStateAthena::SetReplicatedHasBegunPlay(bool bValue)
{
    Object.SetBoolProperty("bReplicatedHasBegunPlay", bValue);
    Object.InvokeFunction("OnRep_ReplicatedHasBegunPlay");
}

FScriptArrayView AFortGameStateAthena::GetPlayerArray() const
{
    return Object.GetArrayProperty("PlayerArray");
}

FObjectHandle AFortGameStateAthena::GetPlayerStateAt(int32 Index) const
{
    return Object.GetArrayElementAsObject("PlayerArray", Index);
}

FObjectHandle AFortGameStateAthena::GetMapInfo() const
{
    return Object.GetObjectProperty("MapInfo");
}

void AFortGameStateAthena::ForceNetUpdate()
{
    Object.InvokeFunction("ForceNetUpdate");
}
