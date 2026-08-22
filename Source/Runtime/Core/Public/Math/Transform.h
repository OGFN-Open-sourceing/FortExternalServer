#pragma once

#include "Runtime/Core/Public/Math/Rotator.h"
#include "Runtime/Core/Public/Math/Vector.h"

struct FTransform
{
    FQuat Rotation = FQuat();
    FVector Translation = FVector();
    float TranslationPadding = 0.0f;
    FVector Scale3D = FVector(1.0f, 1.0f, 1.0f);
    float ScalePadding = 0.0f;

    static FTransform FromLocation(const FVector& Location)
    {
        FTransform Result;
        Result.Translation = Location;
        return Result;
    }

    static FTransform FromLocationAndRotation(const FVector& Location, const FRotator& Rotator)
    {
        FTransform Result;
        Result.Translation = Location;
        Result.Rotation = Rotator.ToQuat();
        return Result;
    }
};
