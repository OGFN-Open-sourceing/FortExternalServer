#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"

#include <vector>

namespace
{
    constexpr uint64 BranchReachDistance = 0x60000000ull;
    constexpr uint64 AllocationGranularity = 0x10000ull;
}

FRemoteAllocation::~FRemoteAllocation()
{
    Release();
}

FRemoteAddress FRemoteAllocation::FindFreeRegionNear(const FProcessAttachment& Process, FRemoteAddress Anchor, size_t Size)
{
    const uint64 LowerBound = Anchor > BranchReachDistance ? Anchor - BranchReachDistance : AllocationGranularity;
    const uint64 UpperBound = Anchor + BranchReachDistance;

    for (uint64 Candidate = AlignUp<uint64>(Anchor + AllocationGranularity, AllocationGranularity); Candidate < UpperBound; Candidate += AllocationGranularity)
    {
        MEMORY_BASIC_INFORMATION Information = {};
        if (::VirtualQueryEx(Process.GetProcessHandle(), reinterpret_cast<LPCVOID>(Candidate), &Information, sizeof(Information)) == 0)
        {
            break;
        }

        if (Information.State != MEM_FREE || Information.RegionSize < Size)
        {
            Candidate = AlignUp<uint64>(reinterpret_cast<uint64>(Information.BaseAddress) + Information.RegionSize, AllocationGranularity) - AllocationGranularity;
            continue;
        }

        const LPVOID Allocated = ::VirtualAllocEx(Process.GetProcessHandle(), reinterpret_cast<LPVOID>(Candidate), Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (Allocated != nullptr)
        {
            return reinterpret_cast<FRemoteAddress>(Allocated);
        }
    }

    uint64 Candidate = AlignDown<uint64>(Anchor - AllocationGranularity, AllocationGranularity);
    while (Candidate > LowerBound)
    {
        MEMORY_BASIC_INFORMATION Information = {};
        if (::VirtualQueryEx(Process.GetProcessHandle(), reinterpret_cast<LPCVOID>(Candidate), &Information, sizeof(Information)) == 0)
        {
            break;
        }

        if (Information.State == MEM_FREE && Information.RegionSize >= Size)
        {
            const LPVOID Allocated = ::VirtualAllocEx(Process.GetProcessHandle(), reinterpret_cast<LPVOID>(Candidate), Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (Allocated != nullptr)
            {
                return reinterpret_cast<FRemoteAddress>(Allocated);
            }
        }

        const uint64 RegionBase = reinterpret_cast<uint64>(Information.AllocationBase != nullptr ? Information.AllocationBase : Information.BaseAddress);
        if (RegionBase < AllocationGranularity)
        {
            break;
        }

        Candidate = AlignDown<uint64>(RegionBase - AllocationGranularity, AllocationGranularity);
    }

    return InvalidRemoteAddress;
}

bool FRemoteAllocation::Reserve(const FProcessAttachment& Process, const FRemoteMemory& InMemory, FRemoteAddress PreferredNeighbour, size_t Size)
{
    Release();

    OwningProcess = &Process;
    Memory = &InMemory;
    TotalSize = AlignUp<size_t>(Size, static_cast<size_t>(AllocationGranularity));

    BaseAddress = FindFreeRegionNear(Process, PreferredNeighbour, TotalSize);

    if (BaseAddress == InvalidRemoteAddress)
    {
        const LPVOID Fallback = ::VirtualAllocEx(Process.GetProcessHandle(), nullptr, TotalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (Fallback == nullptr)
        {
            UE_LOG_ERROR("RemoteProcess", "Failed to reserve remote code arena: " + FStringConv::ToNarrow(FWindowsPlatform::GetLastErrorText(::GetLastError())));
            return false;
        }

        BaseAddress = reinterpret_cast<FRemoteAddress>(Fallback);
        UE_LOG_WARNING("RemoteProcess", "Remote code arena is outside relative branch range of the game image");
    }

    Cursor = 0;

    std::vector<uint8> ZeroFill(TotalSize, 0);
    InMemory.WriteRaw(BaseAddress, ZeroFill.data(), ZeroFill.size());

    UE_LOG_DISPLAY("RemoteProcess", "Reserved remote code arena at " + FStringConv::ToHex(BaseAddress) + " (" + std::to_string(TotalSize) + " bytes)");
    return true;
}

void FRemoteAllocation::Release()
{
    if (OwningProcess != nullptr && BaseAddress != InvalidRemoteAddress && OwningProcess->IsAttached())
    {
        ::VirtualFreeEx(OwningProcess->GetProcessHandle(), reinterpret_cast<LPVOID>(BaseAddress), 0, MEM_RELEASE);
    }

    OwningProcess = nullptr;
    Memory = nullptr;
    BaseAddress = InvalidRemoteAddress;
    TotalSize = 0;
    Cursor = 0;
}

bool FRemoteAllocation::IsValid() const
{
    return BaseAddress != InvalidRemoteAddress && Memory != nullptr;
}

FRemoteAddress FRemoteAllocation::GetBaseAddress() const
{
    return BaseAddress;
}

size_t FRemoteAllocation::GetSize() const
{
    return TotalSize;
}

size_t FRemoteAllocation::GetUsedBytes() const
{
    return Cursor;
}

FRemoteAddress FRemoteAllocation::Allocate(size_t Size, size_t Alignment)
{
    if (!IsValid() || Size == 0)
    {
        return InvalidRemoteAddress;
    }

    const size_t AlignedCursor = AlignUp<size_t>(Cursor, Alignment);
    if (AlignedCursor + Size > TotalSize)
    {
        UE_LOG_ERROR("RemoteProcess", "Remote code arena exhausted");
        return InvalidRemoteAddress;
    }

    Cursor = AlignedCursor + Size;
    return BaseAddress + AlignedCursor;
}

FRemoteAddress FRemoteAllocation::AllocateAndWrite(const void* Source, size_t Size, size_t Alignment)
{
    const FRemoteAddress Address = Allocate(Size, Alignment);
    if (Address == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    if (!Memory->WriteRaw(Address, Source, Size))
    {
        return InvalidRemoteAddress;
    }

    return Address;
}

FRemoteAddress FRemoteAllocation::AllocateWideString(const std::wstring& Value)
{
    return AllocateAndWrite(Value.c_str(), (Value.size() + 1) * sizeof(wchar_t), 8);
}

FRemoteAddress FRemoteAllocation::AllocateZeroed(size_t Size, size_t Alignment)
{
    const FRemoteAddress Address = Allocate(Size, Alignment);
    if (Address == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const std::vector<uint8> ZeroFill(Size, 0);
    Memory->WriteRaw(Address, ZeroFill.data(), ZeroFill.size());

    return Address;
}

bool FRemoteAllocation::IsWithinBranchRange(FRemoteAddress Address) const
{
    if (!IsValid())
    {
        return false;
    }

    const int64 Distance = static_cast<int64>(BaseAddress) - static_cast<int64>(Address);
    return Distance > -static_cast<int64>(BranchReachDistance) && Distance < static_cast<int64>(BranchReachDistance);
}
