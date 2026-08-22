#pragma once

#include "FortniteGame/Public/Athena/FortAthenaMatch.h"
#include "Runtime/Core/Public/Misc/ConfigCacheIni.h"

struct FProcessSettings
{
    std::wstring GameExecutableRelativePath;
    std::wstring ExtraArguments;
    uint32 AttachTimeoutSeconds = 180;
    bool bAttachToRunningProcess = false;
};

struct FEngineExpectations
{
    double FortniteVersion = 0.0;
    double EngineVersion = 0.0;
    int32 Changelist = 0;
    bool bEnforceVersionMatch = true;
};

struct FSessionSettings
{
    uint16 Port = 7777;
};

struct FRuntimeSettings
{
    uint64 FrameTickIntervalMilliseconds = 100;
    int32 MaxTickRate = 30;
    bool bEnableConsoleCommands = true;
    bool bDumpObjectsOnStart = false;
    bool bLogUnresolvedSignatures = true;
};

struct FLoggingSettings
{
    ELogVerbosity Verbosity = ELogVerbosity::Display;
    std::wstring LogFileRelativePath;
};

class FServerSettings
{
public:
    bool Load(const std::wstring& BuildRoot);

    void LogResolvedConfiguration() const;

    const FProcessSettings& GetProcess() const;

    const FEngineExpectations& GetEngineExpectations() const;

    const FSessionSettings& GetSession() const;

    const FRuntimeSettings& GetRuntime() const;

    const FLoggingSettings& GetLogging() const;

    const FAthenaMatchSettings& GetMatch() const;

    std::wstring GetGameExecutablePath(const std::wstring& BuildRoot) const;

    std::wstring GetGameExecutableName() const;

    std::string BuildLaunchArguments() const;

private:
    void ApplyCompiledDefaults();

    void ApplyConfigFile(const FConfigFile& ConfigFile);

    void ApplyCommandLineOverrides();

    FProcessSettings Process;
    FEngineExpectations EngineExpectations;
    FSessionSettings Session;
    FRuntimeSettings Runtime;
    FLoggingSettings Logging;
    FAthenaMatchSettings Match;
    bool bLoadedConfigFile = false;
};
