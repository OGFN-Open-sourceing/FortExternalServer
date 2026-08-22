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
        FRemoteRegionInfo Region;
        if (!Process.QueryRegion(Candidate, Region))
        {
            break;
        }

        if (!Region.bFree || Region.RegionSize < Size)
        {
            Candidate = AlignUp<uint64>(Region.BaseAddress + Region.RegionSize, AllocationGranularity) - AllocationGranularity;
            continue;
        }

        const FRemoteAddress Allocated = Process.AllocateMemory(Candidate, Size);
        if (Allocated != InvalidRemoteAddress)
        {
            return Allocated;
        }
    }

    uint64 Candidate = AlignDown<uint64>(Anchor - AllocationGranularity, AllocationGranularity);
    while (Candidate > LowerBound)
    {
        FRemoteRegionInfo Region;
        if (!Process.QueryRegion(Candidate, Region))
        {
            break;
        }

        if (Region.bFree && Region.RegionSize >= Size)
        {
            const FRemoteAddress Allocated = Process.AllocateMemory(Candidate, Size);
            if (Allocated != InvalidRemoteAddress)
            {
                return Allocated;
            }
        }

        if (Region.BaseAddress < AllocationGranularity)
        {
            break;
        }

        Candidate = AlignDown<uint64>(Region.BaseAddress - AllocationGranularity, AllocationGranularity);
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
        BaseAddress = Process.AllocateMemory(InvalidRemoteAddress, TotalSize);
        if (BaseAddress == InvalidRemoteAddress)
        {
            UE_LOG_ERROR("RemoteProcess", "Failed to reserve remote code arena: " + FStringConv::ToNarrow(FPlatformMisc::GetLastErrorText()));
            return false;
        }

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
        OwningProcess->FreeMemory(BaseAddress, TotalSize);
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
