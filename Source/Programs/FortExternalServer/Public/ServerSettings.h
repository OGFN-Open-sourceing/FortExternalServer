#pragma once

#include "FortniteGame/Public/Athena/AthenaMatchSettings.h"

class FServerSettings
{
public:
    void Resolve();

    void LogResolvedConfiguration() const;

    const FAthenaMatchSettings& GetMatch() const;

    uint16 GetPort() const;

    int32 GetMaxTickRate() const;

    int32 GetMaxClientRate() const;

    bool ShouldAttachToRunningProcess() const;

    bool IsHeadless() const;

    bool ShouldTrackProcessId() const;

    bool ShouldEnforceVersionMatch() const;

    bool AreConsoleCommandsEnabled() const;

    ELogVerbosity GetLogVerbosity() const;

    std::wstring GetLogFileRelativePath() const;

    std::wstring GetGameExecutablePath(const std::wstring& BuildRoot) const;

    std::wstring GetGameExecutableName() const;

    std::wstring GetBackendUrl() const;

    std::string BuildLaunchArguments() const;

private:
    FAthenaMatchSettings Match;
};
