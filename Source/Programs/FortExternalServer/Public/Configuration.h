#pragma once

struct FConfiguration
{
    static inline auto Playlist = "Playlist_DefaultSolo";
    static inline auto MapToLoad = "Athena_Terrain";
    static inline auto StartingLoadout = "";
    static inline auto Port = 7777;
    static inline auto MaxTickRate = 30;
    static inline auto MaxPlayers = 100;
    static inline auto MinimumPlayers = 1;
    static inline auto bAttachToRunningProcess = false;
    static inline auto bSkipVersionCheck = false;
    static inline constexpr auto bEnableConsole = true;
    static inline constexpr auto bVerboseLogs = false;
};
