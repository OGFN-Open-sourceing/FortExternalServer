#include "FortniteGame/Public/Athena/Player/FortPlayerPawnAthena.h"

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

AActor AFortPlayerPawnAthena::EquipWeaponDefinition(const FObjectHandle& WeaponData, const FGuid& ItemEntryGuid) const
{
    struct FEquipWeaponDefinitionParameters
    {
        FRemoteAddress WeaponData = InvalidRemoteAddress;
        FGuid ItemEntryGuid;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    static_assert(sizeof(FEquipWeaponDefinitionParameters) == 0x20);

    Parameters.WeaponData = WeaponData.GetAddress();
    Parameters.ItemEntryGuid = ItemEntryGuid;

    if (!Object.InvokeFunction("EquipWeaponDefinition", &Parameters, sizeof(Parameters)))
    {
        return AActor();
    }

    return AActor(Object.GetRuntime().MakeHandle(Parameters.ReturnValue));
}
