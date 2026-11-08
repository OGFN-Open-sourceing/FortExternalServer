#pragma once

#include "FortniteGame/Public/FortTypes.h"

struct FObjectLayoutProfile
{
    int32 UObjectSize = 0x28;
    int32 UObjectClass = 0x10;
    int32 UObjectName = 0x18;
    int32 UObjectOuter = 0x20;
    int32 UFieldNext = 0x28;
    int32 UStructSuperStruct = 0x30;
    int32 UStructChildren = 0x38;
    int32 UStructPropertiesSize = 0x40;
    int32 UFunctionFlags = 0x88;
    int32 UFunctionExec = 0xB0;
    int32 UPropertyElementSize = 0x34;
    int32 UPropertyFlags = 0x38;
    int32 UPropertyOffsetInternal = 0x44;
    int32 UBoolPropertyFieldMask = 0x73;
    int32 ObjectItemStride = 0x18;
    bool bChunkedObjectArray = false;
    int32 ObjectsPerChunk = 0;
};

struct FAssetPathProfile
{
    std::string MapName;
    std::string GameModeClassPath;
    std::string DefaultPlaylistPath;
    std::string DefaultHeadPart;
    std::string DefaultBodyPart;
    std::string DefaultPickaxe;
    std::string EditTool;
    std::string WoodResource;
    std::string StoneResource;
    std::string MetalResource;
    std::string ItemListStructPath;
    std::string ItemEntryStructPath;
    std::vector<std::string> BuildingItems;
    std::vector<std::string> AbilitySets;
};

struct FEngineOffsetProfile
{
    uint32 GObjects = 0;
    uint32 AppendString = 0;
    uint32 GWorld = 0;
    uint32 ProcessEvent = 0;
    int32 ProcessEventIdx = InvalidIndex;
    uint32 Realloc = 0;
    uint32 StaticFindObject = 0;
    uint32 StaticLoadObject = 0;
    uint32 FNameConstructor = 0;
};

struct FEngineFunctionProfile
{
    uint32 NetDriverTickFlush = 0;
    uint32 CollectGarbage = 0;
    uint32 AbilitySystemGiveAbility = 0;
    uint32 AbilitySystemInternalTryActivateAbility = 0;
    uint32 GameplayAbilitySpecConstructor = 0;
};

struct FNetworkLayoutProfile
{
    int32 NetDriverReplicationDriver = InvalidIndex;
    int32 ServerReplicateActorsVirtualIndex = InvalidIndex;
};

struct FFortBuildProfile
{
    std::string DisplayName;
    double FortniteVersion = 0.0;
    double EngineVersion = 0.0;
    int32 Changelist = 0;

    FObjectLayoutProfile ObjectLayout;
    FAssetPathProfile AssetPaths;
    FEngineOffsetProfile EngineOffsets;
    FEngineFunctionProfile EngineFunctions;
    FNetworkLayoutProfile NetworkLayout;

    std::string ResolvePlaylistPath(std::string_view Identifier) const;

    std::string Describe() const;
};

const FFortBuildProfile& GetActiveBuildProfile();
