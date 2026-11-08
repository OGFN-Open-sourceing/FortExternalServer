#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

class UWorld : public UObject
{
public:
    using UObject::UObject;

    FObjectHandle GetGameState() const;

    FObjectHandle GetAuthorityGameMode() const;

    UNetDriver GetNetDriver() const;

    FObjectHandle GetOwningGameInstance() const;

    std::vector<FObjectHandle> GetAllActorsOfClass(const FObjectHandle& ClassHandle) const;

    FObjectHandle SpawnActor(const FObjectHandle& ClassHandle, const FTransform& Transform, const FObjectHandle& Owner) const;

    bool ExecuteConsoleCommand(const std::string& Command) const;
};
