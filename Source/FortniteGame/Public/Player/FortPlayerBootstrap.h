#pragma once

#include "FortniteGame/Public/Gameplay/FortAbilitySet.h"
#include "FortniteGame/Public/Inventory/FortInventory.h"
#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"
#include "Runtime/Core/Public/Math/RandomStream.h"
#include "Runtime/Engine/Public/Engine/EngineRuntime.h"

struct FPlayerBootstrapSettings
{
    float StartingHealth = 100.0f;
    float StartingShield = 0.0f;
    float MaxHealth = 100.0f;
    float MaxShield = 100.0f;
    int32 BackpackSize = 5;
    bool bHealthRegenEnabled = false;
    std::vector<std::string> StartingLoadout;
};

class FFortPlayerBootstrap
{
public:
    void Initialize(FEngineRuntime& InEngineRuntime, const FPlayerBootstrapSettings& InSettings);

    bool SpawnAndPossess(const AFortPlayerControllerAthena& Controller, const FVector& SpawnLocation) const;

    bool ApplyCosmetics(const AFortPlayerControllerAthena& Controller) const;

    bool ApplyStartingLoadout(const AFortPlayerControllerAthena& Controller) const;

    FVector ChooseWarmupSpawnLocation() const;

    void SetSettings(const FPlayerBootstrapSettings& InSettings);

    const FPlayerBootstrapSettings& GetSettings() const;

private:
    FObjectHandle GetPlayerPawnClass() const;

    FEngineRuntime* EngineRuntime = nullptr;
    FFortAbilityGrantor AbilityGrantor;
    FPlayerBootstrapSettings Settings;
    mutable FRandomStream RandomStream;
};
