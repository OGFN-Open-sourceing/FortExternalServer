#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FDecodedInstruction
{
    uint32 Length = 0;
    bool bValid = false;
    bool bHasRipRelativeOperand = false;
    uint32 RipDisplacementOffset = 0;
    bool bHasRelativeBranch = false;
    uint32 RelativeOperandOffset = 0;
    uint32 RelativeOperandSize = 0;
    bool bIsReturn = false;
};

class FInstructionDecoder
{
public:
    static FDecodedInstruction Decode(const uint8* Code, size_t AvailableBytes);

    static uint32 MeasurePrologue(const uint8* Code, size_t AvailableBytes, uint32 MinimumLength);
};
