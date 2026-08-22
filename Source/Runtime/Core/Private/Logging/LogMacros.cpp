#include "Runtime/Core/Public/Logging/LogMacros.h"
#include "Runtime/Core/Public/Containers/StringConv.h"
#include "Runtime/Core/Public/HAL/PlatformDetection.h"
#include "Runtime/Core/Public/HAL/PlatformMisc.h"
#include "Runtime/Core/Public/Misc/Paths.h"

#include <cstdio>
#include <ctime>
#include <mutex>

namespace
{
    std::mutex GLogMutex;
    FILE* GLogFile = nullptr;
    ELogVerbosity GLogVerbosity = ELogVerbosity::Display;

    const char* VerbosityToLabel(ELogVerbosity Verbosity)
    {
        switch (Verbosity)
        {
        case ELogVerbosity::Fatal:
            return "Fatal";
        case ELogVerbosity::Error:
            return "Error";
        case ELogVerbosity::Warning:
            return "Warning";
        case ELogVerbosity::Verbose:
            return "Verbose";
        default:
            return "Display";
        }
    }

    const char* VerbosityToColor(ELogVerbosity Verbosity)
    {
        switch (Verbosity)
        {
        case ELogVerbosity::Fatal:
            return "\x1b[97;41m";
        case ELogVerbosity::Error:
            return "\x1b[91m";
        case ELogVerbosity::Warning:
            return "\x1b[93m";
        case ELogVerbosity::Verbose:
            return "\x1b[90m";
        default:
            return "\x1b[97m";
        }
    }

    std::string BuildTimestamp()
    {
        SYSTEMTIME LocalTime = {};
        ::GetLocalTime(&LocalTime);

        char Buffer[32] = {};
        std::snprintf(Buffer, sizeof(Buffer), "%02u:%02u:%02u.%03u", LocalTime.wHour, LocalTime.wMinute, LocalTime.wSecond, LocalTime.wMilliseconds);
        return std::string(Buffer);
    }
}

void FServerLog::Initialize(const std::wstring& LogFilePath, ELogVerbosity Verbosity)
{
    std::lock_guard<std::mutex> Lock(GLogMutex);

    GLogVerbosity = Verbosity;

    FPlatformMisc::EnableVirtualTerminalProcessing();

    if (LogFilePath.empty())
    {
        return;
    }

    FPaths::CreateDirectoryTree(FPaths::GetParentDirectory(LogFilePath));

    GLogFile = ::_wfopen(LogFilePath.c_str(), L"w, ccs=UTF-8");
}

void FServerLog::Shutdown()
{
    std::lock_guard<std::mutex> Lock(GLogMutex);

    if (GLogFile != nullptr)
    {
        std::fclose(GLogFile);
        GLogFile = nullptr;
    }
}

void FServerLog::Write(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message)
{
    if (Verbosity > GLogVerbosity)
    {
        return;
    }

    std::lock_guard<std::mutex> Lock(GLogMutex);

    const std::string Timestamp = BuildTimestamp();

    std::printf("%s[%s][%.*s][%s] %.*s\x1b[0m\n", VerbosityToColor(Verbosity), Timestamp.c_str(), static_cast<int>(Category.size()), Category.data(),
        VerbosityToLabel(Verbosity), static_cast<int>(Message.size()), Message.data());
    std::fflush(stdout);

    if (GLogFile != nullptr)
    {
        const std::wstring WideLine = FStringConv::ToWide(std::string("[") + Timestamp + "][" + std::string(Category) + "][" + VerbosityToLabel(Verbosity) + "] " +
            std::string(Message) + "\n");
        std::fputws(WideLine.c_str(), GLogFile);
        std::fflush(GLogFile);
    }
}

void FServerLog::SetVerbosity(ELogVerbosity Verbosity)
{
    std::lock_guard<std::mutex> Lock(GLogMutex);
    GLogVerbosity = Verbosity;
}

ELogVerbosity FServerLog::GetVerbosity()
{
    return GLogVerbosity;
}

ELogVerbosity FServerLog::ParseVerbosity(std::string_view Value, ELogVerbosity DefaultValue)
{
    if (FStringConv::EqualsIgnoreCase(Value, "Fatal"))
    {
        return ELogVerbosity::Fatal;
    }

    if (FStringConv::EqualsIgnoreCase(Value, "Error"))
    {
        return ELogVerbosity::Error;
    }

    if (FStringConv::EqualsIgnoreCase(Value, "Warning"))
    {
        return ELogVerbosity::Warning;
    }

    if (FStringConv::EqualsIgnoreCase(Value, "Display"))
    {
        return ELogVerbosity::Display;
    }

    if (FStringConv::EqualsIgnoreCase(Value, "Verbose"))
    {
        return ELogVerbosity::Verbose;
    }

    return DefaultValue;
}
