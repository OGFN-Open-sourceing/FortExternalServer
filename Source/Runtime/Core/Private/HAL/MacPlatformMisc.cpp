#include "Runtime/Core/Public/HAL/PlatformDetection.h"

#if FORT_PLATFORM_MAC

#include "Runtime/Core/Public/Containers/StringConv.h"
#include "Runtime/Core/Public/HAL/PlatformMisc.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mach-o/dyld.h>
#include <sched.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace
{
    struct FTerminalRawMode
    {
        FTerminalRawMode()
        {
            if (::tcgetattr(STDIN_FILENO, &OriginalState) != 0)
            {
                return;
            }

            bValid = true;

            termios RawState = OriginalState;
            RawState.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
            RawState.c_cc[VMIN] = 0;
            RawState.c_cc[VTIME] = 0;

            ::tcsetattr(STDIN_FILENO, TCSANOW, &RawState);
        }

        ~FTerminalRawMode()
        {
            if (bValid)
            {
                ::tcsetattr(STDIN_FILENO, TCSANOW, &OriginalState);
            }
        }

        termios OriginalState = {};
        bool bValid = false;
    };

    FTerminalRawMode& GetTerminalRawMode()
    {
        static FTerminalRawMode RawMode;
        return RawMode;
    }
}

namespace FPlatformMisc
{
    std::wstring GetErrorText(uint32 ErrorCode)
    {
        if (ErrorCode == 0)
        {
            return L"No error";
        }

        char Buffer[256] = {};
        if (::strerror_r(static_cast<int>(ErrorCode), Buffer, sizeof(Buffer)) != 0)
        {
            return L"Unknown error";
        }

        return FStringConv::ToWide(Buffer);
    }

    std::wstring GetLastErrorText()
    {
        return GetErrorText(static_cast<uint32>(errno));
    }

    void SetConsoleTitleText(const std::wstring& Title)
    {
        std::printf("\x1b]0;%s\x07", FStringConv::ToNarrow(Title).c_str());
        std::fflush(stdout);
    }

    void EnableVirtualTerminalProcessing()
    {
        GetTerminalRawMode();
    }

    uint64 GetTimeMilliseconds()
    {
        timespec Now = {};
        if (::clock_gettime(CLOCK_MONOTONIC, &Now) != 0)
        {
            return 0;
        }

        return static_cast<uint64>(Now.tv_sec) * 1000ull + static_cast<uint64>(Now.tv_nsec) / 1000000ull;
    }

    void SleepMilliseconds(uint32 Milliseconds)
    {
        timespec Request = {};
        Request.tv_sec = static_cast<time_t>(Milliseconds / 1000);
        Request.tv_nsec = static_cast<long>((Milliseconds % 1000) * 1000000);

        ::nanosleep(&Request, nullptr);
    }

    void YieldThread()
    {
        ::sched_yield();
    }

    bool IsKeyPressed()
    {
        GetTerminalRawMode();

        fd_set ReadSet;
        FD_ZERO(&ReadSet);
        FD_SET(STDIN_FILENO, &ReadSet);

        timeval Timeout = {};

        return ::select(STDIN_FILENO + 1, &ReadSet, nullptr, nullptr, &Timeout) > 0;
    }

    int ReadKey()
    {
        GetTerminalRawMode();

        char Character = 0;
        if (::read(STDIN_FILENO, &Character, 1) != 1)
        {
            return 0;
        }

        return static_cast<int>(Character);
    }

    std::wstring GetExecutablePath()
    {
        char Buffer[4096] = {};
        uint32_t Size = sizeof(Buffer);

        if (::_NSGetExecutablePath(Buffer, &Size) != 0)
        {
            return std::wstring();
        }

        char ResolvedBuffer[4096] = {};
        if (::realpath(Buffer, ResolvedBuffer) == nullptr)
        {
            return FStringConv::ToWide(Buffer);
        }

        return FStringConv::ToWide(ResolvedBuffer);
    }

    const char* GetPlatformName()
    {
        return "macOS";
    }
}

#endif
