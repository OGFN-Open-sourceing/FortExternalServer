#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <objbase.h>

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>

namespace FWindowsPlatform
{
    std::wstring GetLastErrorText(unsigned long ErrorCode);

    bool EnableDebugPrivilege();

    void SetConsoleTitleText(const std::wstring& Title);

    void EnableVirtualTerminalProcessing();

    uint64 GetTimeMilliseconds();

    void SleepMilliseconds(uint32 Milliseconds);

    void YieldThread();
}
