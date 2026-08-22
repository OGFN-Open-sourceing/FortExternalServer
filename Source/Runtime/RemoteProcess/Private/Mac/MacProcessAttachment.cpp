#include "Runtime/Core/Public/HAL/PlatformDetection.h"

#if FORT_PLATFORM_MAC

#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <algorithm>
#include <cstring>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/loader.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

extern char** environ;

struct FProcessAttachment::FPlatformState
{
    mach_port_t Task = MACH_PORT_NULL;
    pid_t Pid = 0;
    bool bLaunchedByServer = false;
};

namespace
{
    constexpr uint64 DyldImageInfoNameOffset = 8;
    constexpr uint64 DyldImageInfoStride = 24;

    vm_prot_t ToMachProtection(ERemoteProtection Protection)
    {
        switch (Protection)
        {
        case ERemoteProtection::NoAccess:
            return VM_PROT_NONE;
        case ERemoteProtection::ReadOnly:
            return VM_PROT_READ;
        case ERemoteProtection::ReadWrite:
            return VM_PROT_READ | VM_PROT_WRITE;
        case ERemoteProtection::ExecuteRead:
            return VM_PROT_READ | VM_PROT_EXECUTE;
        default:
            return VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
        }
    }

    std::wstring LastNameComponent(const std::wstring& Path)
    {
        const size_t Separator = Path.find_last_of(L'/');
        return Separator == std::wstring::npos ? Path : Path.substr(Separator + 1);
    }
}

FProcessAttachment::~FProcessAttachment()
{
    Detach();
}

bool FProcessAttachment::PreparePrivileges()
{
    return ::geteuid() == 0;
}

std::string FProcessAttachment::DescribePlatformRequirements()
{
    return "macOS x86_64. task_for_pid requires running the server with sudo, or a build signed with the debugger entitlement.";
}

bool FProcessAttachment::LaunchAndAttach(const std::wstring& ExecutablePath, const std::wstring& CommandLine, const std::wstring& WorkingDirectory)
{
    Detach();

    const std::string NarrowPath = FStringConv::ToNarrow(ExecutablePath);
    const std::string NarrowWorkingDirectory = FStringConv::ToNarrow(WorkingDirectory);

    std::vector<std::string> ArgumentStorage;
    ArgumentStorage.push_back(NarrowPath);

    for (const std::string& Token : FStringConv::SplitBy(FStringConv::ToNarrow(CommandLine), ' '))
    {
        const std::string Trimmed = FStringConv::TrimWhitespace(Token);
        if (!Trimmed.empty())
        {
            ArgumentStorage.push_back(Trimmed);
        }
    }

    std::vector<char*> Arguments;
    Arguments.reserve(ArgumentStorage.size() + 1);

    for (std::string& Argument : ArgumentStorage)
    {
        Arguments.push_back(Argument.data());
    }

    Arguments.push_back(nullptr);

    if (!NarrowWorkingDirectory.empty())
    {
        ::chdir(NarrowWorkingDirectory.c_str());
    }

    pid_t SpawnedPid = 0;
    const int SpawnResult = ::posix_spawn(&SpawnedPid, NarrowPath.c_str(), nullptr, nullptr, Arguments.data(), environ);

    if (SpawnResult != 0)
    {
        UE_LOG_ERROR("RemoteProcess", "posix_spawn failed: " + FStringConv::ToNarrow(FPlatformMisc::GetErrorText(static_cast<uint32>(SpawnResult))));
        return false;
    }

    mach_port_t SpawnedTask = MACH_PORT_NULL;
    const kern_return_t TaskResult = ::task_for_pid(::mach_task_self(), SpawnedPid, &SpawnedTask);

    if (TaskResult != KERN_SUCCESS)
    {
        UE_LOG_ERROR("RemoteProcess", "task_for_pid failed with code " + std::to_string(TaskResult));
        UE_LOG_ERROR("RemoteProcess", DescribePlatformRequirements());
        ::kill(SpawnedPid, SIGKILL);
        return false;
    }

    Platform = new FPlatformState();
    Platform->Task = SpawnedTask;
    Platform->Pid = SpawnedPid;
    Platform->bLaunchedByServer = true;

    ProcessId = static_cast<uint32>(SpawnedPid);
    PrimaryModuleName = LastNameComponent(ExecutablePath);

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

    mach_port_t FoundTask = MACH_PORT_NULL;
    const kern_return_t TaskResult = ::task_for_pid(::mach_task_self(), static_cast<pid_t>(FoundProcessId), &FoundTask);

    if (TaskResult != KERN_SUCCESS)
    {
        UE_LOG_ERROR("RemoteProcess", "task_for_pid failed with code " + std::to_string(TaskResult));
        UE_LOG_ERROR("RemoteProcess", DescribePlatformRequirements());
        return false;
    }

    Platform = new FPlatformState();
    Platform->Task = FoundTask;
    Platform->Pid = static_cast<pid_t>(FoundProcessId);

    ProcessId = FoundProcessId;
    PrimaryModuleName = ProcessImageName;

    UE_LOG_DISPLAY("RemoteProcess", "Attached to " + FStringConv::ToNarrow(ProcessImageName) + " with process id " + std::to_string(ProcessId));
    return true;
}

void FProcessAttachment::Detach()
{
    if (Platform != nullptr)
    {
        if (Platform->Task != MACH_PORT_NULL)
        {
            ::mach_port_deallocate(::mach_task_self(), Platform->Task);
        }

        delete Platform;
        Platform = nullptr;
    }

    ProcessId = 0;
    Modules.clear();
}

bool FProcessAttachment::IsAttached() const
{
    return Platform != nullptr && Platform->Task != MACH_PORT_NULL;
}

bool FProcessAttachment::IsAlive() const
{
    if (!IsAttached())
    {
        return false;
    }

    return ::kill(Platform->Pid, 0) == 0;
}

void FProcessAttachment::Terminate()
{
    if (IsAttached())
    {
        ::kill(Platform->Pid, SIGKILL);
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

    mach_vm_size_t BytesRead = 0;
    const kern_return_t Result = ::mach_vm_read_overwrite(Platform->Task, static_cast<mach_vm_address_t>(Address), static_cast<mach_vm_size_t>(Size),
        reinterpret_cast<mach_vm_address_t>(Destination), &BytesRead);

    return Result == KERN_SUCCESS && BytesRead == Size;
}

bool FProcessAttachment::WriteMemory(FRemoteAddress Address, const void* Source, size_t Size) const
{
    if (!IsAttached())
    {
        return false;
    }

    FRemoteRegionInfo Region;
    const bool bQueried = QueryRegion(Address, Region);

    ProtectMemory(Address, Size, ERemoteProtection::ExecuteReadWrite);

    const kern_return_t Result = ::mach_vm_write(Platform->Task, static_cast<mach_vm_address_t>(Address),
        reinterpret_cast<vm_offset_t>(const_cast<void*>(Source)), static_cast<mach_msg_type_number_t>(Size));

    if (bQueried && !Region.bExecutable)
    {
        ProtectMemory(Address, Size, ERemoteProtection::ReadWrite);
    }

    return Result == KERN_SUCCESS;
}

bool FProcessAttachment::ProtectMemory(FRemoteAddress Address, size_t Size, ERemoteProtection Protection) const
{
    if (!IsAttached())
    {
        return false;
    }

    const kern_return_t Result =
        ::mach_vm_protect(Platform->Task, static_cast<mach_vm_address_t>(Address), static_cast<mach_vm_size_t>(Size), FALSE, ToMachProtection(Protection));

    return Result == KERN_SUCCESS;
}

FRemoteAddress FProcessAttachment::AllocateMemory(FRemoteAddress PreferredAddress, size_t Size) const
{
    if (!IsAttached())
    {
        return InvalidRemoteAddress;
    }

    mach_vm_address_t Allocated = static_cast<mach_vm_address_t>(PreferredAddress);
    const int Flags = PreferredAddress != InvalidRemoteAddress ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE;

    if (::mach_vm_allocate(Platform->Task, &Allocated, static_cast<mach_vm_size_t>(Size), Flags) != KERN_SUCCESS)
    {
        return InvalidRemoteAddress;
    }

    if (::mach_vm_protect(Platform->Task, Allocated, static_cast<mach_vm_size_t>(Size), FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) != KERN_SUCCESS)
    {
        ::mach_vm_deallocate(Platform->Task, Allocated, static_cast<mach_vm_size_t>(Size));
        return InvalidRemoteAddress;
    }

    return static_cast<FRemoteAddress>(Allocated);
}

bool FProcessAttachment::FreeMemory(FRemoteAddress Address, size_t Size) const
{
    if (!IsAttached())
    {
        return false;
    }

    return ::mach_vm_deallocate(Platform->Task, static_cast<mach_vm_address_t>(Address), static_cast<mach_vm_size_t>(Size)) == KERN_SUCCESS;
}

bool FProcessAttachment::QueryRegion(FRemoteAddress Address, FRemoteRegionInfo& OutInfo) const
{
    if (!IsAttached())
    {
        return false;
    }

    mach_vm_address_t RegionAddress = static_cast<mach_vm_address_t>(Address);
    mach_vm_size_t RegionSize = 0;
    vm_region_basic_info_data_64_t Information = {};
    mach_msg_type_number_t InfoCount = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t ObjectName = MACH_PORT_NULL;

    const kern_return_t Result = ::mach_vm_region(Platform->Task, &RegionAddress, &RegionSize, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&Information), &InfoCount, &ObjectName);

    if (Result != KERN_SUCCESS)
    {
        OutInfo.BaseAddress = Address;
        OutInfo.RegionSize = 0;
        OutInfo.bFree = true;
        OutInfo.bCommitted = false;
        OutInfo.bExecutable = false;
        return true;
    }

    OutInfo.BaseAddress = static_cast<FRemoteAddress>(RegionAddress);
    OutInfo.RegionSize = static_cast<uint64>(RegionSize);
    OutInfo.bFree = RegionAddress > static_cast<mach_vm_address_t>(Address);
    OutInfo.bCommitted = !OutInfo.bFree && Information.protection != VM_PROT_NONE;
    OutInfo.bExecutable = !OutInfo.bFree && (Information.protection & VM_PROT_EXECUTE) != 0;

    return true;
}

void FProcessAttachment::FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const
{
}

bool FProcessAttachment::RefreshModules()
{
    Modules.clear();

    if (!IsAttached())
    {
        return false;
    }

    task_dyld_info_data_t DyldInfo = {};
    mach_msg_type_number_t InfoCount = TASK_DYLD_INFO_COUNT;

    if (::task_info(Platform->Task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&DyldInfo), &InfoCount) != KERN_SUCCESS)
    {
        return false;
    }

    uint64 ImageArrayAddress = 0;
    uint32 ImageCount = 0;

    if (!ReadMemory(static_cast<FRemoteAddress>(DyldInfo.all_image_info_addr) + 8, &ImageCount, sizeof(ImageCount)))
    {
        return false;
    }

    if (!ReadMemory(static_cast<FRemoteAddress>(DyldInfo.all_image_info_addr) + 16, &ImageArrayAddress, sizeof(ImageArrayAddress)))
    {
        return false;
    }

    Modules.reserve(ImageCount);

    for (uint32 Index = 0; Index < ImageCount; ++Index)
    {
        const FRemoteAddress EntryAddress = static_cast<FRemoteAddress>(ImageArrayAddress) + static_cast<uint64>(Index) * DyldImageInfoStride;

        uint64 LoadAddress = 0;
        uint64 PathAddress = 0;

        if (!ReadMemory(EntryAddress, &LoadAddress, sizeof(LoadAddress)))
        {
            continue;
        }

        if (!ReadMemory(EntryAddress + DyldImageInfoNameOffset, &PathAddress, sizeof(PathAddress)))
        {
            continue;
        }

        if (LoadAddress == 0 || PathAddress == 0)
        {
            continue;
        }

        std::string Path;
        Path.reserve(256);

        for (uint32 Character = 0; Character < 1024; ++Character)
        {
            char Byte = 0;
            if (!ReadMemory(static_cast<FRemoteAddress>(PathAddress) + Character, &Byte, sizeof(Byte)) || Byte == '\0')
            {
                break;
            }

            Path.push_back(Byte);
        }

        if (Path.empty())
        {
            continue;
        }

        mach_header_64 Header = {};
        if (!ReadMemory(static_cast<FRemoteAddress>(LoadAddress), &Header, sizeof(Header)) || Header.magic != MH_MAGIC_64)
        {
            continue;
        }

        uint64 ImageSize = 0;
        uint64 TextSegmentAddress = 0;
        FRemoteAddress CommandAddress = static_cast<FRemoteAddress>(LoadAddress) + sizeof(mach_header_64);

        for (uint32 Command = 0; Command < Header.ncmds; ++Command)
        {
            load_command LoadCommandHeader = {};
            if (!ReadMemory(CommandAddress, &LoadCommandHeader, sizeof(LoadCommandHeader)))
            {
                break;
            }

            if (LoadCommandHeader.cmd == LC_SEGMENT_64)
            {
                segment_command_64 Segment = {};
                if (ReadMemory(CommandAddress, &Segment, sizeof(Segment)))
                {
                    const uint64 SegmentEnd = Segment.vmaddr + Segment.vmsize;
                    ImageSize = std::max<uint64>(ImageSize, SegmentEnd);

                    if (std::strncmp(Segment.segname, "__TEXT", sizeof(Segment.segname)) == 0)
                    {
                        TextSegmentAddress = Segment.vmaddr;
                    }
                }
            }

            CommandAddress += LoadCommandHeader.cmdsize;
        }

        FRemoteModuleInfo Entry;
        Entry.Name = FStringConv::ToWide(Path.substr(Path.find_last_of('/') == std::string::npos ? 0 : Path.find_last_of('/') + 1));
        Entry.BaseAddress = static_cast<FRemoteAddress>(LoadAddress);
        Entry.SlideOffset = TextSegmentAddress == 0 ? 0 : LoadAddress - TextSegmentAddress;
        Entry.ImageSize = ImageSize > Entry.SlideOffset ? ImageSize - Entry.SlideOffset : ImageSize;

        Modules.push_back(std::move(Entry));
    }

    return !Modules.empty();
}

uint32 FProcessAttachment::FindProcessIdByImageName(std::wstring_view ImageName)
{
    const std::string Target = FStringConv::ToNarrow(std::wstring(ImageName));

    std::vector<pid_t> Pids(4096);
    const int ByteCount = ::proc_listpids(PROC_ALL_PIDS, 0, Pids.data(), static_cast<int>(Pids.size() * sizeof(pid_t)));

    if (ByteCount <= 0)
    {
        return 0;
    }

    const size_t PidCount = static_cast<size_t>(ByteCount) / sizeof(pid_t);

    for (size_t Index = 0; Index < PidCount; ++Index)
    {
        if (Pids[Index] == 0)
        {
            continue;
        }

        char NameBuffer[PROC_PIDPATHINFO_MAXSIZE] = {};
        if (::proc_name(Pids[Index], NameBuffer, sizeof(NameBuffer)) <= 0)
        {
            continue;
        }

        if (Target.rfind(NameBuffer, 0) == 0 || Target == NameBuffer)
        {
            return static_cast<uint32>(Pids[Index]);
        }
    }

    return 0;
}

#endif
