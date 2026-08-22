#include "Server/Public/ServerSettings.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Server/Public/Configuration.h"

namespace
{
    constexpr std::wstring_view GameExecutableRelativePath = L"FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe";
    constexpr std::wstring_view LogFileRelativePath = L"Saved/Logs/FortExternalServer.log";

    constexpr std::string_view LaunchArguments =
        "-AUTH_LOGIN=unused -AUTH_PASSWORD=unused -AUTH_TYPE=epic -epicapp=Fortnite -epicenv=Prod -epiclocale=en-us -epicportal -nosplash -log";

    std::vector<std::string> SplitLoadout(std::string_view Value)
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

void FServerSettings::Resolve()
{
    const FFortBuildProfile& Profile = GetActiveBuildProfile();

    Match.PlaylistPath = Profile.ResolvePlaylistPath(FConfiguration::Playlist);
    Match.MapName = FConfiguration::MapToLoad;
    Match.GameModeClassPath = Profile.AssetPaths.GameModeClassPath;

    Match.MaxPlayers = FConfiguration::MaxPlayers;
    Match.TeamSize = FConfiguration::TeamSize;
    Match.MinimumPlayersToStart = FConfiguration::MinimumPlayers;
    Match.WarmupCountdownSeconds = static_cast<float>(FConfiguration::WarmupTime);

    Match.AircraftFlightSeconds = Profile.AircraftFlightSeconds;
    Match.SafeZoneStartDelaySeconds = Profile.SafeZoneStartDelaySeconds;
    Match.EndOfMatchDelaySeconds = Profile.EndOfMatchDelaySeconds;

    Match.bAllowJoinInProgress = FConfiguration::bJoinInProgress;
    Match.bAllowSpectateAfterDeath = FConfiguration::bSpectateAfterDeath;
    Match.bFriendlyFireEnabled = FConfiguration::bFriendlyFire;
    Match.bUseGameSessions = FConfiguration::bSessions;

    Match.PlayerSettings.StartingHealth = Profile.StartingHealth;
    Match.PlayerSettings.StartingShield = Profile.StartingShield;
    Match.PlayerSettings.MaxHealth = Profile.MaxHealth;
    Match.PlayerSettings.MaxShield = Profile.MaxShield;
    Match.PlayerSettings.BackpackSize = Profile.BackpackSize;
    Match.PlayerSettings.bHealthRegenEnabled = FConfiguration::bHealthRegen;
    Match.PlayerSettings.StartingLoadout = SplitLoadout(FConfiguration::StartingLoadout);
}

void FServerSettings::LogResolvedConfiguration() const
{
    UE_LOG_DISPLAY("Config", "Target build " + GetActiveBuildProfile().Describe());
    UE_LOG_DISPLAY("Config", "Playlist " + Match.PlaylistPath);
    UE_LOG_DISPLAY("Config", "MapToLoad " + Match.MapName);
    UE_LOG_DISPLAY("Config", "Port " + std::to_string(GetPort()) + ", MaxTickRate " + std::to_string(GetMaxTickRate()));
    UE_LOG_DISPLAY("Config", "MaxPlayers " + std::to_string(Match.MaxPlayers) + ", TeamSize " + std::to_string(Match.TeamSize) + ", MinimumPlayers " +
        std::to_string(Match.MinimumPlayersToStart) + ", WarmupTime " + std::to_string(static_cast<int32>(Match.WarmupCountdownSeconds)));
    UE_LOG_DISPLAY("Config", "bSessions " + DescribeBool(Match.bUseGameSessions) + ", bJoinInProgress " + DescribeBool(Match.bAllowJoinInProgress) +
        ", bFriendlyFire " + DescribeBool(Match.bFriendlyFireEnabled));
    UE_LOG_DISPLAY("Config", "bHealthRegen " + DescribeBool(Match.PlayerSettings.bHealthRegenEnabled) + ", bSpectateAfterDeath " +
        DescribeBool(Match.bAllowSpectateAfterDeath) + ", bSkipVersionCheck " + DescribeBool(FConfiguration::bSkipVersionCheck));

    for (const std::string& Entry : Match.PlayerSettings.StartingLoadout)
    {
        UE_LOG_DISPLAY("Config", "StartingLoadout " + Entry);
    }
}

const FAthenaMatchSettings& FServerSettings::GetMatch() const
{
    return Match;
}

uint16 FServerSettings::GetPort() const
{
    return static_cast<uint16>(FConfiguration::Port);
}

int32 FServerSettings::GetMaxTickRate() const
{
    return FConfiguration::MaxTickRate;
}

bool FServerSettings::ShouldAttachToRunningProcess() const
{
    return FConfiguration::bAttachToRunningProcess;
}

bool FServerSettings::ShouldEnforceVersionMatch() const
{
    return !FConfiguration::bSkipVersionCheck;
}

bool FServerSettings::AreConsoleCommandsEnabled() const
{
    return FConfiguration::bEnableConsole;
}

ELogVerbosity FServerSettings::GetLogVerbosity() const
{
    return FConfiguration::bVerboseLogs ? ELogVerbosity::Verbose : ELogVerbosity::Display;
}

std::wstring FServerSettings::GetLogFileRelativePath() const
{
    return std::wstring(LogFileRelativePath);
}

std::wstring FServerSettings::GetGameExecutablePath(const std::wstring& BuildRoot) const
{
    return FPaths::Combine(BuildRoot, std::wstring(GameExecutableRelativePath));
}

std::wstring FServerSettings::GetGameExecutableName() const
{
    const std::wstring Normalized = FPaths::NormalizeSeparators(std::wstring(GameExecutableRelativePath));

    const size_t Separator = Normalized.find_last_of(L'\\');
    return Separator == std::wstring::npos ? Normalized : Normalized.substr(Separator + 1);
}

std::string FServerSettings::BuildLaunchArguments() const
{
    return std::string(LaunchArguments) + " -port=" + std::to_string(GetPort());
}
