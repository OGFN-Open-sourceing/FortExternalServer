#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

#include <cstring>

void FRemoteMemory::Initialize(const FProcessAttachment* InProcess)
{
    Process = InProcess;
    PageCache.clear();
}

bool FRemoteMemory::IsValid() const
{
    return Process != nullptr && Process->IsAttached();
}

bool FRemoteMemory::ReadRaw(FRemoteAddress Address, void* Destination, size_t Size) const
{
    if (!IsValid() || Address == InvalidRemoteAddress || Destination == nullptr || Size == 0)
    {
        return false;
    }

    SIZE_T BytesRead = 0;
    if (::ReadProcessMemory(Process->GetProcessHandle(), reinterpret_cast<LPCVOID>(Address), Destination, Size, &BytesRead) == FALSE)
    {
        return false;
    }

    return BytesRead == Size;
}

bool FRemoteMemory::WriteRaw(FRemoteAddress Address, const void* Source, size_t Size) const
{
    if (!IsValid() || Address == InvalidRemoteAddress || Source == nullptr || Size == 0)
    {
        return false;
    }

    uint32 OldProtection = 0;
    const bool bProtected = ProtectRange(Address, Size, PAGE_EXECUTE_READWRITE, OldProtection);

    SIZE_T BytesWritten = 0;
    const BOOL bWritten = ::WriteProcessMemory(Process->GetProcessHandle(), reinterpret_cast<LPVOID>(Address), Source, Size, &BytesWritten);

    if (bProtected)
    {
        uint32 IgnoredProtection = 0;
        ProtectRange(Address, Size, OldProtection, IgnoredProtection);
    }

    return bWritten != FALSE && BytesWritten == Size;
}

const FRemoteMemory::FCachePage* FRemoteMemory::AcquirePage(FRemoteAddress PageBase) const
{
    const auto Existing = PageCache.find(PageBase);
    if (Existing != PageCache.end())
    {
        return Existing->second.bValid ? &Existing->second : nullptr;
    }

    FCachePage& Page = PageCache[PageBase];

    SIZE_T BytesRead = 0;
    if (::ReadProcessMemory(Process->GetProcessHandle(), reinterpret_cast<LPCVOID>(PageBase), Page.Bytes.data(), CachePageSize, &BytesRead) != FALSE && BytesRead == CachePageSize)
    {
        Page.bValid = true;
        return &Page;
    }

    if (BytesRead > 0)
    {
        Page.bValid = true;
        std::memset(Page.Bytes.data() + BytesRead, 0, CachePageSize - BytesRead);
        return &Page;
    }

    Page.bValid = false;
    return nullptr;
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

    MEMORY_BASIC_INFORMATION Information = {};
    if (::VirtualQueryEx(Process->GetProcessHandle(), reinterpret_cast<LPCVOID>(Address), &Information, sizeof(Information)) == 0)
    {
        return false;
    }

    if (Information.State != MEM_COMMIT)
    {
        return false;
    }

    return (Information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
}

bool FRemoteMemory::IsExecutable(FRemoteAddress Address) const
{
    if (!IsValid() || Address == InvalidRemoteAddress)
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION Information = {};
    if (::VirtualQueryEx(Process->GetProcessHandle(), reinterpret_cast<LPCVOID>(Address), &Information, sizeof(Information)) == 0)
    {
        return false;
    }

    constexpr DWORD ExecutableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return Information.State == MEM_COMMIT && (Information.Protect & ExecutableMask) != 0;
}

bool FRemoteMemory::ProtectRange(FRemoteAddress Address, size_t Size, uint32 NewProtection, uint32& OutOldProtection) const
{
    if (!IsValid())
    {
        return false;
    }

    DWORD PreviousProtection = 0;
    const BOOL bChanged = ::VirtualProtectEx(Process->GetProcessHandle(), reinterpret_cast<LPVOID>(Address), Size, NewProtection, &PreviousProtection);

    OutOldProtection = PreviousProtection;
    return bChanged != FALSE;
}

bool FRemoteMemory::FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const
{
    if (!IsValid())
    {
        return false;
    }

    return ::FlushInstructionCache(Process->GetProcessHandle(), reinterpret_cast<LPCVOID>(Address), Size) != FALSE;
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
