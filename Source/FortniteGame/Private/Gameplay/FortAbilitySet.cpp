#include "FortniteGame/Public/Gameplay/FortAbilitySet.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

namespace
{
    constexpr int32 GameplayAbilitySpecSize = 0x80;
    constexpr int32 GameplayAbilitySpecAbilityOffset = 0x08;
    constexpr int32 GameplayAbilitySpecLevelOffset = 0x10;
    constexpr int32 GameplayAbilitySpecInputIdOffset = 0x14;
    constexpr int32 GameplayAbilitySpecSourceObjectOffset = 0x18;
}

void FFortAbilityGrantor::Initialize(FEngineRuntime& InEngineRuntime)
{
    EngineRuntime = &InEngineRuntime;
}

const std::vector<std::string>& FFortAbilityGrantor::GetProfileAbilitySets()
{
    return GetActiveBuildProfile().AbilitySets;
}

bool FFortAbilityGrantor::GrantSingleAbility(const FObjectHandle& AbilitySystemComponent, const FObjectHandle& AbilityClass) const
{
    if (!AbilitySystemComponent || !AbilityClass || EngineRuntime == nullptr)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();
    const FRemoteAddress GiveAbility = EngineRuntime->GetFunctions().AbilitySystemGiveAbility;

    if (GiveAbility == InvalidRemoteAddress)
    {
        return false;
    }

    const FRemoteAddress SpecStorage = Runtime.AcquireScratch(GameplayAbilitySpecSize);
    const FRemoteAddress HandleStorage = Runtime.AcquireScratch(sizeof(int32) * 2);

    if (SpecStorage == InvalidRemoteAddress || HandleStorage == InvalidRemoteAddress)
    {
        return false;
    }

    Runtime.GetMemory().Write<FRemoteAddress>(SpecStorage + GameplayAbilitySpecAbilityOffset, AbilityClass.GetAddress());
    Runtime.GetMemory().Write<int32>(SpecStorage + GameplayAbilitySpecLevelOffset, 1);
    Runtime.GetMemory().Write<int32>(SpecStorage + GameplayAbilitySpecInputIdOffset, InvalidIndex);
    Runtime.GetMemory().Write<FRemoteAddress>(SpecStorage + GameplayAbilitySpecSourceObjectOffset, AbilitySystemComponent.GetAddress());

    Runtime.GetBridge().CallFunction(GiveAbility, { AbilitySystemComponent.GetAddress(), HandleStorage, SpecStorage });
    return true;
}

bool FFortAbilityGrantor::GrantAbilitySet(const AFortPlayerPawnAthena& Pawn, std::string_view AbilitySetPath) const
{
    if (!Pawn || EngineRuntime == nullptr)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    const FObjectHandle AbilitySet = Runtime.FindOrLoadObject(AbilitySetPath);
    if (!AbilitySet)
    {
        return false;
    }

    const FObjectHandle AbilitySystemComponent = Pawn.GetAbilitySystemComponent();
    if (!AbilitySystemComponent)
    {
        return false;
    }

    const FScriptArrayView GrantedAbilities = AbilitySet.GetArrayProperty("GameplayAbilities");

    int32 GrantedCount = 0;

    for (int32 Index = 0; Index < GrantedAbilities.ArrayNum; ++Index)
    {
        const FObjectHandle AbilityClass = AbilitySet.GetArrayElementAsObject("GameplayAbilities", Index);
        if (!AbilityClass)
        {
            continue;
        }

        if (GrantSingleAbility(AbilitySystemComponent, AbilityClass))
        {
            ++GrantedCount;
        }
    }

    return GrantedCount > 0;
}

bool FFortAbilityGrantor::ApplyAthenaAbilities(const AFortPlayerPawnAthena& Pawn) const
{
    if (!Pawn || EngineRuntime == nullptr)
    {
        return false;
    }

    int32 AppliedSets = 0;

    for (const std::string& AbilitySetPath : GetProfileAbilitySets())
    {
        if (GrantAbilitySet(Pawn, AbilitySetPath))
        {
            ++AppliedSets;
        }
    }

    UE_LOG_VERBOSE("Abilities", "Applied " + std::to_string(AppliedSets) + " ability sets to the pawn");
    return AppliedSets > 0;
}
