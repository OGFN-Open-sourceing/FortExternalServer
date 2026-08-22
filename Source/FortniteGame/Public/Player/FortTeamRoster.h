#pragma once

#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"

#include <map>

class FFortTeamRoster
{
public:
    void Initialize(int32 InTeamSize, int32 InTeamCount);

    void Reset();

    int32 AssignPlayer(const AFortPlayerControllerAthena& Controller);

    void RemovePlayer(const AFortPlayerControllerAthena& Controller);

    int32 GetTeamOf(const AFortPlayerControllerAthena& Controller) const;

    int32 GetTeamMemberCount(int32 TeamIndex) const;

    int32 GetAliveTeamCount() const;

    int32 GetAssignedPlayerCount() const;

    std::vector<FRemoteAddress> GetTeamMembers(int32 TeamIndex) const;

    void MarkPlayerEliminated(const AFortPlayerControllerAthena& Controller);

    bool IsTeamEliminated(int32 TeamIndex) const;

    int32 GetLastStandingTeam() const;

private:
    static constexpr int32 FirstPlayerTeamIndex = 2;

    int32 FindTeamWithSpace() const;

    int32 TeamSize = 1;
    int32 TeamCount = 100;

    std::map<FRemoteAddress, int32> ControllerToTeam;
    std::map<int32, std::vector<FRemoteAddress>> TeamToControllers;
    std::map<FRemoteAddress, bool> EliminatedControllers;
};
