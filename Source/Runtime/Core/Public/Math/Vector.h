#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <cmath>

struct FVector
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;

    constexpr FVector() = default;

    constexpr FVector(float InX, float InY, float InZ)
        : X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    constexpr FVector operator+(const FVector& Other) const
    {
        return FVector(X + Other.X, Y + Other.Y, Z + Other.Z);
    }

    constexpr FVector operator-(const FVector& Other) const
    {
        return FVector(X - Other.X, Y - Other.Y, Z - Other.Z);
    }

    constexpr FVector operator*(float Scale) const
    {
        return FVector(X * Scale, Y * Scale, Z * Scale);
    }

    constexpr bool operator==(const FVector& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    float SizeSquared() const
    {
        return X * X + Y * Y + Z * Z;
    }

    float Size() const
    {
        return std::sqrt(SizeSquared());
    }

    float Size2D() const
    {
        return std::sqrt(X * X + Y * Y);
    }

    FVector GetSafeNormal() const
    {
        const float Length = Size();
        if (Length <= 1.0e-6f)
        {
            return FVector();
        }

        return FVector(X / Length, Y / Length, Z / Length);
    }

    static float Distance(const FVector& From, const FVector& To)
    {
        return (To - From).Size();
    }

    static float Distance2D(const FVector& From, const FVector& To)
    {
        return (To - From).Size2D();
    }
};

struct FVector2D
{
    float X = 0.0f;
    float Y = 0.0f;
};

struct FQuat
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;
};
