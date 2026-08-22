#pragma once

#include "FortniteGame/Public/Gameplay/FortAbilitySet.h"
#include "FortniteGame/Public/Player/FortPlayerBootstrap.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Core/Public/Math/RandomStream.h"

struct FSpawnedAI
{
    FRemoteAddress ControllerAddress = InvalidRemoteAddress;
    FRemoteAddress PawnAddress = InvalidRemoteAddress;
    std::string DisplayName;
    int32 TeamIndex = InvalidIndex;
    bool bIsBoss = false;
    bool bEliminated = false;
};

class FFortAIDirector
{
public:
    void Initialize(FEngineRuntime& InEngineRuntime, const FPlayerBootstrapSettings& InPlayerSettings);

    void Reset();

    bool IsSupported() const;

    std::string DescribeSupport() const;

    FSpawnedAI SpawnAI(const FFortAIDefinition& Definition, int32 TeamIndex) const;

    int32 SpawnPlayerBots(int32 Count, int32 FirstTeamIndex);

    int32 SpawnBosses();

    void UpdateEliminationState();

    int32 GetAliveCount() const;

    int32 GetSpawnedCount() const;

    const std::vector<FSpawnedAI>& GetSpawnedAI() const;

    void SetSpawnLocationProvider(std::function<FVector()> Provider);

    bool SetupSubsystems();

private:
    FObjectHandle ResolveBotPawnClass() const;

    FObjectHandle ResolveBotControllerClass() const;

    FVector ChooseSpawnLocation(const FFortAIDefinition& Definition) const;

    void ApplyDefaultAbilitySets(const FObjectHandle& Pawn) const;

    void RegisterWithDirector(const FObjectHandle& Pawn) const;

    FEngineRuntime* EngineRuntime = nullptr;
    FFortAbilityGrantor AbilityGrantor;
    FPlayerBootstrapSettings PlayerSettings;
    std::function<FVector()> SpawnLocationProvider;

    std::vector<FSpawnedAI> SpawnedAI;
    FRemoteAddress AIDirectorHandle = InvalidRemoteAddress;
    mutable FRandomStream RandomStream;
    mutable bool bLoggedUnsupported = false;
    bool bSubsystemsReady = false;
};
