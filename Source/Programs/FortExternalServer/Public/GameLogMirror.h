#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

#include <algorithm>
#include <atomic>
#include <string_view>
#include <string>
#include <thread>

class FGameLogMirror
{
public:
    ~FGameLogMirror();

    void Start(const std::wstring& InLogFilePath, bool bSkipExistingFile);

    void Stop();

    static constexpr std::wstring_view GameLogFileName = L"FortExternalServerGame.log";

    static std::wstring GetDefaultLogFilePath();

private:
    struct FFileState
    {
        uint32 VolumeSerialNumber = 0;
        uint64 FileIndex = 0;
        uint64 Size = 0;
        std::string Header;

        bool IsSameLog(const FFileState& Other) const
        {
            const size_t Length = std::min(Header.size(), Other.Header.size());
            return VolumeSerialNumber == Other.VolumeSerialNumber && FileIndex == Other.FileIndex && Header.compare(0, Length, Other.Header, 0, Length) == 0;
        }
    };

    static bool ReadFileState(const std::wstring& Path, FFileState& OutState);

    void Run();

    void ForwardLine(std::string Line) const;

    std::wstring LogFilePath;
    FFileState SkippedState;
    bool bHasSkippedState = false;
    std::atomic<bool> bRunning = false;
    std::thread Worker;
};
