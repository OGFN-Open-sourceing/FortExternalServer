#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

namespace
{
    FFortAIDefinition MakeBoss(std::string DisplayName, std::vector<std::string> Loadout, const FVector& SpawnLocation)
    {
        FFortAIDefinition Boss;
        Boss.DisplayName = std::move(DisplayName);
        Boss.Loadout = std::move(Loadout);
        Boss.SpawnLocation = SpawnLocation;
        Boss.Health = 100.0f;
        Boss.Shield = 200.0f;
        Boss.bIsBoss = true;
        return Boss;
    }

    FFortBuildProfile BuildChapterTwoSeasonThreeProfile()
    {
        FFortBuildProfile Profile;

        Profile.DisplayName = "Fortnite 13.40 Chapter 2 Season 3";
        Profile.FortniteVersion = 13.40;
        Profile.EngineVersion = 4.25;
        Profile.Changelist = 14113327;

        Profile.ObjectLayout.UObjectSize = 0x28;
        Profile.ObjectLayout.UObjectClass = 0x10;
        Profile.ObjectLayout.UObjectName = 0x18;
        Profile.ObjectLayout.UObjectOuter = 0x20;
        Profile.ObjectLayout.UFieldNext = 0x28;
        Profile.ObjectLayout.UStructSuperStruct = 0x40;
        Profile.ObjectLayout.UStructChildren = 0x48;
        Profile.ObjectLayout.UStructChildProperties = 0x50;
        Profile.ObjectLayout.UStructPropertiesSize = 0x58;
        Profile.ObjectLayout.UFunctionFlags = 0xB0;
        Profile.ObjectLayout.UFunctionExec = 0xD8;
        Profile.ObjectLayout.UPropertyElementSize = 0x3C;
        Profile.ObjectLayout.UPropertyFlags = 0x40;
        Profile.ObjectLayout.UPropertyOffsetInternal = 0x4C;
        Profile.ObjectLayout.UBoolPropertyFieldMask = 0x7B;
        Profile.ObjectLayout.FFieldNext = 0x20;
        Profile.ObjectLayout.FFieldName = 0x28;
        Profile.ObjectLayout.ObjectItemStride = 0x18;
        Profile.ObjectLayout.bChunkedObjectArray = true;
        Profile.ObjectLayout.ObjectsPerChunk = 64 * 1024;

        Profile.AssetPaths.MapName = "Apollo_Terrain";
        Profile.AssetPaths.GameModeClassPath = "";
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

        Profile.AI.BotPawnClassPath = "Class /Game/Athena/AI/Phoebe/BP_PlayerPawn_Athena_Phoebe.BP_PlayerPawn_Athena_Phoebe_C";
        Profile.AI.BotControllerClassPath = "Class /Game/Athena/AI/Phoebe/BP_PhoebePlayerController.BP_PhoebePlayerController_C";
        Profile.AI.AIDirectorClassPath = "FortniteGame.AthenaAIDirector";
        Profile.AI.ServerBotManagerClassPath = "FortniteGame.FortServerBotManagerAthena";
        Profile.AI.BotMutatorClassPath = "FortniteGame.FortAthenaMutator_Bots";
        Profile.AI.BotLoadout = { "WID_Harvest_Pickaxe_Athena_C_T01", "WID_Assault_AutoHigh_Athena_R_Ore_T03" };

        Profile.AI.Bosses = {
            MakeBoss("Ocean", { "WID_Harvest_Pickaxe_Athena_C_T01", "WID_Assault_Burst_Athena_UC_Ore_T03", "Athena_Bottomless_ChugJug" },
                FVector(-90000.0f, -60000.0f, 2000.0f)),
            MakeBoss("Jules", { "WID_Harvest_Pickaxe_Athena_C_T01", "WID_Pistol_AutoHeavySuppressed_Athena_SR_Ore_T03", "WID_Athena_GliderGun" },
                FVector(0.0f, 0.0f, 2000.0f)),
            MakeBoss("Kit", { "WID_Harvest_Pickaxe_Athena_C_T01", "WID_Shotgun_Charge_Athena_SR_Ore_T03", "WID_Launcher_Shockwave_Athena_SR_Ore_T03" },
                FVector(95000.0f, 75000.0f, 2000.0f))
        };

        Profile.AbilitySets = { "FortAbilitySet GAB_Athena_Sprint.GAB_Athena_Sprint", "FortAbilitySet GAB_Athena_Jump.GAB_Athena_Jump",
            "FortAbilitySet GAB_Athena_DBNO.GAB_Athena_DBNO", "FortAbilitySet GAB_Athena_Emote.GAB_Athena_Emote",
            "FortAbilitySet GA_DefaultPlayer_InteractSearch.GA_DefaultPlayer_InteractSearch",
            "FortAbilitySet GA_DefaultPlayer_InteractUse.GA_DefaultPlayer_InteractUse", "FortAbilitySet GAB_AthenaSpawnGear.GAB_AthenaSpawnGear",
            "FortAbilitySet GAB_Athena_Swimming.GAB_Athena_Swimming" };

        Profile.SafeZonePhases = { { 180.0f, 150.0f, 118000.0f, 1.0f }, { 120.0f, 120.0f, 59000.0f, 1.0f }, { 90.0f, 90.0f, 29500.0f, 2.0f },
            { 90.0f, 90.0f, 14750.0f, 5.0f }, { 60.0f, 60.0f, 7375.0f, 8.0f }, { 60.0f, 60.0f, 3687.0f, 10.0f }, { 45.0f, 45.0f, 1843.0f, 10.0f },
            { 45.0f, 45.0f, 921.0f, 10.0f }, { 30.0f, 30.0f, 0.0f, 10.0f } };

        Profile.KnownPlaylists = { "Playlist_DefaultSolo", "Playlist_DefaultDuo", "Playlist_DefaultSquad", "Playlist_Respawn_24", "Playlist_Playground" };

        Profile.FallbackSpawnLocation = FVector(1250.0f, 1818.0f, 3284.0f);
        Profile.MapRadius = 125000.0f;
        Profile.AircraftAltitude = 12000.0f;
        Profile.SkydiveDropHeight = 10000.0f;

        Profile.AircraftFlightSeconds = 45.0f;
        Profile.SafeZoneStartDelaySeconds = 30.0f;
        Profile.EndOfMatchDelaySeconds = 15.0f;

        Profile.StartingHealth = 100.0f;
        Profile.StartingShield = 0.0f;
        Profile.MaxHealth = 100.0f;
        Profile.MaxShield = 100.0f;
        Profile.BackpackSize = 5;

        return Profile;
    }
}

bool FFortAIDefinition::HasExplicitSpawnLocation() const
{
    return SpawnLocation.SizeSquared() > 1.0f;
}

bool FAIProfile::IsSupported() const
{
    return !BotPawnClassPath.empty() && !BotControllerClassPath.empty();
}

bool FObjectLayoutProfile::UsesFieldProperties() const
{
    return UStructChildProperties >= 0;
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
    static const FFortBuildProfile Profile = BuildChapterTwoSeasonThreeProfile();
    return Profile;
}
