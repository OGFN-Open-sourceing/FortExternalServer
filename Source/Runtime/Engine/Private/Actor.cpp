#include "Runtime/Engine/Classes/GameFramework/Actor.h"

FVector AActor::GetActorLocation() const
{
    struct FGetActorLocationParameters
    {
        FVector ReturnValue;
    } Parameters;

    if (!Object.InvokeFunction("K2_GetActorLocation", &Parameters, sizeof(Parameters)))
    {
        return FVector();
    }

    return Parameters.ReturnValue;
}

FRotator AActor::GetActorRotation() const
{
    struct FGetActorRotationParameters
    {
        FRotator ReturnValue;
    } Parameters;

    if (!Object.InvokeFunction("K2_GetActorRotation", &Parameters, sizeof(Parameters)))
    {
        return FRotator();
    }

    return Parameters.ReturnValue;
}

bool AActor::SetActorLocation(const FVector& Location, bool bSweep)
{
    struct FSetActorLocationParameters
    {
        FVector NewLocation;
        bool bSweep = false;
        uint8 Padding[7] = {};
        uint8 SweepHitResult[0x88] = {};
        bool bTeleport = false;
        uint8 TeleportPadding[7] = {};
        bool ReturnValue = false;
    } Parameters;

    Parameters.NewLocation = Location;
    Parameters.bSweep = bSweep;

    if (!Object.InvokeFunction("K2_SetActorLocation", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return Parameters.ReturnValue;
}

bool AActor::TeleportTo(const FVector& Location, const FRotator& Rotation)
{
    struct FTeleportToParameters
    {
        FVector DestLocation;
        FRotator DestRotation;
        bool ReturnValue = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.DestLocation = Location;
    Parameters.DestRotation = Rotation;

    if (!Object.InvokeFunction("K2_TeleportTo", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return Parameters.ReturnValue;
}

bool AActor::DestroyActor()
{
    struct FDestroyActorParameters
    {
        bool ReturnValue = false;
        uint8 Padding[7] = {};
    } Parameters;

    if (!Object.InvokeFunction("K2_DestroyActor", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return true;
}

void AActor::ForceNetUpdate()
{
    Object.InvokeFunction("ForceNetUpdate");
}

void AActor::SetOwner(const AActor& NewOwner)
{
    Object.SetObjectProperty("Owner", NewOwner.GetObject());
    Object.InvokeFunction("OnRep_Owner");
}

AActor AActor::GetOwner() const
{
    return AActor(Object.GetObjectProperty("Owner"));
}

void AActor::SetReplicates(bool bReplicates)
{
    struct FSetReplicatesParameters
    {
        bool bInReplicates = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bInReplicates = bReplicates;
    Object.InvokeFunction("SetReplicates", &Parameters, sizeof(Parameters));
}

void AActor::SetActorHiddenInGame(bool bHidden)
{
    struct FSetActorHiddenInGameParameters
    {
        bool bNewHidden = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bNewHidden = bHidden;
    Object.InvokeFunction("SetActorHiddenInGame", &Parameters, sizeof(Parameters));
}

void AActor::SetNetCullDistanceSquared(float Value)
{
    Object.SetProperty<float>("NetCullDistanceSquared", Value);
}

void AActor::SetActorTickEnabled(bool bEnabled)
{
    struct FSetActorTickEnabledParameters
    {
        bool bEnabled = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bEnabled = bEnabled;
    Object.InvokeFunction("SetActorTickEnabled", &Parameters, sizeof(Parameters));
}
