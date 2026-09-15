#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FRemoteModuleInfo
{
    std::wstring Name;
    FRemoteAddress BaseAddress = InvalidRemoteAddress;
    uint64 ImageSize = 0;
    uint64 SlideOffset = 0;
};

struct FRemoteRegionInfo
{
    FRemoteAddress BaseAddress = InvalidRemoteAddress;
    uint64 RegionSize = 0;
    bool bCommitted = false;
    bool bFree = false;
    bool bExecutable = false;
};

enum class ERemoteProtection : uint8
{
    NoAccess,
    ReadOnly,
    ReadWrite,
    ExecuteRead,
    ExecuteReadWrite
};

class FProcessAttachment
{
public:
    FProcessAttachment() = default;

    ~FProcessAttachment();

    FProcessAttachment(const FProcessAttachment&) = delete;
    FProcessAttachment& operator=(const FProcessAttachment&) = delete;

    bool LaunchAndAttach(const std::wstring& ExecutablePath, const std::wstring& CommandLine, const std::wstring& WorkingDirectory);

    bool AttachToRunning(const std::wstring& ProcessImageName, uint32 TimeoutMilliseconds);

    void Detach();

    bool IsAttached() const;

    bool IsAlive() const;

    bool SuspendOtherThreads(FRemoteAddress GuardedRangeStart, size_t GuardedRangeSize, uint32 MaximumAttempts);

    void ResumeSuspendedThreads();

    void Terminate();

    uint32 GetProcessId() const;

    bool ReadMemory(FRemoteAddress Address, void* Destination, size_t Size) const;

    bool WriteMemory(FRemoteAddress Address, const void* Source, size_t Size) const;

    bool ProtectMemory(FRemoteAddress Address, size_t Size, ERemoteProtection Protection) const;

    FRemoteAddress AllocateMemory(FRemoteAddress PreferredAddress, size_t Size) const;

    bool FreeMemory(FRemoteAddress Address, size_t Size) const;

    bool QueryRegion(FRemoteAddress Address, FRemoteRegionInfo& OutInfo) const;

    void FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const;

    bool RefreshModules();

    const std::vector<FRemoteModuleInfo>& GetModules() const;

    const FRemoteModuleInfo* FindModule(std::wstring_view ModuleName) const;

    const FRemoteModuleInfo* GetPrimaryModule() const;

    static uint32 FindProcessIdByImageName(std::wstring_view ImageName);

    static bool PreparePrivileges();

    static std::string DescribePlatformRequirements();

private:
    struct FPlatformState;

    FPlatformState* Platform = nullptr;
    uint32 ProcessId = 0;
    std::wstring PrimaryModuleName;
    std::vector<FRemoteModuleInfo> Modules;
};
