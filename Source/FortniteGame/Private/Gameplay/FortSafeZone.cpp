#include "FortniteGame/Public/Gameplay/FortSafeZone.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

#include <algorithm>

void FFortSafeZoneDirector::Initialize(const AFortGameStateAthena& InGameState, const FVector& InMapCenter)
{
    GameState = InGameState;
    MapCenter = InMapCenter;
    Phases = GetProfilePhases();

    Reset();
}

void FFortSafeZoneDirector::Reset()
{
    CurrentPhase = 0;
    bActive = false;

    CurrentCenter = MapCenter;
    TargetCenter = MapCenter;

    CurrentRadius = Phases.empty() ? 0.0f : Phases.front().Radius;
    TargetRadius = CurrentRadius;

    PhaseStartTime = 0.0f;
    PhaseShrinkStartTime = 0.0f;
    PhaseEndTime = 0.0f;
}

bool FFortSafeZoneDirector::IsActive() const
{
    return bActive;
}

int32 FFortSafeZoneDirector::GetCurrentPhase() const
{
    return CurrentPhase;
}

float FFortSafeZoneDirector::GetCurrentRadius() const
{
    return CurrentRadius;
}

FVector FFortSafeZoneDirector::GetCurrentCenter() const
{
    return CurrentCenter;
}

bool FFortSafeZoneDirector::HasFinished() const
{
    return CurrentPhase >= static_cast<int32>(Phases.size());
}

std::vector<FSafeZonePhaseDefinition> FFortSafeZoneDirector::GetProfilePhases()
{
    return GetActiveBuildProfile().SafeZonePhases;
}

void FFortSafeZoneDirector::Start(float CurrentTimeSeconds)
{
    if (Phases.empty())
    {
        return;
    }

    bActive = true;
    CurrentPhase = 0;
    CurrentCenter = MapCenter;
    CurrentRadius = Phases.front().Radius;

    AdvanceToNextPhase(CurrentTimeSeconds);
    UE_LOG_DISPLAY("SafeZone", "Storm cycle started");
}

void FFortSafeZoneDirector::AdvanceToNextPhase(float CurrentTimeSeconds)
{
    if (CurrentPhase >= static_cast<int32>(Phases.size()))
    {
        bActive = false;
        return;
    }

    const FSafeZonePhaseDefinition& Phase = Phases[static_cast<size_t>(CurrentPhase)];

    PhaseStartTime = CurrentTimeSeconds;
    PhaseShrinkStartTime = CurrentTimeSeconds + Phase.WaitSeconds;
    PhaseEndTime = PhaseShrinkStartTime + Phase.ShrinkSeconds;

    TargetRadius = Phase.Radius;

    const float MaximumDrift = std::max(CurrentRadius - TargetRadius, 0.0f);
    TargetCenter = RandomStream.RandomPointInCircle(CurrentCenter, MaximumDrift);
    TargetCenter.Z = CurrentCenter.Z;

    if (GameState)
    {
        GameState.SetSafeZonePhase(static_cast<uint8>(CurrentPhase + 1));
        GameState.SetSafeZoneStartTime(PhaseShrinkStartTime);
    }

    ApplyToGameState();

    UE_LOG_DISPLAY("SafeZone", "Phase " + std::to_string(CurrentPhase + 1) + " target radius " + std::to_string(static_cast<int32>(TargetRadius)));
}

void FFortSafeZoneDirector::Tick(float CurrentTimeSeconds)
{
    if (!bActive || Phases.empty())
    {
        return;
    }

    if (CurrentTimeSeconds < PhaseShrinkStartTime)
    {
        return;
    }

    if (CurrentTimeSeconds >= PhaseEndTime)
    {
        CurrentRadius = TargetRadius;
        CurrentCenter = TargetCenter;

        ++CurrentPhase;
        AdvanceToNextPhase(CurrentTimeSeconds);
        return;
    }

    const float ShrinkDuration = std::max(PhaseEndTime - PhaseShrinkStartTime, 0.001f);
    const float Alpha = std::clamp((CurrentTimeSeconds - PhaseShrinkStartTime) / ShrinkDuration, 0.0f, 1.0f);

    const FSafeZonePhaseDefinition& Phase = Phases[static_cast<size_t>(std::min<int32>(CurrentPhase, static_cast<int32>(Phases.size()) - 1))];
    const float StartRadius = CurrentPhase == 0 ? Phases.front().Radius : Phases[static_cast<size_t>(CurrentPhase - 1)].Radius;

    CurrentRadius = StartRadius + (Phase.Radius - StartRadius) * Alpha;
    CurrentCenter = CurrentCenter + (TargetCenter - CurrentCenter) * Alpha;

    ApplyToGameState();
}

void FFortSafeZoneDirector::ApplyToGameState() const
{
    if (!GameState)
    {
        return;
    }

    AFortGameStateAthena MutableGameState = GameState;
    MutableGameState.SetSafeZoneCenter(CurrentCenter);
    MutableGameState.SetSafeZoneRadius(CurrentRadius);
    MutableGameState.ForceNetUpdate();
}
