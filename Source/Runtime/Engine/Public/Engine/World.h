#pragma once

#include "Runtime/Engine/Public/Engine/ObjectWrapper.h"

class AActor : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

    FVector GetActorLocation() const;

    FRotator GetActorRotation() const;

    bool SetActorLocation(const FVector& Location, bool bSweep = false);

    bool TeleportTo(const FVector& Location, const FRotator& Rotation);

    bool DestroyActor();

    void ForceNetUpdate();

    void SetOwner(const AActor& NewOwner);

    AActor GetOwner() const;

    void SetReplicates(bool bReplicates);

    void SetActorHiddenInGame(bool bHidden);

    void SetNetCullDistanceSquared(float Value);

    void SetActorTickEnabled(bool bEnabled);
};

class UNetConnection : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

    FObjectHandle GetPlayerController() const;

    void SetPlayerController(const FObjectHandle& Controller);

    FObjectHandle GetOwningActor() const;

    void SetCurrentNetSpeed(int32 Value);

    bool IsInternalAck() const;

    FRemoteAddress GetPlayerIdAddress() const;

    FRemoteAddress GetRequestUrlAddress() const;

    FRemoteAddress GetClientResponseAddress() const;

    std::string GetRequestUrl() const;
};

class UNetDriver : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

    FScriptArrayView GetClientConnections() const;

    UNetConnection GetClientConnection(int32 Index) const;

    int32 GetClientConnectionCount() const;

    FObjectHandle GetReplicationDriver() const;

    FObjectHandle GetServerConnection() const;

    void SetNetServerMaxTickRate(int32 Value);
};

class UWorld : public FObjectWrapper
{
public:
    using FObjectWrapper::FObjectWrapper;

    FObjectHandle GetGameState() const;

    FObjectHandle GetAuthorityGameMode() const;

    UNetDriver GetNetDriver() const;

    FObjectHandle GetOwningGameInstance() const;

    std::vector<FObjectHandle> GetAllActorsOfClass(const FObjectHandle& ClassHandle) const;

    FObjectHandle SpawnActor(const FObjectHandle& ClassHandle, const FTransform& Transform, const FObjectHandle& Owner) const;

    bool ExecuteConsoleCommand(const std::string& Command) const;
};
