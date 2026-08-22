#pragma once

#include "FortniteGame/Public/Gameplay/FortAthenaTypes.h"
#include "Runtime/Engine/Public/Engine/World.h"

class AFortPlayerStateAthena : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

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

    void ForceNetUpdate();
};

class AFortPlayerPawnAthena : public AActor
{
public:
    using AActor::AActor;

    void SetMaxHealth(float Value);

    void SetMaxShield(float Value);

    void SetHealth(float Value);

    void SetShield(float Value);

    float GetHealth() const;

    float GetShield() const;

    void RemoveMinimumClamps();

    void DisableHealthRegeneration();

    void SetCanBeDamaged(bool bValue);

    FObjectHandle GetAbilitySystemComponent() const;

    FObjectHandle GetCharacterMovement() const;

    void SetMovementMode(EMovementMode Mode, uint8 CustomMode);
};

class AFortPlayerControllerAthena : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

    AFortPlayerStateAthena GetPlayerState() const;

    AFortPlayerPawnAthena GetPawn() const;

    void SetPawn(const AFortPlayerPawnAthena& Pawn);

    FObjectHandle GetNetConnection() const;

    FObjectHandle GetWorldInventory() const;

    FObjectHandle GetQuickBars() const;

    bool IsDisconnecting() const;

    void MarkLoadingComplete();

    void Possess(const AFortPlayerPawnAthena& Pawn);

    void SetOverriddenBackpackSize(int32 Value);

    void ServerReturnToMainMenu();

    void ClientTravelToMap(const std::string& Url);

    void RespawnAfterDeath();

    void ActivateQuickBarSlot(EFortQuickBars QuickBar, int32 Slot);

    void ForceUpdateQuickBar(EFortQuickBars QuickBar);

    void HandleWorldInventoryLocalUpdate();

    void SendEndOfMatch(bool bVictory);
};
