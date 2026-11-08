#include "FortniteGame/Public/Abilities/FortAbilitySet.h"

namespace
{
    constexpr size_t GameplayAbilitySpecSize = 0xC8;
    constexpr size_t GameplayAbilitySpecHandleSize = 0x4;
    constexpr int32 DefaultAbilityLevel = 1;
}

bool UFortAbilitySet::GiveAbility(const FEngineRuntime& EngineRuntime, const FObjectHandle& AbilitySystemComponent, const FObjectHandle& AbilityClass,
    const FObjectHandle& SourceObject)
{
    const FEngineFunctions& Functions = EngineRuntime.GetFunctions();

    if (!AbilitySystemComponent || !AbilityClass || Functions.AbilitySystemGiveAbility == InvalidRemoteAddress ||
        Functions.GameplayAbilitySpecConstructor == InvalidRemoteAddress)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime.GetUnrealRuntime();

    const FObjectHandle Ability = Runtime.GetDefaultObject(AbilityClass);
    if (!Ability)
    {
        return false;
    }

    const FRemoteAddress Spec = Runtime.AcquireScratch(GameplayAbilitySpecSize);
    const FRemoteAddress Handle = Runtime.AcquireScratch(GameplayAbilitySpecHandleSize);

    if (Spec == InvalidRemoteAddress || Handle == InvalidRemoteAddress)
    {
        return false;
    }

    FGameThreadBridge& Bridge = Runtime.GetBridge();

    Bridge.CallFunction(Functions.GameplayAbilitySpecConstructor,
        { Spec, Ability.GetAddress(), static_cast<uint64>(DefaultAbilityLevel), static_cast<uint64>(static_cast<uint32>(InvalidIndex)), SourceObject.GetAddress() });

    Bridge.CallFunction(Functions.AbilitySystemGiveAbility, { AbilitySystemComponent.GetAddress(), Handle, Spec });
    return true;
}

int32 UFortAbilitySet::GiveToAbilitySystem(const FEngineRuntime& EngineRuntime, const FObjectHandle& AbilitySystemComponent) const
{
    if (!IsValid() || !AbilitySystemComponent)
    {
        return 0;
    }

    const FScriptArrayView GameplayAbilities = Object.GetArrayProperty("GameplayAbilities");

    int32 GrantedCount = 0;

    for (int32 Index = 0; Index < GameplayAbilities.ArrayNum; ++Index)
    {
        if (GiveAbility(EngineRuntime, AbilitySystemComponent, Object.GetArrayElementAsObject("GameplayAbilities", Index), FObjectHandle()))
        {
            ++GrantedCount;
        }
    }

    return GrantedCount;
}
