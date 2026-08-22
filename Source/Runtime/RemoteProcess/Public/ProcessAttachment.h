#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FRemoteModuleInfo
{
    std::wstring Name;
    FRemoteAddress BaseAddress = InvalidRemoteAddress;
    uint32 ImageSize = 0;
};

class FProcessAttachment
{
public:
    FProcessAttachment() = default;

    ~FProcessAttachment();

    FProcessAttachment(const FProcessAttachment&) = delete;
    FProcessAttachment& operator=(const FProcessAttachment&) = delete;

    bool LaunchAndAttach(const std::wstring& ExecutablePath, const std::wstring& CommandLine, const std::wstring& WorkingDirectory, bool bStartSuspended);

    bool AttachToRunning(const std::wstring& ProcessImageName, uint32 TimeoutMilliseconds);

    void Detach();

    bool IsAttached() const;

    bool IsAlive() const;

    bool ResumeMainThread();

    void Terminate();

    uint32 GetProcessId() const;

    void* GetProcessHandle() const;

    bool RefreshModules();

    const std::vector<FRemoteModuleInfo>& GetModules() const;

    const FRemoteModuleInfo* FindModule(std::wstring_view ModuleName) const;

    const FRemoteModuleInfo* GetPrimaryModule() const;

    static uint32 FindProcessIdByImageName(std::wstring_view ImageName);

private:
    void* ProcessHandle = nullptr;
    void* MainThreadHandle = nullptr;
    uint32 ProcessId = 0;
    std::wstring PrimaryModuleName;
    std::vector<FRemoteModuleInfo> Modules;
};
