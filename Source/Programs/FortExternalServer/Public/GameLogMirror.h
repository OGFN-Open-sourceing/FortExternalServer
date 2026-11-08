#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

#include <atomic>
#include <string>
#include <thread>

class FGameLogMirror
{
public:
    ~FGameLogMirror();

    void Start(const std::wstring& InLogFilePath, bool bSkipExistingFile);

    void Stop();

    static std::wstring GetDefaultLogFilePath();

private:
    struct FFileIdentity
    {
        uint32 VolumeSerialNumber = 0;
        uint64 FileIndex = 0;

        bool operator==(const FFileIdentity& Other) const = default;
    };

    static bool ReadFileIdentity(const std::wstring& Path, FFileIdentity& OutIdentity);

    void Run();

    void ForwardLine(std::string Line) const;

    std::wstring LogFilePath;
    FFileIdentity StaleIdentity;
    bool bHasStaleIdentity = false;
    std::atomic<bool> bRunning = false;
    std::thread Worker;
};
