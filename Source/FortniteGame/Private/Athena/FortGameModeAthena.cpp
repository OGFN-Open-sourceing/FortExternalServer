#include "FortniteGame/Public/Athena/FortGameModeAthena.h"

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

    Object.SetObjectProperty("CurrentPlaylistData", Playlist.GetObject());

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
