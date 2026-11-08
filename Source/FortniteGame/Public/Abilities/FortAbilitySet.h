#pragma once

#include "FortniteGame/Public/Athena/Player/FortPlayerPawnAthena.h"
#include "Runtime/Engine/Public/EngineRuntime.h"

class UFortAbilitySet : public UObject
{
public:
    using UObject::UObject;

    int32 GiveToAbilitySystem(const FEngineRuntime& EngineRuntime, const FObjectHandle& AbilitySystemComponent) const;

    static bool GiveAbility(const FEngineRuntime& EngineRuntime, const FObjectHandle& AbilitySystemComponent, const FObjectHandle& AbilityClass,
        const FObjectHandle& SourceObject);
};
