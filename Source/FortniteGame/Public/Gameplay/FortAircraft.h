#pragma once

#include "FortniteGame/Public/Athena/FortGameStateAthena.h"
#include "Runtime/Core/Public/Math/RandomStream.h"
#include "Runtime/Engine/Public/Engine/EngineRuntime.h"

class FFortAircraftDirector
{
public:
    void Initialize(FEngineRuntime& InEngineRuntime, const AFortGameStateAthena& InGameState);

    void Reset();

    bool SpawnFlightPath(const FVector& MapCenter, float MapRadius);

    void Start(float FlightDurationSeconds, float CurrentTimeSeconds);

    void Tick(float CurrentTimeSeconds);

    bool IsFlying() const;

    bool HasLanded() const;

    FObjectHandle GetAircraft() const;

    FVector GetFlightStart() const;

    FVector GetFlightEnd() const;

    FVector GetPositionAtTime(float CurrentTimeSeconds) const;

private:
    FEngineRuntime* EngineRuntime = nullptr;
    AFortGameStateAthena GameState;
    FObjectHandle Aircraft;
    FRandomStream RandomStream;

    FVector FlightStart;
    FVector FlightEnd;

    float FlightStartTime = 0.0f;
    float FlightEndTime = 0.0f;
    bool bFlying = false;
};
