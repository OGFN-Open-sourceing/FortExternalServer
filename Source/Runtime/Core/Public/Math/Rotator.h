#pragma once

#include "Runtime/Core/Public/CoreTypes.h"
#include "Runtime/Core/Public/Math/Vector.h"

struct FRotator
{
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;

    constexpr FRotator() = default;

    constexpr FRotator(float InPitch, float InYaw, float InRoll)
        : Pitch(InPitch)
        , Yaw(InYaw)
        , Roll(InRoll)
    {
    }

    FVector ToDirection() const;

    FQuat ToQuat() const;

    static FRotator FromDirection(const FVector& Direction);
};
