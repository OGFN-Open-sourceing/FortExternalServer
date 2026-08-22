#pragma once

struct FConfiguration
{
    static inline auto Playlist = "Playlist_DefaultSolo";
    static inline auto MapToLoad = "Athena_Terrain";
    static inline auto StartingLoadout = "WID_Harvest_Pickaxe_Athena_C_T01";
    static inline auto Port = 7777;
    static inline auto MaxTickRate = 30;
    static inline auto MaxPlayers = 100;
    static inline auto TeamSize = 1;
    static inline auto MinimumPlayers = 2;
    static inline auto WarmupTime = 120;
    static inline auto bSessions = false;
    static inline auto bJoinInProgress = false;
    static inline auto bFriendlyFire = false;
    static inline auto bHealthRegen = false;
    static inline auto bSpectateAfterDeath = true;
    static inline auto bAttachToRunningProcess = false;
    static inline auto bSkipVersionCheck = false;
    static inline constexpr auto bEnableConsole = true;
    static inline constexpr auto bVerboseLogs = false;
};
