#pragma once

#include "FortniteGame/Public/Athena/FortGameStateAthena.h"
#include "Runtime/Core/Public/Math/RandomStream.h"

struct FSafeZonePhaseDefinition
{
    float WaitSeconds = 0.0f;
    float ShrinkSeconds = 0.0f;
    float Radius = 0.0f;
    float DamagePerSecond = 0.0f;
};

class FFortSafeZoneDirector
{
public:
    void Initialize(const AFortGameStateAthena& InGameState, const FVector& InMapCenter);

    void Reset();

    bool IsActive() const;

    void Start(float CurrentTimeSeconds);

    void Tick(float CurrentTimeSeconds);

    int32 GetCurrentPhase() const;

    float GetCurrentRadius() const;

    FVector GetCurrentCenter() const;

    bool HasFinished() const;

    static std::vector<FSafeZonePhaseDefinition> GetChapterOneSeasonThreePhases();

private:
    void AdvanceToNextPhase(float CurrentTimeSeconds);

    void ApplyToGameState() const;

    AFortGameStateAthena GameState;
    std::vector<FSafeZonePhaseDefinition> Phases;
    FRandomStream RandomStream;

    FVector MapCenter;
    FVector CurrentCenter;
    FVector TargetCenter;

    float CurrentRadius = 0.0f;
    float TargetRadius = 0.0f;
    float PhaseStartTime = 0.0f;
    float PhaseShrinkStartTime = 0.0f;
    float PhaseEndTime = 0.0f;

    int32 CurrentPhase = 0;
    bool bActive = false;
};
