#include "FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"
#include "FortniteGame/Public/Abilities/FortAbilitySet.h"
#include "FortniteGame/Public/FortniteGameModule.h"
#include "FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

AFortPlayerStateAthena AFortPlayerControllerAthena::GetPlayerState() const
{
    return AFortPlayerStateAthena(Object.GetObjectProperty("PlayerState"));
}

AFortPlayerPawnAthena AFortPlayerControllerAthena::GetPawn() const
{
    return AFortPlayerPawnAthena(Object.GetObjectProperty("Pawn"));
}

void AFortPlayerControllerAthena::SetPawn(const AFortPlayerPawnAthena& Pawn)
{
    Object.SetObjectProperty("Pawn", Pawn.GetObject());
    Object.SetObjectProperty("AcknowledgedPawn", Pawn.GetObject());
    Object.InvokeFunction("OnRep_Pawn");
}

FObjectHandle AFortPlayerControllerAthena::GetNetConnection() const
{
    return Object.GetObjectProperty("NetConnection");
}

FObjectHandle AFortPlayerControllerAthena::GetWorldInventory() const
{
    return Object.GetObjectProperty("WorldInventory");
}

FObjectHandle AFortPlayerControllerAthena::GetQuickBars() const
{
    return Object.GetObjectProperty("QuickBars");
}

bool AFortPlayerControllerAthena::IsDisconnecting() const
{
    return Object.GetBoolProperty("bIsDisconnecting", false);
}

void AFortPlayerControllerAthena::MarkLoadingComplete()
{
    Object.SetBoolProperty("bIsDisconnecting", false);
    Object.SetBoolProperty("bHasClientFinishedLoading", true);
    Object.SetBoolProperty("bHasServerFinishedLoading", true);
    Object.SetBoolProperty("bHasInitiallySpawned", true);
    Object.InvokeFunction("OnRep_bHasServerFinishedLoading");
}

void AFortPlayerControllerAthena::Possess(const AFortPlayerPawnAthena& Pawn)
{
    struct FPossessParameters
    {
        FRemoteAddress InPawn = InvalidRemoteAddress;
    } Parameters;

    Parameters.InPawn = Pawn.GetAddress();
    Object.InvokeFunction("Possess", &Parameters, sizeof(Parameters));
}

void AFortPlayerControllerAthena::SetOverriddenBackpackSize(int32 Value)
{
    if (Object.HasProperty("OverriddenBackpackSize"))
    {
        Object.SetProperty<int32>("OverriddenBackpackSize", Value);
    }
}

void AFortPlayerControllerAthena::ServerReturnToMainMenu()
{
    Object.InvokeFunction("ServerReturnToMainMenu");
}

void AFortPlayerControllerAthena::ClientTravelToMap(const std::string& Url)
{
    const FUnrealRuntime& Runtime = Object.GetRuntime();

    const std::wstring WideUrl = FStringConv::ToWide(Url);
    const FRemoteAddress UrlStorage = Runtime.AllocateTransientWideString(WideUrl);

    struct FSwitchLevelParameters
    {
        FRemoteAddress UrlData = InvalidRemoteAddress;
        int32 UrlNum = 0;
        int32 UrlMax = 0;
    } Parameters;

    Parameters.UrlData = UrlStorage;
    Parameters.UrlNum = static_cast<int32>(WideUrl.size() + 1);
    Parameters.UrlMax = Parameters.UrlNum;

    Object.InvokeFunction("SwitchLevel", &Parameters, sizeof(Parameters));
}

void AFortPlayerControllerAthena::RespawnAfterDeath()
{
    Object.InvokeFunction("RespawnPlayerAfterDeath");
}

void AFortPlayerControllerAthena::ActivateQuickBarSlot(EFortQuickBars QuickBar, int32 Slot)
{
    struct FActivateSlotParameters
    {
        uint8 QuickBarType = 0;
        uint8 Padding[3] = {};
        int32 Slot = 0;
        float ActivateDelay = 0.0f;
        bool bUpdatePreviousFocusedSlot = true;
        uint8 TrailingPadding[3] = {};
    } Parameters;

    Parameters.QuickBarType = ToUnderlying(QuickBar);
    Parameters.Slot = Slot;

    Object.InvokeFunction("ActivateSlot", &Parameters, sizeof(Parameters));
}

void AFortPlayerControllerAthena::ForceUpdateQuickBar(EFortQuickBars QuickBar)
{
    struct FForceUpdateQuickbarParameters
    {
        uint8 QuickBarToRefresh = 0;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.QuickBarToRefresh = ToUnderlying(QuickBar);
    Object.InvokeFunction("ForceUpdateQuickbar", &Parameters, sizeof(Parameters));
}

void AFortPlayerControllerAthena::HandleWorldInventoryLocalUpdate()
{
    Object.InvokeFunction("HandleWorldInventoryLocalUpdate");
}

void AFortPlayerControllerAthena::SendEndOfMatch(bool bVictory)
{
    struct FClientSendEndBattleRoyaleMatchForPlayerParameters
    {
        bool bSuccess = false;
        uint8 Padding[7] = {};
        uint8 RewardResult[0x40] = {};
    } Parameters;

    Parameters.bSuccess = bVictory;
    Object.InvokeFunction("ClientSendEndBattleRoyaleMatchForPlayer", &Parameters, sizeof(Parameters));
}

bool AFortPlayerControllerAthena::IsLocalController() const
{
    struct FIsLocalControllerParameters
    {
        bool ReturnValue = false;
    } Parameters;

    return Object.InvokeFunction("IsLocalController", &Parameters, sizeof(Parameters)) && Parameters.ReturnValue;
}

void AFortPlayerControllerAthena::EnsureWorldInventory() const
{
    if (GetWorldInventory())
    {
        return;
    }

    const FObjectHandle InventoryClass = Object.GetObjectProperty("WorldInventoryClass");
    if (!InventoryClass)
    {
        return;
    }

    const AActor Inventory = UGameplayStatics::SpawnActor(*this, InventoryClass, FTransform(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn, *this);
    if (Inventory)
    {
        Object.SetObjectProperty("WorldInventory", Inventory.GetObject());
    }
}

void AFortPlayerControllerAthena::ApplyDefaultCharacterParts() const
{
    const FFortBuildProfile& Profile = GetActiveBuildProfile();
    const FUnrealRuntime& Runtime = Object.GetRuntime();

    AFortPlayerStateAthena PlayerState = GetPlayerState();
    if (!PlayerState)
    {
        return;
    }

    PlayerState.SetCharacterPart(EFortCustomPartType::Head, Runtime.FindOrLoadObject(Profile.AssetPaths.DefaultHeadPart));
    PlayerState.SetCharacterPart(EFortCustomPartType::Body, Runtime.FindOrLoadObject(Profile.AssetPaths.DefaultBodyPart));
    PlayerState.ApplyCharacterParts();
}

void AFortPlayerControllerAthena::GiveAthenaAbilities() const
{
    const FEngineRuntime& EngineRuntime = FFortniteGameModule::Get().GetEngineRuntime();
    const FObjectHandle AbilitySystemComponent = GetPlayerState().GetObject().GetObjectProperty("AbilitySystemComponent");

    if (!AbilitySystemComponent)
    {
        UE_LOG_WARNING("Athena", GetName() + " has no ability system component");
        return;
    }

    for (const std::string& AbilitySetPath : GetActiveBuildProfile().AssetPaths.AbilitySets)
    {
        const UFortAbilitySet AbilitySet(Object.GetRuntime().FindOrLoadObject(AbilitySetPath));
        if (AbilitySet && AbilitySet.GiveToAbilitySystem(EngineRuntime, AbilitySystemComponent) > 0)
        {
            return;
        }
    }

    UE_LOG_WARNING("Athena", "No ability set could be given to " + GetName());
}

void AFortPlayerControllerAthena::GiveStartingInventory() const
{
    const FFortInventory Inventory(*this);
    if (!Inventory.IsValid() || Inventory.GetItemCount() > 0)
    {
        return;
    }

    const FAssetPathProfile& Paths = GetActiveBuildProfile().AssetPaths;
    const FUnrealRuntime& Runtime = Object.GetRuntime();

    std::vector<std::string> Items = { Paths.DefaultPickaxe, Paths.EditTool };
    Items.insert(Items.end(), Paths.BuildingItems.begin(), Paths.BuildingItems.end());

    for (const std::string& ItemPath : Items)
    {
        Inventory.GiveItem(Runtime.FindOrLoadObject(ItemPath), 1, 1);
    }

    Inventory.ApplyDefaultLoadout(FFortniteGameModule::Get().GetMatchSettings().StartingLoadout);
}

void AFortPlayerControllerAthena::SetupAthenaPawn(const AFortPlayerPawnAthena& Pawn) const
{
    if (!Pawn)
    {
        return;
    }

    ApplyDefaultCharacterParts();
    GiveAthenaAbilities();
    GiveStartingInventory();
}

ENativeOverrideResult AFortPlayerControllerAthena::execServerAcknowledgePossession(const FNativeFunctionCall& Call)
{
    struct FServerAcknowledgePossessionParameters
    {
        FRemoteAddress P = InvalidRemoteAddress;
    };

    const FServerAcknowledgePossessionParameters Parameters = Call.GetParameters<FServerAcknowledgePossessionParameters>();
    Call.GetContext().SetObjectProperty("AcknowledgedPawn", Call.GetRuntime().MakeHandle(Parameters.P));
    return ENativeOverrideResult::Handled;
}

ENativeOverrideResult AFortPlayerControllerAthena::execServerExecuteInventoryItem(const FNativeFunctionCall& Call)
{
    struct FServerExecuteInventoryItemParameters
    {
        FGuid ItemGuid;
    };

    static const FObjectHandle WeaponDefinitionClass = Call.GetRuntime().FindClass("FortWeaponItemDefinition");

    const FServerExecuteInventoryItemParameters Parameters = Call.GetParameters<FServerExecuteInventoryItemParameters>();
    const AFortPlayerControllerAthena Controller(Call.GetContext());
    const AFortPlayerPawnAthena Pawn = Controller.GetPawn();

    if (!Pawn)
    {
        return ENativeOverrideResult::Handled;
    }

    const FFortItemEntryView Entry = FFortInventory(Controller).FindItemByGuid(Parameters.ItemGuid);
    if (Entry.IsValid() && Entry.ItemDefinition.IsA(WeaponDefinitionClass))
    {
        Pawn.EquipWeaponDefinition(Entry.ItemDefinition, Parameters.ItemGuid);
    }

    return ENativeOverrideResult::Handled;
}
