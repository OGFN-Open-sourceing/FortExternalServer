#include "Runtime/RemoteProcess/Public/SignatureScanner.h"

#include <cstring>

namespace
{
    int16 ParseHexPair(char High, char Low)
    {
        const auto ToNibble = [](char Character) -> int16 {
            if (Character >= '0' && Character <= '9')
            {
                return static_cast<int16>(Character - '0');
            }

            if (Character >= 'a' && Character <= 'f')
            {
                return static_cast<int16>(Character - 'a' + 10);
            }

            if (Character >= 'A' && Character <= 'F')
            {
                return static_cast<int16>(Character - 'A' + 10);
            }

            return -1;
        };

        const int16 HighNibble = ToNibble(High);
        const int16 LowNibble = ToNibble(Low);

        if (HighNibble < 0 || LowNibble < 0)
        {
            return -1;
        }

        return static_cast<int16>((HighNibble << 4) | LowNibble);
    }
}

FSignaturePattern FSignaturePattern::FromIdaStyle(std::string_view Signature)
{
    FSignaturePattern Pattern;
    Pattern.Elements.reserve(Signature.size() / 3 + 1);

    size_t Index = 0;
    while (Index < Signature.size())
    {
        const char Character = Signature[Index];

        if (Character == ' ')
        {
            ++Index;
            continue;
        }

        if (Character == '?')
        {
            Pattern.Elements.push_back(-1);
            ++Index;

            if (Index < Signature.size() && Signature[Index] == '?')
            {
                ++Index;
            }

            continue;
        }

        if (Index + 1 >= Signature.size())
        {
            break;
        }

        Pattern.Elements.push_back(ParseHexPair(Character, Signature[Index + 1]));
        Index += 2;
    }

    return Pattern;
}

FSignaturePattern FSignaturePattern::FromBytes(const std::vector<uint8>& Bytes)
{
    FSignaturePattern Pattern;
    Pattern.Elements.reserve(Bytes.size());

    for (const uint8 Byte : Bytes)
    {
        Pattern.Elements.push_back(static_cast<int16>(Byte));
    }

    return Pattern;
}

bool FSignaturePattern::IsValid() const
{
    return !Elements.empty();
}

size_t FSignaturePattern::Length() const
{
    return Elements.size();
}

FSignatureScanner::FSignatureScanner(const FModuleImage& InImage)
    : Image(InImage)
{
}

bool FSignatureScanner::MatchesAt(size_t Offset, const FSignaturePattern& Pattern) const
{
    const std::vector<uint8>& Bytes = Image.GetBytes();

    for (size_t Index = 0; Index < Pattern.Elements.size(); ++Index)
    {
        const int16 Element = Pattern.Elements[Index];
        if (Element < 0)
        {
            continue;
        }

        if (Bytes[Offset + Index] != static_cast<uint8>(Element))
        {
            return false;
        }
    }

    return true;
}

FRemoteAddress FSignatureScanner::FindPattern(std::string_view Signature, bool bExecutableOnly) const
{
    const std::vector<FRemoteAddress> Results = FindAllPatterns(Signature, bExecutableOnly, 1);
    return Results.empty() ? InvalidRemoteAddress : Results.front();
}

std::vector<FRemoteAddress> FSignatureScanner::FindAllPatterns(std::string_view Signature, bool bExecutableOnly, size_t Limit) const
{
    std::vector<FRemoteAddress> Results;

    const FSignaturePattern Pattern = FSignaturePattern::FromIdaStyle(Signature);
    if (!Pattern.IsValid() || !Image.IsLoaded())
    {
        return Results;
    }

    const std::vector<uint8>& Bytes = Image.GetBytes();

    for (const FImageSection& Section : Image.GetSections())
    {
        if (bExecutableOnly && !Section.IsExecutable())
        {
            continue;
        }

        const size_t Start = Image.AddressToOffset(Section.VirtualAddress);
        const size_t End = std::min<size_t>(Start + Section.VirtualSize, Bytes.size());

        if (End <= Start || End - Start < Pattern.Length())
        {
            continue;
        }

        const size_t Last = End - Pattern.Length();
        const int16 FirstElement = Pattern.Elements.front();

        for (size_t Offset = Start; Offset <= Last; ++Offset)
        {
            if (FirstElement >= 0 && Bytes[Offset] != static_cast<uint8>(FirstElement))
            {
                continue;
            }

            if (!MatchesAt(Offset, Pattern))
            {
                continue;
            }

            Results.push_back(Image.OffsetToAddress(Offset));
            if (Results.size() >= Limit)
            {
                return Results;
            }
        }
    }

    return Results;
}

FRemoteAddress FSignatureScanner::FindFirstAvailable(const std::vector<std::string_view>& Signatures, bool bExecutableOnly) const
{
    for (const std::string_view Signature : Signatures)
    {
        const FRemoteAddress Found = FindPattern(Signature, bExecutableOnly);
        if (Found != InvalidRemoteAddress)
        {
            return Found;
        }
    }

    return InvalidRemoteAddress;
}

FRemoteAddress FSignatureScanner::FindDataAddressOfBytes(const uint8* Data, size_t Size, size_t Alignment) const
{
    const std::vector<uint8>& Bytes = Image.GetBytes();

    for (const FImageSection& Section : Image.GetSections())
    {
        if (!Section.IsReadOnlyData())
        {
            continue;
        }

        const size_t Start = Image.AddressToOffset(Section.VirtualAddress);
        const size_t End = std::min<size_t>(Start + Section.VirtualSize, Bytes.size());

        if (End <= Start || End - Start < Size)
        {
            continue;
        }

        for (size_t Offset = AlignUp<size_t>(Start, Alignment); Offset + Size <= End; Offset += Alignment)
        {
            if (std::memcmp(Bytes.data() + Offset, Data, Size) == 0)
            {
                return Image.OffsetToAddress(Offset);
            }
        }
    }

    return InvalidRemoteAddress;
}

FRemoteAddress FSignatureScanner::FindWideStringLiteral(std::wstring_view Literal) const
{
    std::vector<uint8> Needle((Literal.size() + 1) * sizeof(wchar_t));
    std::memcpy(Needle.data(), Literal.data(), Literal.size() * sizeof(wchar_t));

    return FindDataAddressOfBytes(Needle.data(), Needle.size(), 2);
}

FRemoteAddress FSignatureScanner::FindAnsiStringLiteral(std::string_view Literal) const
{
    std::vector<uint8> Needle(Literal.size() + 1);
    std::memcpy(Needle.data(), Literal.data(), Literal.size());

    return FindDataAddressOfBytes(Needle.data(), Needle.size(), 1);
}

std::vector<FRemoteAddress> FSignatureScanner::FindReferencesTo(FRemoteAddress TargetAddress) const
{
    std::vector<FRemoteAddress> Results;

    if (TargetAddress == InvalidRemoteAddress)
    {
        return Results;
    }

    const std::vector<uint8>& Bytes = Image.GetBytes();

    for (const FImageSection& Section : Image.GetSections())
    {
        if (!Section.IsExecutable())
        {
            continue;
        }

        const size_t Start = Image.AddressToOffset(Section.VirtualAddress);
        const size_t End = std::min<size_t>(Start + Section.VirtualSize, Bytes.size());

        if (End <= Start + sizeof(int32))
        {
            continue;
        }

        for (size_t Offset = Start; Offset + sizeof(int32) <= End; ++Offset)
        {
            int32 Displacement = 0;
            std::memcpy(&Displacement, Bytes.data() + Offset, sizeof(Displacement));

            const FRemoteAddress Candidate = Image.OffsetToAddress(Offset) + sizeof(int32) + static_cast<int64>(Displacement);
            if (Candidate == TargetAddress)
            {
                Results.push_back(Image.OffsetToAddress(Offset));
            }
        }
    }

    return Results;
}

FRemoteAddress FSignatureScanner::FindWideStringReference(std::wstring_view Literal, size_t Occurrence) const
{
    const FRemoteAddress LiteralAddress = FindWideStringLiteral(Literal);
    if (LiteralAddress == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const std::vector<FRemoteAddress> References = FindReferencesTo(LiteralAddress);
    if (Occurrence >= References.size())
    {
        return InvalidRemoteAddress;
    }

    return References[Occurrence];
}

FRemoteAddress FSignatureScanner::FindAnsiStringReference(std::string_view Literal, size_t Occurrence) const
{
    const FRemoteAddress LiteralAddress = FindAnsiStringLiteral(Literal);
    if (LiteralAddress == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const std::vector<FRemoteAddress> References = FindReferencesTo(LiteralAddress);
    if (Occurrence >= References.size())
    {
        return InvalidRemoteAddress;
    }

    return References[Occurrence];
}

FRemoteAddress FSignatureScanner::ScanForSignature(FRemoteAddress Origin, std::string_view Signature, bool bForward, size_t Occurrence, size_t SearchRange) const
{
    const FSignaturePattern Pattern = FSignaturePattern::FromIdaStyle(Signature);
    if (!Pattern.IsValid() || Origin == InvalidRemoteAddress || !Image.ContainsAddress(Origin))
    {
        return InvalidRemoteAddress;
    }

    const size_t OriginOffset = Image.AddressToOffset(Origin);
    const size_t ImageLength = Image.GetBytes().size();

    size_t Matches = 0;

    for (size_t Step = 0; Step < SearchRange; ++Step)
    {
        if (!bForward && Step > OriginOffset)
        {
            break;
        }

        const size_t Offset = bForward ? OriginOffset + Step : OriginOffset - Step;

        if (Offset + Pattern.Length() > ImageLength)
        {
            if (bForward)
            {
                break;
            }

            continue;
        }

        if (!MatchesAt(Offset, Pattern))
        {
            continue;
        }

        if (Matches == Occurrence)
        {
            return Image.OffsetToAddress(Offset);
        }

        ++Matches;
    }

    return InvalidRemoteAddress;
}

FRemoteAddress FSignatureScanner::ResolveRelativeOperand(FRemoteAddress InstructionAddress, uint32 OperandOffset, uint32 InstructionLength) const
{
    const uint8* Local = Image.GetLocalPointer(InstructionAddress + OperandOffset);
    if (Local == nullptr)
    {
        return InvalidRemoteAddress;
    }

    int32 Displacement = 0;
    std::memcpy(&Displacement, Local, sizeof(Displacement));

    return InstructionAddress + InstructionLength + static_cast<int64>(Displacement);
}

FRemoteAddress FSignatureScanner::ResolveCallTarget(FRemoteAddress CallInstructionAddress) const
{
    const uint8* Local = Image.GetLocalPointer(CallInstructionAddress);
    if (Local == nullptr || (*Local != 0xE8 && *Local != 0xE9))
    {
        return InvalidRemoteAddress;
    }

    return ResolveRelativeOperand(CallInstructionAddress, 1, 5);
}

FRemoteAddress FSignatureScanner::FindFunctionStart(FRemoteAddress InteriorAddress, size_t MaximumBacktrack) const
{
    static constexpr std::string_view Prologues[] = { "40 55", "48 8B C4", "4C 8B DC", "48 89 5C 24", "48 89 4C 24", "40 53", "48 83 EC" };

    FRemoteAddress Best = InvalidRemoteAddress;
    size_t BestDistance = MaximumBacktrack + 1;

    for (const std::string_view Prologue : Prologues)
    {
        const FRemoteAddress Found = ScanForSignature(InteriorAddress, Prologue, false, 0, MaximumBacktrack);
        if (Found == InvalidRemoteAddress)
        {
            continue;
        }

        const size_t Distance = static_cast<size_t>(InteriorAddress - Found);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Best = Found;
        }
    }

    return Best;
}
