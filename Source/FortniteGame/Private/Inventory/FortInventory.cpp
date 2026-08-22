#include "FortniteGame/Public/Inventory/FortInventory.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

namespace
{

    struct FItemListLayout
    {
        int32 ReplicatedEntriesOffset = InvalidIndex;
        int32 ItemInstancesOffset = InvalidIndex;
        int32 EntryStride = 0;
        int32 EntryCountOffset = InvalidIndex;
        int32 EntryDefinitionOffset = InvalidIndex;
        int32 EntryGuidOffset = InvalidIndex;

        bool IsValid() const
        {
            return ReplicatedEntriesOffset >= 0 && EntryStride > 0 && EntryDefinitionOffset >= 0 && EntryGuidOffset >= 0;
        }
    };

    const FItemListLayout& ResolveItemListLayout(const FUnrealRuntime& Runtime)
    {
        static FItemListLayout Layout;
        static bool bResolved = false;

        if (bResolved)
        {
            return Layout;
        }

        bResolved = true;

        const FObjectHandle ItemListStruct = Runtime.FindObject(GetActiveBuildProfile().AssetPaths.ItemListStructPath);
        const FObjectHandle ItemEntryStruct = Runtime.FindObject(GetActiveBuildProfile().AssetPaths.ItemEntryStructPath);

        if (!ItemListStruct || !ItemEntryStruct)
        {
            UE_LOG_ERROR("Inventory", "Failed to resolve the inventory struct layout");
            return Layout;
        }

        const FPropertyInfo ReplicatedEntries = Runtime.FindPropertyInStruct(ItemListStruct.GetAddress(), "ReplicatedEntries");
        const FPropertyInfo ItemInstances = Runtime.FindPropertyInStruct(ItemListStruct.GetAddress(), "ItemInstances");

        Layout.ReplicatedEntriesOffset = ReplicatedEntries.Offset;
        Layout.ItemInstancesOffset = ItemInstances.Offset;
        Layout.EntryStride = Runtime.GetMemory().Read<int32>(ItemEntryStruct.GetAddress() + FUnrealLayout::UStruct_PropertiesSize);

        Layout.EntryDefinitionOffset = Runtime.FindPropertyInStruct(ItemEntryStruct.GetAddress(), "ItemDefinition").Offset;
        Layout.EntryCountOffset = Runtime.FindPropertyInStruct(ItemEntryStruct.GetAddress(), "Count").Offset;
        Layout.EntryGuidOffset = Runtime.FindPropertyInStruct(ItemEntryStruct.GetAddress(), "ItemGuid").Offset;

        UE_LOG_DISPLAY("Inventory", "Item entry stride " + std::to_string(Layout.EntryStride) + ", definition offset " + std::to_string(Layout.EntryDefinitionOffset));
        return Layout;
    }

    std::string ResourceTypeToPath(EFortResourceType ResourceType)
    {
        switch (ResourceType)
        {
        case EFortResourceType::Wood:
            return GetActiveBuildProfile().AssetPaths.WoodResource;
        case EFortResourceType::Stone:
            return GetActiveBuildProfile().AssetPaths.StoneResource;
        case EFortResourceType::Metal:
            return GetActiveBuildProfile().AssetPaths.MetalResource;
        default:
            return std::string();
        }
    }
}

FFortInventory::FFortInventory(const AFortPlayerControllerAthena& InController)
    : Controller(InController)
{
}

bool FFortInventory::IsValid() const
{
    return Controller.IsValid() && GetInventoryComponent().IsValid();
}

FObjectHandle FFortInventory::GetInventoryComponent() const
{
    return Controller.GetWorldInventory();
}

int32 FFortInventory::GetItemCount() const
{
    const FObjectHandle Component = GetInventoryComponent();
    if (!Component)
    {
        return 0;
    }

    const FItemListLayout& Layout = ResolveItemListLayout(Component.GetRuntime());
    if (!Layout.IsValid())
    {
        return 0;
    }

    const FRemoteAddress InventoryAddress = Component.GetPropertyAddress("Inventory");
    if (InventoryAddress == InvalidRemoteAddress)
    {
        return 0;
    }

    const FRemoteAddress EntriesAddress = InventoryAddress + static_cast<uint64>(Layout.ReplicatedEntriesOffset);
    return Component.GetRuntime().GetMemory().Read<int32>(EntriesAddress + FUnrealLayout::FScriptArray_ArrayNum);
}

FFortItemEntryView FFortInventory::GetItemAt(int32 Index) const
{
    FFortItemEntryView View;

    const FObjectHandle Component = GetInventoryComponent();
    if (!Component || Index < 0)
    {
        return View;
    }

    const FUnrealRuntime& Runtime = Component.GetRuntime();
    const FItemListLayout& Layout = ResolveItemListLayout(Runtime);

    if (!Layout.IsValid())
    {
        return View;
    }

    const FRemoteAddress InventoryAddress = Component.GetPropertyAddress("Inventory");
    if (InventoryAddress == InvalidRemoteAddress)
    {
        return View;
    }

    const FRemoteAddress EntriesAddress = InventoryAddress + static_cast<uint64>(Layout.ReplicatedEntriesOffset);
    const FRemoteAddress EntriesData = Runtime.GetMemory().ReadPointer(EntriesAddress + FUnrealLayout::FScriptArray_Data);
    const int32 EntriesNum = Runtime.GetMemory().Read<int32>(EntriesAddress + FUnrealLayout::FScriptArray_ArrayNum);

    if (EntriesData == InvalidRemoteAddress || Index >= EntriesNum)
    {
        return View;
    }

    View.EntryAddress = EntriesData + static_cast<uint64>(Index) * static_cast<uint64>(Layout.EntryStride);
    View.ItemDefinition = Runtime.MakeHandle(Runtime.GetMemory().ReadPointer(View.EntryAddress + Layout.EntryDefinitionOffset));
    View.ItemGuid = Runtime.GetMemory().Read<FGuid>(View.EntryAddress + Layout.EntryGuidOffset);

    if (Layout.EntryCountOffset >= 0)
    {
        View.Count = Runtime.GetMemory().Read<int32>(View.EntryAddress + Layout.EntryCountOffset);
    }

    return View;
}

FFortItemEntryView FFortInventory::FindItemByDefinition(const FObjectHandle& ItemDefinition) const
{
    const int32 Count = GetItemCount();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FFortItemEntryView Entry = GetItemAt(Index);
        if (Entry.IsValid() && Entry.ItemDefinition == ItemDefinition)
        {
            return Entry;
        }
    }

    return FFortItemEntryView();
}

FFortItemEntryView FFortInventory::FindItemByGuid(const FGuid& ItemGuid) const
{
    const int32 Count = GetItemCount();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FFortItemEntryView Entry = GetItemAt(Index);
        if (Entry.IsValid() && Entry.ItemGuid == ItemGuid)
        {
            return Entry;
        }
    }

    return FFortItemEntryView();
}

FFortItemEntryView FFortInventory::FindFirstItemOfClass(std::string_view ItemDefinitionClassPath) const
{
    const FObjectHandle TargetClass = Controller.GetRuntime().FindClass(ItemDefinitionClassPath);
    if (!TargetClass)
    {
        return FFortItemEntryView();
    }

    const int32 Count = GetItemCount();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FFortItemEntryView Entry = GetItemAt(Index);
        if (Entry.IsValid() && Entry.ItemDefinition.IsA(TargetClass))
        {
            return Entry;
        }
    }

    return FFortItemEntryView();
}

FObjectHandle FFortInventory::FindItemDefinition(std::string_view Identifier) const
{
    const FUnrealRuntime& Runtime = Controller.GetRuntime();

    const FObjectHandle Direct = Runtime.FindObject(Identifier);
    if (Direct)
    {
        return Direct;
    }

    const std::string Qualified = std::string(Identifier) + '.' + std::string(Identifier);
    return Runtime.FindOrLoadObject(Qualified);
}

bool FFortInventory::GiveItem(const FObjectHandle& ItemDefinition, int32 Count, int32 Level) const
{
    if (!ItemDefinition || !Controller)
    {
        return false;
    }

    const FObjectHandle KismetLibrary = Controller.GetRuntime().GetClassDefaultObject("FortKismetLibrary");
    if (KismetLibrary)
    {
        struct FGiveItemToPlayerParameters
        {
            FRemoteAddress PlayerController = InvalidRemoteAddress;
            FRemoteAddress ItemDefinition = InvalidRemoteAddress;
            int32 NumberToGive = 1;
            bool bNotifyPlayer = true;
            uint8 Padding[3] = {};
            bool ReturnValue = false;
            uint8 TrailingPadding[7] = {};
        } LibraryParameters;

        LibraryParameters.PlayerController = Controller.GetAddress();
        LibraryParameters.ItemDefinition = ItemDefinition.GetAddress();
        LibraryParameters.NumberToGive = Count;

        if (KismetLibrary.InvokeFunction("K2_GiveItemToPlayer", &LibraryParameters, sizeof(LibraryParameters)))
        {
            Update();
            return LibraryParameters.ReturnValue;
        }
    }

    const FObjectHandle CheatManager = Controller.GetObject().GetObjectProperty("CheatManager");
    if (!CheatManager)
    {
        return false;
    }

    struct FGiveItemParameters
    {
        FRemoteAddress ItemDefinition = InvalidRemoteAddress;
        int32 NumberToGive = 1;
        int32 LevelToGive = 1;
    } Parameters;

    Parameters.ItemDefinition = ItemDefinition.GetAddress();
    Parameters.NumberToGive = Count;
    Parameters.LevelToGive = Level;

    if (!CheatManager.InvokeFunction("GiveItem", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    Update();
    return true;
}

bool FFortInventory::RemoveItem(const FGuid& ItemGuid) const
{
    const FObjectHandle Component = GetInventoryComponent();
    if (!Component)
    {
        return false;
    }

    struct FRemoveInventoryItemParameters
    {
        FGuid ItemGuid;
        int32 Count = -1;
        bool bForceRemoveFromQuickBars = true;
        uint8 Padding[3] = {};
    } Parameters;

    Parameters.ItemGuid = ItemGuid;

    if (!Component.InvokeFunction("RemoveInventoryItem", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    Update(true);
    return true;
}

bool FFortInventory::EquipItem(const FGuid& ItemGuid) const
{
    if (!Controller)
    {
        return false;
    }

    struct FServerExecuteInventoryItemParameters
    {
        FGuid ItemGuid;
    } Parameters;

    Parameters.ItemGuid = ItemGuid;

    return Controller.GetObject().InvokeFunction("ServerExecuteInventoryItem", &Parameters, sizeof(Parameters));
}

bool FFortInventory::AddResource(EFortResourceType ResourceType, int32 Count) const
{
    const std::string Path = ResourceTypeToPath(ResourceType);
    if (Path.empty())
    {
        return false;
    }

    const FObjectHandle Definition = Controller.GetRuntime().FindObject(Path);
    if (!Definition)
    {
        return false;
    }

    return GiveItem(Definition, Count, 1);
}

void FFortInventory::ClearInventory() const
{
    for (int32 Index = GetItemCount() - 1; Index >= 0; --Index)
    {
        const FFortItemEntryView Entry = GetItemAt(Index);
        if (Entry.IsValid())
        {
            RemoveItem(Entry.ItemGuid);
        }
    }
}

void FFortInventory::ApplyDefaultLoadout(const std::vector<std::string>& WeaponIdentifiers) const
{
    for (const std::string& Identifier : WeaponIdentifiers)
    {
        const FObjectHandle Definition = FindItemDefinition(Identifier);
        if (!Definition)
        {
            UE_LOG_WARNING("Inventory", "Loadout entry " + Identifier + " could not be resolved");
            continue;
        }

        GiveItem(Definition, 1, 1);
    }

    Update();
}

void FFortInventory::Update(bool bRemovedItem) const
{
    const FObjectHandle Component = GetInventoryComponent();
    if (!Component || !Controller)
    {
        return;
    }

    Component.SetBoolProperty("bRequiresLocalUpdate", true);
    Component.InvokeFunction("HandleInventoryLocalUpdate");
    Component.InvokeFunction("ForceNetUpdate");

    AFortPlayerControllerAthena MutableController = Controller;
    MutableController.HandleWorldInventoryLocalUpdate();

    const FObjectHandle QuickBars = Controller.GetQuickBars();
    if (QuickBars)
    {
        QuickBars.InvokeFunction("OnRep_PrimaryQuickBar");
        QuickBars.InvokeFunction("OnRep_SecondaryQuickBar");
        QuickBars.InvokeFunction("ForceNetUpdate");
    }

    MutableController.ForceUpdateQuickBar(EFortQuickBars::Primary);
    MutableController.ForceUpdateQuickBar(EFortQuickBars::Secondary);

    if (bRemovedItem)
    {
        Component.InvokeFunction("MarkArrayDirty");
    }
}
