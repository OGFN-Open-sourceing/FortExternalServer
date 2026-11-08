#pragma once

#include "FortniteGame/Public/FortTypes.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class AFortPlayerStateAthena : public AActor
{
public:
    using AActor::AActor;

    std::string GetPlayerName() const;

    int32 GetTeamIndex() const;

    void SetTeamIndex(int32 Value, bool bAssignSquadId);

    void SetSquadId(uint8 Value);

    int32 GetKillScore() const;

    void SetKillScore(int32 Value);

    void SetPlace(int32 Value);

    void SetHasFinishedLoading(bool bValue);

    void SetHasStartedPlaying(bool bValue);

    void SetHeroType(const FObjectHandle& HeroType);

    void SetCharacterPart(EFortCustomPartType PartType, const FObjectHandle& Part);

    void ApplyCharacterParts();

    FObjectHandle GetCurrentPawn() const;

    void SetDeathCause(EDeathCause Cause);
};
