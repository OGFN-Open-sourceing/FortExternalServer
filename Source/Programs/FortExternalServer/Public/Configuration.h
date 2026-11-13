#pragma once

struct FConfiguration
{
    static inline auto Playlist = "Playlist_DefaultSolo";
    static inline auto MapToLoad = "Athena_Terrain";
    static inline auto StartingLoadout = "";
    static inline auto BackendUrl = "http://127.0.0.1:5595";
    static inline auto Port = 7777;
    static inline auto MaxTickRate = 120;
    static inline auto MaxClientRate = 1000000;
    static inline auto MaxPlayers = 100;
    static inline auto MinimumPlayers = 1;
    static inline auto bAttachToRunningProcess = false;
    static inline auto bTrackProcessId = true; // keep this on and dont touch this!!
    static inline auto bSkipVersionCheck = false;
    static inline auto bHeadless = true;
    static inline constexpr auto bEnableConsole = true;
    static inline constexpr auto bVerboseLogs = false;
};
