#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

enum class ELevelCollectionType : uint8
{
    DynamicSourceLevels,
    DynamicDuplicatedLevels,
    StaticLevels,
};

struct FLevelCollectionLayout
{
    static constexpr int32 Size = 0x80;
    static constexpr int32 CollectionType = 0x00;
    static constexpr int32 NetDriver = 0x10;
};

struct FURLLayout
{
    static constexpr int32 Size = 0x70;
    static constexpr int32 Port = 0x20;
};

struct FWorldLayout
{
    static constexpr int32 NetworkNotify = 0x28;
};

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
