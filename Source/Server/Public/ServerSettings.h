#pragma once

#include "FortniteGame/Public/Athena/FortAthenaMatch.h"

class FServerSettings
{
public:
    void Resolve();

    void LogResolvedConfiguration() const;

    const FAthenaMatchSettings& GetMatch() const;

    uint16 GetPort() const;

    int32 GetMaxTickRate() const;

    bool ShouldAttachToRunningProcess() const;

    bool ShouldEnforceVersionMatch() const;

    bool AreConsoleCommandsEnabled() const;

    ELogVerbosity GetLogVerbosity() const;

    std::wstring GetLogFileRelativePath() const;

    std::wstring GetGameExecutablePath(const std::wstring& BuildRoot) const;

    std::wstring GetGameExecutableName() const;

    std::string BuildLaunchArguments() const;

private:
    FAthenaMatchSettings Match;
};
