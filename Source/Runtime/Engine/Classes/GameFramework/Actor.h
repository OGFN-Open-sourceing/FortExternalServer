#pragma once

#include "Runtime/CoreUObject/Public/UObject/Object.h"

class AActor : public UObject
{
public:
    using UObject::UObject;

    FVector GetActorLocation() const;

    FRotator GetActorRotation() const;

    bool SetActorLocation(const FVector& Location, bool bSweep = false);

    bool TeleportTo(const FVector& Location, const FRotator& Rotation);

    bool DestroyActor();

    void ForceNetUpdate();

    void SetOwner(const AActor& NewOwner);

    AActor GetOwner() const;

    void SetReplicates(bool bReplicates);

    void SetActorHiddenInGame(bool bHidden);

    void SetNetCullDistanceSquared(float Value);

    void SetActorTickEnabled(bool bEnabled);
};
