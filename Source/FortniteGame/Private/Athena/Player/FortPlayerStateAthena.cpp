#include "FortniteGame/Public/Athena/Player/FortPlayerStateAthena.h"

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
