#include "Runtime/Core/Public/HAL/WindowsPlatform.h"

namespace FWindowsPlatform
{
    std::wstring GetLastErrorText(unsigned long ErrorCode)
    {
        if (ErrorCode == 0)
        {
            return L"No error";
        }

        LPWSTR Buffer = nullptr;
        const DWORD Length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, ErrorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&Buffer), 0, nullptr);

        if (Length == 0 || Buffer == nullptr)
        {
            return L"Unknown error";
        }

        std::wstring Result(Buffer, Length);
        ::LocalFree(Buffer);

        while (!Result.empty() && (Result.back() == L'\r' || Result.back() == L'\n'))
        {
            Result.pop_back();
        }

        return Result;
    }

    bool EnableDebugPrivilege()
    {
        HANDLE TokenHandle = nullptr;
        if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle) == FALSE)
        {
            return false;
        }

        LUID PrivilegeId = {};
        if (::LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &PrivilegeId) == FALSE)
        {
            ::CloseHandle(TokenHandle);
            return false;
        }

        TOKEN_PRIVILEGES Privileges = {};
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Luid = PrivilegeId;
        Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        const BOOL bAdjusted = ::AdjustTokenPrivileges(TokenHandle, FALSE, &Privileges, sizeof(Privileges), nullptr, nullptr);
        const DWORD ErrorCode = ::GetLastError();

        ::CloseHandle(TokenHandle);

        return bAdjusted != FALSE && ErrorCode == ERROR_SUCCESS;
    }

    void SetConsoleTitleText(const std::wstring& Title)
    {
        ::SetConsoleTitleW(Title.c_str());
    }

    void EnableVirtualTerminalProcessing()
    {
        const HANDLE OutputHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (OutputHandle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        DWORD Mode = 0;
        if (::GetConsoleMode(OutputHandle, &Mode) == FALSE)
        {
            return;
        }

        ::SetConsoleMode(OutputHandle, Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    uint64 GetTimeMilliseconds()
    {
        return static_cast<uint64>(::GetTickCount64());
    }

    void SleepMilliseconds(uint32 Milliseconds)
    {
        ::Sleep(static_cast<DWORD>(Milliseconds));
    }

    void YieldThread()
    {
        ::SwitchToThread();
    }
}
