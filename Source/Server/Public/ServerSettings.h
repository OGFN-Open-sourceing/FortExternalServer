#pragma once

#include "FortniteGame/Public/Athena/FortAthenaMatch.h"
#include "Runtime/Core/Public/Misc/ConfigCacheIni.h"

struct FProcessSettings
{
    std::wstring GameExecutableRelativePath = L"FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe";
    std::wstring ExtraArguments;
    uint32 AttachTimeoutSeconds = 180;
    bool bAttachToRunningProcess = false;
};

struct FEngineExpectations
{
    double FortniteVersion = 3.6;
    double EngineVersion = 4.19;
    int32 Changelist = 4019403;
    bool bEnforceVersionMatch = true;
};

struct FSessionSettings
{
    uint16 Port = 7777;
};

struct FLoggingSettings
{
    ELogVerbosity Verbosity = ELogVerbosity::Display;
    std::wstring LogFileRelativePath = L"Saved/Logs/FortExternalServer.log";
};

class FServerSettings
{
public:
    bool Load(const std::wstring& BuildRoot);

    const FProcessSettings& GetProcess() const;

    const FEngineExpectations& GetEngineExpectations() const;

    const FSessionSettings& GetSession() const;

    const FLoggingSettings& GetLogging() const;

    const FAthenaMatchSettings& GetMatch() const;

    std::wstring GetGameExecutablePath(const std::wstring& BuildRoot) const;

    std::wstring GetGameExecutableName() const;

    std::string BuildLaunchArguments() const;

private:
    void ApplyCommandLineOverrides();

    FProcessSettings Process;
    FEngineExpectations EngineExpectations;
    FSessionSettings Session;
    FLoggingSettings Logging;
    FAthenaMatchSettings Match;
};
