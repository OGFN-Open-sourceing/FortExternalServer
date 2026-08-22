#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

namespace
{
    FFortBuildProfile BuildChapterOneSeasonThreeProfile()
    {
        FFortBuildProfile Profile;

        Profile.DisplayName = "Fortnite 3.6 Chapter 1 Season 3";
        Profile.FortniteVersion = 3.6;
        Profile.EngineVersion = 4.19;
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

        Profile.AssetPaths.MapName = "Athena_Terrain";
        Profile.AssetPaths.GameModeClassPath = "/Game/Athena/Athena_GameMode.Athena_GameMode_C";
        Profile.AssetPaths.PlayerPawnClassPath = "Class /Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C";
        Profile.AssetPaths.PlayerPawnFallbackClass = "FortniteGame.FortPlayerPawnAthena";
        Profile.AssetPaths.WarmupPlayerStartClass = "FortPlayerStartWarmup";
        Profile.AssetPaths.AircraftClass = "FortAthenaAircraft";
        Profile.AssetPaths.DefaultHeadPart = "CustomCharacterPart F_Med_Head1.F_Med_Head1";
        Profile.AssetPaths.DefaultBodyPart = "CustomCharacterPart F_Med_Soldier_01.F_Med_Soldier_01";
        Profile.AssetPaths.DefaultPickaxe = "WID_Harvest_Pickaxe_Athena_C_T01";
        Profile.AssetPaths.WoodResource = "AthenaWoodItemDefinition WoodItemData.WoodItemData";
        Profile.AssetPaths.StoneResource = "AthenaStoneItemDefinition StoneItemData.StoneItemData";
        Profile.AssetPaths.MetalResource = "AthenaMetalItemDefinition MetalItemData.MetalItemData";
        Profile.AssetPaths.ItemListStructPath = "ScriptStruct FortniteGame.FortItemList";
        Profile.AssetPaths.ItemEntryStructPath = "ScriptStruct FortniteGame.FortItemEntry";

        Profile.AbilitySets = { "FortAbilitySet GAB_Athena_Sprint.GAB_Athena_Sprint", "FortAbilitySet GAB_Athena_Jump.GAB_Athena_Jump",
            "FortAbilitySet GAB_Athena_DBNO.GAB_Athena_DBNO", "FortAbilitySet GAB_Athena_Emote.GAB_Athena_Emote",
            "FortAbilitySet GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch",
            "FortAbilitySet GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse", "FortAbilitySet GAB_AthenaSpawnGear.GAB_AthenaSpawnGear" };

        Profile.SafeZonePhases = { { 210.0f, 180.0f, 133000.0f, 1.0f }, { 120.0f, 120.0f, 66500.0f, 1.0f }, { 90.0f, 90.0f, 33250.0f, 2.0f },
            { 90.0f, 90.0f, 16625.0f, 5.0f }, { 60.0f, 60.0f, 8312.0f, 8.0f }, { 60.0f, 60.0f, 4156.0f, 10.0f }, { 45.0f, 45.0f, 2078.0f, 10.0f },
            { 45.0f, 45.0f, 1039.0f, 10.0f }, { 30.0f, 30.0f, 0.0f, 10.0f } };

        Profile.KnownPlaylists = { "Playlist_DefaultSolo", "Playlist_DefaultDuo", "Playlist_DefaultSquad", "Playlist_50v50" };

        Profile.FallbackSpawnLocation = FVector(1250.0f, 1818.0f, 3284.0f);
        Profile.MapRadius = 140000.0f;
        Profile.AircraftAltitude = 12000.0f;
        Profile.SkydiveDropHeight = 10000.0f;

        return Profile;
    }
}

std::string FFortBuildProfile::ResolvePlaylistPath(std::string_view Identifier) const
{
    if (Identifier.find('.') != std::string_view::npos)
    {
        return std::string(Identifier);
    }

    return "FortPlaylistAthena " + std::string(Identifier) + '.' + std::string(Identifier);
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
