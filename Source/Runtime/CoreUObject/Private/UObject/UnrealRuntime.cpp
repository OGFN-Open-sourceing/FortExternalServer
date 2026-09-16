#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"
#include "Runtime/CoreUObject/Public/UObject/CoreUObjectSignatures.h"
#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <filesystem>
#include <fstream>

#include <windows.h>
#include <psapi.h>

namespace
{
    constexpr size_t ScratchArenaBytes = 0x20000;

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

void FUnrealRuntime::SetOffsetOverrides(const FUnrealGlobals& Overrides)
{
    OffsetOverrides = Overrides;
}

void FUnrealRuntime::SetObjectArrayLayout(bool bChunked, int32 InObjectsPerChunk)
{
    bChunkedObjectArray = bChunked;
    ObjectsPerChunk = InObjectsPerChunk > 0 ? InObjectsPerChunk : FUnrealLayout::FChunkedObjectArray_ChunkBytes / FUnrealLayout::FUObjectItem_Stride;
}

bool FUnrealRuntime::Initialize(FGameThreadBridge& InBridge, const FModuleImage& InImage)
{
    Bridge = &InBridge;
    Image = &InImage;

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
    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (GetObjectCount() >= MinimumObjectCount)
        {
            return true;
        }

        FWindowsPlatform::SleepMilliseconds(250);
    }

    return false;
}

bool FUnrealRuntime::IsInitialized() const
{
    return Bridge != nullptr && Image != nullptr && Globals.IsComplete();
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

    ApplyOffsetOverrides();

    if (!Globals.IsComplete())
    {
        UE_LOG_ERROR("CoreUObject", "Failed to resolve every required engine global");
        UE_LOG_ERROR("CoreUObject", "ObjectArray " + FStringConv::ToHex(Globals.ObjectArray) + " StaticFindObject " + FStringConv::ToHex(Globals.StaticFindObject));
        UE_LOG_ERROR("CoreUObject", "NameConstructor " + FStringConv::ToHex(Globals.NameConstructor) + " NameToString " + FStringConv::ToHex(Globals.NameToString));
        UE_LOG_ERROR("CoreUObject", "ProcessEvent " + FStringConv::ToHex(Globals.ProcessEvent) + " Realloc " + FStringConv::ToHex(Globals.MemoryRealloc));
        ReportResolutionFailure(Scanner);
        return false;
    }

    return true;
}

void FUnrealRuntime::ApplyOffsetOverrides()
{
    const auto Apply = [this](FRemoteAddress& Target, FRemoteAddress Override, const char* Label) {
        if (Override == InvalidRemoteAddress || Override == 0)
        {
            return;
        }

        Target = Image->GetBaseAddress() + Override;
        UE_LOG_DISPLAY("CoreUObject", std::string(Label) + " taken from the build profile at " + FStringConv::ToHex(Target));
    };

    Apply(Globals.ObjectArray, OffsetOverrides.ObjectArray, "ObjectArray");
    Apply(Globals.StaticFindObject, OffsetOverrides.StaticFindObject, "StaticFindObject");
    Apply(Globals.StaticLoadObject, OffsetOverrides.StaticLoadObject, "StaticLoadObject");
    Apply(Globals.NameConstructor, OffsetOverrides.NameConstructor, "NameConstructor");
    Apply(Globals.NameToString, OffsetOverrides.NameToString, "NameToString");
    Apply(Globals.ProcessEvent, OffsetOverrides.ProcessEvent, "ProcessEvent");
    Apply(Globals.MemoryRealloc, OffsetOverrides.MemoryRealloc, "MemoryRealloc");
    Apply(Globals.SpawnActor, OffsetOverrides.SpawnActor, "SpawnActor");

    if (Globals.ObjectArray != InvalidRemoteAddress && Globals.ObjectArray != 0)
    {
        const FRemoteAddress ObjectArrayRaw = Globals.ObjectArray;
        const uint64 ValAtBase = GetMemory().Read<uint64>(ObjectArrayRaw);

        if (ValAtBase == 0)
        {
            UE_LOG_DISPLAY("CoreUObject", "ObjectArray RVA " + FStringConv::ToHex(ObjectArrayRaw) + " is zero - scanning all committed memory...");

            const HANDLE ProcessHandle = static_cast<HANDLE>(Bridge->GetMemory().GetProcess()->GetProcessHandle());
            if (ProcessHandle != nullptr)
            {
                MEMORY_BASIC_INFORMATION Mbi = {};
                uint64 ScanAddress = 0;
                int32 CandidatesFound = 0;

                while (VirtualQueryEx(ProcessHandle, reinterpret_cast<LPCVOID>(ScanAddress), &Mbi, sizeof(Mbi)) != 0)
                {
                    if (Mbi.State == MEM_COMMIT && (Mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0
                        && (Mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0)
                    {
                        const uint64 RegionBase = reinterpret_cast<uint64>(Mbi.BaseAddress);
                        const uint64 RegionSize = Mbi.RegionSize;

                        constexpr DWORD ExecutableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
                        const bool bExecutable = (Mbi.Protect & ExecutableMask) != 0;

                        for (uint64 Off = 0; Off + 0x20 <= RegionSize && CandidatesFound < 10; Off += 8)
                        {
                            const FRemoteAddress ProbeAddr = RegionBase + Off;
                            const uint64 Ptr0 = GetMemory().Read<uint64>(ProbeAddr);
                            const int32 Count0C = GetMemory().Read<int32>(ProbeAddr + 0x0C);
                            const int32 Count14 = GetMemory().Read<int32>(ProbeAddr + 0x14);

                            const bool bCandidate0C = Count0C >= 40000 && Count0C <= 120000;
                            const bool bCandidate14 = Count14 >= 40000 && Count14 <= 120000;

                            if (bCandidate0C || bCandidate14)
                            {
                                const uint64 PtrVal = GetMemory().Read<uint64>(ProbeAddr);
                                const bool bPtrReadable = GetMemory().IsCommitted(PtrVal) && PtrVal > 0x10000;

                                if (bPtrReadable)
                                {
                                    char label[256];
                                    snprintf(label, sizeof(label), "ObjectArray candidate at %s (RVA 0x%llX)%s: Ptr=0x%llX Count@0C=%d Count@14=%d",
                                        FStringConv::ToHex(ProbeAddr).c_str(), (unsigned long long)(ProbeAddr - Image->GetBaseAddress()),
                                        bExecutable ? " [exec]" : " [data]",
                                        (unsigned long long)Ptr0, Count0C, Count14);
                                    UE_LOG_DISPLAY("CoreUObject", label);

                                    if ((bCandidate14 && Count14 >= 80000) || (bCandidate0C && Count0C >= 80000 && !bCandidate14))
                                    {
                                        Globals.ObjectArray = ProbeAddr;
                                        UE_LOG_DISPLAY("CoreUObject", "ObjectArray set to " + FStringConv::ToHex(ProbeAddr));
                                        CandidatesFound = -1;
                                        break;
                                    }

                                    ++CandidatesFound;
                                }
                            }
                        }

                        if (CandidatesFound == -1)
                        {
                            break;
                        }
                    }

                    const uint64 NextAddress = reinterpret_cast<uint64>(Mbi.BaseAddress) + Mbi.RegionSize;
                    if (NextAddress <= ScanAddress)
                    {
                        break;
                    }
                    ScanAddress = NextAddress;
                }

                if (CandidatesFound >= 0 && CandidatesFound > 0)
                {
                    UE_LOG_DISPLAY("CoreUObject", "Found " + std::to_string(CandidatesFound) + " candidates but none matched heuristic, using first with readable pointer");
                }
                else if (CandidatesFound == 0)
                {
                    UE_LOG_WARNING("CoreUObject", "No ObjectArray candidates found in any committed memory region");
                }
            }
        }
    }
}

void FUnrealRuntime::ReportResolutionFailure(const FSignatureScanner& Scanner) const
{
    UE_LOG_ERROR("CoreUObject", "Image report for " + FStringConv::ToHex(Image->GetBaseAddress()) + " size " + std::to_string(Image->GetImageSize()));

    const std::vector<uint8>& Bytes = Image->GetBytes();

    for (const FImageSection& Section : Image->GetSections())
    {
        const size_t Start = Image->AddressToOffset(Section.VirtualAddress);
        const size_t End = std::min<size_t>(Start + Section.VirtualSize, Bytes.size());

        size_t NonZero = 0;
        for (size_t Offset = Start; Offset < End; Offset += 64)
        {
            if (Bytes[Offset] != 0)
            {
                ++NonZero;
            }
        }

        const size_t Sampled = End > Start ? (End - Start + 63) / 64 : 0;
        const size_t Percent = Sampled != 0 ? NonZero * 100 / Sampled : 0;

        UE_LOG_ERROR("CoreUObject", "Section " + Section.Name + " rva " + FStringConv::ToHex(Section.VirtualAddress - Image->GetBaseAddress()) + " size " +
            std::to_string(Section.VirtualSize) + " executable " + (Section.IsExecutable() ? "yes" : "no") + " populated " + std::to_string(Percent) + "%");
    }

    const std::vector<std::pair<std::wstring_view, const char*>> Anchors = {
        { FCoreUObjectSignatures::NameConstructorAnchor, "NameConstructorAnchor" },
        { FCoreUObjectSignatures::ProcessEventAnchor, "ProcessEventAnchor" },
        { FCoreUObjectSignatures::StaticLoadObjectAnchor, "StaticLoadObjectAnchor" },
        { FCoreUObjectSignatures::SpawnActorAnchor, "SpawnActorAnchor" }
    };

    for (const auto& Anchor : Anchors)
    {
        const FRemoteAddress Literal = Scanner.FindWideStringLiteral(Anchor.first);
        const FRemoteAddress Reference = Literal != InvalidRemoteAddress ? Scanner.FindWideStringReference(Anchor.first) : InvalidRemoteAddress;

        UE_LOG_ERROR("CoreUObject", std::string(Anchor.second) + " literal " + FStringConv::ToHex(Literal) + " reference " + FStringConv::ToHex(Reference));
    }

    UE_LOG_ERROR("CoreUObject", "Set the EngineOffsets block in the build profile to run this build without a signature scan");
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

    const FRemoteAddress VirtualTable = GetMemory().ReadPointer(ProbeObject + FUnrealLayout::UObject_VirtualTable);
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
    const int32 CountOffset = bChunkedObjectArray ? FUnrealLayout::FChunkedObjectArray_NumElements : FUnrealLayout::TUObjectArray_NumElements;
    return GetMemory().Read<int32>(Globals.ObjectArray + CountOffset);
}

FObjectHandle FUnrealRuntime::GetObjectByIndex(int32 Index) const
{
    if (Index < 0 || Index >= GetObjectCount())
    {
        return FObjectHandle();
    }

    const FRemoteAddress ItemArray = GetObjectItemArray();
    if (ItemArray == 0 || ItemArray == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    FRemoteAddress ItemAddress = InvalidRemoteAddress;

    if (bChunkedObjectArray)
    {
        const FRemoteAddress Chunk = GetMemory().ReadCachedPointer(ItemArray + static_cast<uint64>(Index / ObjectsPerChunk) * sizeof(FRemoteAddress));
        if (Chunk == 0 || Chunk == InvalidRemoteAddress)
        {
            return FObjectHandle();
        }

        ItemAddress = Chunk + static_cast<uint64>(Index % ObjectsPerChunk) * FUnrealLayout::FUObjectItem_Stride;
    }
    else
    {
        ItemAddress = ItemArray + static_cast<uint64>(Index) * FUnrealLayout::FUObjectItem_Stride;
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
    const FRemoteAddress ItemArray = GetObjectItemArray();

    GetMemory().InvalidateCache();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FRemoteAddress ObjectAddress = GetMemory().ReadCachedPointer(ItemArray + static_cast<uint64>(Index) * FUnrealLayout::FUObjectItem_Stride);
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
    const FRemoteAddress ItemArray = GetObjectItemArray();

    GetMemory().InvalidateCache();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FRemoteAddress ObjectAddress = GetMemory().ReadCachedPointer(ItemArray + static_cast<uint64>(Index) * FUnrealLayout::FUObjectItem_Stride);
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

    for (FRemoteAddress CurrentStruct = StructAddress; CurrentStruct != InvalidRemoteAddress;
         CurrentStruct = GetMemory().ReadPointer(CurrentStruct + FUnrealLayout::UStruct_SuperStruct))
    {
        for (FRemoteAddress Field = GetMemory().ReadPointer(CurrentStruct + FUnrealLayout::UStruct_Children); Field != InvalidRemoteAddress;
             Field = GetMemory().ReadPointer(Field + FUnrealLayout::UField_Next))
        {
            const FName FieldName = GetMemory().Read<FName>(Field + FUnrealLayout::UObject_Name);
            if (FieldName.ComparisonIndex != TargetName.ComparisonIndex)
            {
                continue;
            }

            Info.PropertyAddress = Field;
            Info.Offset = GetMemory().Read<int32>(Field + FUnrealLayout::UProperty_Offset_Internal);
            Info.ElementSize = GetMemory().Read<int32>(Field + FUnrealLayout::UProperty_ElementSize);
            Info.ArrayDim = GetMemory().Read<int32>(Field + FUnrealLayout::UProperty_ArrayDim);
            Info.PropertyFlags = GetMemory().Read<uint64>(Field + FUnrealLayout::UProperty_PropertyFlags);

            const FObjectHandle FieldClass = MakeHandle(GetMemory().ReadPointer(Field + FUnrealLayout::UObject_Class));
            if (FieldClass && FieldClass.GetName() == "BoolProperty")
            {
                Info.bIsBitfield = true;
                Info.ByteOffset = GetMemory().Read<uint8>(Field + FUnrealLayout::UBoolProperty_ByteOffset);
                Info.FieldMask = GetMemory().Read<uint8>(Field + FUnrealLayout::UBoolProperty_FieldMask);
            }

            PropertyCache[CacheKey] = Info;
            return Info;
        }
    }

    PropertyCache[CacheKey] = Info;
    return Info;
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
         CurrentStruct = GetMemory().ReadPointer(CurrentStruct + FUnrealLayout::UStruct_SuperStruct))
    {
        for (FRemoteAddress Field = GetMemory().ReadPointer(CurrentStruct + FUnrealLayout::UStruct_Children); Field != InvalidRemoteAddress;
             Field = GetMemory().ReadPointer(Field + FUnrealLayout::UField_Next))
        {
            const FName FieldName = GetMemory().Read<FName>(Field + FUnrealLayout::UObject_Name);
            if (FieldName.ComparisonIndex != TargetName.ComparisonIndex)
            {
                continue;
            }

            const FObjectHandle FieldClass = MakeHandle(GetMemory().ReadPointer(Field + FUnrealLayout::UObject_Class));
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
        const FRemoteAddress VirtualTable = GetMemory().ReadPointer(ObjectAddress + FUnrealLayout::UObject_VirtualTable);
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
