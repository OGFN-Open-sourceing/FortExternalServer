#include "Programs/FortExternalServer/Public/GameLogMirror.h"
#include "Runtime/Core/Public/HAL/WindowsPlatform.h"

namespace
{
    constexpr uint32 PollIntervalMilliseconds = 50;
    constexpr size_t ReadChunkBytes = 64 * 1024;
    constexpr std::string_view Utf8ByteOrderMark = "\xEF\xBB\xBF";

    HANDLE OpenSharedForRead(const std::wstring& Path)
    {
        return ::CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    ELogVerbosity ClassifyLine(std::string_view Line)
    {
        if (Line.find("Error:") != std::string_view::npos || Line.find("Fatal error") != std::string_view::npos)
        {
            return ELogVerbosity::Error;
        }

        if (Line.find("Warning:") != std::string_view::npos)
        {
            return ELogVerbosity::Warning;
        }

        return ELogVerbosity::Display;
    }

    std::string_view StripEngineTimestamp(std::string_view Line)
    {
        for (int32 Prefix = 0; Prefix < 2 && !Line.empty() && Line.front() == '['; ++Prefix)
        {
            const size_t Close = Line.find(']');
            if (Close == std::string_view::npos)
            {
                break;
            }

            Line.remove_prefix(Close + 1);
        }

        return Line;
    }
}

FGameLogMirror::~FGameLogMirror()
{
    Stop();
}

std::wstring FGameLogMirror::GetDefaultLogFilePath()
{
    wchar_t LocalAppData[MAX_PATH] = {};
    const DWORD Length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", LocalAppData, MAX_PATH);
    if (Length == 0 || Length >= MAX_PATH)
    {
        return std::wstring();
    }

    return std::wstring(LocalAppData) + L"/FortniteGame/Saved/Logs/FortniteGame.log";
}

bool FGameLogMirror::ReadFileIdentity(const std::wstring& Path, FFileIdentity& OutIdentity)
{
    const HANDLE File = OpenSharedForRead(Path);
    if (File == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    BY_HANDLE_FILE_INFORMATION Information = {};
    const bool bRead = ::GetFileInformationByHandle(File, &Information) != FALSE;
    ::CloseHandle(File);

    if (!bRead)
    {
        return false;
    }

    OutIdentity.VolumeSerialNumber = Information.dwVolumeSerialNumber;
    OutIdentity.FileIndex = (static_cast<uint64>(Information.nFileIndexHigh) << 32) | Information.nFileIndexLow;
    return true;
}

void FGameLogMirror::Start(const std::wstring& InLogFilePath, bool bSkipExistingFile)
{
    Stop();

    if (InLogFilePath.empty())
    {
        return;
    }

    LogFilePath = InLogFilePath;
    bHasStaleIdentity = bSkipExistingFile && ReadFileIdentity(LogFilePath, StaleIdentity);
    bRunning = true;
    Worker = std::thread(&FGameLogMirror::Run, this);
}

void FGameLogMirror::Stop()
{
    bRunning = false;

    if (Worker.joinable())
    {
        Worker.join();
    }
}

void FGameLogMirror::Run()
{
    FFileIdentity CurrentIdentity;
    bool bHasCurrent = false;
    uint64 Offset = 0;
    std::string Pending;
    std::vector<char> Buffer(ReadChunkBytes);

    while (bRunning)
    {
        FFileIdentity Identity;
        if (!ReadFileIdentity(LogFilePath, Identity) || (bHasStaleIdentity && Identity == StaleIdentity))
        {
            ::Sleep(PollIntervalMilliseconds);
            continue;
        }

        if (!bHasCurrent || !(Identity == CurrentIdentity))
        {
            CurrentIdentity = Identity;
            bHasCurrent = true;
            Offset = 0;
            Pending.clear();
        }

        const HANDLE File = OpenSharedForRead(LogFilePath);
        if (File == INVALID_HANDLE_VALUE)
        {
            ::Sleep(PollIntervalMilliseconds);
            continue;
        }

        LARGE_INTEGER Position = {};
        Position.QuadPart = static_cast<LONGLONG>(Offset);
        ::SetFilePointerEx(File, Position, nullptr, FILE_BEGIN);

        DWORD BytesRead = 0;
        while (bRunning && ::ReadFile(File, Buffer.data(), static_cast<DWORD>(Buffer.size()), &BytesRead, nullptr) && BytesRead > 0)
        {
            Offset += BytesRead;
            Pending.append(Buffer.data(), BytesRead);

            size_t LineStart = 0;
            for (size_t NewLine = Pending.find('\n'); NewLine != std::string::npos; NewLine = Pending.find('\n', LineStart))
            {
                ForwardLine(Pending.substr(LineStart, NewLine - LineStart));
                LineStart = NewLine + 1;
            }

            Pending.erase(0, LineStart);
        }

        ::CloseHandle(File);
        ::Sleep(PollIntervalMilliseconds);
    }
}

void FGameLogMirror::ForwardLine(std::string Line) const
{
    if (Line.starts_with(Utf8ByteOrderMark))
    {
        Line.erase(0, Utf8ByteOrderMark.size());
    }

    while (!Line.empty() && (Line.back() == '\r' || Line.back() == '\n'))
    {
        Line.pop_back();
    }

    if (Line.empty())
    {
        return;
    }

    FServerLog::Write(ClassifyLine(Line), "Game", StripEngineTimestamp(Line));
}
