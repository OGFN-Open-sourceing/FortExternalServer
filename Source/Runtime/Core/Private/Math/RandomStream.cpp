#include "Runtime/Core/Public/Math/RandomStream.h"
#include "Runtime/Core/Public/HAL/PlatformDetection.h"
#include "Runtime/Core/Public/HAL/PlatformMisc.h"

#include <cmath>

FRandomStream::FRandomStream(uint64 InSeed)
    : State(InSeed != 0 ? InSeed : 0x9E3779B97F4A7C15ull)
{
}

FRandomStream::FRandomStream()
    : FRandomStream(FPlatformMisc::GetTimeMilliseconds() * 0x2545F4914F6CDD1Dull + 0x9E3779B97F4A7C15ull)
{
}

uint32 FRandomStream::NextUInt32()
{
    State ^= State << 13;
    State ^= State >> 7;
    State ^= State << 17;
    return static_cast<uint32>(State >> 32);
}

int32 FRandomStream::RandomRange(int32 Minimum, int32 MaximumExclusive)
{
    if (MaximumExclusive <= Minimum)
    {
        return Minimum;
    }

    const uint32 Span = static_cast<uint32>(MaximumExclusive - Minimum);
    return Minimum + static_cast<int32>(NextUInt32() % Span);
}

float FRandomStream::RandomFloat()
{
    return static_cast<float>(NextUInt32() >> 8) / static_cast<float>(1 << 24);
}

float FRandomStream::RandomFloatRange(float Minimum, float Maximum)
{
    return Minimum + (Maximum - Minimum) * RandomFloat();
}

bool FRandomStream::RandomChance(float Probability)
{
    return RandomFloat() < Probability;
}

FVector FRandomStream::RandomPointInCircle(const FVector& Center, float Radius)
{
    constexpr float TwoPi = 6.28318530717958647692f;

    const float Angle = RandomFloat() * TwoPi;
    const float Distance = std::sqrt(RandomFloat()) * Radius;

    return FVector(Center.X + std::cos(Angle) * Distance, Center.Y + std::sin(Angle) * Distance, Center.Z);
}
