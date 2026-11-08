#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

FObjectHandle UGameplayStatics::GetDefaultObject(const FUnrealRuntime& Runtime)
{
    return Runtime.GetClassDefaultObject("GameplayStatics");
}

AActor UGameplayStatics::BeginDeferredActorSpawnFromClass(const UObject& WorldContextObject, const FObjectHandle& ActorClass, const FTransform& SpawnTransform,
    ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, const AActor& Owner)
{
    if (!WorldContextObject || !ActorClass)
    {
        return AActor();
    }

    struct FBeginDeferredActorSpawnFromClassParameters
    {
        FRemoteAddress WorldContextObject = InvalidRemoteAddress;
        FRemoteAddress ActorClass = InvalidRemoteAddress;
        FTransform SpawnTransform;
        ESpawnActorCollisionHandlingMethod CollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined;
        uint8 Padding[7] = {};
        FRemoteAddress Owner = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
        uint8 TrailingPadding[8] = {};
    } Parameters;

    static_assert(sizeof(FBeginDeferredActorSpawnFromClassParameters) == 0x60);

    Parameters.WorldContextObject = WorldContextObject.GetAddress();
    Parameters.ActorClass = ActorClass.GetAddress();
    Parameters.SpawnTransform = SpawnTransform;
    Parameters.CollisionHandlingOverride = CollisionHandlingOverride;
    Parameters.Owner = Owner.GetAddress();

    const FUnrealRuntime& Runtime = WorldContextObject.GetRuntime();
    const FObjectHandle Statics = GetDefaultObject(Runtime);

    if (!Statics || !Statics.InvokeFunction("BeginDeferredActorSpawnFromClass", &Parameters, sizeof(Parameters)))
    {
        return AActor();
    }

    return AActor(Runtime.MakeHandle(Parameters.ReturnValue));
}

AActor UGameplayStatics::FinishSpawningActor(const AActor& Actor, const FTransform& SpawnTransform)
{
    if (!Actor)
    {
        return AActor();
    }

    struct FFinishSpawningActorParameters
    {
        FRemoteAddress Actor = InvalidRemoteAddress;
        uint8 Padding[8] = {};
        FTransform SpawnTransform;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
        uint8 TrailingPadding[8] = {};
    } Parameters;

    static_assert(sizeof(FFinishSpawningActorParameters) == 0x50);

    Parameters.Actor = Actor.GetAddress();
    Parameters.SpawnTransform = SpawnTransform;

    const FUnrealRuntime& Runtime = Actor.GetRuntime();
    const FObjectHandle Statics = GetDefaultObject(Runtime);

    if (!Statics || !Statics.InvokeFunction("FinishSpawningActor", &Parameters, sizeof(Parameters)))
    {
        return AActor();
    }

    return AActor(Runtime.MakeHandle(Parameters.ReturnValue));
}

AActor UGameplayStatics::SpawnActor(const UObject& WorldContextObject, const FObjectHandle& ActorClass, const FTransform& SpawnTransform,
    ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, const AActor& Owner)
{
    const AActor Deferred = BeginDeferredActorSpawnFromClass(WorldContextObject, ActorClass, SpawnTransform, CollisionHandlingOverride, Owner);
    if (!Deferred)
    {
        return AActor();
    }

    return FinishSpawningActor(Deferred, SpawnTransform);
}

float UGameplayStatics::GetTimeSeconds(const UObject& WorldContextObject)
{
    if (!WorldContextObject)
    {
        return 0.0f;
    }

    struct FGetTimeSecondsParameters
    {
        FRemoteAddress WorldContextObject = InvalidRemoteAddress;
        float ReturnValue = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Parameters.WorldContextObject = WorldContextObject.GetAddress();

    const FObjectHandle Statics = GetDefaultObject(WorldContextObject.GetRuntime());
    if (!Statics || !Statics.InvokeFunction("GetTimeSeconds", &Parameters, sizeof(Parameters)))
    {
        return 0.0f;
    }

    return Parameters.ReturnValue;
}
