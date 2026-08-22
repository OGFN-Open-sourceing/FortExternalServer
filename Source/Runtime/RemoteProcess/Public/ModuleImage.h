#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

struct FImageSection
{
    std::string Name;
    FRemoteAddress VirtualAddress = InvalidRemoteAddress;
    uint32 VirtualSize = 0;
    uint32 Characteristics = 0;

    bool IsExecutable() const;

    bool IsReadOnlyData() const;
};

class FModuleImage
{
public:
    bool Load(const FRemoteMemory& Memory, const FRemoteModuleInfo& ModuleInfo);

    bool IsLoaded() const;

    FRemoteAddress GetBaseAddress() const;

    uint32 GetImageSize() const;

    const std::vector<uint8>& GetBytes() const;

    const std::vector<FImageSection>& GetSections() const;

    const FImageSection* FindSection(std::string_view Name) const;

    const FImageSection* FindSectionContaining(FRemoteAddress Address) const;

    bool ContainsAddress(FRemoteAddress Address) const;

    FRemoteAddress OffsetToAddress(size_t Offset) const;

    size_t AddressToOffset(FRemoteAddress Address) const;

    const uint8* GetLocalPointer(FRemoteAddress Address) const;

private:
    FRemoteAddress BaseAddress = InvalidRemoteAddress;
    uint32 ImageSize = 0;
    std::vector<uint8> Bytes;
    std::vector<FImageSection> Sections;
};
