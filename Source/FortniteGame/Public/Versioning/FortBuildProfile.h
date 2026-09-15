#pragma once

#include "FortniteGame/Public/Gameplay/FortAthenaTypes.h"

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
};

struct FAssetPathProfile
{
    std::string MapName;
    std::string GameModeClassPath;
    std::string PlayerPawnClassPath;
    std::string PlayerPawnFallbackClass;
    std::string WarmupPlayerStartClass;
    std::string AircraftClass;
    std::string DefaultHeadPart;
    std::string DefaultBodyPart;
    std::string DefaultPickaxe;
    std::string WoodResource;
    std::string StoneResource;
    std::string MetalResource;
    std::string ItemListStructPath;
    std::string ItemEntryStructPath;
};

struct FEngineOffsetProfile
{
    uint32 ObjectArray = 0;
    uint32 StaticFindObject = 0;
    uint32 StaticLoadObject = 0;
    uint32 NameConstructor = 0;
    uint32 NameToString = 0;
    uint32 ProcessEvent = 0;
    uint32 MemoryRealloc = 0;
    uint32 SpawnActor = 0;
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

    std::vector<std::string> AbilitySets;
    std::vector<FSafeZonePhaseDefinition> SafeZonePhases;
    std::vector<std::string> KnownPlaylists;

    FVector FallbackSpawnLocation = FVector();
    float MapRadius = 0.0f;
    float AircraftAltitude = 0.0f;
    float SkydiveDropHeight = 0.0f;

    float AircraftFlightSeconds = 45.0f;
    float SafeZoneStartDelaySeconds = 30.0f;
    float EndOfMatchDelaySeconds = 15.0f;

    float StartingHealth = 100.0f;
    float StartingShield = 0.0f;
    float MaxHealth = 100.0f;
    float MaxShield = 100.0f;
    int32 BackpackSize = 5;

    std::string ResolvePlaylistPath(std::string_view Identifier) const;

    std::string Describe() const;
};

const FFortBuildProfile& GetActiveBuildProfile();
