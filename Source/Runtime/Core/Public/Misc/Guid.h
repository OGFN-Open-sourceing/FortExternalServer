#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>

struct FGuid
{
    uint32 A = 0;
    uint32 B = 0;
    uint32 C = 0;
    uint32 D = 0;

    constexpr bool IsValid() const
    {
        return (A | B | C | D) != 0;
    }

    constexpr bool operator==(const FGuid& Other) const
    {
        return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
    }

    constexpr bool operator!=(const FGuid& Other) const
    {
        return !(*this == Other);
    }

    std::string ToString() const;

    static FGuid NewGuid();
};
