#pragma once

#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

struct FEngineFunctions
{
    FRemoteAddress NetDriverTickFlush = InvalidRemoteAddress;
    FRemoteAddress EngineTick = InvalidRemoteAddress;
    FRemoteAddress AbilitySystemGiveAbility = InvalidRemoteAddress;
    FRemoteAddress AbilitySystemInternalTryActivateAbility = InvalidRemoteAddress;
    FRemoteAddress GameplayAbilitySpecConstructor = InvalidRemoteAddress;
    FRemoteAddress EngineCreateNetDriver = InvalidRemoteAddress;
    FRemoteAddress NetDriverSetWorld = InvalidRemoteAddress;
    FRemoteAddress GameSessionKickPlayer = InvalidRemoteAddress;

    std::vector<std::pair<FRemoteAddress, const char*>> GetAllEntries() const;
};

struct FEngineNetworkLayout
{
    int32 NetDriverReplicationDriver = InvalidIndex;
    int32 ServerReplicateActorsVirtualIndex = InvalidIndex;
    int32 InitListenVirtualIndex = InvalidIndex;
};

struct FEngineGlobals
{
    FRemoteAddress GWorld = InvalidRemoteAddress;
};

class UWorld;

class FEngineRuntime
{
public:
    bool Initialize(FUnrealRuntime& InUnrealRuntime, const FEngineGlobals& InGlobals, const FEngineFunctions& InFunctions, const FEngineNetworkLayout& InNetworkLayout);

    bool IsInitialized() const;

    const FEngineFunctions& GetFunctions() const;

    const FEngineNetworkLayout& GetNetworkLayout() const;

    FUnrealRuntime& GetUnrealRuntime() const;

    FObjectHandle GetEngine() const;

    FObjectHandle GetGameInstance() const;

    UWorld GetWorld() const;

    FObjectHandle GetGameViewportClient() const;

    FObjectHandle GetLocalPlayer(int32 Index = 0) const;

    bool RemoveLocalPlayers() const;

    bool Listen(const UWorld& World, int32 Port) const;

    FObjectHandle SpawnObject(const FObjectHandle& ClassHandle, const FObjectHandle& Outer) const;

    void SetEngine(const FObjectHandle& Engine);

private:
    FUnrealRuntime* UnrealRuntime = nullptr;
    FEngineGlobals Globals;
    FEngineFunctions Functions;
    FEngineNetworkLayout NetworkLayout;
    mutable FRemoteAddress CachedEngineAddress = InvalidRemoteAddress;
};
