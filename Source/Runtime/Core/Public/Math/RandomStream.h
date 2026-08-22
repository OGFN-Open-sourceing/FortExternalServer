#pragma once

#include "Runtime/Core/Public/CoreTypes.h"
#include "Runtime/Core/Public/Math/Vector.h"

#include <vector>

class FRandomStream
{
public:
    explicit FRandomStream(uint64 InSeed);

    FRandomStream();

    uint32 NextUInt32();

    int32 RandomRange(int32 Minimum, int32 MaximumExclusive);

    float RandomFloat();

    float RandomFloatRange(float Minimum, float Maximum);

    bool RandomChance(float Probability);

    FVector RandomPointInCircle(const FVector& Center, float Radius);

    template<typename ElementType>
    const ElementType& PickRandom(const std::vector<ElementType>& Elements)
    {
        return Elements[static_cast<size_t>(RandomRange(0, static_cast<int32>(Elements.size())))];
    }

private:
    uint64 State;
};
