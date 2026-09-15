#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

#include <cstring>

namespace
{
    constexpr size_t PartialReadGranularity = 0x1000;
}

void FRemoteMemory::Initialize(const FProcessAttachment* InProcess)
{
    Process = InProcess;
    PageCache.clear();
}

bool FRemoteMemory::IsValid() const
{
    return Process != nullptr && Process->IsAttached();
}

const FProcessAttachment* FRemoteMemory::GetProcess() const
{
    return Process;
}

bool FRemoteMemory::ReadRaw(FRemoteAddress Address, void* Destination, size_t Size) const
{
    if (!IsValid() || Address == InvalidRemoteAddress || Destination == nullptr || Size == 0)
    {
        return false;
    }

    return Process->ReadMemory(Address, Destination, Size);
}

bool FRemoteMemory::WriteRaw(FRemoteAddress Address, const void* Source, size_t Size) const
{
    if (!IsValid() || Address == InvalidRemoteAddress || Source == nullptr || Size == 0)
    {
        return false;
    }

    return Process->WriteMemory(Address, Source, Size);
}

const FRemoteMemory::FCachePage* FRemoteMemory::AcquirePage(FRemoteAddress PageBase) const
{
    const auto Existing = PageCache.find(PageBase);
    if (Existing != PageCache.end())
    {
        return Existing->second.bValid ? &Existing->second : nullptr;
    }

    FCachePage& Page = PageCache[PageBase];

    if (Process->ReadMemory(PageBase, Page.Bytes.data(), CachePageSize))
    {
        Page.bValid = true;
        return &Page;
    }

    std::memset(Page.Bytes.data(), 0, CachePageSize);

    size_t RecoveredBytes = 0;
    while (RecoveredBytes < CachePageSize)
    {
        if (!Process->ReadMemory(PageBase + RecoveredBytes, Page.Bytes.data() + RecoveredBytes, PartialReadGranularity))
        {
            break;
        }

        RecoveredBytes += PartialReadGranularity;
    }

    Page.bValid = RecoveredBytes > 0;
    return Page.bValid ? &Page : nullptr;
}

bool FRemoteMemory::ReadCachedRaw(FRemoteAddress Address, void* Destination, size_t Size) const
{
    if (!IsValid() || Address == InvalidRemoteAddress || Destination == nullptr || Size == 0)
    {
        return false;
    }

    uint8* Output = static_cast<uint8*>(Destination);
    size_t Remaining = Size;
    FRemoteAddress Cursor = Address;

    while (Remaining > 0)
    {
        const FRemoteAddress PageBase = AlignDown<FRemoteAddress>(Cursor, CachePageSize);
        const size_t PageOffset = static_cast<size_t>(Cursor - PageBase);
        const size_t Chunk = std::min(Remaining, CachePageSize - PageOffset);

        const FCachePage* Page = AcquirePage(PageBase);
        if (Page == nullptr)
        {
            return ReadRaw(Address, Destination, Size);
        }

        std::memcpy(Output, Page->Bytes.data() + PageOffset, Chunk);

        Output += Chunk;
        Cursor += Chunk;
        Remaining -= Chunk;
    }

    return true;
}

void FRemoteMemory::InvalidateCache() const
{
    PageCache.clear();
}

bool FRemoteMemory::IsCommitted(FRemoteAddress Address) const
{
    if (!IsValid() || Address == InvalidRemoteAddress)
    {
        return false;
    }

    FRemoteRegionInfo Region;
    if (!Process->QueryRegion(Address, Region))
    {
        return false;
    }

    return Region.bCommitted;
}

bool FRemoteMemory::IsExecutable(FRemoteAddress Address) const
{
    if (!IsValid() || Address == InvalidRemoteAddress)
    {
        return false;
    }

    FRemoteRegionInfo Region;
    if (!Process->QueryRegion(Address, Region))
    {
        return false;
    }

    return Region.bExecutable;
}

bool FRemoteMemory::ProtectRange(FRemoteAddress Address, size_t Size, ERemoteProtection Protection) const
{
    if (!IsValid())
    {
        return false;
    }

    return Process->ProtectMemory(Address, Size, Protection);
}

bool FRemoteMemory::FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const
{
    if (!IsValid())
    {
        return false;
    }

    Process->FlushInstructionCacheRange(Address, Size);
    return true;
}

std::string FRemoteMemory::ReadAnsiString(FRemoteAddress Address, size_t MaximumLength) const
{
    std::string Result;
    Result.reserve(std::min<size_t>(MaximumLength, 64));

    for (size_t Index = 0; Index < MaximumLength; ++Index)
    {
        const char Character = ReadCached<char>(Address + Index);
        if (Character == '\0')
        {
            break;
        }

        Result.push_back(Character);
    }

    return Result;
}

std::wstring FRemoteMemory::ReadWideString(FRemoteAddress Address, size_t MaximumLength) const
{
    std::wstring Result;
    Result.reserve(std::min<size_t>(MaximumLength, 64));

    for (size_t Index = 0; Index < MaximumLength; ++Index)
    {
        const wchar_t Character = ReadCached<wchar_t>(Address + Index * sizeof(wchar_t));
        if (Character == L'\0')
        {
            break;
        }

        Result.push_back(Character);
    }

    return Result;
}

FRemoteAddress FRemoteMemory::ReadPointer(FRemoteAddress Address) const
{
    return Read<FRemoteAddress>(Address);
}

FRemoteAddress FRemoteMemory::ReadCachedPointer(FRemoteAddress Address) const
{
    return ReadCached<FRemoteAddress>(Address);
}

FRemoteAddress FRemoteMemory::ResolveRelativeAddress(FRemoteAddress InstructionAddress, uint32 OperandOffset, uint32 InstructionLength) const
{
    if (InstructionAddress == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const int32 Displacement = Read<int32>(InstructionAddress + OperandOffset);
    return InstructionAddress + InstructionLength + static_cast<int64>(Displacement);
}
