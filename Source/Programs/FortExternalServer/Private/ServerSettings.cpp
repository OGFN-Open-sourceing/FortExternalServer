#include "Programs/FortExternalServer/Public/ServerSettings.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Programs/FortExternalServer/Public/Configuration.h"

namespace
{
    constexpr std::wstring_view GameExecutableRelativePath = L"FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe";
    constexpr std::wstring_view LogFileRelativePath = L"Saved/Logs/FortExternalServer.log";

    constexpr std::string_view LaunchArguments =
        "-AUTH_LOGIN=unused -AUTH_PASSWORD=unused -AUTH_TYPE=epic -epicapp=Fortnite -epicenv=Prod -epiclocale=en-us -epicportal -skippatchcheck -nobe -fromfl=eac -fltoken=3db3ba5dcbd2e16703f3978d -nosplash -http=wininet";

    constexpr std::string_view HeadlessArguments = " -nullrhi -nosound -unattended";

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
    Match.MinimumPlayersToStart = FConfiguration::MinimumPlayers;
    Match.StartingLoadout = SplitLoadout(FConfiguration::StartingLoadout);
}

void FServerSettings::LogResolvedConfiguration() const
{
    UE_LOG_DISPLAY("Config", "Target build " + GetActiveBuildProfile().Describe());
    UE_LOG_DISPLAY("Config", "Playlist " + Match.PlaylistPath);
    UE_LOG_DISPLAY("Config", "MapToLoad " + Match.MapName);
    UE_LOG_DISPLAY("Config", "Port " + std::to_string(GetPort()) + ", MaxTickRate " + std::to_string(GetMaxTickRate()));
    UE_LOG_DISPLAY("Config", "MaxPlayers " + std::to_string(Match.MaxPlayers) + ", MinimumPlayers " + std::to_string(Match.MinimumPlayersToStart));
    UE_LOG_DISPLAY("Config", "bSkipVersionCheck " + DescribeBool(FConfiguration::bSkipVersionCheck));
    UE_LOG_DISPLAY("Config", "bHeadless " + DescribeBool(FConfiguration::bHeadless));

    for (const std::string& Entry : Match.StartingLoadout)
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

bool FServerSettings::IsHeadless() const
{
    return FConfiguration::bHeadless;
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

std::wstring FServerSettings::GetBackendUrl() const
{
    return FStringConv::ToWide(FConfiguration::BackendUrl);
}

std::wstring FServerSettings::GetGameExecutableName() const
{
    const std::wstring Normalized = FPaths::NormalizeSeparators(std::wstring(GameExecutableRelativePath));

    const size_t Separator = Normalized.find_last_of(L'\\');
    return Separator == std::wstring::npos ? Normalized : Normalized.substr(Separator + 1);
}

std::string FServerSettings::BuildLaunchArguments() const
{
    return std::string(LaunchArguments) + (IsHeadless() ? std::string(HeadlessArguments) : std::string()) + " -port=" + std::to_string(GetPort());
}
