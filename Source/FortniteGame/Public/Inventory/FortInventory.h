#pragma once

#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"

struct FFortItemEntryView
{
    FRemoteAddress EntryAddress = InvalidRemoteAddress;
    FObjectHandle ItemDefinition;
    FGuid ItemGuid;
    int32 Count = 0;

    bool IsValid() const
    {
        return EntryAddress != InvalidRemoteAddress;
    }
};

class FFortInventory
{
public:
    FFortInventory() = default;

    explicit FFortInventory(const AFortPlayerControllerAthena& InController);

    bool IsValid() const;

    FObjectHandle GetInventoryComponent() const;

    int32 GetItemCount() const;

    FFortItemEntryView GetItemAt(int32 Index) const;

    FFortItemEntryView FindItemByDefinition(const FObjectHandle& ItemDefinition) const;

    FFortItemEntryView FindItemByGuid(const FGuid& ItemGuid) const;

    FFortItemEntryView FindFirstItemOfClass(std::string_view ItemDefinitionClassPath) const;

    bool GiveItem(const FObjectHandle& ItemDefinition, int32 Count, int32 Level) const;

    bool RemoveItem(const FGuid& ItemGuid) const;

    bool EquipItem(const FGuid& ItemGuid) const;

    bool AddResource(EFortResourceType ResourceType, int32 Count) const;

    void ClearInventory() const;

    void ApplyDefaultLoadout(const std::vector<std::string>& WeaponIdentifiers) const;

    void Update(bool bRemovedItem = false) const;

private:
    FObjectHandle FindItemDefinition(std::string_view Identifier) const;

    AFortPlayerControllerAthena Controller;
};
