#include "Runtime/RemoteProcess/Public/ModuleImage.h"

#include <algorithm>
#include <cstring>

bool FImageSection::IsExecutable() const
{
    return (Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
}

bool FImageSection::IsReadOnlyData() const
{
    return (Characteristics & IMAGE_SCN_MEM_READ) != 0 && (Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0;
}

bool FModuleImage::Load(const FRemoteMemory& Memory, const FRemoteModuleInfo& ModuleInfo)
{
    BaseAddress = ModuleInfo.BaseAddress;
    ImageSize = ModuleInfo.ImageSize;
    Bytes.clear();
    Sections.clear();

    if (BaseAddress == InvalidRemoteAddress || ImageSize == 0)
    {
        return false;
    }

    Bytes.resize(ImageSize);

    constexpr size_t ChunkSize = 0x100000;
    constexpr size_t PageSize = 0x1000;

    UnreadableBytes = 0;

    for (size_t Offset = 0; Offset < ImageSize; Offset += ChunkSize)
    {
        const size_t Remaining = std::min<size_t>(ChunkSize, ImageSize - Offset);
        if (Memory.ReadRaw(BaseAddress + Offset, Bytes.data() + Offset, Remaining))
        {
            continue;
        }

        for (size_t PageOffset = Offset; PageOffset < Offset + Remaining; PageOffset += PageSize)
        {
            const size_t PageBytes = std::min<size_t>(PageSize, Offset + Remaining - PageOffset);
            if (!Memory.ReadRaw(BaseAddress + PageOffset, Bytes.data() + PageOffset, PageBytes))
            {
                std::memset(Bytes.data() + PageOffset, 0, PageBytes);
                UnreadableBytes += PageBytes;
            }
        }
    }

    if (UnreadableBytes != 0)
    {
        UE_LOG_WARNING("RemoteProcess", std::to_string(UnreadableBytes) + " of " + std::to_string(ImageSize) + " image bytes could not be read");
    }

    const auto* DosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(Bytes.data());
    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    if (static_cast<size_t>(DosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > Bytes.size())
    {
        return false;
    }

    const auto* NtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Bytes.data() + DosHeader->e_lfanew);
    if (NtHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    const auto* SectionHeader = IMAGE_FIRST_SECTION(NtHeaders);
    Sections.reserve(NtHeaders->FileHeader.NumberOfSections);

    for (uint16 Index = 0; Index < NtHeaders->FileHeader.NumberOfSections; ++Index)
    {
        const IMAGE_SECTION_HEADER& Header = SectionHeader[Index];

        FImageSection Section;
        Section.Name.assign(reinterpret_cast<const char*>(Header.Name), strnlen(reinterpret_cast<const char*>(Header.Name), IMAGE_SIZEOF_SHORT_NAME));
        Section.VirtualAddress = BaseAddress + Header.VirtualAddress;
        Section.VirtualSize = Header.Misc.VirtualSize != 0 ? Header.Misc.VirtualSize : Header.SizeOfRawData;
        Section.Characteristics = Header.Characteristics;

        Sections.push_back(std::move(Section));
    }

    return !Sections.empty();
}

bool FModuleImage::IsLoaded() const
{
    return !Bytes.empty() && !Sections.empty();
}

size_t FModuleImage::GetUnreadableByteCount() const
{
    return UnreadableBytes;
}

bool FModuleImage::IsExecutableImagePopulated(size_t MinimumPercent) const
{
    size_t Total = 0;
    size_t Populated = 0;

    for (const FImageSection& Section : Sections)
    {
        if (!Section.IsExecutable())
        {
            continue;
        }

        const size_t Start = AddressToOffset(Section.VirtualAddress);
        const size_t End = std::min<size_t>(Start + Section.VirtualSize, Bytes.size());

        for (size_t Offset = Start; Offset < End; Offset += 0x1000)
        {
            ++Total;

            const size_t Limit = std::min<size_t>(Offset + 0x1000, End);
            for (size_t Probe = Offset; Probe < Limit; Probe += 64)
            {
                if (Bytes[Probe] != 0)
                {
                    ++Populated;
                    break;
                }
            }
        }
    }

    if (Total == 0)
    {
        return false;
    }

    return Populated * 100 / Total >= MinimumPercent;
}

FRemoteAddress FModuleImage::GetBaseAddress() const
{
    return BaseAddress;
}

uint32 FModuleImage::GetImageSize() const
{
    return ImageSize;
}

const std::vector<uint8>& FModuleImage::GetBytes() const
{
    return Bytes;
}

const std::vector<FImageSection>& FModuleImage::GetSections() const
{
    return Sections;
}

const FImageSection* FModuleImage::FindSection(std::string_view Name) const
{
    const auto Found = std::find_if(Sections.begin(), Sections.end(), [Name](const FImageSection& Section) { return Section.Name == Name; });
    return Found != Sections.end() ? &(*Found) : nullptr;
}

const FImageSection* FModuleImage::FindSectionContaining(FRemoteAddress Address) const
{
    const auto Found = std::find_if(Sections.begin(), Sections.end(), [Address](const FImageSection& Section) {
        return Address >= Section.VirtualAddress && Address < Section.VirtualAddress + Section.VirtualSize;
    });

    return Found != Sections.end() ? &(*Found) : nullptr;
}

bool FModuleImage::ContainsAddress(FRemoteAddress Address) const
{
    return Address >= BaseAddress && Address < BaseAddress + ImageSize;
}

FRemoteAddress FModuleImage::OffsetToAddress(size_t Offset) const
{
    return BaseAddress + Offset;
}

size_t FModuleImage::AddressToOffset(FRemoteAddress Address) const
{
    return static_cast<size_t>(Address - BaseAddress);
}

const uint8* FModuleImage::GetLocalPointer(FRemoteAddress Address) const
{
    if (!ContainsAddress(Address))
    {
        return nullptr;
    }

    return Bytes.data() + AddressToOffset(Address);
}
