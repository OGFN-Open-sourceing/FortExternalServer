#pragma once

#include "Runtime/Core/Public/CoreTypes.h"
#include "Runtime/Core/Public/HAL/PlatformDetection.h"

#include <string>

namespace FPlatformMisc
{
    std::wstring GetErrorText(uint32 ErrorCode);

    std::wstring GetLastErrorText();

    void SetConsoleTitleText(const std::wstring& Title);

    void EnableVirtualTerminalProcessing();

    uint64 GetTimeMilliseconds();

    void SleepMilliseconds(uint32 Milliseconds);

    void YieldThread();

    bool IsKeyPressed();

    int ReadKey();

    std::wstring GetExecutablePath();

    const char* GetPlatformName();
}
