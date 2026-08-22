#include "FortniteGame/Public/Gameplay/FortAircraft.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float TwoPi = 6.28318530717958647692f;
}

void FFortAircraftDirector::Initialize(FEngineRuntime& InEngineRuntime, const AFortGameStateAthena& InGameState)
{
    EngineRuntime = &InEngineRuntime;
    GameState = InGameState;
    Reset();
}

void FFortAircraftDirector::Reset()
{
    Aircraft = FObjectHandle();
    FlightStart = FVector();
    FlightEnd = FVector();
    FlightStartTime = 0.0f;
    FlightEndTime = 0.0f;
    bFlying = false;
}

bool FFortAircraftDirector::SpawnFlightPath(const FVector& MapCenter, float MapRadius)
{
    if (EngineRuntime == nullptr)
    {
        return false;
    }

    const float Angle = RandomStream.RandomFloat() * TwoPi;
    const float Offset = RandomStream.RandomFloatRange(-MapRadius * 0.25f, MapRadius * 0.25f);

    const FVector Direction(std::cos(Angle), std::sin(Angle), 0.0f);
    const FVector Perpendicular(-Direction.Y, Direction.X, 0.0f);

    const FVector PathCenter = MapCenter + Perpendicular * Offset;

    FlightStart = PathCenter - Direction * MapRadius;
    FlightEnd = PathCenter + Direction * MapRadius;

    FlightStart.Z = GetActiveBuildProfile().AircraftAltitude;
    FlightEnd.Z = GetActiveBuildProfile().AircraftAltitude;

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return false;
    }

    const std::vector<FObjectHandle> ExistingAircraft = World.GetAllActorsOfClass(EngineRuntime->GetUnrealRuntime().FindClass(GetActiveBuildProfile().AssetPaths.AircraftClass));

    if (!ExistingAircraft.empty())
    {
        Aircraft = ExistingAircraft.front();

        AActor AircraftActor(Aircraft);
        AircraftActor.SetActorLocation(FlightStart);

        UE_LOG_DISPLAY("Aircraft", "Using the existing battle bus actor for the flight path");
        return true;
    }

    UE_LOG_WARNING("Aircraft", "No battle bus actor is present, players will be dropped from the flight path directly");
    return true;
}

void FFortAircraftDirector::Start(float FlightDurationSeconds, float CurrentTimeSeconds)
{
    FlightStartTime = CurrentTimeSeconds;
    FlightEndTime = CurrentTimeSeconds + std::max(FlightDurationSeconds, 1.0f);
    bFlying = true;

    if (GameState)
    {
        AFortGameStateAthena MutableGameState = GameState;
        MutableGameState.SetAircraftStartTime(FlightStartTime);
        MutableGameState.SetSkipAircraft(false);
        MutableGameState.ForceNetUpdate();
    }

    UE_LOG_DISPLAY("Aircraft", "Battle bus launched for " + std::to_string(static_cast<int32>(FlightDurationSeconds)) + " seconds");
}

void FFortAircraftDirector::Tick(float CurrentTimeSeconds)
{
    if (!bFlying)
    {
        return;
    }

    if (CurrentTimeSeconds >= FlightEndTime)
    {
        bFlying = false;
        UE_LOG_DISPLAY("Aircraft", "Battle bus flight finished");
        return;
    }

    if (!Aircraft)
    {
        return;
    }

    AActor AircraftActor(Aircraft);
    AircraftActor.SetActorLocation(GetPositionAtTime(CurrentTimeSeconds));
}

bool FFortAircraftDirector::IsFlying() const
{
    return bFlying;
}

bool FFortAircraftDirector::HasLanded() const
{
    return !bFlying && FlightEndTime > 0.0f;
}

FObjectHandle FFortAircraftDirector::GetAircraft() const
{
    return Aircraft;
}

FVector FFortAircraftDirector::GetFlightStart() const
{
    return FlightStart;
}

FVector FFortAircraftDirector::GetFlightEnd() const
{
    return FlightEnd;
}

FVector FFortAircraftDirector::GetPositionAtTime(float CurrentTimeSeconds) const
{
    const float Duration = std::max(FlightEndTime - FlightStartTime, 0.001f);
    const float Alpha = std::clamp((CurrentTimeSeconds - FlightStartTime) / Duration, 0.0f, 1.0f);

    return FlightStart + (FlightEnd - FlightStart) * Alpha;
}
