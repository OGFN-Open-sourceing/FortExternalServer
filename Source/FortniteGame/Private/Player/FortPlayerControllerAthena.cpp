#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"

std::string AFortPlayerStateAthena::GetPlayerName() const
{
    const std::string Replicated = Object.GetStringProperty("PlayerName");
    if (!Replicated.empty())
    {
        return Replicated;
    }

    return Object.GetStringProperty("SavedNetworkAddress");
}

int32 AFortPlayerStateAthena::GetTeamIndex() const
{
    return Object.GetProperty<uint8>("TeamIndex", 0);
}

void AFortPlayerStateAthena::SetTeamIndex(int32 Value, bool bAssignSquadId)
{
    Object.SetProperty<uint8>("TeamIndex", static_cast<uint8>(Value));
    Object.InvokeFunction("OnRep_TeamIndex");

    if (bAssignSquadId)
    {
        SetSquadId(static_cast<uint8>(Value));
    }

    ForceNetUpdate();
}

void AFortPlayerStateAthena::SetSquadId(uint8 Value)
{
    if (!Object.HasProperty("SquadId"))
    {
        return;
    }

    Object.SetProperty<uint8>("SquadId", Value);
    Object.InvokeFunction("OnRep_SquadId");
}

int32 AFortPlayerStateAthena::GetKillScore() const
{
    return Object.GetProperty<int32>("KillScore", 0);
}

void AFortPlayerStateAthena::SetKillScore(int32 Value)
{
    Object.SetProperty<int32>("KillScore", Value);
    Object.InvokeFunction("OnRep_KillScore");
}

void AFortPlayerStateAthena::SetPlace(int32 Value)
{
    Object.SetProperty<int32>("Place", Value);
}

void AFortPlayerStateAthena::SetHasFinishedLoading(bool bValue)
{
    Object.SetBoolProperty("bHasFinishedLoading", bValue);
}

void AFortPlayerStateAthena::SetHasStartedPlaying(bool bValue)
{
    Object.SetBoolProperty("bHasStartedPlaying", bValue);
    Object.InvokeFunction("OnRep_bHasStartedPlaying");
}

void AFortPlayerStateAthena::SetHeroType(const FObjectHandle& HeroType)
{
    if (!HeroType)
    {
        return;
    }

    Object.SetObjectProperty("HeroType", HeroType);
    Object.InvokeFunction("OnRep_HeroType");
}

void AFortPlayerStateAthena::SetCharacterPart(EFortCustomPartType PartType, const FObjectHandle& Part)
{
    if (!Part)
    {
        return;
    }

    const FRemoteAddress PartsAddress = Object.GetPropertyAddress("CharacterParts");
    if (PartsAddress == InvalidRemoteAddress)
    {
        return;
    }

    const FRemoteAddress SlotAddress = PartsAddress + static_cast<uint64>(ToUnderlying(PartType)) * sizeof(FRemoteAddress);
    Object.GetRuntime().GetMemory().Write<FRemoteAddress>(SlotAddress, Part.GetAddress());
}

void AFortPlayerStateAthena::ApplyCharacterParts()
{
    Object.InvokeFunction("OnRep_CharacterParts");
    ForceNetUpdate();
}

FObjectHandle AFortPlayerStateAthena::GetCurrentPawn() const
{
    struct FGetCurrentPawnParameters
    {
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    if (Object.InvokeFunction("GetCurrentPawn", &Parameters, sizeof(Parameters)) && Parameters.ReturnValue != InvalidRemoteAddress)
    {
        return Object.GetRuntime().MakeHandle(Parameters.ReturnValue);
    }

    return Object.GetObjectProperty("PawnPrivate");
}

void AFortPlayerStateAthena::SetDeathCause(EDeathCause Cause)
{
    if (!Object.HasProperty("DeathCause"))
    {
        return;
    }

    Object.SetProperty<uint8>("DeathCause", ToUnderlying(Cause));
}

void AFortPlayerStateAthena::ForceNetUpdate()
{
    Object.InvokeFunction("ForceNetUpdate");
}

void AFortPlayerPawnAthena::SetMaxHealth(float Value)
{
    struct FSetMaxHealthParameters
    {
        float NewHealthVal = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Parameters.NewHealthVal = Value;
    Object.InvokeFunction("SetMaxHealth", &Parameters, sizeof(Parameters));
}

void AFortPlayerPawnAthena::SetMaxShield(float Value)
{
    struct FSetMaxShieldParameters
    {
        float NewShieldVal = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Parameters.NewShieldVal = Value;
    Object.InvokeFunction("SetMaxShield", &Parameters, sizeof(Parameters));
}

void AFortPlayerPawnAthena::SetHealth(float Value)
{
    struct FSetHealthParameters
    {
        float NewHealthVal = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Parameters.NewHealthVal = Value;
    Object.InvokeFunction("SetHealth", &Parameters, sizeof(Parameters));
}

void AFortPlayerPawnAthena::SetShield(float Value)
{
    struct FSetShieldParameters
    {
        float NewShieldVal = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Parameters.NewShieldVal = Value;
    Object.InvokeFunction("SetShield", &Parameters, sizeof(Parameters));
}

float AFortPlayerPawnAthena::GetHealth() const
{
    struct FGetHealthParameters
    {
        float ReturnValue = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Object.InvokeFunction("GetHealth", &Parameters, sizeof(Parameters));
    return Parameters.ReturnValue;
}

float AFortPlayerPawnAthena::GetShield() const
{
    struct FGetShieldParameters
    {
        float ReturnValue = 0.0f;
        uint8 Padding[4] = {};
    } Parameters;

    Object.InvokeFunction("GetShield", &Parameters, sizeof(Parameters));
    return Parameters.ReturnValue;
}

void AFortPlayerPawnAthena::RemoveMinimumClamps()
{
    const FObjectHandle HealthSet = Object.GetObjectProperty("HealthSet");
    if (!HealthSet)
    {
        return;
    }

    const FRemoteAddress HealthAddress = HealthSet.GetPropertyAddress("Health");
    const FRemoteAddress ShieldAddress = HealthSet.GetPropertyAddress("CurrentShield");

    const FRemoteMemory& Memory = Object.GetRuntime().GetMemory();

    if (HealthAddress != InvalidRemoteAddress)
    {
        Memory.Write<float>(HealthAddress + sizeof(float) * 2, 0.0f);
    }

    if (ShieldAddress != InvalidRemoteAddress)
    {
        Memory.Write<float>(ShieldAddress + sizeof(float) * 2, 0.0f);
    }
}

void AFortPlayerPawnAthena::DisableHealthRegeneration()
{
    static constexpr std::string_view RegenerationEffects[] = { "HealthRegenDelayGameplayEffect", "HealthRegenGameplayEffect", "ShieldRegenDelayGameplayEffect",
        "ShieldRegenGameplayEffect" };

    const FObjectHandle AbilitySystem = GetAbilitySystemComponent();

    for (const std::string_view EffectName : RegenerationEffects)
    {
        const FObjectHandle Effect = Object.GetObjectProperty(EffectName);
        if (!Effect || !AbilitySystem)
        {
            continue;
        }

        struct FRemoveActiveGameplayEffectBySourceEffectParameters
        {
            FRemoteAddress GameplayEffect = InvalidRemoteAddress;
            FRemoteAddress InstigatorAbilitySystemComponent = InvalidRemoteAddress;
            int32 StacksToRemove = 1;
            uint8 Padding[4] = {};
        } Parameters;

        Parameters.GameplayEffect = Effect.GetAddress();
        Parameters.InstigatorAbilitySystemComponent = AbilitySystem.GetAddress();

        AbilitySystem.InvokeFunction("RemoveActiveGameplayEffectBySourceEffect", &Parameters, sizeof(Parameters));

        Object.SetObjectProperty(EffectName, FObjectHandle());
    }
}

void AFortPlayerPawnAthena::SetCanBeDamaged(bool bValue)
{
    Object.SetBoolProperty("bCanBeDamaged", bValue);
}

FObjectHandle AFortPlayerPawnAthena::GetAbilitySystemComponent() const
{
    return Object.GetObjectProperty("AbilitySystemComponent");
}

FObjectHandle AFortPlayerPawnAthena::GetCharacterMovement() const
{
    return Object.GetObjectProperty("CharacterMovement");
}

void AFortPlayerPawnAthena::SetMovementMode(EMovementMode Mode, uint8 CustomMode)
{
    const FObjectHandle Movement = GetCharacterMovement();
    if (!Movement)
    {
        return;
    }

    struct FSetMovementModeParameters
    {
        uint8 NewMovementMode = 0;
        uint8 NewCustomMode = 0;
        uint8 Padding[6] = {};
    } Parameters;

    Parameters.NewMovementMode = ToUnderlying(Mode);
    Parameters.NewCustomMode = CustomMode;

    Movement.InvokeFunction("SetMovementMode", &Parameters, sizeof(Parameters));
}

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
