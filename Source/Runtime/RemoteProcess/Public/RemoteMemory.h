#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <array>
#include <unordered_map>

class FRemoteMemory
{
public:
    static constexpr size_t CachePageSize = 0x10000;

    void Initialize(const FProcessAttachment* InProcess);

    bool IsValid() const;

    const FProcessAttachment* GetProcess() const;

    bool ReadRaw(FRemoteAddress Address, void* Destination, size_t Size) const;

    bool WriteRaw(FRemoteAddress Address, const void* Source, size_t Size) const;

    bool ReadCachedRaw(FRemoteAddress Address, void* Destination, size_t Size) const;

    void InvalidateCache() const;

    bool IsCommitted(FRemoteAddress Address) const;

    bool IsExecutable(FRemoteAddress Address) const;

    bool ProtectRange(FRemoteAddress Address, size_t Size, ERemoteProtection Protection) const;

    bool FlushInstructionCacheRange(FRemoteAddress Address, size_t Size) const;

    std::string ReadAnsiString(FRemoteAddress Address, size_t MaximumLength) const;

    std::wstring ReadWideString(FRemoteAddress Address, size_t MaximumLength) const;

    template<typename ValueType>
    ValueType Read(FRemoteAddress Address) const
    {
        ValueType Value = {};
        ReadRaw(Address, &Value, sizeof(ValueType));
        return Value;
    }

    template<typename ValueType>
    ValueType ReadCached(FRemoteAddress Address) const
    {
        ValueType Value = {};
        ReadCachedRaw(Address, &Value, sizeof(ValueType));
        return Value;
    }

    template<typename ValueType>
    bool Write(FRemoteAddress Address, const ValueType& Value) const
    {
        return WriteRaw(Address, &Value, sizeof(ValueType));
    }

    FRemoteAddress ReadPointer(FRemoteAddress Address) const;

    FRemoteAddress ReadCachedPointer(FRemoteAddress Address) const;

    FRemoteAddress ResolveRelativeAddress(FRemoteAddress InstructionAddress, uint32 OperandOffset, uint32 InstructionLength) const;

private:
    struct FCachePage
    {
        std::array<uint8, CachePageSize> Bytes = {};
        bool bValid = false;
    };

    const FCachePage* AcquirePage(FRemoteAddress PageBase) const;

    const FProcessAttachment* Process = nullptr;
    mutable std::unordered_map<FRemoteAddress, FCachePage> PageCache;
};
