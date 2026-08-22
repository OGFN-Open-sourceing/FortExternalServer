#include "Runtime/Core/Public/Math/Rotator.h"

#include <cmath>

namespace
{
    constexpr float PiValue = 3.14159265358979323846f;

    constexpr float DegreesToRadians(float Degrees)
    {
        return Degrees * (PiValue / 180.0f);
    }

    constexpr float RadiansToDegrees(float Radians)
    {
        return Radians * (180.0f / PiValue);
    }
}

FVector FRotator::ToDirection() const
{
    const float PitchRadians = DegreesToRadians(Pitch);
    const float YawRadians = DegreesToRadians(Yaw);

    const float CosPitch = std::cos(PitchRadians);
    const float SinPitch = std::sin(PitchRadians);
    const float CosYaw = std::cos(YawRadians);
    const float SinYaw = std::sin(YawRadians);

    return FVector(CosPitch * CosYaw, CosPitch * SinYaw, SinPitch);
}

FQuat FRotator::ToQuat() const
{
    const float HalfPitch = DegreesToRadians(Pitch) * 0.5f;
    const float HalfYaw = DegreesToRadians(Yaw) * 0.5f;
    const float HalfRoll = DegreesToRadians(Roll) * 0.5f;

    const float SinPitch = std::sin(HalfPitch);
    const float CosPitch = std::cos(HalfPitch);
    const float SinYaw = std::sin(HalfYaw);
    const float CosYaw = std::cos(HalfYaw);
    const float SinRoll = std::sin(HalfRoll);
    const float CosRoll = std::cos(HalfRoll);

    FQuat Result;
    Result.X = CosRoll * SinPitch * SinYaw - SinRoll * CosPitch * CosYaw;
    Result.Y = -CosRoll * SinPitch * CosYaw - SinRoll * CosPitch * SinYaw;
    Result.Z = CosRoll * CosPitch * SinYaw - SinRoll * SinPitch * CosYaw;
    Result.W = CosRoll * CosPitch * CosYaw + SinRoll * SinPitch * SinYaw;
    return Result;
}

FRotator FRotator::FromDirection(const FVector& Direction)
{
    const FVector Normalized = Direction.GetSafeNormal();

    FRotator Result;
    Result.Yaw = RadiansToDegrees(std::atan2(Normalized.Y, Normalized.X));
    Result.Pitch = RadiansToDegrees(std::atan2(Normalized.Z, Normalized.Size2D()));
    Result.Roll = 0.0f;
    return Result;
}
