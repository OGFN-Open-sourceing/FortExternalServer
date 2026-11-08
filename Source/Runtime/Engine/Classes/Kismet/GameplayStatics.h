#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"

enum class ESpawnActorCollisionHandlingMethod : uint8
{
    Undefined,
    AlwaysSpawn,
    AdjustIfPossibleButAlwaysSpawn,
    AdjustIfPossibleButDontSpawnIfColliding,
    DontSpawnIfColliding
};

class UGameplayStatics
{
public:
    static AActor BeginDeferredActorSpawnFromClass(const UObject& WorldContextObject, const FObjectHandle& ActorClass, const FTransform& SpawnTransform,
        ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, const AActor& Owner);

    static AActor FinishSpawningActor(const AActor& Actor, const FTransform& SpawnTransform);

    static AActor SpawnActor(const UObject& WorldContextObject, const FObjectHandle& ActorClass, const FTransform& SpawnTransform,
        ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, const AActor& Owner);

    static float GetTimeSeconds(const UObject& WorldContextObject);

private:
    static FObjectHandle GetDefaultObject(const FUnrealRuntime& Runtime);
};
