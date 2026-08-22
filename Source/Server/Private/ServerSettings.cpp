#include "Server/Public/ServerSettings.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Core/Public/Misc/CommandLine.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Server/Public/Configuration.h"

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

    std::string DescribeBool(bool Value)
    {
        return Value ? "true" : "false";
    }
}

void FServerSettings::ApplyCompiledDefaults()
{
    Process.GameExecutableRelativePath = L"FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe";
    Process.ExtraArguments = FStringConv::ToWide(FConfiguration::ExtraLaunchArguments);
    Process.AttachTimeoutSeconds = static_cast<uint32>(FConfiguration::AttachTimeoutSeconds);
    Process.bAttachToRunningProcess = FConfiguration::bAttachToRunningProcess;

    EngineExpectations.FortniteVersion = GetActiveBuildProfile().FortniteVersion;
    EngineExpectations.EngineVersion = GetActiveBuildProfile().EngineVersion;
    EngineExpectations.Changelist = GetActiveBuildProfile().Changelist;
    EngineExpectations.bEnforceVersionMatch = FConfiguration::bEnforceVersionMatch;

    Session.Port = static_cast<uint16>(FConfiguration::Port);

    Runtime.FrameTickIntervalMilliseconds = static_cast<uint64>(FConfiguration::FrameTickIntervalMilliseconds);
    Runtime.MaxTickRate = FConfiguration::MaxTickRate;
    Runtime.bEnableConsoleCommands = FConfiguration::bEnableConsoleCommands;
    Runtime.bDumpObjectsOnStart = FConfiguration::bDumpObjectsOnStart;
    Runtime.bLogUnresolvedSignatures = FConfiguration::bLogUnresolvedSignatures;

    Logging.Verbosity = FServerLog::ParseVerbosity(FConfiguration::LogLevel, ELogVerbosity::Display);
    Logging.LogFileRelativePath = FStringConv::ToWide(FConfiguration::LogFile);

    Match.MapName = FConfiguration::MapToLoad;
    Match.GameModeClassPath = FConfiguration::GameMode;
    Match.PlaylistPath = GetActiveBuildProfile().ResolvePlaylistPath(FConfiguration::Playlist);
    Match.MaxPlayers = FConfiguration::MaxPlayers;
    Match.TeamSize = FConfiguration::TeamSize;
    Match.MinimumPlayersToStart = FConfiguration::bStartWithoutPlayers ? 0 : FConfiguration::MinimumPlayersToStart;
    Match.WarmupCountdownSeconds = FConfiguration::bSkipWarmup ? 0.0f : FConfiguration::WarmupCountdownSeconds;
    Match.AircraftFlightSeconds = FConfiguration::AircraftFlightSeconds;
    Match.SafeZoneStartDelaySeconds = FConfiguration::SafeZoneStartDelaySeconds;
    Match.EndOfMatchDelaySeconds = FConfiguration::EndOfMatchDelaySeconds;
    Match.bAllowJoinInProgress = FConfiguration::bJoinInProgress;
    Match.bAllowSpectateAfterDeath = FConfiguration::bSpectateAfterDeath;
    Match.bFriendlyFireEnabled = FConfiguration::bFriendlyFire;

    Match.PlayerSettings.StartingHealth = FConfiguration::StartingHealth;
    Match.PlayerSettings.StartingShield = FConfiguration::StartingShield;
    Match.PlayerSettings.MaxHealth = FConfiguration::MaxHealth;
    Match.PlayerSettings.MaxShield = FConfiguration::MaxShield;
    Match.PlayerSettings.BackpackSize = FConfiguration::BackpackSize;
    Match.PlayerSettings.bHealthRegenEnabled = FConfiguration::bHealthRegen;
    Match.PlayerSettings.StartingLoadout = ParseLoadoutList(FConfiguration::StartingLoadout);
}

void FServerSettings::ApplyConfigFile(const FConfigFile& ConfigFile)
{
    if (!ConfigFile.IsLoaded())
    {
        return;
    }

    Process.GameExecutableRelativePath =
        FStringConv::ToWide(ConfigFile.GetString("Process", "GameExecutable", FStringConv::ToNarrow(Process.GameExecutableRelativePath)));
    Process.ExtraArguments = FStringConv::ToWide(ConfigFile.GetString("Process", "ExtraArguments", FStringConv::ToNarrow(Process.ExtraArguments)));
    Process.AttachTimeoutSeconds = static_cast<uint32>(ConfigFile.GetInt("Process", "AttachTimeoutSeconds", Process.AttachTimeoutSeconds));
    Process.bAttachToRunningProcess = ConfigFile.GetBool("Process", "AttachToRunningProcess", Process.bAttachToRunningProcess);

    EngineExpectations.bEnforceVersionMatch = ConfigFile.GetBool("Engine", "EnforceVersionMatch", EngineExpectations.bEnforceVersionMatch);

    Session.Port = static_cast<uint16>(ConfigFile.GetInt("Session", "Port", Session.Port));

    Runtime.FrameTickIntervalMilliseconds = static_cast<uint64>(ConfigFile.GetInt("Runtime", "FrameTickIntervalMilliseconds", static_cast<int64>(Runtime.FrameTickIntervalMilliseconds)));
    Runtime.MaxTickRate = static_cast<int32>(ConfigFile.GetInt("Runtime", "MaxTickRate", Runtime.MaxTickRate));
    Runtime.bEnableConsoleCommands = ConfigFile.GetBool("Runtime", "EnableConsoleCommands", Runtime.bEnableConsoleCommands);
    Runtime.bDumpObjectsOnStart = ConfigFile.GetBool("Runtime", "DumpObjectsOnStart", Runtime.bDumpObjectsOnStart);
    Runtime.bLogUnresolvedSignatures = ConfigFile.GetBool("Runtime", "LogUnresolvedSignatures", Runtime.bLogUnresolvedSignatures);

    Logging.Verbosity = FServerLog::ParseVerbosity(ConfigFile.GetString("Logging", "LogLevel", "Display"), Logging.Verbosity);
    Logging.LogFileRelativePath = FStringConv::ToWide(ConfigFile.GetString("Logging", "LogFile", FStringConv::ToNarrow(Logging.LogFileRelativePath)));

    Match.MapName = ConfigFile.GetString("Session", "Map", Match.MapName);
    Match.GameModeClassPath = ConfigFile.GetString("Session", "GameMode", Match.GameModeClassPath);
    Match.PlaylistPath = GetActiveBuildProfile().ResolvePlaylistPath(ConfigFile.GetString("Session", "Playlist", Match.PlaylistPath));
    Match.MaxPlayers = static_cast<int32>(ConfigFile.GetInt("Session", "MaxPlayers", Match.MaxPlayers));
    Match.TeamSize = static_cast<int32>(ConfigFile.GetInt("Session", "TeamSize", Match.TeamSize));
    Match.bAllowJoinInProgress = ConfigFile.GetBool("Session", "AllowJoinInProgress", Match.bAllowJoinInProgress);
    Match.bAllowSpectateAfterDeath = ConfigFile.GetBool("Session", "AllowSpectateAfterDeath", Match.bAllowSpectateAfterDeath);

    Match.WarmupCountdownSeconds = ConfigFile.GetFloat("Match", "WarmupCountdownSeconds", Match.WarmupCountdownSeconds);
    Match.MinimumPlayersToStart = static_cast<int32>(ConfigFile.GetInt("Match", "MinimumPlayersToStart", Match.MinimumPlayersToStart));
    Match.AircraftFlightSeconds = ConfigFile.GetFloat("Match", "AircraftFlightSeconds", Match.AircraftFlightSeconds);
    Match.SafeZoneStartDelaySeconds = ConfigFile.GetFloat("Match", "SafeZoneStartDelaySeconds", Match.SafeZoneStartDelaySeconds);
    Match.EndOfMatchDelaySeconds = ConfigFile.GetFloat("Match", "EndOfMatchDelaySeconds", Match.EndOfMatchDelaySeconds);

    Match.bFriendlyFireEnabled = ConfigFile.GetBool("Gameplay", "FriendlyFireEnabled", Match.bFriendlyFireEnabled);
    Match.PlayerSettings.StartingHealth = ConfigFile.GetFloat("Gameplay", "StartingHealth", Match.PlayerSettings.StartingHealth);
    Match.PlayerSettings.StartingShield = ConfigFile.GetFloat("Gameplay", "StartingShield", Match.PlayerSettings.StartingShield);
    Match.PlayerSettings.MaxHealth = ConfigFile.GetFloat("Gameplay", "MaxHealth", Match.PlayerSettings.MaxHealth);
    Match.PlayerSettings.MaxShield = ConfigFile.GetFloat("Gameplay", "MaxShield", Match.PlayerSettings.MaxShield);
    Match.PlayerSettings.BackpackSize = static_cast<int32>(ConfigFile.GetInt("Gameplay", "BackpackSize", Match.PlayerSettings.BackpackSize));
    Match.PlayerSettings.bHealthRegenEnabled = ConfigFile.GetBool("Gameplay", "HealthRegenEnabled", Match.PlayerSettings.bHealthRegenEnabled);

    const std::string LoadoutValue = ConfigFile.GetString("Gameplay", "StartingLoadout", "");
    if (!LoadoutValue.empty())
    {
        Match.PlayerSettings.StartingLoadout = ParseLoadoutList(LoadoutValue);
    }
}

void FServerSettings::ApplyCommandLineOverrides()
{
    const std::string PlaylistOverride = FCommandLine::GetValue("Playlist", "");
    if (!PlaylistOverride.empty())
    {
        Match.PlaylistPath = GetActiveBuildProfile().ResolvePlaylistPath(PlaylistOverride);
    }

    const std::string MapOverride = FCommandLine::GetValue("MapToLoad", FCommandLine::GetValue("Map", ""));
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

    const std::string WarmupOverride = FCommandLine::GetValue("WarmupSeconds", "");
    if (!WarmupOverride.empty())
    {
        Match.WarmupCountdownSeconds = static_cast<float>(FStringConv::ParseDouble(WarmupOverride, Match.WarmupCountdownSeconds));
    }

    const std::string PortOverride = FCommandLine::GetValue("Port", "");
    if (!PortOverride.empty())
    {
        Session.Port = static_cast<uint16>(FStringConv::ParseInt(PortOverride, Session.Port));
    }

    const std::string TickRateOverride = FCommandLine::GetValue("MaxTickRate", "");
    if (!TickRateOverride.empty())
    {
        Runtime.MaxTickRate = static_cast<int32>(FStringConv::ParseInt(TickRateOverride, Runtime.MaxTickRate));
    }

    const std::string VerbosityOverride = FCommandLine::GetValue("LogLevel", "");
    if (!VerbosityOverride.empty())
    {
        Logging.Verbosity = FServerLog::ParseVerbosity(VerbosityOverride, Logging.Verbosity);
    }

    const std::string LoadoutOverride = FCommandLine::GetValue("StartingLoadout", "");
    if (!LoadoutOverride.empty())
    {
        Match.PlayerSettings.StartingLoadout = ParseLoadoutList(LoadoutOverride);
    }

    if (FCommandLine::HasSwitch("bAttachToRunningProcess"))
    {
        Process.bAttachToRunningProcess = true;
    }

    if (FCommandLine::HasSwitch("bJoinInProgress"))
    {
        Match.bAllowJoinInProgress = true;
    }

    if (FCommandLine::HasSwitch("bFriendlyFire"))
    {
        Match.bFriendlyFireEnabled = true;
    }

    if (FCommandLine::HasSwitch("bSkipWarmup"))
    {
        Match.WarmupCountdownSeconds = 0.0f;
    }

    if (FCommandLine::HasSwitch("bStartWithoutPlayers"))
    {
        Match.MinimumPlayersToStart = 0;
    }

    if (FCommandLine::HasSwitch("bHealthRegen"))
    {
        Match.PlayerSettings.bHealthRegenEnabled = true;
    }

    if (FCommandLine::HasSwitch("bSkipVersionCheck"))
    {
        EngineExpectations.bEnforceVersionMatch = false;
    }

    if (FCommandLine::HasSwitch("bDumpObjectsOnStart"))
    {
        Runtime.bDumpObjectsOnStart = true;
    }

    if (FCommandLine::HasSwitch("bDisableConsole"))
    {
        Runtime.bEnableConsoleCommands = false;
    }
}

bool FServerSettings::Load(const std::wstring& BuildRoot)
{
    ApplyCompiledDefaults();

    FConfigFile ConfigFile;

    const std::wstring ConfigPath = FPaths::Combine(FPaths::Combine(BuildRoot, L"Config"), L"DefaultServer.ini");

    if (!ConfigFile.Load(ConfigPath))
    {
        const std::wstring FallbackPath = FPaths::Combine(FPaths::Combine(FPaths::GetExecutableDirectory(), L"Config"), L"DefaultServer.ini");
        ConfigFile.Load(FallbackPath);
    }

    ApplyConfigFile(ConfigFile);
    ApplyCommandLineOverrides();

    bLoadedConfigFile = ConfigFile.IsLoaded();
    return true;
}

void FServerSettings::LogResolvedConfiguration() const
{
    UE_LOG_DISPLAY("Config", "Target build " + GetActiveBuildProfile().Describe());
    UE_LOG_DISPLAY("Config", std::string("Configuration file ") + (bLoadedConfigFile ? "loaded" : "not found, using compiled defaults"));
    UE_LOG_DISPLAY("Config", "Playlist " + Match.PlaylistPath);
    UE_LOG_DISPLAY("Config", "Map " + Match.MapName);
    UE_LOG_DISPLAY("Config", "GameMode " + Match.GameModeClassPath);
    UE_LOG_DISPLAY("Config", "Port " + std::to_string(Session.Port) + ", MaxPlayers " + std::to_string(Match.MaxPlayers) + ", TeamSize " + std::to_string(Match.TeamSize));
    UE_LOG_DISPLAY("Config", "MinimumPlayersToStart " + std::to_string(Match.MinimumPlayersToStart) + ", WarmupCountdownSeconds " +
        std::to_string(static_cast<int32>(Match.WarmupCountdownSeconds)));
    UE_LOG_DISPLAY("Config", "AircraftFlightSeconds " + std::to_string(static_cast<int32>(Match.AircraftFlightSeconds)) + ", SafeZoneStartDelaySeconds " +
        std::to_string(static_cast<int32>(Match.SafeZoneStartDelaySeconds)));
    UE_LOG_DISPLAY("Config", "JoinInProgress " + DescribeBool(Match.bAllowJoinInProgress) + ", SpectateAfterDeath " +
        DescribeBool(Match.bAllowSpectateAfterDeath) + ", FriendlyFire " + DescribeBool(Match.bFriendlyFireEnabled));
    UE_LOG_DISPLAY("Config", "Health " + std::to_string(static_cast<int32>(Match.PlayerSettings.StartingHealth)) + "/" +
        std::to_string(static_cast<int32>(Match.PlayerSettings.MaxHealth)) + ", Shield " +
        std::to_string(static_cast<int32>(Match.PlayerSettings.StartingShield)) + "/" + std::to_string(static_cast<int32>(Match.PlayerSettings.MaxShield)) +
        ", BackpackSize " + std::to_string(Match.PlayerSettings.BackpackSize) + ", HealthRegen " + DescribeBool(Match.PlayerSettings.bHealthRegenEnabled));

    for (const std::string& Entry : Match.PlayerSettings.StartingLoadout)
    {
        UE_LOG_DISPLAY("Config", "StartingLoadout entry " + Entry);
    }

    UE_LOG_DISPLAY("Config", "MaxTickRate " + std::to_string(Runtime.MaxTickRate) + ", FrameTickIntervalMilliseconds " +
        std::to_string(Runtime.FrameTickIntervalMilliseconds) + ", EnforceVersionMatch " + DescribeBool(EngineExpectations.bEnforceVersionMatch));
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

const FRuntimeSettings& FServerSettings::GetRuntime() const
{
    return Runtime;
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

    Arguments.append(" -port=");
    Arguments.append(std::to_string(Session.Port));

    const std::string Extra = FStringConv::ToNarrow(Process.ExtraArguments);
    if (!Extra.empty())
    {
        Arguments.push_back(' ');
        Arguments.append(Extra);
    }

    return Arguments;
}
