#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FConfiguration
{
    static inline auto Playlist = "Playlist_DefaultSolo";
    static inline auto MapToLoad = "Athena_Terrain";
    static inline auto GameMode = "/Game/Athena/Athena_GameMode.Athena_GameMode_C";

    static inline auto Port = 7777;
    static inline auto MaxPlayers = 100;
    static inline auto TeamSize = 1;
    static inline auto MaxTickRate = 30;

    static inline auto MinimumPlayersToStart = 2;
    static inline auto WarmupCountdownSeconds = 120.0f;
    static inline auto AircraftFlightSeconds = 45.0f;
    static inline auto SafeZoneStartDelaySeconds = 30.0f;
    static inline auto EndOfMatchDelaySeconds = 15.0f;

    static inline auto bJoinInProgress = false;
    static inline auto bSpectateAfterDeath = true;
    static inline auto bFriendlyFire = false;
    static inline auto bSkipWarmup = false;
    static inline auto bStartWithoutPlayers = false;

    static inline auto StartingHealth = 100.0f;
    static inline auto StartingShield = 0.0f;
    static inline auto MaxHealth = 100.0f;
    static inline auto MaxShield = 100.0f;
    static inline auto BackpackSize = 5;
    static inline auto bHealthRegen = false;

    static inline auto StartingLoadout = "WID_Harvest_Pickaxe_Athena_C_T01";

    static inline auto bEnforceVersionMatch = true;
    static inline auto bAttachToRunningProcess = false;
    static inline auto AttachTimeoutSeconds = 180;
    static inline auto ExtraLaunchArguments = "";

    static inline auto FrameTickIntervalMilliseconds = 100;
    static inline auto bEnableConsoleCommands = true;
    static inline auto bDumpObjectsOnStart = false;
    static inline auto bLogUnresolvedSignatures = true;
    static inline auto LogLevel = "Display";
    static inline auto LogFile = "Saved/Logs/FortExternalServer.log";
};
