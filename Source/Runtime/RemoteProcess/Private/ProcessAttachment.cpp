#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <algorithm>

FProcessAttachment::~FProcessAttachment()
{
    Detach();
}

bool FProcessAttachment::LaunchAndAttach(const std::wstring& ExecutablePath, const std::wstring& CommandLine, const std::wstring& WorkingDirectory, bool bStartSuspended)
{
    Detach();

    std::wstring MutableCommandLine;
    MutableCommandLine.reserve(ExecutablePath.size() + CommandLine.size() + 4);
    MutableCommandLine.push_back(L'"');
    MutableCommandLine.append(ExecutablePath);
    MutableCommandLine.push_back(L'"');

    if (!CommandLine.empty())
    {
        MutableCommandLine.push_back(L' ');
        MutableCommandLine.append(CommandLine);
    }

    MutableCommandLine.push_back(L'\0');

    STARTUPINFOW StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);

    PROCESS_INFORMATION ProcessInformation = {};

    const DWORD CreationFlags = bStartSuspended ? CREATE_SUSPENDED : 0u;

    const BOOL bCreated = ::CreateProcessW(ExecutablePath.c_str(), MutableCommandLine.data(), nullptr, nullptr, FALSE, CreationFlags, nullptr,
        WorkingDirectory.empty() ? nullptr : WorkingDirectory.c_str(), &StartupInfo, &ProcessInformation);

    if (bCreated == FALSE)
    {
        UE_LOG_ERROR("RemoteProcess", "CreateProcess failed: " + FStringConv::ToNarrow(FWindowsPlatform::GetLastErrorText(::GetLastError())));
        return false;
    }

    ProcessHandle = ProcessInformation.hProcess;
    MainThreadHandle = ProcessInformation.hThread;
    ProcessId = ProcessInformation.dwProcessId;
    MainThreadId = ProcessInformation.dwThreadId;

    const size_t Separator = ExecutablePath.find_last_of(L'\\');
    PrimaryModuleName = Separator == std::wstring::npos ? ExecutablePath : ExecutablePath.substr(Separator + 1);

    UE_LOG_DISPLAY("RemoteProcess", "Launched " + FStringConv::ToNarrow(PrimaryModuleName) + " with process id " + std::to_string(ProcessId));
    return true;
}

bool FProcessAttachment::AttachToRunning(const std::wstring& ProcessImageName, uint32 TimeoutMilliseconds)
{
    Detach();

    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    uint32 FoundProcessId = 0;
    while (FWindowsPlatform::GetTimeMilliseconds() <= Deadline)
    {
        FoundProcessId = FindProcessIdByImageName(ProcessImageName);
        if (FoundProcessId != 0)
        {
            break;
        }

        FWindowsPlatform::SleepMilliseconds(250);
    }

    if (FoundProcessId == 0)
    {
        UE_LOG_ERROR("RemoteProcess", "Timed out waiting for " + FStringConv::ToNarrow(ProcessImageName));
        return false;
    }

    constexpr DWORD DesiredAccess = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | SYNCHRONIZE;

    ProcessHandle = ::OpenProcess(DesiredAccess, FALSE, FoundProcessId);
    if (ProcessHandle == nullptr)
    {
        UE_LOG_ERROR("RemoteProcess", "OpenProcess failed: " + FStringConv::ToNarrow(FWindowsPlatform::GetLastErrorText(::GetLastError())));
        return false;
    }

    ProcessId = FoundProcessId;
    MainThreadId = FindOldestThreadId(FoundProcessId);
    PrimaryModuleName = ProcessImageName;

    UE_LOG_DISPLAY("RemoteProcess", "Attached to " + FStringConv::ToNarrow(ProcessImageName) + " with process id " + std::to_string(ProcessId));
    return true;
}

void FProcessAttachment::Detach()
{
    ResumeSuspendedThreads();

    if (MainThreadHandle != nullptr)
    {
        ::CloseHandle(MainThreadHandle);
        MainThreadHandle = nullptr;
    }

    if (ProcessHandle != nullptr)
    {
        ::CloseHandle(ProcessHandle);
        ProcessHandle = nullptr;
    }

    ProcessId = 0;
    MainThreadId = 0;
    Modules.clear();
}

bool FProcessAttachment::IsAttached() const
{
    return ProcessHandle != nullptr;
}

bool FProcessAttachment::IsAlive() const
{
    if (ProcessHandle == nullptr)
    {
        return false;
    }

    DWORD ExitCode = 0;
    if (::GetExitCodeProcess(ProcessHandle, &ExitCode) == FALSE)
    {
        return false;
    }

    return ExitCode == STILL_ACTIVE;
}

bool FProcessAttachment::ResumeMainThread()
{
    if (MainThreadHandle == nullptr)
    {
        return false;
    }

    return ::ResumeThread(MainThreadHandle) != static_cast<DWORD>(-1);
}

bool FProcessAttachment::LoadLibraryInto(const std::wstring& LibraryPath, uint32 TimeoutMilliseconds)
{
    if (ProcessHandle == nullptr)
    {
        return false;
    }

    const HMODULE Kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    const FARPROC LoadLibraryAddress = Kernel32 != nullptr ? ::GetProcAddress(Kernel32, "LoadLibraryW") : nullptr;
    if (LoadLibraryAddress == nullptr)
    {
        return false;
    }

    const size_t PathBytes = (LibraryPath.size() + 1) * sizeof(wchar_t);
    void* const RemotePath = ::VirtualAllocEx(ProcessHandle, nullptr, PathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (RemotePath == nullptr)
    {
        return false;
    }

    bool bLoaded = false;
    if (::WriteProcessMemory(ProcessHandle, RemotePath, LibraryPath.c_str(), PathBytes, nullptr) != FALSE)
    {
        const HANDLE Thread = ::CreateRemoteThread(ProcessHandle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(LoadLibraryAddress)), RemotePath, 0, nullptr);
        if (Thread != nullptr)
        {
            DWORD ExitCode = 0;
            bLoaded = ::WaitForSingleObject(Thread, TimeoutMilliseconds) == WAIT_OBJECT_0 && ::GetExitCodeThread(Thread, &ExitCode) != FALSE && ExitCode != 0;
            ::CloseHandle(Thread);
        }
    }

    ::VirtualFreeEx(ProcessHandle, RemotePath, 0, MEM_RELEASE);
    return bLoaded;
}

void FProcessAttachment::Terminate()
{
    if (ProcessHandle != nullptr)
    {
        ::TerminateProcess(ProcessHandle, 0);
    }
}

uint32 FProcessAttachment::GetProcessId() const
{
    return ProcessId;
}

uint32 FProcessAttachment::GetMainThreadId() const
{
    return MainThreadId;
}

uint32 FProcessAttachment::FindOldestThreadId(uint32 OwnerProcessId)
{
    const HANDLE Snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (Snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    THREADENTRY32 Entry = {};
    Entry.dwSize = sizeof(Entry);

    uint32 OldestThreadId = 0;
    uint64 OldestCreationTime = ~0ull;

    for (BOOL bHasEntry = ::Thread32First(Snapshot, &Entry); bHasEntry != FALSE; bHasEntry = ::Thread32Next(Snapshot, &Entry))
    {
        if (Entry.th32OwnerProcessID != OwnerProcessId)
        {
            continue;
        }

        const HANDLE Thread = ::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, Entry.th32ThreadID);
        if (Thread == nullptr)
        {
            continue;
        }

        FILETIME CreationTime = {};
        FILETIME ExitTime = {};
        FILETIME KernelTime = {};
        FILETIME UserTime = {};

        if (::GetThreadTimes(Thread, &CreationTime, &ExitTime, &KernelTime, &UserTime) != FALSE)
        {
            const uint64 Created = (static_cast<uint64>(CreationTime.dwHighDateTime) << 32) | CreationTime.dwLowDateTime;
            if (Created < OldestCreationTime)
            {
                OldestCreationTime = Created;
                OldestThreadId = Entry.th32ThreadID;
            }
        }

        ::CloseHandle(Thread);
    }

    ::CloseHandle(Snapshot);
    return OldestThreadId;
}

void* FProcessAttachment::GetProcessHandle() const
{
    return ProcessHandle;
}

bool FProcessAttachment::RefreshModules()
{
    Modules.clear();

    if (ProcessHandle == nullptr)
    {
        return false;
    }

    std::vector<HMODULE> ModuleHandles(1024);
    DWORD BytesNeeded = 0;

    if (::EnumProcessModulesEx(ProcessHandle, ModuleHandles.data(), static_cast<DWORD>(ModuleHandles.size() * sizeof(HMODULE)), &BytesNeeded, LIST_MODULES_ALL) == FALSE)
    {
        return false;
    }

    const size_t ModuleCount = BytesNeeded / sizeof(HMODULE);
    if (ModuleCount > ModuleHandles.size())
    {
        ModuleHandles.resize(ModuleCount);
        if (::EnumProcessModulesEx(ProcessHandle, ModuleHandles.data(), static_cast<DWORD>(ModuleHandles.size() * sizeof(HMODULE)), &BytesNeeded, LIST_MODULES_ALL) == FALSE)
        {
            return false;
        }
    }

    Modules.reserve(ModuleCount);

    for (size_t Index = 0; Index < ModuleCount; ++Index)
    {
        MODULEINFO ModuleInformation = {};
        if (::GetModuleInformation(ProcessHandle, ModuleHandles[Index], &ModuleInformation, sizeof(ModuleInformation)) == FALSE)
        {
            continue;
        }

        wchar_t NameBuffer[MAX_PATH] = {};
        if (::GetModuleBaseNameW(ProcessHandle, ModuleHandles[Index], NameBuffer, MAX_PATH) == 0)
        {
            continue;
        }

        FRemoteModuleInfo Entry;
        Entry.Name = NameBuffer;
        Entry.BaseAddress = reinterpret_cast<FRemoteAddress>(ModuleInformation.lpBaseOfDll);
        Entry.ImageSize = ModuleInformation.SizeOfImage;

        Modules.push_back(std::move(Entry));
    }

    return !Modules.empty();
}

const std::vector<FRemoteModuleInfo>& FProcessAttachment::GetModules() const
{
    return Modules;
}

const FRemoteModuleInfo* FProcessAttachment::FindModule(std::wstring_view ModuleName) const
{
    const auto Found = std::find_if(Modules.begin(), Modules.end(), [ModuleName](const FRemoteModuleInfo& Entry) {
        return _wcsicmp(Entry.Name.c_str(), std::wstring(ModuleName).c_str()) == 0;
    });

    return Found != Modules.end() ? &(*Found) : nullptr;
}

const FRemoteModuleInfo* FProcessAttachment::GetPrimaryModule() const
{
    return FindModule(PrimaryModuleName);
}

bool FProcessAttachment::SuspendOtherThreads(FRemoteAddress GuardedRangeStart, size_t GuardedRangeSize, uint32 MaximumAttempts)
{
    if (!IsAttached())
    {
        return false;
    }

    ResumeSuspendedThreads();

    for (uint32 Attempt = 0; Attempt < MaximumAttempts; ++Attempt)
    {
        const HANDLE Snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (Snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        THREADENTRY32 Entry = {};
        Entry.dwSize = sizeof(Entry);

        if (::Thread32First(Snapshot, &Entry) != FALSE)
        {
            do
            {
                if (Entry.dwSize < FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(Entry.th32OwnerProcessID))
                {
                    continue;
                }

                if (Entry.th32OwnerProcessID != ProcessId)
                {
                    continue;
                }

                const HANDLE Thread = ::OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, Entry.th32ThreadID);
                if (Thread == nullptr)
                {
                    continue;
                }

                if (::SuspendThread(Thread) == static_cast<DWORD>(-1))
                {
                    ::CloseHandle(Thread);
                    continue;
                }

                SuspendedThreads.push_back(Thread);
            }
            while (::Thread32Next(Snapshot, &Entry) != FALSE);
        }

        ::CloseHandle(Snapshot);

        if (SuspendedThreads.empty())
        {
            return false;
        }

        bool bThreadInsideRange = false;

        for (void* const ThreadHandle : SuspendedThreads)
        {
            CONTEXT ThreadContext = {};
            ThreadContext.ContextFlags = CONTEXT_CONTROL;

            if (::GetThreadContext(static_cast<HANDLE>(ThreadHandle), &ThreadContext) == FALSE)
            {
                continue;
            }

            const FRemoteAddress InstructionPointer = static_cast<FRemoteAddress>(ThreadContext.Rip);
            if (InstructionPointer >= GuardedRangeStart && InstructionPointer < GuardedRangeStart + GuardedRangeSize)
            {
                bThreadInsideRange = true;
                break;
            }
        }

        if (!bThreadInsideRange)
        {
            return true;
        }

        ResumeSuspendedThreads();
        ::Sleep(1);
    }

    UE_LOG_WARNING("Process", "A game thread kept executing inside the range at " + FStringConv::ToHex(GuardedRangeStart));
    return false;
}

void FProcessAttachment::ResumeSuspendedThreads()
{
    for (void* const ThreadHandle : SuspendedThreads)
    {
        ::ResumeThread(static_cast<HANDLE>(ThreadHandle));
        ::CloseHandle(static_cast<HANDLE>(ThreadHandle));
    }

    SuspendedThreads.clear();
}

uint32 FProcessAttachment::FindProcessIdByImageName(std::wstring_view ImageName)
{
    const HANDLE Snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32W Entry = {};
    Entry.dwSize = sizeof(Entry);

    uint32 Result = 0;
    const std::wstring Target(ImageName);

    if (::Process32FirstW(Snapshot, &Entry) != FALSE)
    {
        do
        {
            if (_wcsicmp(Entry.szExeFile, Target.c_str()) == 0)
            {
                Result = Entry.th32ProcessID;
                break;
            }
        } while (::Process32NextW(Snapshot, &Entry) != FALSE);
    }

    ::CloseHandle(Snapshot);
    return Result;
}
