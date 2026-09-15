#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"
#include "Runtime/CoreUObject/Public/UObject/CoreUObjectSignatures.h"

#include <filesystem>
#include <fstream>

namespace
{
    constexpr size_t ScratchArenaBytes = 0x20000;
    constexpr int32 FieldClassOffset = 0x08;

    uint64 MakeCacheKey(FRemoteAddress OwnerAddress, std::string_view Name)
    {
        uint64 Hash = 0xCBF29CE484222325ull ^ OwnerAddress;

        for (const char Character : Name)
        {
            Hash ^= static_cast<uint64>(static_cast<uint8>(Character));
            Hash *= 0x100000001B3ull;
        }

        return Hash;
    }

    uint64 MakeNameKey(FName Name)
    {
        return (static_cast<uint64>(static_cast<uint32>(Name.ComparisonIndex)) << 32) | static_cast<uint32>(Name.Number);
    }
}

bool FUnrealGlobals::IsComplete() const
{
    return ObjectArray != InvalidRemoteAddress && StaticFindObject != InvalidRemoteAddress && NameConstructor != InvalidRemoteAddress &&
        NameToString != InvalidRemoteAddress && ProcessEvent != InvalidRemoteAddress && MemoryRealloc != InvalidRemoteAddress;
}

bool FUnrealRuntime::Initialize(FGameThreadBridge& InBridge, const FModuleImage& InImage, const FObjectLayout& InLayout)
{
    Bridge = &InBridge;
    Image = &InImage;
    Layout = InLayout;

    ScratchArena = Bridge->GetArena().AllocateZeroed(ScratchArenaBytes, 16);
    ScratchArenaSize = ScratchArena != InvalidRemoteAddress ? ScratchArenaBytes : 0;
    ScratchCursor = 0;

    if (ScratchArena == InvalidRemoteAddress)
    {
        UE_LOG_ERROR("CoreUObject", "Failed to reserve the remote scratch arena");
        return false;
    }

    if (!ResolveGlobals())
    {
        return false;
    }

    UE_LOG_DISPLAY("CoreUObject", "Object array at " + FStringConv::ToHex(Globals.ObjectArray) + " holding " + std::to_string(GetObjectCount()) + " objects");
    return true;
}

bool FUnrealRuntime::WaitForObjectArray(int32 MinimumObjectCount, uint64 TimeoutMilliseconds) const
{
    const uint64 Deadline = FPlatformMisc::GetTimeMilliseconds() + TimeoutMilliseconds;

    while (FPlatformMisc::GetTimeMilliseconds() < Deadline)
    {
        if (GetObjectCount() >= MinimumObjectCount)
        {
            return true;
        }

        FPlatformMisc::SleepMilliseconds(250);
    }

    return false;
}

bool FUnrealRuntime::IsInitialized() const
{
    return Bridge != nullptr && Image != nullptr && Globals.IsComplete();
}

const FObjectLayout& FUnrealRuntime::GetLayout() const
{
    return Layout;
}

const FUnrealGlobals& FUnrealRuntime::GetGlobals() const
{
    return Globals;
}

const FEngineVersionInfo& FUnrealRuntime::GetVersionInfo() const
{
    return VersionInfo;
}

const FRemoteMemory& FUnrealRuntime::GetMemory() const
{
    return Bridge->GetMemory();
}

FGameThreadBridge& FUnrealRuntime::GetBridge() const
{
    return *Bridge;
}

const FModuleImage& FUnrealRuntime::GetImage() const
{
    return *Image;
}

bool FUnrealRuntime::ResolveGlobals()
{
    const FSignatureScanner Scanner(*Image);

    FRemoteAddress ObjectArrayReference = Scanner.FindPattern(FCoreUObjectSignatures::ObjectArrayReference);
    if (ObjectArrayReference == InvalidRemoteAddress)
    {
        ObjectArrayReference = Scanner.FindPattern(FCoreUObjectSignatures::ObjectArrayReferenceAlternate);
    }

    if (ObjectArrayReference != InvalidRemoteAddress)
    {
        Globals.ObjectArray = Scanner.ResolveRelativeOperand(ObjectArrayReference, 3, 7);
    }

    Globals.StaticFindObject = Scanner.FindFirstAvailable({ FCoreUObjectSignatures::StaticFindObject, FCoreUObjectSignatures::StaticFindObjectAlternate });

    if (Globals.StaticFindObject == InvalidRemoteAddress)
    {
        const FRemoteAddress FindObjectAnchor = Scanner.FindWideStringReference(FCoreUObjectSignatures::StaticFindObjectAnchor);
        if (FindObjectAnchor != InvalidRemoteAddress)
        {
            Globals.StaticFindObject = Scanner.FindFunctionStart(FindObjectAnchor, FCoreUObjectSignatures::FunctionStartBacktrack);
        }
    }
    Globals.MemoryRealloc = Scanner.FindPattern(FCoreUObjectSignatures::MemoryRealloc);
    Globals.NameToString = Scanner.FindPattern(FCoreUObjectSignatures::NameToString);

    const FRemoteAddress LoadObjectAnchor = Scanner.FindWideStringReference(FCoreUObjectSignatures::StaticLoadObjectAnchor);
    if (LoadObjectAnchor != InvalidRemoteAddress)
    {
        Globals.StaticLoadObject = Scanner.FindFunctionStart(LoadObjectAnchor, FCoreUObjectSignatures::FunctionStartBacktrack);
    }

    const FRemoteAddress SpawnActorAnchor = Scanner.FindWideStringReference(FCoreUObjectSignatures::SpawnActorAnchor);
    if (SpawnActorAnchor != InvalidRemoteAddress)
    {
        Globals.SpawnActor = Scanner.ScanForSignature(SpawnActorAnchor, "4C 8B DC", false, 0, FCoreUObjectSignatures::AnchorScanRange);
    }

    ResolveNameConstructor(Scanner);
    ResolveProcessEvent(Scanner);

    if (!Globals.IsComplete())
    {
        UE_LOG_ERROR("CoreUObject", "Failed to resolve every required engine global");
        UE_LOG_ERROR("CoreUObject", "ObjectArray " + FStringConv::ToHex(Globals.ObjectArray) + " StaticFindObject " + FStringConv::ToHex(Globals.StaticFindObject));
        UE_LOG_ERROR("CoreUObject", "NameConstructor " + FStringConv::ToHex(Globals.NameConstructor) + " NameToString " + FStringConv::ToHex(Globals.NameToString));
        UE_LOG_ERROR("CoreUObject", "ProcessEvent " + FStringConv::ToHex(Globals.ProcessEvent) + " Realloc " + FStringConv::ToHex(Globals.MemoryRealloc));
        return false;
    }

    return true;
}

bool FUnrealRuntime::ResolveNameConstructor(const FSignatureScanner& Scanner)
{
    const FRemoteAddress Anchor = Scanner.FindWideStringReference(FCoreUObjectSignatures::NameConstructorAnchor);
    if (Anchor == InvalidRemoteAddress)
    {
        return false;
    }

    for (size_t Offset = 0; Offset < FCoreUObjectSignatures::NameConstructorScanRange; ++Offset)
    {
        const uint8* Local = Image->GetLocalPointer(Anchor + Offset);
        if (Local == nullptr)
        {
            break;
        }

        if (Local[0] != 0x48 || Local[1] != 0x8D)
        {
            continue;
        }

        const uint8 BranchOpcode = Local[7];
        if (BranchOpcode != 0xE8 && BranchOpcode != 0xE9)
        {
            continue;
        }

        const FRemoteAddress Target = Scanner.ResolveRelativeOperand(Anchor + Offset + 7, 1, 5);
        if (Image->ContainsAddress(Target))
        {
            Globals.NameConstructor = Target;
            return true;
        }
    }

    return false;
}

bool FUnrealRuntime::ResolveProcessEvent(const FSignatureScanner& Scanner)
{
    const FRemoteAddress Anchor = Scanner.FindWideStringReference(FCoreUObjectSignatures::ProcessEventAnchor);
    if (Anchor == InvalidRemoteAddress)
    {
        return false;
    }

    std::vector<FRemoteAddress> Candidates;

    const FRemoteAddress BackwardStart = Scanner.FindFunctionStart(Anchor, FCoreUObjectSignatures::FunctionStartBacktrack);
    if (BackwardStart != InvalidRemoteAddress)
    {
        Candidates.push_back(BackwardStart);
    }

    const FRemoteAddress ForwardStart = Scanner.ScanForSignature(Anchor, "40 55", true, 0, FCoreUObjectSignatures::AnchorScanRange);
    if (ForwardStart != InvalidRemoteAddress)
    {
        Candidates.push_back(ForwardStart);
    }

    const FRemoteAddress BackwardPrologue = Scanner.ScanForSignature(Anchor, "40 55", false, 0, FCoreUObjectSignatures::FunctionStartBacktrack);
    if (BackwardPrologue != InvalidRemoteAddress)
    {
        Candidates.push_back(BackwardPrologue);
    }

    const FRemoteAddress ProbeObject = GetMemory().ReadPointer(GetObjectItemArray() + FUnrealLayout::FUObjectItem_Object);
    if (ProbeObject == InvalidRemoteAddress)
    {
        return false;
    }

    const FRemoteAddress VirtualTable = GetMemory().ReadPointer(ProbeObject + Layout.UObject_VirtualTable);
    if (VirtualTable == InvalidRemoteAddress)
    {
        return false;
    }

    for (int32 Index = 0; Index < FCoreUObjectSignatures::VirtualTableProbeCount; ++Index)
    {
        const FRemoteAddress Entry = GetMemory().ReadPointer(VirtualTable + static_cast<uint64>(Index) * sizeof(FRemoteAddress));

        for (const FRemoteAddress Candidate : Candidates)
        {
            if (Entry == Candidate)
            {
                Globals.ProcessEvent = Candidate;
                Globals.ProcessEventVirtualIndex = Index;
                UE_LOG_DISPLAY("CoreUObject", "ProcessEvent at " + FStringConv::ToHex(Candidate) + " virtual index " + std::to_string(Index));
                return true;
            }
        }
    }

    if (!Candidates.empty())
    {
        Globals.ProcessEvent = Candidates.front();
        UE_LOG_WARNING("CoreUObject", "ProcessEvent virtual index was not confirmed, falling back to a direct call");
        return true;
    }

    return false;
}

bool FUnrealRuntime::RefreshVersionInfo()
{
    const FObjectHandle DefaultObject = GetClassDefaultObject("KismetSystemLibrary");
    if (!DefaultObject)
    {
        return false;
    }

    struct FGetEngineVersionParameters
    {
        FRemoteAddress ReturnValueData = InvalidRemoteAddress;
        int32 ReturnValueNum = 0;
        int32 ReturnValueMax = 0;
    } Parameters;

    if (!DefaultObject.InvokeFunction("GetEngineVersion", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    VersionInfo.VersionString = FStringConv::ToNarrow(GetMemory().ReadWideString(Parameters.ReturnValueData, 256));

    const size_t FirstDash = VersionInfo.VersionString.find('-');
    const size_t FirstPlus = VersionInfo.VersionString.find('+');

    if (FirstDash != std::string::npos && FirstPlus != std::string::npos && FirstPlus > FirstDash)
    {
        std::string EngineText = VersionInfo.VersionString.substr(0, FirstDash);
        if (EngineText.find('.') != EngineText.rfind('.'))
        {
            EngineText = EngineText.substr(0, EngineText.rfind('.'));
        }

        VersionInfo.EngineVersion = FStringConv::ParseDouble(EngineText, 0.0);
        VersionInfo.Changelist = static_cast<int32>(FStringConv::ParseInt(VersionInfo.VersionString.substr(FirstDash + 1, FirstPlus - FirstDash - 1), 0));
    }

    const size_t ReleaseMarker = VersionInfo.VersionString.rfind("Release-");
    if (ReleaseMarker != std::string::npos)
    {
        VersionInfo.FortniteVersion = FStringConv::ParseDouble(VersionInfo.VersionString.substr(ReleaseMarker + 8), 0.0);
    }

    UE_LOG_DISPLAY("CoreUObject", "Engine version string " + VersionInfo.VersionString);
    return true;
}

FRemoteAddress FUnrealRuntime::GetObjectItemArray() const
{
    return GetMemory().ReadPointer(Globals.ObjectArray + FUnrealLayout::TUObjectArray_Objects);
}

int32 FUnrealRuntime::GetObjectCount() const
{
    return GetMemory().Read<int32>(Globals.ObjectArray + Layout.GetObjectCountOffset());
}

FRemoteAddress FUnrealRuntime::GetObjectItemAddress(int32 Index) const
{
    const FRemoteAddress ItemArray = GetObjectItemArray();
    if (ItemArray == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    if (!Layout.bChunkedObjectArray)
    {
        return ItemArray + static_cast<uint64>(Index) * Layout.ObjectItemStride;
    }

    const int32 ChunkIndex = Index / Layout.ObjectsPerChunk;
    const int32 IndexWithinChunk = Index % Layout.ObjectsPerChunk;

    const FRemoteAddress Chunk = GetMemory().ReadCachedPointer(ItemArray + static_cast<uint64>(ChunkIndex) * sizeof(FRemoteAddress));
    if (Chunk == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    return Chunk + static_cast<uint64>(IndexWithinChunk) * Layout.ObjectItemStride;
}

FObjectHandle FUnrealRuntime::GetObjectByIndex(int32 Index) const
{
    if (Index < 0 || Index >= GetObjectCount())
    {
        return FObjectHandle();
    }

    const FRemoteAddress ItemAddress = GetObjectItemAddress(Index);
    if (ItemAddress == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    return MakeHandle(GetMemory().ReadCachedPointer(ItemAddress + FUnrealLayout::FUObjectItem_Object));
}

FObjectHandle FUnrealRuntime::MakeHandle(FRemoteAddress Address) const
{
    return FObjectHandle(this, Address);
}

FRemoteAddress FUnrealRuntime::AcquireScratch(size_t Size) const
{
    if (ScratchArena == InvalidRemoteAddress || Size == 0 || Size > ScratchArenaSize)
    {
        UE_LOG_ERROR("Runtime", "Scratch request of " + std::to_string(Size) + " bytes cannot be served");
        return InvalidRemoteAddress;
    }

    size_t AlignedCursor = AlignUp<size_t>(ScratchCursor, 16);
    if (AlignedCursor + Size > ScratchArenaSize)
    {
        AlignedCursor = 0;
    }

    ScratchCursor = AlignedCursor + Size;

    const FRemoteAddress Address = ScratchArena + AlignedCursor;

    const std::vector<uint8> ZeroFill(Size, 0);
    if (!GetMemory().WriteRaw(Address, ZeroFill.data(), ZeroFill.size()))
    {
        return InvalidRemoteAddress;
    }

    return Address;
}

void FUnrealRuntime::ReleaseScratch() const
{
    ScratchCursor = 0;
}

void FUnrealRuntime::ReleaseTransientAllocations() const
{
    ReleaseScratch();
}

FRemoteAddress FUnrealRuntime::AllocateTransientWideString(const std::wstring& Value) const
{
    const size_t Size = (Value.size() + 1) * sizeof(wchar_t);
    const FRemoteAddress Address = AcquireScratch(Size);

    if (Address == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    GetMemory().WriteRaw(Address, Value.c_str(), Size);
    return Address;
}

std::wstring FUnrealRuntime::ReadUnrealString(FRemoteAddress StringAddress) const
{
    const FRemoteAddress Data = GetMemory().ReadPointer(StringAddress + FUnrealLayout::FScriptArray_Data);
    const int32 Length = GetMemory().Read<int32>(StringAddress + FUnrealLayout::FScriptArray_ArrayNum);

    if (Data == InvalidRemoteAddress || Length <= 0)
    {
        return std::wstring();
    }

    return GetMemory().ReadWideString(Data, static_cast<size_t>(Length));
}

FName FUnrealRuntime::FindName(std::string_view Value) const
{
    const std::string Key(Value);

    const auto Cached = NameLookupCache.find(Key);
    if (Cached != NameLookupCache.end())
    {
        FName Result;
        Result.ComparisonIndex = Cached->second;
        return Result;
    }

    const FRemoteAddress NameStorage = AcquireScratch(sizeof(FName));
    const FRemoteAddress TextStorage = AllocateTransientWideString(FStringConv::ToWide(Value));

    if (NameStorage == InvalidRemoteAddress || TextStorage == InvalidRemoteAddress)
    {
        return FName();
    }

    Bridge->CallFunction(Globals.NameConstructor, { NameStorage, TextStorage, static_cast<uint64>(FUnrealLayout::FindNameMode) });

    const FName Result = GetMemory().Read<FName>(NameStorage);
    NameLookupCache[Key] = Result.ComparisonIndex;

    return Result;
}

FName FUnrealRuntime::CreateName(std::string_view Value) const
{
    const FRemoteAddress NameStorage = AcquireScratch(sizeof(FName));
    const FRemoteAddress TextStorage = AllocateTransientWideString(FStringConv::ToWide(Value));

    if (NameStorage == InvalidRemoteAddress || TextStorage == InvalidRemoteAddress)
    {
        return FName();
    }

    Bridge->CallFunction(Globals.NameConstructor, { NameStorage, TextStorage, static_cast<uint64>(FUnrealLayout::AddNameMode) });

    const FName Result = GetMemory().Read<FName>(NameStorage);
    NameLookupCache[std::string(Value)] = Result.ComparisonIndex;

    return Result;
}

std::string FUnrealRuntime::GetNameString(FName Name) const
{
    if (Name.ComparisonIndex == 0)
    {
        return "None";
    }

    const uint64 Key = MakeNameKey(Name);

    const auto Cached = NameStringCache.find(Key);
    if (Cached != NameStringCache.end())
    {
        return Cached->second;
    }

    const FRemoteAddress NameStorage = AcquireScratch(sizeof(FName));
    const FRemoteAddress StringStorage = AcquireScratch(FUnrealLayout::FScriptArray_Size);

    if (NameStorage == InvalidRemoteAddress || StringStorage == InvalidRemoteAddress)
    {
        return std::string();
    }

    GetMemory().Write<FName>(NameStorage, Name);

    Bridge->CallFunction(Globals.NameToString, { NameStorage, StringStorage });

    const std::wstring Wide = ReadUnrealString(StringStorage);
    const std::string Result = FStringConv::ToNarrow(Wide);

    const FRemoteAddress Buffer = GetMemory().ReadPointer(StringStorage + FUnrealLayout::FScriptArray_Data);
    if (Buffer != InvalidRemoteAddress)
    {
        Bridge->CallFunction(Globals.MemoryRealloc, { Buffer, 0, 0 });
    }

    NameStringCache[Key] = Result;
    return Result;
}

FObjectHandle FUnrealRuntime::FindObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle) const
{
    const FRemoteAddress TextStorage = AllocateTransientWideString(FStringConv::ToWide(ObjectPath));
    if (TextStorage == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    const uint64 ClassArgument = ClassHandle.IsValid() ? ClassHandle.GetAddress() : 0;
    const uint64 PackageArgument = static_cast<uint64>(static_cast<int64>(FUnrealLayout::AnyPackageSentinel));

    const uint64 Result = Bridge->CallFunction(Globals.StaticFindObject, { ClassArgument, PackageArgument, TextStorage, 0 });
    return MakeHandle(Result);
}

FObjectHandle FUnrealRuntime::FindClass(std::string_view ClassPath) const
{
    static const std::string ClassPrefix = "Class ";

    if (FStringConv::StartsWithIgnoreCase(ClassPath, ClassPrefix))
    {
        return FindObject(ClassPath.substr(ClassPrefix.size()));
    }

    return FindObject(ClassPath);
}

FObjectHandle FUnrealRuntime::LoadObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle) const
{
    if (Globals.StaticLoadObject == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    const FRemoteAddress TextStorage = AllocateTransientWideString(FStringConv::ToWide(ObjectPath));
    if (TextStorage == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    const uint64 ClassArgument = ClassHandle.IsValid() ? ClassHandle.GetAddress() : 0;

    const uint64 Result = Bridge->CallFunction(Globals.StaticLoadObject, { ClassArgument, 0, TextStorage, 0, 0, 0, 0 });
    return MakeHandle(Result);
}

FObjectHandle FUnrealRuntime::FindOrLoadObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle) const
{
    const FObjectHandle Existing = FindObject(ObjectPath, ClassHandle);
    if (Existing)
    {
        return Existing;
    }

    return LoadObject(ObjectPath, ClassHandle);
}

FObjectHandle FUnrealRuntime::GetClassDefaultObject(std::string_view ClassName) const
{
    return FindObject(std::string("Default__") + std::string(ClassName));
}

std::vector<FObjectHandle> FUnrealRuntime::FindObjectsOfClass(const FObjectHandle& ClassHandle, int32 Limit) const
{
    std::vector<FObjectHandle> Results;

    if (!ClassHandle)
    {
        return Results;
    }

    const int32 Count = GetObjectCount();

    GetMemory().InvalidateCache();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FRemoteAddress ItemAddress = GetObjectItemAddress(Index);
        if (ItemAddress == InvalidRemoteAddress)
        {
            continue;
        }

        const FRemoteAddress ObjectAddress = GetMemory().ReadCachedPointer(ItemAddress + FUnrealLayout::FUObjectItem_Object);
        if (ObjectAddress == InvalidRemoteAddress)
        {
            continue;
        }

        const FObjectHandle Candidate = MakeHandle(ObjectAddress);
        if (!Candidate.IsA(ClassHandle))
        {
            continue;
        }

        Results.push_back(Candidate);
        if (Limit > 0 && static_cast<int32>(Results.size()) >= Limit)
        {
            break;
        }
    }

    return Results;
}

std::vector<FObjectHandle> FUnrealRuntime::FindObjectsByNamePrefix(std::string_view Prefix, int32 Limit) const
{
    std::vector<FObjectHandle> Results;

    const int32 Count = GetObjectCount();

    GetMemory().InvalidateCache();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FRemoteAddress ItemAddress = GetObjectItemAddress(Index);
        if (ItemAddress == InvalidRemoteAddress)
        {
            continue;
        }

        const FRemoteAddress ObjectAddress = GetMemory().ReadCachedPointer(ItemAddress + FUnrealLayout::FUObjectItem_Object);
        if (ObjectAddress == InvalidRemoteAddress)
        {
            continue;
        }

        const FObjectHandle Candidate = MakeHandle(ObjectAddress);
        if (!FStringConv::StartsWithIgnoreCase(Candidate.GetName(), Prefix))
        {
            continue;
        }

        Results.push_back(Candidate);
        if (Limit > 0 && static_cast<int32>(Results.size()) >= Limit)
        {
            break;
        }
    }

    return Results;
}

FPropertyInfo FUnrealRuntime::FindPropertyInStruct(FRemoteAddress StructAddress, std::string_view PropertyName) const
{
    FPropertyInfo Info;

    if (StructAddress == InvalidRemoteAddress)
    {
        return Info;
    }

    const uint64 CacheKey = MakeCacheKey(StructAddress, PropertyName);

    const auto Cached = PropertyCache.find(CacheKey);
    if (Cached != PropertyCache.end())
    {
        return Cached->second;
    }

    const FName TargetName = FindName(PropertyName);
    if (TargetName.ComparisonIndex == 0)
    {
        PropertyCache[CacheKey] = Info;
        return Info;
    }

    const int32 PropertyListOffset = Layout.GetPropertyListOffset();
    const int32 PropertyNextOffset = Layout.GetPropertyNextOffset();
    const int32 PropertyNameOffset = Layout.GetPropertyNameOffset();

    for (FRemoteAddress CurrentStruct = StructAddress; CurrentStruct != InvalidRemoteAddress;
         CurrentStruct = GetMemory().ReadPointer(CurrentStruct + Layout.UStruct_SuperStruct))
    {
        for (FRemoteAddress Field = GetMemory().ReadPointer(CurrentStruct + PropertyListOffset); Field != InvalidRemoteAddress;
             Field = GetMemory().ReadPointer(Field + PropertyNextOffset))
        {
            const FName FieldName = GetMemory().Read<FName>(Field + PropertyNameOffset);
            if (FieldName.ComparisonIndex != TargetName.ComparisonIndex)
            {
                continue;
            }

            Info.PropertyAddress = Field;
            Info.Offset = GetMemory().Read<int32>(Field + Layout.UProperty_Offset_Internal);
            Info.ElementSize = GetMemory().Read<int32>(Field + Layout.UProperty_ElementSize);
            Info.ArrayDim = GetMemory().Read<int32>(Field + Layout.UProperty_ArrayDim);
            Info.PropertyFlags = GetMemory().Read<uint64>(Field + Layout.UProperty_PropertyFlags);

            if (IsBooleanProperty(Field))
            {
                Info.bIsBitfield = true;
                Info.ByteOffset = GetMemory().Read<uint8>(Field + Layout.UBoolProperty_FieldMask - 1);
                Info.FieldMask = GetMemory().Read<uint8>(Field + Layout.UBoolProperty_FieldMask);
            }

            PropertyCache[CacheKey] = Info;
            return Info;
        }
    }

    PropertyCache[CacheKey] = Info;
    return Info;
}

bool FUnrealRuntime::IsBooleanProperty(FRemoteAddress PropertyAddress) const
{
    if (!Layout.UsesFieldProperties())
    {
        const FObjectHandle PropertyClass = MakeHandle(GetMemory().ReadPointer(PropertyAddress + Layout.UObject_Class));
        return PropertyClass && PropertyClass.GetName() == "BoolProperty";
    }

    const FRemoteAddress FieldClass = GetMemory().ReadPointer(PropertyAddress + FieldClassOffset);
    if (FieldClass == InvalidRemoteAddress)
    {
        return false;
    }

    const FName FieldClassName = GetMemory().Read<FName>(FieldClass);
    return GetNameString(FieldClassName) == "BoolProperty";
}

FRemoteAddress FUnrealRuntime::FindFunctionInClass(FRemoteAddress ClassAddress, std::string_view FunctionName) const
{
    if (ClassAddress == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    const uint64 CacheKey = MakeCacheKey(ClassAddress, FunctionName);

    const auto Cached = FunctionCache.find(CacheKey);
    if (Cached != FunctionCache.end())
    {
        return Cached->second;
    }

    const FName TargetName = FindName(FunctionName);
    if (TargetName.ComparisonIndex == 0)
    {
        FunctionCache[CacheKey] = InvalidRemoteAddress;
        return InvalidRemoteAddress;
    }

    for (FRemoteAddress CurrentStruct = ClassAddress; CurrentStruct != InvalidRemoteAddress;
         CurrentStruct = GetMemory().ReadPointer(CurrentStruct + Layout.UStruct_SuperStruct))
    {
        for (FRemoteAddress Field = GetMemory().ReadPointer(CurrentStruct + Layout.UStruct_Children); Field != InvalidRemoteAddress;
             Field = GetMemory().ReadPointer(Field + Layout.UField_Next))
        {
            const FName FieldName = GetMemory().Read<FName>(Field + Layout.UObject_Name);
            if (FieldName.ComparisonIndex != TargetName.ComparisonIndex)
            {
                continue;
            }

            const FObjectHandle FieldClass = MakeHandle(GetMemory().ReadPointer(Field + Layout.UObject_Class));
            if (!FieldClass || FieldClass.GetName() != "Function")
            {
                continue;
            }

            FunctionCache[CacheKey] = Field;
            return Field;
        }
    }

    FunctionCache[CacheKey] = InvalidRemoteAddress;
    return InvalidRemoteAddress;
}

bool FUnrealRuntime::CallProcessEvent(FRemoteAddress ObjectAddress, FRemoteAddress FunctionAddress, void* Parameters, size_t ParametersSize) const
{
    if (ObjectAddress == InvalidRemoteAddress || FunctionAddress == InvalidRemoteAddress)
    {
        return false;
    }

    FRemoteAddress ProcessEventAddress = Globals.ProcessEvent;

    if (Globals.ProcessEventVirtualIndex != InvalidIndex)
    {
        const FRemoteAddress VirtualTable = GetMemory().ReadPointer(ObjectAddress + Layout.UObject_VirtualTable);
        if (VirtualTable != InvalidRemoteAddress)
        {
            const FRemoteAddress Entry = GetMemory().ReadPointer(VirtualTable + static_cast<uint64>(Globals.ProcessEventVirtualIndex) * sizeof(FRemoteAddress));
            if (Entry != InvalidRemoteAddress)
            {
                ProcessEventAddress = Entry;
            }
        }
    }

    if (ParametersSize == 0)
    {
        Bridge->CallFunction(ProcessEventAddress, { ObjectAddress, FunctionAddress, 0 });
        return true;
    }

    const FRemoteAddress ParameterStorage = AcquireScratch(ParametersSize);
    if (ParameterStorage == InvalidRemoteAddress)
    {
        return false;
    }

    if (Parameters != nullptr && !GetMemory().WriteRaw(ParameterStorage, Parameters, ParametersSize))
    {
        return false;
    }

    Bridge->CallFunction(ProcessEventAddress, { ObjectAddress, FunctionAddress, ParameterStorage });

    if (Parameters != nullptr)
    {
        GetMemory().ReadRaw(ParameterStorage, Parameters, ParametersSize);
    }

    return true;
}

bool FUnrealRuntime::DumpObjectsToFile(const std::wstring& FilePath) const
{
    std::ofstream Stream{ std::filesystem::path(FilePath) };
    if (!Stream.is_open())
    {
        return false;
    }

    const int32 Count = GetObjectCount();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FObjectHandle Object = GetObjectByIndex(Index);
        if (!Object)
        {
            continue;
        }

        Stream << '[' << Index << "] " << Object.GetFullName() << '\n';
    }

    UE_LOG_DISPLAY("CoreUObject", "Dumped " + std::to_string(Count) + " objects");
    return true;
}
