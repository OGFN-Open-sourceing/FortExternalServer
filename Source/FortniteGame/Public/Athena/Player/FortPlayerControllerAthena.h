#pragma once

#include "FortniteGame/Public/Athena/Player/FortPlayerStateAthena.h"
#include "FortniteGame/Public/Athena/Player/FortPlayerPawnAthena.h"
#include "Runtime/CoreUObject/Public/UObject/NativeFunctionOverrides.h"

class AFortPlayerControllerAthena : public AActor
{
public:
    using AActor::AActor;

    AFortPlayerStateAthena GetPlayerState() const;

    AFortPlayerPawnAthena GetPawn() const;

    void SetPawn(const AFortPlayerPawnAthena& Pawn);

    FObjectHandle GetNetConnection() const;

    FObjectHandle GetWorldInventory() const;

    FObjectHandle GetQuickBars() const;

    bool IsDisconnecting() const;

    void MarkLoadingComplete();

    void Possess(const AFortPlayerPawnAthena& Pawn);

    void SetOverriddenBackpackSize(int32 Value);

    void ServerReturnToMainMenu();

    void ClientTravelToMap(const std::string& Url);

    void RespawnAfterDeath();

    void ActivateQuickBarSlot(EFortQuickBars QuickBar, int32 Slot);

    void ForceUpdateQuickBar(EFortQuickBars QuickBar);

    void HandleWorldInventoryLocalUpdate();

    void SendEndOfMatch(bool bVictory);

    bool IsLocalController() const;

    void EnsureWorldInventory() const;

    void EnsureQuickBars() const;

    void SetupAthenaPawn(const AFortPlayerPawnAthena& Pawn) const;

    static ENativeOverrideResult execServerAcknowledgePossession(const FNativeFunctionCall& Call);

    static ENativeOverrideResult execServerExecuteInventoryItem(const FNativeFunctionCall& Call);

private:
    void ApplyDefaultCharacterParts() const;

    void GiveAthenaAbilities() const;

    void GiveStartingInventory() const;
};
