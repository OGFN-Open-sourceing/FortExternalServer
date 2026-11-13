#include "FortniteGame/Public/Items/FortInventory.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"

#include <algorithm>
#include <vector>

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

    struct FItemEntryLayout
    {
        static constexpr int32 AlterationDefinitions = 0x30;
        static constexpr int32 ItemSource = 0x40;
        static constexpr int32 StateValues = 0x90;
        static constexpr int32 AlterationInstances = 0xB0;
    };

    struct FFastArrayLayout
    {
        static constexpr int32 ItemReplicationID = 0x00;
        static constexpr int32 ItemReplicationKey = 0x04;
        static constexpr int32 IDCounter = 0x50;
        static constexpr int32 ArrayReplicationKey = 0x54;
        static constexpr int32 CachedNumItems = 0xA8;
        static constexpr int32 CachedNumItemsToConsiderForWriting = 0xAC;
    };

    int32 AppendToRemoteArray(const FUnrealRuntime& Runtime, FRemoteAddress ArrayAddress, const void* Element, int32 ElementSize)
    {
        const FRemoteMemory& Memory = Runtime.GetMemory();

        FRemoteAddress Data = Memory.ReadPointer(ArrayAddress + FUnrealLayout::FScriptArray_Data);
        const int32 Num = Memory.Read<int32>(ArrayAddress + FUnrealLayout::FScriptArray_ArrayNum);
        const int32 Max = Memory.Read<int32>(ArrayAddress + FUnrealLayout::FScriptArray_ArrayMax);

        if (Num >= Max)
        {
            const int32 NewMax = std::max(Num + 1, std::max(4, Max * 2));
            Data = Runtime.GetBridge().CallFunction(Runtime.GetGlobals().MemoryRealloc,
                { Data, static_cast<uint64>(NewMax) * static_cast<uint64>(ElementSize), 0 });

            if (Data == InvalidRemoteAddress)
            {
                return InvalidIndex;
            }

            Memory.Write<FRemoteAddress>(ArrayAddress + FUnrealLayout::FScriptArray_Data, Data);
            Memory.Write<int32>(ArrayAddress + FUnrealLayout::FScriptArray_ArrayMax, NewMax);
        }

        if (!Memory.WriteRaw(Data + static_cast<uint64>(Num) * static_cast<uint64>(ElementSize), Element, static_cast<size_t>(ElementSize)))
        {
            return InvalidIndex;
        }

        Memory.Write<int32>(ArrayAddress + FUnrealLayout::FScriptArray_ArrayNum, Num + 1);
        return Num;
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
    if (!ItemDefinition || !Controller || Count <= 0)
    {
        return false;
    }

    const FObjectHandle Component = GetInventoryComponent();
    if (!Component)
    {
        UE_LOG_WARNING("Inventory", "No world inventory to give " + ItemDefinition.GetName() + " to");
        return false;
    }

    const FFortItemEntryView Existing = FindItemByDefinition(ItemDefinition);
    if (Existing.IsValid() && IsStackable(ItemDefinition))
    {
        return AddToStack(Existing, Count);
    }

    struct FCreateTemporaryItemInstanceParameters
    {
        int32 Count = 1;
        int32 Level = 1;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } CreateParameters;

    CreateParameters.Count = Count;
    CreateParameters.Level = Level;

    if (!ItemDefinition.InvokeFunction("CreateTemporaryItemInstanceBP", &CreateParameters, sizeof(CreateParameters)) || CreateParameters.ReturnValue == InvalidRemoteAddress)
    {
        UE_LOG_WARNING("Inventory", "Failed to create an item instance of " + ItemDefinition.GetName());
        return false;
    }

    const FUnrealRuntime& Runtime = Controller.GetRuntime();
    const FObjectHandle Item = Runtime.MakeHandle(CreateParameters.ReturnValue);

    struct FSetOwningControllerParameters
    {
        FRemoteAddress InController = InvalidRemoteAddress;
    } OwnerParameters;

    OwnerParameters.InController = Controller.GetAddress();
    Item.InvokeFunction("SetOwningControllerForTemporaryItem", &OwnerParameters, sizeof(OwnerParameters));

    const FItemListLayout& Layout = ResolveItemListLayout(Runtime);
    const FRemoteAddress InventoryAddress = Component.GetPropertyAddress("Inventory");
    const FRemoteAddress ItemEntryAddress = Item.GetPropertyAddress("ItemEntry");

    if (!Layout.IsValid() || Layout.ItemInstancesOffset < 0 || InventoryAddress == InvalidRemoteAddress || ItemEntryAddress == InvalidRemoteAddress)
    {
        UE_LOG_WARNING("Inventory", "Inventory layout is not resolved, cannot add " + ItemDefinition.GetName());
        return false;
    }

    std::vector<uint8> Entry(static_cast<size_t>(Layout.EntryStride));
    Runtime.GetMemory().ReadRaw(ItemEntryAddress, Entry.data(), Entry.size());

    for (const int32 ContainerOffset : { FItemEntryLayout::AlterationDefinitions, FItemEntryLayout::ItemSource, FItemEntryLayout::StateValues, FItemEntryLayout::AlterationInstances })
    {
        if (ContainerOffset + FUnrealLayout::FScriptArray_Size <= Layout.EntryStride)
        {
            std::fill_n(Entry.begin() + ContainerOffset, FUnrealLayout::FScriptArray_Size, uint8(0));
        }
    }

    const FRemoteAddress EntriesAddress = InventoryAddress + static_cast<uint64>(Layout.ReplicatedEntriesOffset);
    const FRemoteAddress InstancesAddress = InventoryAddress + static_cast<uint64>(Layout.ItemInstancesOffset);
    const FRemoteAddress ItemAddress = Item.GetAddress();

    if (AppendToRemoteArray(Runtime, InstancesAddress, &ItemAddress, sizeof(ItemAddress)) < 0)
    {
        UE_LOG_WARNING("Inventory", "Failed to grow ItemInstances for " + ItemDefinition.GetName());
        return false;
    }

    const int32 EntryIndex = AppendToRemoteArray(Runtime, EntriesAddress, Entry.data(), Layout.EntryStride);
    if (EntryIndex < 0)
    {
        UE_LOG_WARNING("Inventory", "Failed to grow ReplicatedEntries for " + ItemDefinition.GetName());
        return false;
    }

    const FFortItemEntryView NewEntry = GetItemAt(EntryIndex);
    MarkItemDirty(InventoryAddress, NewEntry.EntryAddress);
    OnItemGiven(NewEntry, Count);
    return true;
}

bool FFortInventory::IsStackable(const FObjectHandle& ItemDefinition) const
{
    static const FObjectHandle ResourceDefinitionClass = Controller.GetRuntime().FindClass("FortResourceItemDefinition");
    static const FObjectHandle AmmoDefinitionClass = Controller.GetRuntime().FindClass("FortAmmoItemDefinition");
    return ItemDefinition.IsA(ResourceDefinitionClass) || ItemDefinition.IsA(AmmoDefinitionClass);
}

bool FFortInventory::AddToStack(const FFortItemEntryView& Entry, int32 Count) const
{
    const FItemListLayout& Layout = ResolveItemListLayout(Controller.GetRuntime());
    const FRemoteMemory& Memory = Controller.GetRuntime().GetMemory();
    const int32 NewCount = Entry.Count + Count;

    Memory.Write<int32>(Entry.EntryAddress + Layout.EntryCountOffset, NewCount);

    const FObjectHandle Instance = FindItemInstance(Entry.ItemGuid);
    if (Instance)
    {
        const FRemoteAddress InstanceEntry = Instance.GetPropertyAddress("ItemEntry");
        if (InstanceEntry != InvalidRemoteAddress)
        {
            Memory.Write<int32>(InstanceEntry + Layout.EntryCountOffset, NewCount);
        }
    }

    MarkItemDirty(GetInventoryComponent().GetPropertyAddress("Inventory"), Entry.EntryAddress);
    UE_LOG_DISPLAY("Inventory", "Stacked " + std::to_string(Count) + "x " + Entry.ItemDefinition.GetName() + " (now " + std::to_string(NewCount) + ")");
    Update();
    return true;
}

FObjectHandle FFortInventory::FindItemInstance(const FGuid& ItemGuid) const
{
    const FObjectHandle Component = GetInventoryComponent();
    const FUnrealRuntime& Runtime = Controller.GetRuntime();
    const FItemListLayout& Layout = ResolveItemListLayout(Runtime);
    const FRemoteAddress InventoryAddress = Component ? Component.GetPropertyAddress("Inventory") : InvalidRemoteAddress;

    if (InventoryAddress == InvalidRemoteAddress || Layout.ItemInstancesOffset < 0)
    {
        return FObjectHandle();
    }

    const FRemoteMemory& Memory = Runtime.GetMemory();
    const FRemoteAddress InstancesAddress = InventoryAddress + static_cast<uint64>(Layout.ItemInstancesOffset);
    const FRemoteAddress InstancesData = Memory.ReadPointer(InstancesAddress + FUnrealLayout::FScriptArray_Data);
    const int32 InstancesNum = Memory.Read<int32>(InstancesAddress + FUnrealLayout::FScriptArray_ArrayNum);

    for (int32 Index = 0; InstancesData != InvalidRemoteAddress && Index < InstancesNum; ++Index)
    {
        const FObjectHandle Instance = Runtime.MakeHandle(Memory.ReadPointer(InstancesData + static_cast<uint64>(Index) * sizeof(FRemoteAddress)));
        const FRemoteAddress InstanceEntry = Instance ? Instance.GetPropertyAddress("ItemEntry") : InvalidRemoteAddress;

        if (InstanceEntry != InvalidRemoteAddress && Memory.Read<FGuid>(InstanceEntry + Layout.EntryGuidOffset) == ItemGuid)
        {
            return Instance;
        }
    }

    return FObjectHandle();
}

void FFortInventory::MarkItemDirty(FRemoteAddress InventoryAddress, FRemoteAddress EntryAddress) const
{
    if (InventoryAddress == InvalidRemoteAddress || EntryAddress == InvalidRemoteAddress)
    {
        return;
    }

    const FRemoteMemory& Memory = Controller.GetRuntime().GetMemory();

    if (Memory.Read<int32>(EntryAddress + FFastArrayLayout::ItemReplicationID) == InvalidIndex)
    {
        const int32 NewId = Memory.Read<int32>(InventoryAddress + FFastArrayLayout::IDCounter) + 1;
        Memory.Write<int32>(InventoryAddress + FFastArrayLayout::IDCounter, NewId);
        Memory.Write<int32>(EntryAddress + FFastArrayLayout::ItemReplicationID, NewId);
    }

    Memory.Write<int32>(EntryAddress + FFastArrayLayout::ItemReplicationKey, Memory.Read<int32>(EntryAddress + FFastArrayLayout::ItemReplicationKey) + 1);
    Memory.Write<int32>(InventoryAddress + FFastArrayLayout::ArrayReplicationKey, Memory.Read<int32>(InventoryAddress + FFastArrayLayout::ArrayReplicationKey) + 1);
    Memory.Write<int32>(InventoryAddress + FFastArrayLayout::CachedNumItems, InvalidIndex);
    Memory.Write<int32>(InventoryAddress + FFastArrayLayout::CachedNumItemsToConsiderForWriting, InvalidIndex);
}

void FFortInventory::OnItemGiven(const FFortItemEntryView& Entry, int32 Count) const
{
    if (!Entry.IsValid())
    {
        UE_LOG_WARNING("Inventory", "A new inventory entry could not be read back");
        return;
    }

    UE_LOG_DISPLAY("Inventory", "Gave " + std::to_string(Count) + "x " + Entry.ItemDefinition.GetName() + " to " + Controller.GetName());
    AddToQuickBar(Entry);
    Update();
}

bool FFortInventory::IsInQuickBar(const FObjectHandle& QuickBars, EFortQuickBars QuickBar, const FGuid& ItemGuid) const
{
    constexpr int32 QuickBarSlotsOffset = 0x10;
    constexpr int32 QuickBarSlotStride = 0x18;

    const FRemoteAddress QuickBarAddress = QuickBars.GetPropertyAddress(QuickBar == EFortQuickBars::Primary ? "PrimaryQuickBar" : "SecondaryQuickBar");
    if (QuickBarAddress == InvalidRemoteAddress)
    {
        return false;
    }

    const FRemoteMemory& Memory = QuickBars.GetRuntime().GetMemory();
    const FRemoteAddress SlotsAddress = QuickBarAddress + QuickBarSlotsOffset;
    const FRemoteAddress SlotsData = Memory.ReadPointer(SlotsAddress + FUnrealLayout::FScriptArray_Data);
    const int32 SlotsNum = Memory.Read<int32>(SlotsAddress + FUnrealLayout::FScriptArray_ArrayNum);

    for (int32 SlotIndex = 0; SlotsData != InvalidRemoteAddress && SlotIndex < SlotsNum; ++SlotIndex)
    {
        const FRemoteAddress SlotAddress = SlotsData + static_cast<uint64>(SlotIndex) * QuickBarSlotStride;
        const FRemoteAddress ItemsData = Memory.ReadPointer(SlotAddress + FUnrealLayout::FScriptArray_Data);
        const int32 ItemsNum = Memory.Read<int32>(SlotAddress + FUnrealLayout::FScriptArray_ArrayNum);

        for (int32 ItemIndex = 0; ItemsData != InvalidRemoteAddress && ItemIndex < ItemsNum; ++ItemIndex)
        {
            if (Memory.Read<FGuid>(ItemsData + static_cast<uint64>(ItemIndex) * sizeof(FGuid)) == ItemGuid)
            {
                return true;
            }
        }
    }

    return false;
}

void FFortInventory::AddToQuickBar(const FFortItemEntryView& Entry) const
{
    static const FObjectHandle WeaponDefinitionClass = Controller.GetRuntime().FindClass("FortWeaponItemDefinition");
    static const FObjectHandle BuildingDefinitionClass = Controller.GetRuntime().FindClass("FortBuildingItemDefinition");
    static const FObjectHandle EditToolDefinitionClass = Controller.GetRuntime().FindClass("FortEditToolItemDefinition");

    if (!Entry.ItemDefinition.IsA(WeaponDefinitionClass) || Entry.ItemDefinition.IsA(EditToolDefinitionClass))
    {
        return;
    }

    const FObjectHandle QuickBars = Controller.GetQuickBars();
    if (!QuickBars)
    {
        UE_LOG_WARNING("Inventory", "No quickbars to add " + Entry.ItemDefinition.GetName() + " to");
        return;
    }

    const EFortQuickBars QuickBar = Entry.ItemDefinition.IsA(BuildingDefinitionClass) ? EFortQuickBars::Secondary : EFortQuickBars::Primary;
    if (IsInQuickBar(QuickBars, QuickBar, Entry.ItemGuid))
    {
        return;
    }

    struct FServerAddItemInternalParameters
    {
        FGuid Item;
        uint8 InQuickBar = 0;
        uint8 Padding[3] = {};
        int32 Slot = -1;
    } Parameters;

    Parameters.Item = Entry.ItemGuid;
    Parameters.InQuickBar = ToUnderlying(QuickBar);

    if (QuickBars.InvokeFunction("ServerAddItemInternal", &Parameters, sizeof(Parameters)))
    {
        UE_LOG_DISPLAY("Inventory", "Added " + Entry.ItemDefinition.GetName() + (QuickBar == EFortQuickBars::Primary ? " to primary quickbar" : " to secondary quickbar"));
    }
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
