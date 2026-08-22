#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/ModuleImage.h"

struct FSignaturePattern
{
    std::vector<int16> Elements;

    static FSignaturePattern FromIdaStyle(std::string_view Signature);

    static FSignaturePattern FromBytes(const std::vector<uint8>& Bytes);

    bool IsValid() const;

    size_t Length() const;
};

class FSignatureScanner
{
public:
    explicit FSignatureScanner(const FModuleImage& InImage);

    FRemoteAddress FindPattern(std::string_view Signature, bool bExecutableOnly = true) const;

    std::vector<FRemoteAddress> FindAllPatterns(std::string_view Signature, bool bExecutableOnly = true, size_t Limit = 64) const;

    FRemoteAddress FindFirstAvailable(const std::vector<std::string_view>& Signatures, bool bExecutableOnly = true) const;

    FRemoteAddress FindWideStringLiteral(std::wstring_view Literal) const;

    FRemoteAddress FindAnsiStringLiteral(std::string_view Literal) const;

    FRemoteAddress FindWideStringReference(std::wstring_view Literal, size_t Occurrence = 0) const;

    FRemoteAddress FindAnsiStringReference(std::string_view Literal, size_t Occurrence = 0) const;

    FRemoteAddress ScanForSignature(FRemoteAddress Origin, std::string_view Signature, bool bForward, size_t Occurrence, size_t SearchRange) const;

    FRemoteAddress ResolveRelativeOperand(FRemoteAddress InstructionAddress, uint32 OperandOffset, uint32 InstructionLength) const;

    FRemoteAddress ResolveCallTarget(FRemoteAddress CallInstructionAddress) const;

    FRemoteAddress FindFunctionStart(FRemoteAddress InteriorAddress, size_t MaximumBacktrack) const;

private:
    bool MatchesAt(size_t Offset, const FSignaturePattern& Pattern) const;

    FRemoteAddress FindDataAddressOfBytes(const uint8* Data, size_t Size, size_t Alignment) const;

    std::vector<FRemoteAddress> FindReferencesTo(FRemoteAddress TargetAddress) const;

    const FModuleImage& Image;
};
