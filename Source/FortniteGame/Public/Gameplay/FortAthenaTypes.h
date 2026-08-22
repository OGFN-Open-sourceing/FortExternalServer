#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

enum class EAthenaGamePhase : uint8
{
    None = 0,
    Setup = 1,
    Warmup = 2,
    Aircraft = 3,
    SafeZones = 4,
    EndGame = 5
};

enum class EDeathCause : uint8
{
    OutsideSafeZone = 0,
    FallDamage = 1,
    Pistol = 2,
    Shotgun = 3,
    Rifle = 4,
    SMG = 5,
    Sniper = 6,
    Melee = 7,
    InfinityBlade = 8,
    GrenadeLauncher = 9,
    RocketLauncher = 10,
    Grenade = 11,
    C4 = 12,
    Trap = 13,
    Bow = 14,
    Minigun = 15,
    VehicleCannon = 16,
    Turret = 17,
    Unspecified = 18
};

enum class EFortQuickBars : uint8
{
    Primary = 0,
    Secondary = 1,
    MaxNone = 2
};

enum class EFortCustomPartType : uint8
{
    Head = 0,
    Body = 1,
    Hat = 2,
    Backpack = 3,
    Charm = 4,
    Face = 5,
    NumTypes = 6
};

enum class EFortResourceType : uint8
{
    Wood = 0,
    Stone = 1,
    Metal = 2,
    Permanite = 3,
    None = 4
};

enum class EFortTeamMemberState : uint8
{
    Alive = 0,
    Reviving = 1,
    Dead = 2,
    Escaped = 3,
    Disconnected = 4
};

enum class EAthenaRespawnType : uint8
{
    NoRespawn = 0,
    InfiniteRespawn = 1,
    LimitedRespawn = 2
};

enum class EAthenaRespawnLocation : uint8
{
    Air = 0,
    Ground = 1,
    Cheat = 2
};

enum class EFriendlyFireType : uint8
{
    Off = 0,
    On = 1
};

enum class EMovementMode : uint8
{
    None = 0,
    Walking = 1,
    NavWalking = 2,
    Falling = 3,
    Swimming = 4,
    Flying = 5,
    Custom = 6
};

struct FSafeZonePhaseDefinition
{
    float WaitSeconds = 0.0f;
    float ShrinkSeconds = 0.0f;
    float Radius = 0.0f;
    float DamagePerSecond = 0.0f;
};

namespace FAthenaPaths
{
    inline constexpr std::string_view AthenaMap = "Athena_Terrain";
    inline constexpr std::string_view AthenaGameMode = "/Game/Athena/Athena_GameMode.Athena_GameMode_C";
    inline constexpr std::string_view AthenaGameStateClass = "Athena_GameState_C";
    inline constexpr std::string_view AthenaGameModeClass = "FortGameModeAthena";
    inline constexpr std::string_view AthenaPlayerPawnClass = "PlayerPawn_Athena_C";
    inline constexpr std::string_view AthenaPlayerControllerClass = "FortPlayerControllerAthena";
    inline constexpr std::string_view AthenaPlayerStateClass = "FortPlayerStateAthena";
    inline constexpr std::string_view WarmupPlayerStartClass = "FortPlayerStartWarmup";
    inline constexpr std::string_view AircraftClass = "FortAthenaAircraft";
    inline constexpr std::string_view SafeZoneIndicatorClass = "FortSafeZoneIndicator";
    inline constexpr std::string_view SupplyDropClass = "Class FortniteGame.FortAthenaSupplyDrop";
    inline constexpr std::string_view PickupClass = "Class FortniteGame.FortPickupAthena";

    inline constexpr std::string_view DefaultSoloPlaylist = "FortPlaylistAthena Playlist_DefaultSolo.Playlist_DefaultSolo";
    inline constexpr std::string_view DefaultDuoPlaylist = "FortPlaylistAthena Playlist_DefaultDuo.Playlist_DefaultDuo";
    inline constexpr std::string_view DefaultSquadPlaylist = "FortPlaylistAthena Playlist_DefaultSquad.Playlist_DefaultSquad";
    inline constexpr std::string_view FiftyFiftyPlaylist = "FortPlaylistAthena Playlist_50v50.Playlist_50v50";

    inline constexpr std::string_view DefaultHeadPart = "CustomCharacterPart F_Med_Head1.F_Med_Head1";
    inline constexpr std::string_view DefaultBodyPart = "CustomCharacterPart F_Med_Soldier_01.F_Med_Soldier_01";

    inline constexpr std::string_view DefaultPickaxe = "WID_Harvest_Pickaxe_Athena_C_T01";
    inline constexpr std::string_view WoodResource = "AthenaWoodItemDefinition WoodItemData.WoodItemData";
    inline constexpr std::string_view StoneResource = "AthenaStoneItemDefinition StoneItemData.StoneItemData";
    inline constexpr std::string_view MetalResource = "AthenaMetalItemDefinition MetalItemData.MetalItemData";
}

namespace FAthenaSpawn
{
    inline constexpr FVector FallbackWarmupLocation = FVector(1250.0f, 1818.0f, 3284.0f);
    inline constexpr float DefaultAircraftAltitude = 12000.0f;
}
