#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>
#include <string_view>

enum class ELogVerbosity : uint8
{
    Fatal = 0,
    Error,
    Warning,
    Display,
    Verbose
};

class FServerLog
{
public:
    static void Initialize(const std::wstring& LogFilePath, ELogVerbosity Verbosity);

    static void Shutdown();

    static void Write(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message);

    static void SetVerbosity(ELogVerbosity Verbosity);

    static ELogVerbosity GetVerbosity();

    static ELogVerbosity ParseVerbosity(std::string_view Value, ELogVerbosity DefaultValue);
};

#define UE_LOG_DISPLAY(Category, Message) FServerLog::Write(ELogVerbosity::Display, Category, Message)
#define UE_LOG_VERBOSE(Category, Message) FServerLog::Write(ELogVerbosity::Verbose, Category, Message)
#define UE_LOG_WARNING(Category, Message) FServerLog::Write(ELogVerbosity::Warning, Category, Message)
#define UE_LOG_ERROR(Category, Message) FServerLog::Write(ELogVerbosity::Error, Category, Message)
#define UE_LOG_FATAL(Category, Message) FServerLog::Write(ELogVerbosity::Fatal, Category, Message)
