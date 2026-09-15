#include "Runtime/RemoteProcess/Public/ModuleImage.h"

#include <algorithm>
#include <cstring>

namespace MachObject
{
    constexpr uint32 Magic64 = 0xFEEDFACFu;
    constexpr uint32 LoadCommandSegment64 = 0x19u;

    constexpr uint32 SectionAttributePureInstructions = 0x80000000u;
    constexpr uint32 SectionAttributeSomeInstructions = 0x00000400u;

    constexpr uint32 ProtectionRead = 0x1u;
    constexpr uint32 ProtectionWrite = 0x2u;
    constexpr uint32 ProtectionExecute = 0x4u;

    struct FHeader64
    {
        uint32 Magic;
        int32 CpuType;
        int32 CpuSubType;
        uint32 FileType;
        uint32 CommandCount;
        uint32 SizeOfCommands;
        uint32 Flags;
        uint32 Reserved;
    };

    struct FLoadCommand
    {
        uint32 Command;
        uint32 CommandSize;
    };

    struct FSegmentCommand64
    {
        uint32 Command;
        uint32 CommandSize;
        char SegmentName[16];
        uint64 VirtualAddress;
        uint64 VirtualSize;
        uint64 FileOffset;
        uint64 FileSize;
        uint32 MaximumProtection;
        uint32 InitialProtection;
        uint32 SectionCount;
        uint32 Flags;
    };

    struct FSection64
    {
        char SectionName[16];
        char SegmentName[16];
        uint64 Address;
        uint64 Size;
        uint32 Offset;
        uint32 Alignment;
        uint32 RelocationOffset;
        uint32 RelocationCount;
        uint32 Flags;
        uint32 Reserved1;
        uint32 Reserved2;
        uint32 Reserved3;
    };

    std::string ReadFixedName(const char* Name, size_t Capacity)
    {
        return std::string(Name, ::strnlen(Name, Capacity));
    }
}

bool FImageSection::IsExecutable() const
{
    return bExecutable;
}

bool FImageSection::IsReadOnlyData() const
{
    return bReadable && !bExecutable;
}

bool FModuleImage::ParseMachObject()
{
    if (Bytes.size() < sizeof(MachObject::FHeader64))
    {
        return false;
    }

    MachObject::FHeader64 Header = {};
    std::memcpy(&Header, Bytes.data(), sizeof(Header));

    if (Header.Magic != MachObject::Magic64)
    {
        return false;
    }

    size_t CommandOffset = sizeof(MachObject::FHeader64);

    for (uint32 CommandIndex = 0; CommandIndex < Header.CommandCount; ++CommandIndex)
    {
        if (CommandOffset + sizeof(MachObject::FLoadCommand) > Bytes.size())
        {
            break;
        }

        MachObject::FLoadCommand Command = {};
        std::memcpy(&Command, Bytes.data() + CommandOffset, sizeof(Command));

        if (Command.CommandSize == 0 || CommandOffset + Command.CommandSize > Bytes.size())
        {
            break;
        }

        if (Command.Command == MachObject::LoadCommandSegment64)
        {
            MachObject::FSegmentCommand64 Segment = {};
            std::memcpy(&Segment, Bytes.data() + CommandOffset, sizeof(Segment));

            size_t SectionOffset = CommandOffset + sizeof(MachObject::FSegmentCommand64);

            for (uint32 SectionIndex = 0; SectionIndex < Segment.SectionCount; ++SectionIndex)
            {
                if (SectionOffset + sizeof(MachObject::FSection64) > Bytes.size())
                {
                    break;
                }

                MachObject::FSection64 SectionHeader = {};
                std::memcpy(&SectionHeader, Bytes.data() + SectionOffset, sizeof(SectionHeader));

                FImageSection Section;
                Section.Name = MachObject::ReadFixedName(SectionHeader.SegmentName, sizeof(SectionHeader.SegmentName)) + "," +
                    MachObject::ReadFixedName(SectionHeader.SectionName, sizeof(SectionHeader.SectionName));
                Section.VirtualAddress = SectionHeader.Address + SlideOffset;
                Section.VirtualSize = static_cast<uint32>(SectionHeader.Size);

                const bool bInstructionSection =
                    (SectionHeader.Flags & (MachObject::SectionAttributePureInstructions | MachObject::SectionAttributeSomeInstructions)) != 0;

                Section.bExecutable = bInstructionSection || (Segment.InitialProtection & MachObject::ProtectionExecute) != 0;
                Section.bReadable = (Segment.InitialProtection & MachObject::ProtectionRead) != 0;
                Section.bWritable = (Segment.InitialProtection & MachObject::ProtectionWrite) != 0;

                if (Section.VirtualSize > 0)
                {
                    Sections.push_back(std::move(Section));
                }

                SectionOffset += sizeof(MachObject::FSection64);
            }
        }

        CommandOffset += Command.CommandSize;
    }

    return !Sections.empty();
}

#if FORT_PLATFORM_WINDOWS

bool FModuleImage::ParsePortableExecutable()
{
    if (Bytes.size() < sizeof(IMAGE_DOS_HEADER))
    {
        return false;
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
        Section.Name.assign(reinterpret_cast<const char*>(Header.Name), ::strnlen(reinterpret_cast<const char*>(Header.Name), IMAGE_SIZEOF_SHORT_NAME));
        Section.VirtualAddress = BaseAddress + Header.VirtualAddress;
        Section.VirtualSize = Header.Misc.VirtualSize != 0 ? Header.Misc.VirtualSize : Header.SizeOfRawData;
        Section.bExecutable = (Header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        Section.bReadable = (Header.Characteristics & IMAGE_SCN_MEM_READ) != 0;
        Section.bWritable = (Header.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;

        Sections.push_back(std::move(Section));
    }

    return !Sections.empty();
}

#else

bool FModuleImage::ParsePortableExecutable()
{
    return false;
}

#endif

bool FModuleImage::Load(const FRemoteMemory& Memory, const FRemoteModuleInfo& ModuleInfo)
{
    BaseAddress = ModuleInfo.BaseAddress;
    ImageSize = static_cast<uint32>(ModuleInfo.ImageSize);
    SlideOffset = ModuleInfo.SlideOffset;
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

    if (ParsePortableExecutable())
    {
        return true;
    }

    return ParseMachObject();
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
