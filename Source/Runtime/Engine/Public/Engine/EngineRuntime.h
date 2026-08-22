#pragma once

#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"
#include "Runtime/Engine/Public/Engine/ObjectWrapper.h"

struct FEngineFunctions
{
    FRemoteAddress NetDriverTickFlush = InvalidRemoteAddress;
    FRemoteAddress NetDriverInitListen = InvalidRemoteAddress;
    FRemoteAddress NetDriverNetDebug = InvalidRemoteAddress;
    FRemoteAddress WorldNotifyControlMessage = InvalidRemoteAddress;
    FRemoteAddress WorldNotifyAcceptingConnection = InvalidRemoteAddress;
    FRemoteAddress WorldWelcomePlayer = InvalidRemoteAddress;
    FRemoteAddress WorldSpawnPlayActor = InvalidRemoteAddress;
    FRemoteAddress ActorGetNetMode = InvalidRemoteAddress;
    FRemoteAddress OnlineSessionKickPlayer = InvalidRemoteAddress;
    FRemoteAddress BeaconPauseRequests = InvalidRemoteAddress;
    FRemoteAddress BeaconHostInitHost = InvalidRemoteAddress;
    FRemoteAddress BeaconNotifyAcceptingConnection = InvalidRemoteAddress;
    FRemoteAddress BeaconHostNotifyControlMessage = InvalidRemoteAddress;
    FRemoteAddress NetConnectionReceiveFString = InvalidRemoteAddress;
    FRemoteAddress NetConnectionReceiveUniqueIdRepl = InvalidRemoteAddress;
    FRemoteAddress CollectGarbage = InvalidRemoteAddress;
    FRemoteAddress PlayerControllerGetPlayerViewPoint = InvalidRemoteAddress;
    FRemoteAddress LocalPlayerSpawnPlayActor = InvalidRemoteAddress;
    FRemoteAddress AbilitySystemGiveAbility = InvalidRemoteAddress;
    FRemoteAddress AbilitySystemInternalTryActivateAbility = InvalidRemoteAddress;
    FRemoteAddress AbilitySystemMarkAbilitySpecDirty = InvalidRemoteAddress;

    bool HasNetworkEssentials() const;

    std::vector<FRemoteAddress> GetAllEntries() const;

    int32 CountResolved() const;

    int32 CountTotal() const;

    std::vector<std::string> GetUnresolvedNames() const;
};

class UWorld;

class FEngineRuntime
{
public:
    bool Initialize(FUnrealRuntime& InUnrealRuntime);

    bool IsInitialized() const;

    const FEngineFunctions& GetFunctions() const;

    FUnrealRuntime& GetUnrealRuntime() const;

    FObjectHandle GetEngine() const;

    FObjectHandle GetGameInstance() const;

    UWorld GetWorld() const;

    FObjectHandle GetGameViewportClient() const;

    FObjectHandle GetLocalPlayer(int32 Index = 0) const;

    FObjectHandle SpawnObject(const FObjectHandle& ClassHandle, const FObjectHandle& Outer) const;

    void InvalidateCachedEngine();

private:
    bool ResolveFunctions();

    void ResolveAnchoredFallbacks(const class FSignatureScanner& Scanner);

    FUnrealRuntime* UnrealRuntime = nullptr;
    FEngineFunctions Functions;
    mutable FRemoteAddress CachedEngineAddress = InvalidRemoteAddress;
};
