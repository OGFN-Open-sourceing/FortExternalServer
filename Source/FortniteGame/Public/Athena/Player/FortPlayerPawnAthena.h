#pragma once

#include "FortniteGame/Public/FortTypes.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

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

    AActor EquipWeaponDefinition(const FObjectHandle& WeaponData, const FGuid& ItemEntryGuid) const;

    FObjectHandle GetCharacterMovement() const;

    void SetMovementMode(EMovementMode Mode, uint8 CustomMode);
};
