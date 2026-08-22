#include "Runtime/Core/Public/HAL/PlatformDetection.h"

#if FORT_PLATFORM_WINDOWS

#include "Runtime/Core/Public/HAL/WindowsPlatform.h"
#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <algorithm>

struct FProcessAttachment::FPlatformState
{
    HANDLE ProcessHandle = nullptr;
    HANDLE MainThreadHandle = nullptr;
};

namespace
{
    DWORD ToWindowsProtection(ERemoteProtection Protection)
    {
        switch (Protection)
        {
        case ERemoteProtection::NoAccess:
            return PAGE_NOACCESS;
        case ERemoteProtection::ReadOnly:
            return PAGE_READONLY;
        case ERemoteProtection::ReadWrite:
            return PAGE_READWRITE;
        case ERemoteProtection::ExecuteRead:
            return PAGE_EXECUTE_READ;
        default:
            return PAGE_EXECUTE_READWRITE;
        }
    }
}

FProcessAttachment::~FProcessAttachment()
{
    Detach();
}

bool FProcessAttachment::PreparePrivileges()
{
    return FWindowsPlatform::EnableDebugPrivilege();
}

std::string FProcessAttachment::DescribePlatformRequirements()
{
    return "Windows x64. Run as administrator if attaching to a game you started yourself.";
}

bool FProcessAttachment::LaunchAndAttach(const std::wstring& ExecutablePath, const std::wstring& CommandLine, const std::wstring& WorkingDirectory)
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

    const BOOL bCreated = ::CreateProcessW(ExecutablePath.c_str(), MutableCommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
        WorkingDirectory.empty() ? nullptr : WorkingDirectory.c_str(), &StartupInfo, &ProcessInformation);

    if (bCreated == FALSE)
    {
        UE_LOG_ERROR("RemoteProcess", "CreateProcess failed: " + FStringConv::ToNarrow(FPlatformMisc::GetLastErrorText()));
        return false;
    }

    Platform = new FPlatformState();
    Platform->ProcessHandle = ProcessInformation.hProcess;
    Platform->MainThreadHandle = ProcessInformation.hThread;
    ProcessId = ProcessInformation.dwProcessId;

    const size_t Separator = ExecutablePath.find_last_of(L'\\');
    PrimaryModuleName = Separator == std::wstring::npos ? ExecutablePath : ExecutablePath.substr(Separator + 1);

    UE_LOG_DISPLAY("RemoteProcess", "Launched " + FStringConv::ToNarrow(PrimaryModuleName) + " with process id " + std::to_string(ProcessId));
    return true;
}

bool FProcessAttachment::AttachToRunning(const std::wstring& ProcessImageName, uint32 TimeoutMilliseconds)
{
    Detach();

    const uint64 Deadline = FPlatformMisc::GetTimeMilliseconds() + TimeoutMilliseconds;

    uint32 FoundProcessId = 0;
    while (FPlatformMisc::GetTimeMilliseconds() <= Deadline)
    {
        FoundProcessId = FindProcessIdByImageName(ProcessImageName);
        if (FoundProcessId != 0)
        {
            break;
        }

        FPlatformMisc::SleepMilliseconds(250);
    }

    if (FoundProcessId == 0)
    {
        UE_LOG_ERROR("RemoteProcess", "Timed out waiting for " + FStringConv::ToNarrow(ProcessImageName));
        return false;
    }

    constexpr DWORD DesiredAccess = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | SYNCHRONIZE;

    const HANDLE OpenedProcess = ::OpenProcess(DesiredAccess, FALSE, FoundProcessId);
    if (OpenedProcess == nullptr)
    {
        UE_LOG_ERROR("RemoteProcess", "OpenProcess failed: " + FStringConv::ToNarrow(FPlatformMisc::GetLastErrorText()));
        return false;
    }

    Platform = new FPlatformState();
    Platform->ProcessHandle = OpenedProcess;
    ProcessId = FoundProcessId;
    PrimaryModuleName = ProcessImageName;

    UE_LOG_DISPLAY("RemoteProcess", "Attached to " + FStringConv::ToNarrow(ProcessImageName) + " with process id " + std::to_string(ProcessId));
    return true;
}

void FProcessAttachment::Detach()
{
    if (Platform != nullptr)
    {
        if (Platform->MainThreadHandle != nullptr)
        {
            ::CloseHandle(Platform->MainThreadHandle);
        }

        if (Platform->ProcessHandle != nullptr)
        {
            ::CloseHandle(Platform->ProcessHandle);
        }

        delete Platform;
        Platform = nullptr;
    }

    ProcessId = 0;
    Modules.clear();
}

bool FProcessAttachment::IsAttached() const
{
    return Platform != nullptr && Platform->ProcessHandle != nullptr;
}

bool FProcessAttachment::IsAlive() const
{
    if (!IsAttached())
    {
        return false;
    }

    DWORD ExitCode = 0;
    if (::GetExitCodeProcess(Platform->ProcessHandle, &ExitCode) == FALSE)
    {
        return false;
    }

    return ExitCode == STILL_ACTIVE;
}

void FProcessAttachment::Terminate()
{
    if (IsAttached())
    {
        ::TerminateProcess(Platform->ProcessHandle, 0);
    }
}

uint32 FProcessAttachment::GetProcessId() const
{
    return ProcessId;
}

bool FProcessAttachment::ReadMemory(FRemoteAddress Address, void* Destination, size_t Size) const
{
    if (!IsAttached())
    {
        return false;
    }

    SIZE_T BytesRead = 0;
    if (::ReadProcessMemory(Platform->ProcessHandle, reinterpret_cast<LPCVOID>(Address), Destination, Size, &BytesRead) == FALSE)
    {
        return false;
    }

    return BytesRead == Size;
}

bool FProcessAttachment::WriteMemory(FRemoteAddress Address, const void* Source, size_t Size) const
{
    if (!IsAttached())
    {
        return false;
    }

    DWORD PreviousProtection = 0;
    const BOOL bProtected = ::VirtualProtectEx(Platform->ProcessHandle, reinterpret_cast<LPVOID>(Address), Size, PAGE_EXECUTE_READWRITE, &PreviousProtection);

    SIZE_T BytesWritten = 0;
    const BOOL bWritten = ::WriteProcessMemory(Platform->ProcessHandle, reinterpret_cast<LPVOID>(Address), Source, Size, &BytesWritten);

    if (bProtected != FALSE)
    {
        DWORD IgnoredProtection = 0;
        ::VirtualProtectEx(Platform->ProcessHandle, reinterpret_cast<LPVOID>(Address), Size, PreviousProtection, &IgnoredProtection);
    }

    return bWritten != FALSE && BytesWritten == Size;
}

bool FProcessAttachment::ProtectMemory(FRemoteAddress Address, size_t Size, ERemoteProtection Protection) const
{
    if (!IsAttached())
    {
        return false;
    }

    DWORD PreviousProtection = 0;
    return ::VirtualProtectEx(Platform->ProcessHandle, reinterpret_cast<LPVOID>(Address), Size, ToWindowsProtection(Protection), &PreviousProtection) != FALSE;
}

FRemoteAddress FProcessAttachment::AllocateMemory(FRemoteAddress PreferredAddress, size_t Size) const
{
    if (!IsAttached())
    {
        return InvalidRemoteAddress;
    }

    const LPVOID Allocated =
        ::VirtualAllocEx(Platform->ProcessHandle, reinterpret_cast<LPVOID>(PreferredAddress), Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    return reinterpret_cast<FRemoteAddress>(Allocated);
}

bool FProcessAttachment::FreeMemory(FRemoteAddress Address, size_t Size) const
{
    if (!IsAttached())
    {
        return false;
    }

    return ::VirtualFreeEx(Platform->ProcessHandle, reinterpret_cast<LPVOID>(Address), 0, MEM_RELEASE) != FALSE;
}

bool FProcessAttachment::QueryRegion(FRemoteAddress Address, FRemoteRegionInfo& OutInfo) const
{
    if (!IsAttached())
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION Information = {};
    if (::VirtualQueryEx(Platform->ProcessHandle, reinterpret_cast<LPCVOID>(Address), &Information, sizeof(Information)) == 0)
    {
        return false;
    }

    constexpr DWORD ExecutableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    OutInfo.BaseAddress = reinterpret_cast<FRemoteAddress>(Information.BaseAddress);
    OutInfo.RegionSize = Information.RegionSize;
    OutInfo.bFree = Information.State == MEM_FREE;
    OutInfo.bCommitted = Information.State == MEM_COMMIT && (Information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
    OutInfo.bExecutable = Information.State == MEM_COMMIT && (Information.Protect & ExecutableMask) != 0;

    return true;
}

void FProcessAttachment::FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const
{
    if (IsAttached())
    {
        ::FlushInstructionCache(Platform->ProcessHandle, reinterpret_cast<LPCVOID>(Address), Size);
    }
}

bool FProcessAttachment::RefreshModules()
{
    Modules.clear();

    if (!IsAttached())
    {
        return false;
    }

    std::vector<HMODULE> ModuleHandles(1024);
    DWORD BytesNeeded = 0;

    if (::EnumProcessModulesEx(Platform->ProcessHandle, ModuleHandles.data(), static_cast<DWORD>(ModuleHandles.size() * sizeof(HMODULE)), &BytesNeeded,
            LIST_MODULES_ALL) == FALSE)
    {
        return false;
    }

    const size_t ModuleCount = BytesNeeded / sizeof(HMODULE);
    if (ModuleCount > ModuleHandles.size())
    {
        ModuleHandles.resize(ModuleCount);
        if (::EnumProcessModulesEx(Platform->ProcessHandle, ModuleHandles.data(), static_cast<DWORD>(ModuleHandles.size() * sizeof(HMODULE)), &BytesNeeded,
                LIST_MODULES_ALL) == FALSE)
        {
            return false;
        }
    }

    Modules.reserve(ModuleCount);

    for (size_t Index = 0; Index < ModuleCount; ++Index)
    {
        MODULEINFO ModuleInformation = {};
        if (::GetModuleInformation(Platform->ProcessHandle, ModuleHandles[Index], &ModuleInformation, sizeof(ModuleInformation)) == FALSE)
        {
            continue;
        }

        wchar_t NameBuffer[MAX_PATH] = {};
        if (::GetModuleBaseNameW(Platform->ProcessHandle, ModuleHandles[Index], NameBuffer, MAX_PATH) == 0)
        {
            continue;
        }

        FRemoteModuleInfo Entry;
        Entry.Name = NameBuffer;
        Entry.BaseAddress = reinterpret_cast<FRemoteAddress>(ModuleInformation.lpBaseOfDll);
        Entry.ImageSize = ModuleInformation.SizeOfImage;
        Entry.SlideOffset = 0;

        Modules.push_back(std::move(Entry));
    }

    return !Modules.empty();
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

#endif
