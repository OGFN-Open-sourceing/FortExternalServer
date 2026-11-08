#include "FortniteGame/Public/Athena/FortPlaylistAthena.h"

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
