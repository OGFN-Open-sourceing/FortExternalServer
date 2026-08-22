#include "FortniteGame/Public/Player/FortPlayerBootstrap.h"

void FFortPlayerBootstrap::Initialize(FEngineRuntime& InEngineRuntime, const FPlayerBootstrapSettings& InSettings)
{
    EngineRuntime = &InEngineRuntime;
    Settings = InSettings;

    AbilityGrantor.Initialize(InEngineRuntime);
}

void FFortPlayerBootstrap::SetSettings(const FPlayerBootstrapSettings& InSettings)
{
    Settings = InSettings;
}

const FPlayerBootstrapSettings& FFortPlayerBootstrap::GetSettings() const
{
    return Settings;
}

FObjectHandle FFortPlayerBootstrap::GetPlayerPawnClass() const
{
    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    const FObjectHandle BlueprintClass = Runtime.FindObject("Class /Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
    if (BlueprintClass)
    {
        return BlueprintClass;
    }

    const FObjectHandle ByName = Runtime.FindClass(FAthenaPaths::AthenaPlayerPawnClass);
    if (ByName)
    {
        return ByName;
    }

    return Runtime.FindClass("FortniteGame.FortPlayerPawnAthena");
}

FVector FFortPlayerBootstrap::ChooseWarmupSpawnLocation() const
{
    if (EngineRuntime == nullptr)
    {
        return FAthenaSpawn::FallbackWarmupLocation;
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return FAthenaSpawn::FallbackWarmupLocation;
    }

    const FObjectHandle WarmupStartClass = EngineRuntime->GetUnrealRuntime().FindClass(FAthenaPaths::WarmupPlayerStartClass);
    if (!WarmupStartClass)
    {
        return FAthenaSpawn::FallbackWarmupLocation;
    }

    const std::vector<FObjectHandle> Starts = World.GetAllActorsOfClass(WarmupStartClass);
    if (Starts.empty())
    {
        return FAthenaSpawn::FallbackWarmupLocation;
    }

    const int32 ChosenIndex = RandomStream.RandomRange(0, static_cast<int32>(Starts.size()));
    const AActor ChosenStart(Starts[static_cast<size_t>(ChosenIndex)]);

    return ChosenStart.GetActorLocation();
}

bool FFortPlayerBootstrap::SpawnAndPossess(const AFortPlayerControllerAthena& Controller, const FVector& SpawnLocation) const
{
    if (!Controller || EngineRuntime == nullptr)
    {
        return false;
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return false;
    }

    const FObjectHandle PawnClass = GetPlayerPawnClass();
    if (!PawnClass)
    {
        UE_LOG_ERROR("PlayerBootstrap", "The Athena player pawn class could not be resolved");
        return false;
    }

    AFortPlayerControllerAthena MutableController = Controller;

    AFortPlayerPawnAthena ExistingPawn = Controller.GetPawn();
    if (ExistingPawn)
    {
        ExistingPawn.DestroyActor();
    }

    const FTransform SpawnTransform = FTransform::FromLocation(SpawnLocation);
    const FObjectHandle SpawnedPawn = World.SpawnActor(PawnClass, SpawnTransform, Controller.GetObject());

    if (!SpawnedPawn)
    {
        UE_LOG_ERROR("PlayerBootstrap", "Failed to spawn the player pawn");
        return false;
    }

    AFortPlayerPawnAthena Pawn(SpawnedPawn);
    Pawn.SetOwner(AActor(Controller.GetObject()));
    Pawn.SetNetCullDistanceSquared(0.0f);

    MutableController.SetPawn(Pawn);
    MutableController.Possess(Pawn);
    MutableController.SetOverriddenBackpackSize(Settings.BackpackSize);
    MutableController.MarkLoadingComplete();

    Pawn.RemoveMinimumClamps();
    Pawn.SetMaxHealth(Settings.MaxHealth);
    Pawn.SetMaxShield(Settings.MaxShield);
    Pawn.SetHealth(Settings.StartingHealth);
    Pawn.SetShield(Settings.StartingShield);

    if (!Settings.bHealthRegenEnabled)
    {
        Pawn.DisableHealthRegeneration();
    }

    AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
    if (PlayerState)
    {
        PlayerState.SetHasFinishedLoading(true);
        PlayerState.SetHasStartedPlaying(true);
    }

    AbilityGrantor.ApplyAthenaAbilities(Pawn);

    UE_LOG_DISPLAY("PlayerBootstrap", "Spawned pawn for " + (PlayerState ? PlayerState.GetPlayerName() : std::string("unknown player")));
    return true;
}

bool FFortPlayerBootstrap::ApplyCosmetics(const AFortPlayerControllerAthena& Controller) const
{
    if (!Controller || EngineRuntime == nullptr)
    {
        return false;
    }

    AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
    if (!PlayerState)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();

    const FObjectHandle GameInstance = EngineRuntime->GetGameInstance();
    if (GameInstance)
    {
        const FObjectHandle RegisteredPlayer = GameInstance.GetArrayElementAsObject("RegisteredPlayers", 0);
        if (RegisteredPlayer)
        {
            const FObjectHandle MenuHero = RegisteredPlayer.GetObjectProperty("AthenaMenuHeroDef");
            if (MenuHero)
            {
                struct FGetHeroTypeParameters
                {
                    FRemoteAddress ReturnValue = InvalidRemoteAddress;
                } Parameters;

                if (MenuHero.InvokeFunction("GetHeroTypeBP", &Parameters, sizeof(Parameters)))
                {
                    PlayerState.SetHeroType(Runtime.MakeHandle(Parameters.ReturnValue));
                }
            }
        }
    }

    const FObjectHandle HeadPart = Runtime.FindOrLoadObject(FAthenaPaths::DefaultHeadPart);
    const FObjectHandle BodyPart = Runtime.FindOrLoadObject(FAthenaPaths::DefaultBodyPart);

    if (HeadPart)
    {
        PlayerState.SetCharacterPart(EFortCustomPartType::Head, HeadPart);
    }

    if (BodyPart)
    {
        PlayerState.SetCharacterPart(EFortCustomPartType::Body, BodyPart);
    }

    PlayerState.ApplyCharacterParts();
    return true;
}

bool FFortPlayerBootstrap::ApplyStartingLoadout(const AFortPlayerControllerAthena& Controller) const
{
    if (!Controller)
    {
        return false;
    }

    const FFortInventory Inventory(Controller);
    if (!Inventory.IsValid())
    {
        UE_LOG_WARNING("PlayerBootstrap", "The joining player has no world inventory component");
        return false;
    }

    std::vector<std::string> Loadout = Settings.StartingLoadout;
    if (Loadout.empty())
    {
        Loadout.push_back(std::string(FAthenaPaths::DefaultPickaxe));
    }

    Inventory.ApplyDefaultLoadout(Loadout);

    const FFortItemEntryView Pickaxe = Inventory.FindFirstItemOfClass("FortniteGame.FortWeaponMeleeItemDefinition");
    if (Pickaxe.IsValid())
    {
        Inventory.EquipItem(Pickaxe.ItemGuid);
    }

    AFortPlayerControllerAthena MutableController = Controller;
    MutableController.ActivateQuickBarSlot(EFortQuickBars::Primary, 0);

    return true;
}
