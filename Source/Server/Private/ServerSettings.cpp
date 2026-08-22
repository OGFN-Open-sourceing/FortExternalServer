#include "Server/Public/ServerSettings.h"
#include "Runtime/Core/Public/Misc/CommandLine.h"
#include "Runtime/Core/Public/Misc/Paths.h"

namespace
{
    constexpr std::string_view DefaultLaunchArguments =
        "-AUTH_LOGIN=unused -AUTH_PASSWORD=unused -AUTH_TYPE=epic -epicapp=Fortnite -epicenv=Prod -epiclocale=en-us -epicportal -nosplash -log";

    std::vector<std::string> ParseLoadoutList(const std::string& Value)
    {
        std::vector<std::string> Entries;

        for (const std::string& Raw : FStringConv::SplitBy(Value, ','))
        {
            const std::string Trimmed = FStringConv::TrimWhitespace(Raw);
            if (!Trimmed.empty())
            {
                Entries.push_back(Trimmed);
            }
        }

        return Entries;
    }

    std::string ResolvePlaylistPath(const std::string& Identifier)
    {
        if (Identifier.find('.') != std::string::npos)
        {
            return Identifier;
        }

        return "FortPlaylistAthena " + Identifier + '.' + Identifier;
    }
}

bool FServerSettings::Load(const std::wstring& BuildRoot)
{
    FConfigFile ConfigFile;

    const std::wstring ConfigPath = FPaths::Combine(FPaths::Combine(BuildRoot, L"Config"), L"DefaultServer.ini");

    if (!ConfigFile.Load(ConfigPath))
    {
        const std::wstring FallbackPath = FPaths::Combine(FPaths::Combine(FPaths::GetExecutableDirectory(), L"Config"), L"DefaultServer.ini");
        ConfigFile.Load(FallbackPath);
    }

    Process.GameExecutableRelativePath = FStringConv::ToWide(
        ConfigFile.GetString("Process", "GameExecutable", "FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe"));
    Process.ExtraArguments = FStringConv::ToWide(ConfigFile.GetString("Process", "ExtraArguments", ""));
    Process.AttachTimeoutSeconds = static_cast<uint32>(ConfigFile.GetInt("Process", "AttachTimeoutSeconds", 180));
    Process.bAttachToRunningProcess = ConfigFile.GetBool("Process", "AttachToRunningProcess", false);

    EngineExpectations.FortniteVersion = ConfigFile.GetDouble("Engine", "ExpectedFortniteVersion", 3.6);
    EngineExpectations.EngineVersion = ConfigFile.GetDouble("Engine", "ExpectedEngineVersion", 4.19);
    EngineExpectations.Changelist = static_cast<int32>(ConfigFile.GetInt("Engine", "ExpectedChangelist", 4019403));
    EngineExpectations.bEnforceVersionMatch = ConfigFile.GetBool("Engine", "EnforceVersionMatch", true);

    Session.Port = static_cast<uint16>(ConfigFile.GetInt("Session", "Port", 7777));

    Logging.Verbosity = FServerLog::ParseVerbosity(ConfigFile.GetString("Logging", "LogLevel", "Display"), ELogVerbosity::Display);
    Logging.LogFileRelativePath = FStringConv::ToWide(ConfigFile.GetString("Logging", "LogFile", "Saved/Logs/FortExternalServer.log"));

    Match.MapName = ConfigFile.GetString("Session", "Map", std::string(FAthenaPaths::AthenaMap));
    Match.GameModeClassPath = ConfigFile.GetString("Session", "GameMode", std::string(FAthenaPaths::AthenaGameMode));
    Match.PlaylistPath = ResolvePlaylistPath(ConfigFile.GetString("Session", "Playlist", "Playlist_DefaultSolo"));
    Match.MaxPlayers = static_cast<int32>(ConfigFile.GetInt("Session", "MaxPlayers", 100));
    Match.TeamSize = static_cast<int32>(ConfigFile.GetInt("Session", "TeamSize", 1));
    Match.bAllowJoinInProgress = ConfigFile.GetBool("Session", "AllowJoinInProgress", false);
    Match.bAllowSpectateAfterDeath = ConfigFile.GetBool("Session", "AllowSpectateAfterDeath", true);

    Match.WarmupCountdownSeconds = ConfigFile.GetFloat("Match", "WarmupCountdownSeconds", 120.0f);
    Match.MinimumPlayersToStart = static_cast<int32>(ConfigFile.GetInt("Match", "MinimumPlayersToStart", 2));
    Match.AircraftFlightSeconds = ConfigFile.GetFloat("Match", "AircraftFlightSeconds", 45.0f);
    Match.SafeZoneStartDelaySeconds = ConfigFile.GetFloat("Match", "SafeZoneStartDelaySeconds", 30.0f);
    Match.EndOfMatchDelaySeconds = ConfigFile.GetFloat("Match", "EndOfMatchDelaySeconds", 15.0f);

    Match.bFriendlyFireEnabled = ConfigFile.GetBool("Gameplay", "FriendlyFireEnabled", false);

    Match.PlayerSettings.StartingHealth = ConfigFile.GetFloat("Gameplay", "StartingHealth", 100.0f);
    Match.PlayerSettings.StartingShield = ConfigFile.GetFloat("Gameplay", "StartingShield", 0.0f);
    Match.PlayerSettings.MaxHealth = ConfigFile.GetFloat("Gameplay", "MaxHealth", 100.0f);
    Match.PlayerSettings.MaxShield = ConfigFile.GetFloat("Gameplay", "MaxShield", 100.0f);
    Match.PlayerSettings.BackpackSize = static_cast<int32>(ConfigFile.GetInt("Gameplay", "BackpackSize", 5));
    Match.PlayerSettings.bHealthRegenEnabled = ConfigFile.GetBool("Gameplay", "HealthRegenEnabled", false);
    Match.PlayerSettings.StartingLoadout = ParseLoadoutList(ConfigFile.GetString("Gameplay", "StartingLoadout", std::string(FAthenaPaths::DefaultPickaxe)));

    ApplyCommandLineOverrides();

    return true;
}

void FServerSettings::ApplyCommandLineOverrides()
{
    const std::string PlaylistOverride = FCommandLine::GetValue("Playlist", "");
    if (!PlaylistOverride.empty())
    {
        Match.PlaylistPath = ResolvePlaylistPath(PlaylistOverride);
    }

    const std::string MapOverride = FCommandLine::GetValue("Map", "");
    if (!MapOverride.empty())
    {
        Match.MapName = MapOverride;
    }

    const std::string MaxPlayersOverride = FCommandLine::GetValue("MaxPlayers", "");
    if (!MaxPlayersOverride.empty())
    {
        Match.MaxPlayers = static_cast<int32>(FStringConv::ParseInt(MaxPlayersOverride, Match.MaxPlayers));
    }

    const std::string TeamSizeOverride = FCommandLine::GetValue("TeamSize", "");
    if (!TeamSizeOverride.empty())
    {
        Match.TeamSize = static_cast<int32>(FStringConv::ParseInt(TeamSizeOverride, Match.TeamSize));
    }

    const std::string MinimumPlayersOverride = FCommandLine::GetValue("MinimumPlayers", "");
    if (!MinimumPlayersOverride.empty())
    {
        Match.MinimumPlayersToStart = static_cast<int32>(FStringConv::ParseInt(MinimumPlayersOverride, Match.MinimumPlayersToStart));
    }

    const std::string PortOverride = FCommandLine::GetValue("Port", "");
    if (!PortOverride.empty())
    {
        Session.Port = static_cast<uint16>(FStringConv::ParseInt(PortOverride, Session.Port));
    }

    const std::string VerbosityOverride = FCommandLine::GetValue("LogLevel", "");
    if (!VerbosityOverride.empty())
    {
        Logging.Verbosity = FServerLog::ParseVerbosity(VerbosityOverride, Logging.Verbosity);
    }

    if (FCommandLine::HasSwitch("AttachToRunningProcess"))
    {
        Process.bAttachToRunningProcess = true;
    }

    if (FCommandLine::HasSwitch("AllowJoinInProgress"))
    {
        Match.bAllowJoinInProgress = true;
    }

    if (FCommandLine::HasSwitch("SkipVersionCheck"))
    {
        EngineExpectations.bEnforceVersionMatch = false;
    }
}

const FProcessSettings& FServerSettings::GetProcess() const
{
    return Process;
}

const FEngineExpectations& FServerSettings::GetEngineExpectations() const
{
    return EngineExpectations;
}

const FSessionSettings& FServerSettings::GetSession() const
{
    return Session;
}

const FLoggingSettings& FServerSettings::GetLogging() const
{
    return Logging;
}

const FAthenaMatchSettings& FServerSettings::GetMatch() const
{
    return Match;
}

std::wstring FServerSettings::GetGameExecutablePath(const std::wstring& BuildRoot) const
{
    return FPaths::Combine(BuildRoot, Process.GameExecutableRelativePath);
}

std::wstring FServerSettings::GetGameExecutableName() const
{
    const std::wstring Normalized = FPaths::NormalizeSeparators(Process.GameExecutableRelativePath);

    const size_t Separator = Normalized.find_last_of(L'\\');
    return Separator == std::wstring::npos ? Normalized : Normalized.substr(Separator + 1);
}

std::string FServerSettings::BuildLaunchArguments() const
{
    std::string Arguments(DefaultLaunchArguments);

    const std::string Extra = FStringConv::ToNarrow(Process.ExtraArguments);
    if (!Extra.empty())
    {
        Arguments.push_back(' ');
        Arguments.append(Extra);
    }

    return Arguments;
}
