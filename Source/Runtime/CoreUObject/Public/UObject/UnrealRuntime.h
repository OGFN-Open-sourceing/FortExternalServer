#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/CoreUObject/Public/UObject/ObjectHandle.h"
#include "Runtime/CoreUObject/Public/UObject/UnrealLayout.h"
#include "Runtime/CoreUObject/Public/UObject/UnrealTypes.h"
#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"
#include "Runtime/RemoteProcess/Public/ModuleImage.h"
#include "Runtime/RemoteProcess/Public/SignatureScanner.h"

#include <unordered_map>

struct FUnrealGlobals
{
    FRemoteAddress ObjectArray = InvalidRemoteAddress;
    FRemoteAddress StaticFindObject = InvalidRemoteAddress;
    FRemoteAddress StaticLoadObject = InvalidRemoteAddress;
    FRemoteAddress NameConstructor = InvalidRemoteAddress;
    FRemoteAddress NameToString = InvalidRemoteAddress;
    FRemoteAddress ProcessEvent = InvalidRemoteAddress;
    FRemoteAddress MemoryRealloc = InvalidRemoteAddress;
    FRemoteAddress SpawnActor = InvalidRemoteAddress;
    int32 ProcessEventVirtualIndex = InvalidIndex;

    bool IsComplete() const;
};

struct FEngineVersionInfo
{
    std::string VersionString;
    double EngineVersion = 0.0;
    double FortniteVersion = 0.0;
    int32 Changelist = 0;
};

class FUnrealRuntime
{
public:
    bool Initialize(FGameThreadBridge& InBridge, const FModuleImage& InImage);

    bool IsInitialized() const;

    bool RefreshVersionInfo();

    bool WaitForObjectArray(int32 MinimumObjectCount, uint64 TimeoutMilliseconds) const;

    const FUnrealGlobals& GetGlobals() const;

    const FEngineVersionInfo& GetVersionInfo() const;

    const FRemoteMemory& GetMemory() const;

    FGameThreadBridge& GetBridge() const;

    const FModuleImage& GetImage() const;

    FObjectHandle MakeHandle(FRemoteAddress Address) const;

    FObjectHandle FindObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle = FObjectHandle()) const;

    FObjectHandle FindClass(std::string_view ClassPath) const;

    FObjectHandle LoadObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle = FObjectHandle()) const;

    FObjectHandle FindOrLoadObject(std::string_view ObjectPath, const FObjectHandle& ClassHandle = FObjectHandle()) const;

    FObjectHandle GetClassDefaultObject(std::string_view ClassName) const;

    int32 GetObjectCount() const;

    FObjectHandle GetObjectByIndex(int32 Index) const;

    std::vector<FObjectHandle> FindObjectsOfClass(const FObjectHandle& ClassHandle, int32 Limit) const;

    std::vector<FObjectHandle> FindObjectsByNamePrefix(std::string_view Prefix, int32 Limit) const;

    FName FindName(std::string_view Value) const;

    FName CreateName(std::string_view Value) const;

    std::string GetNameString(FName Name) const;

    FPropertyInfo FindPropertyInStruct(FRemoteAddress StructAddress, std::string_view PropertyName) const;

    FRemoteAddress FindFunctionInClass(FRemoteAddress ClassAddress, std::string_view FunctionName) const;

    bool CallProcessEvent(FRemoteAddress ObjectAddress, FRemoteAddress FunctionAddress, void* Parameters, size_t ParametersSize) const;

    FRemoteAddress AcquireScratch(size_t Size) const;

    void ReleaseScratch() const;

    std::wstring ReadUnrealString(FRemoteAddress StringAddress) const;

    FRemoteAddress AllocateTransientWideString(const std::wstring& Value) const;

    void ReleaseTransientAllocations() const;

    bool DumpObjectsToFile(const std::wstring& FilePath) const;

private:
    bool ResolveGlobals();

    bool ResolveProcessEvent(const FSignatureScanner& Scanner);

    bool ResolveNameConstructor(const FSignatureScanner& Scanner);

    FRemoteAddress GetObjectItemArray() const;

    FGameThreadBridge* Bridge = nullptr;
    const FModuleImage* Image = nullptr;
    FUnrealGlobals Globals;
    FEngineVersionInfo VersionInfo;

    FRemoteAddress ScratchArena = InvalidRemoteAddress;
    size_t ScratchArenaSize = 0;
    mutable size_t ScratchCursor = 0;

    mutable std::unordered_map<std::string, int32> NameLookupCache;
    mutable std::unordered_map<uint64, std::string> NameStringCache;
    mutable std::unordered_map<uint64, FPropertyInfo> PropertyCache;
    mutable std::unordered_map<uint64, FRemoteAddress> FunctionCache;
};

template<typename ValueType>
ValueType FObjectHandle::ReadField(int32 Offset, ValueType DefaultValue) const
{
    if (!IsValid())
    {
        return DefaultValue;
    }

    ValueType Value = DefaultValue;
    if (!Runtime->GetMemory().ReadRaw(Address + static_cast<uint64>(Offset), &Value, sizeof(ValueType)))
    {
        return DefaultValue;
    }

    return Value;
}

template<typename ValueType>
bool FObjectHandle::WriteField(int32 Offset, const ValueType& Value) const
{
    if (!IsValid())
    {
        return false;
    }

    return Runtime->GetMemory().WriteRaw(Address + static_cast<uint64>(Offset), &Value, sizeof(ValueType));
}
