#include "FortniteGame/Public/Player/FortTeamRoster.h"

#include <algorithm>

void FFortTeamRoster::Initialize(int32 InTeamSize, int32 InTeamCount)
{
    TeamSize = std::max(InTeamSize, 1);
    TeamCount = std::max(InTeamCount, 1);
    Reset();
}

void FFortTeamRoster::Reset()
{
    ControllerToTeam.clear();
    TeamToControllers.clear();
    EliminatedControllers.clear();
}

int32 FFortTeamRoster::FindTeamWithSpace() const
{
    for (int32 Team = FirstPlayerTeamIndex; Team < FirstPlayerTeamIndex + TeamCount; ++Team)
    {
        const auto Existing = TeamToControllers.find(Team);
        if (Existing == TeamToControllers.end() || static_cast<int32>(Existing->second.size()) < TeamSize)
        {
            return Team;
        }
    }

    return InvalidIndex;
}

int32 FFortTeamRoster::AssignPlayer(const AFortPlayerControllerAthena& Controller)
{
    if (!Controller)
    {
        return InvalidIndex;
    }

    const FRemoteAddress Key = Controller.GetAddress();

    const auto Existing = ControllerToTeam.find(Key);
    if (Existing != ControllerToTeam.end())
    {
        return Existing->second;
    }

    const int32 Team = FindTeamWithSpace();
    if (Team == InvalidIndex)
    {
        UE_LOG_WARNING("Teams", "No team slot remains for the joining player");
        return InvalidIndex;
    }

    ControllerToTeam[Key] = Team;
    TeamToControllers[Team].push_back(Key);
    EliminatedControllers[Key] = false;

    AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
    if (PlayerState)
    {
        PlayerState.SetTeamIndex(Team);
    }

    UE_LOG_DISPLAY("Teams", "Assigned player to team " + std::to_string(Team));
    return Team;
}

void FFortTeamRoster::RemovePlayer(const AFortPlayerControllerAthena& Controller)
{
    if (!Controller)
    {
        return;
    }

    const FRemoteAddress Key = Controller.GetAddress();

    const auto Existing = ControllerToTeam.find(Key);
    if (Existing == ControllerToTeam.end())
    {
        return;
    }

    std::vector<FRemoteAddress>& Members = TeamToControllers[Existing->second];
    Members.erase(std::remove(Members.begin(), Members.end(), Key), Members.end());

    ControllerToTeam.erase(Existing);
    EliminatedControllers.erase(Key);
}

int32 FFortTeamRoster::GetTeamOf(const AFortPlayerControllerAthena& Controller) const
{
    if (!Controller)
    {
        return InvalidIndex;
    }

    const auto Existing = ControllerToTeam.find(Controller.GetAddress());
    return Existing != ControllerToTeam.end() ? Existing->second : InvalidIndex;
}

int32 FFortTeamRoster::GetTeamMemberCount(int32 TeamIndex) const
{
    const auto Existing = TeamToControllers.find(TeamIndex);
    return Existing != TeamToControllers.end() ? static_cast<int32>(Existing->second.size()) : 0;
}

std::vector<FRemoteAddress> FFortTeamRoster::GetTeamMembers(int32 TeamIndex) const
{
    const auto Existing = TeamToControllers.find(TeamIndex);
    return Existing != TeamToControllers.end() ? Existing->second : std::vector<FRemoteAddress>();
}

int32 FFortTeamRoster::GetAssignedPlayerCount() const
{
    return static_cast<int32>(ControllerToTeam.size());
}

void FFortTeamRoster::MarkPlayerEliminated(const AFortPlayerControllerAthena& Controller)
{
    if (!Controller)
    {
        return;
    }

    EliminatedControllers[Controller.GetAddress()] = true;
}

bool FFortTeamRoster::IsTeamEliminated(int32 TeamIndex) const
{
    const std::vector<FRemoteAddress> Members = GetTeamMembers(TeamIndex);
    if (Members.empty())
    {
        return true;
    }

    for (const FRemoteAddress Member : Members)
    {
        const auto Eliminated = EliminatedControllers.find(Member);
        if (Eliminated == EliminatedControllers.end() || !Eliminated->second)
        {
            return false;
        }
    }

    return true;
}

int32 FFortTeamRoster::GetAliveTeamCount() const
{
    int32 Alive = 0;

    for (const auto& Entry : TeamToControllers)
    {
        if (!Entry.second.empty() && !IsTeamEliminated(Entry.first))
        {
            ++Alive;
        }
    }

    return Alive;
}

int32 FFortTeamRoster::GetLastStandingTeam() const
{
    int32 LastTeam = InvalidIndex;
    int32 Alive = 0;

    for (const auto& Entry : TeamToControllers)
    {
        if (!Entry.second.empty() && !IsTeamEliminated(Entry.first))
        {
            LastTeam = Entry.first;
            ++Alive;
        }
    }

    return Alive == 1 ? LastTeam : InvalidIndex;
}
