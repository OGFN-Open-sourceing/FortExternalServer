#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

class FRemoteAllocation
{
public:
    FRemoteAllocation() = default;

    ~FRemoteAllocation();

    FRemoteAllocation(const FRemoteAllocation&) = delete;
    FRemoteAllocation& operator=(const FRemoteAllocation&) = delete;

    bool Reserve(const FProcessAttachment& Process, const FRemoteMemory& Memory, FRemoteAddress PreferredNeighbour, size_t Size);

    void Release();

    bool IsValid() const;

    FRemoteAddress GetBaseAddress() const;

    size_t GetSize() const;

    size_t GetUsedBytes() const;

    FRemoteAddress Allocate(size_t Size, size_t Alignment = 16);

    FRemoteAddress AllocateAndWrite(const void* Source, size_t Size, size_t Alignment = 16);

    FRemoteAddress AllocateWideString(const std::wstring& Value);

    FRemoteAddress AllocateZeroed(size_t Size, size_t Alignment = 16);

    bool IsWithinBranchRange(FRemoteAddress Address) const;

private:
    static FRemoteAddress FindFreeRegionNear(const FProcessAttachment& Process, FRemoteAddress Anchor, size_t Size);

    const FProcessAttachment* OwningProcess = nullptr;
    const FRemoteMemory* Memory = nullptr;
    FRemoteAddress BaseAddress = InvalidRemoteAddress;
    size_t TotalSize = 0;
    size_t Cursor = 0;
};
