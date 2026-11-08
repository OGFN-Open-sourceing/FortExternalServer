#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FAthenaMatchSettings
{
    std::string PlaylistPath;
    std::string MapName;
    std::string GameModeClassPath;
    int32 MaxPlayers = 100;
    int32 MinimumPlayersToStart = 1;
    std::vector<std::string> StartingLoadout;
};
