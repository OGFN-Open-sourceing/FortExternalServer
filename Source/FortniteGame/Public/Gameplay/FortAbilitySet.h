#pragma once

#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"
#include "Runtime/Engine/Public/Engine/EngineRuntime.h"

class FFortAbilityGrantor
{
public:
    void Initialize(FEngineRuntime& InEngineRuntime);

    bool ApplyAthenaAbilities(const AFortPlayerPawnAthena& Pawn) const;

    bool GrantAbilitySet(const AFortPlayerPawnAthena& Pawn, std::string_view AbilitySetPath) const;

    static const std::vector<std::string>& GetChapterOneSeasonThreeAbilitySets();

private:
    bool GrantSingleAbility(const FObjectHandle& AbilitySystemComponent, const FObjectHandle& AbilityClass) const;

    FEngineRuntime* EngineRuntime = nullptr;
};
