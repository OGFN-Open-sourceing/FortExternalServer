#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

namespace
{
    FFortBuildProfile BuildChapterOneSeasonThreeProfile()
    {
        FFortBuildProfile Profile;

        Profile.DisplayName = "Fortnite 3.6 Chapter 1 Season 3";
        Profile.FortniteVersion = 3.6;
        Profile.EngineVersion = 4.20;
        Profile.Changelist = 4019403;

        Profile.ObjectLayout.UObjectSize = 0x28;
        Profile.ObjectLayout.UObjectClass = 0x10;
        Profile.ObjectLayout.UObjectName = 0x18;
        Profile.ObjectLayout.UObjectOuter = 0x20;
        Profile.ObjectLayout.UFieldNext = 0x28;
        Profile.ObjectLayout.UStructSuperStruct = 0x30;
        Profile.ObjectLayout.UStructChildren = 0x38;
        Profile.ObjectLayout.UStructPropertiesSize = 0x40;
        Profile.ObjectLayout.UFunctionFlags = 0x88;
        Profile.ObjectLayout.UFunctionExec = 0xB0;
        Profile.ObjectLayout.UPropertyElementSize = 0x34;
        Profile.ObjectLayout.UPropertyFlags = 0x38;
        Profile.ObjectLayout.UPropertyOffsetInternal = 0x44;
        Profile.ObjectLayout.UBoolPropertyFieldMask = 0x73;
        Profile.ObjectLayout.ObjectItemStride = 0x18;
        Profile.ObjectLayout.bChunkedObjectArray = false;
        Profile.ObjectLayout.ObjectsPerChunk = 0;

        Profile.AssetPaths.MapName = "Athena_Terrain";
        Profile.AssetPaths.GameModeClassPath = "/Game/Athena/Athena_GameMode.Athena_GameMode_C";
        Profile.AssetPaths.DefaultPlaylistPath = "/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
        Profile.AssetPaths.DefaultHeadPart = "/Game/Characters/CharacterParts/Female/Medium/Heads/F_Med_Head1.F_Med_Head1";
        Profile.AssetPaths.DefaultBodyPart = "/Game/Characters/CharacterParts/Female/Medium/Bodies/F_Med_Soldier_01.F_Med_Soldier_01";
        Profile.AssetPaths.DefaultPickaxe = "/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01";
        Profile.AssetPaths.EditTool = "/Game/Items/Weapons/BuildingTools/EditTool.EditTool";
        Profile.AssetPaths.WoodResource = "/Game/Items/ResourcePickups/WoodItemData.WoodItemData";
        Profile.AssetPaths.StoneResource = "/Game/Items/ResourcePickups/StoneItemData.StoneItemData";
        Profile.AssetPaths.MetalResource = "/Game/Items/ResourcePickups/MetalItemData.MetalItemData";
        Profile.AssetPaths.ItemListStructPath = "/Script/FortniteGame.FortItemList";
        Profile.AssetPaths.ItemEntryStructPath = "/Script/FortniteGame.FortItemEntry";
        Profile.AssetPaths.BuildingItems = { "/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall",
            "/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor",
            "/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W",
            "/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS" };
        Profile.AssetPaths.AbilitySets = { "/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_DefaultPlayer.GAS_DefaultPlayer" };

        Profile.EngineOffsets.GObjects = 0x04BA7768;
        Profile.EngineOffsets.AppendString = 0x013298E0;
        Profile.EngineOffsets.GWorld = 0x04CA7250;
        Profile.EngineOffsets.WorldOwningGameInstance = 0x190;
        Profile.EngineOffsets.ProcessEvent = 0x014C04C0;
        Profile.EngineOffsets.ProcessEventIdx = 0x40;
        Profile.EngineOffsets.Realloc = 0x01280550;
        Profile.EngineOffsets.StaticFindObject = 0x014E5890;
        Profile.EngineOffsets.StaticLoadObject = 0x014E6B50;
        Profile.EngineOffsets.FNameConstructor = 0x01327FE0;
        Profile.EngineOffsets.HttpRequestVirtualTable = 0x03932578;

        Profile.EngineFunctions.NetDriverTickFlush = 0x022B6AB0;
        Profile.EngineFunctions.EngineTick = 0x00BE4D40;
        Profile.EngineFunctions.AbilitySystemGiveAbility = 0x00527580;
        Profile.EngineFunctions.AbilitySystemInternalTryActivateAbility = 0x00528B50;
        Profile.EngineFunctions.GameplayAbilitySpecConstructor = 0x0103DA30;
        Profile.EngineFunctions.EngineCreateNetDriver = 0x02501480;
        Profile.EngineFunctions.NetDriverSetWorld = 0x022B56F0;
        Profile.EngineFunctions.GameSessionKickPlayer = 0x00D6B720;
        Profile.EngineFunctions.HttpRequestSetURL = 0x01C07C70;

        Profile.NetworkLayout.NetDriverReplicationDriver = 0x500;
        Profile.NetworkLayout.ServerReplicateActorsVirtualIndex = 0x53;
        Profile.NetworkLayout.InitListenVirtualIndex = 0x4A;

        return Profile;
    }
}

std::string FFortBuildProfile::ResolvePlaylistPath(std::string_view Identifier) const
{
    if (Identifier.empty())
    {
        return AssetPaths.DefaultPlaylistPath;
    }

    if (Identifier.find('/') != std::string_view::npos)
    {
        return std::string(Identifier);
    }

    return "/Game/Athena/Playlists/" + std::string(Identifier) + '.' + std::string(Identifier);
}

std::string FFortBuildProfile::Describe() const
{
    return DisplayName + " (changelist " + std::to_string(Changelist) + ", engine " + std::to_string(EngineVersion) + ")";
}

const FFortBuildProfile& GetActiveBuildProfile()
{
    static const FFortBuildProfile Profile = BuildChapterOneSeasonThreeProfile();
    return Profile;
}
