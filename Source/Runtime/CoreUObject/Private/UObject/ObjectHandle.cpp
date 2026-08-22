#include "Runtime/CoreUObject/Public/UObject/ObjectHandle.h"
#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"

FObjectHandle::FObjectHandle(const FUnrealRuntime* InRuntime, FRemoteAddress InAddress)
    : Runtime(InRuntime)
    , Address(InAddress)
{
}

bool FObjectHandle::IsValid() const
{
    return Runtime != nullptr && Address != InvalidRemoteAddress && Address > 0x10000;
}

FObjectHandle::operator bool() const
{
    return IsValid();
}

bool FObjectHandle::operator==(const FObjectHandle& Other) const
{
    return Address == Other.Address;
}

bool FObjectHandle::operator!=(const FObjectHandle& Other) const
{
    return Address != Other.Address;
}

FRemoteAddress FObjectHandle::GetAddress() const
{
    return Address;
}

const FUnrealRuntime& FObjectHandle::GetRuntime() const
{
    return *Runtime;
}

FRemoteAddress FObjectHandle::GetFieldAddress(int32 Offset) const
{
    return IsValid() ? Address + static_cast<uint64>(Offset) : InvalidRemoteAddress;
}

FName FObjectHandle::GetFName() const
{
    if (!IsValid())
    {
        return FName();
    }

    return Runtime->GetMemory().Read<FName>(Address + FUnrealLayout::UObject_Name);
}

std::string FObjectHandle::GetName() const
{
    if (!IsValid())
    {
        return std::string();
    }

    return Runtime->GetNameString(GetFName());
}

std::string FObjectHandle::GetPathName() const
{
    if (!IsValid())
    {
        return std::string();
    }

    std::string Result = GetName();

    for (FObjectHandle Outer = GetOuter(); Outer; Outer = Outer.GetOuter())
    {
        Result = Outer.GetName() + '.' + Result;
    }

    return Result;
}

std::string FObjectHandle::GetFullName() const
{
    if (!IsValid())
    {
        return std::string();
    }

    const FObjectHandle ClassHandle = GetClass();
    if (!ClassHandle)
    {
        return GetPathName();
    }

    return ClassHandle.GetName() + ' ' + GetPathName();
}

FObjectHandle FObjectHandle::GetClass() const
{
    if (!IsValid())
    {
        return FObjectHandle();
    }

    return FObjectHandle(Runtime, Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_Class));
}

FObjectHandle FObjectHandle::GetOuter() const
{
    if (!IsValid())
    {
        return FObjectHandle();
    }

    return FObjectHandle(Runtime, Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_Outer));
}

int32 FObjectHandle::GetInternalIndex() const
{
    if (!IsValid())
    {
        return InvalidIndex;
    }

    return Runtime->GetMemory().Read<int32>(Address + FUnrealLayout::UObject_InternalIndex);
}

bool FObjectHandle::IsA(const FObjectHandle& ClassHandle) const
{
    if (!IsValid() || !ClassHandle)
    {
        return false;
    }

    const FRemoteAddress TargetClass = ClassHandle.GetAddress();

    for (FRemoteAddress CurrentClass = Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_Class); CurrentClass != InvalidRemoteAddress;
         CurrentClass = Runtime->GetMemory().ReadPointer(CurrentClass + FUnrealLayout::UStruct_SuperStruct))
    {
        if (CurrentClass == TargetClass)
        {
            return true;
        }
    }

    return false;
}

bool FObjectHandle::IsA(std::string_view ClassPath) const
{
    if (!IsValid())
    {
        return false;
    }

    return IsA(Runtime->FindClass(ClassPath));
}

FPropertyInfo FObjectHandle::FindProperty(std::string_view PropertyName) const
{
    if (!IsValid())
    {
        return FPropertyInfo();
    }

    return Runtime->FindPropertyInStruct(Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_Class), PropertyName);
}

bool FObjectHandle::HasProperty(std::string_view PropertyName) const
{
    return FindProperty(PropertyName).IsValid();
}

FRemoteAddress FObjectHandle::GetPropertyAddress(std::string_view PropertyName) const
{
    const FPropertyInfo Info = FindProperty(PropertyName);
    if (!Info.IsValid())
    {
        return InvalidRemoteAddress;
    }

    return Address + static_cast<uint64>(Info.Offset);
}

bool FObjectHandle::ReadPropertyRaw(std::string_view PropertyName, void* Destination, size_t Size) const
{
    const FPropertyInfo Info = FindProperty(PropertyName);
    if (!Info.IsValid())
    {
        return false;
    }

    return Runtime->GetMemory().ReadRaw(Address + static_cast<uint64>(Info.Offset), Destination, Size);
}

bool FObjectHandle::WritePropertyRaw(std::string_view PropertyName, const void* Source, size_t Size) const
{
    const FPropertyInfo Info = FindProperty(PropertyName);
    if (!Info.IsValid())
    {
        return false;
    }

    return Runtime->GetMemory().WriteRaw(Address + static_cast<uint64>(Info.Offset), Source, Size);
}

bool FObjectHandle::GetBoolProperty(std::string_view PropertyName, bool DefaultValue) const
{
    const FPropertyInfo Info = FindProperty(PropertyName);
    if (!Info.IsValid())
    {
        return DefaultValue;
    }

    if (!Info.bIsBitfield)
    {
        return Runtime->GetMemory().Read<uint8>(Address + static_cast<uint64>(Info.Offset)) != 0;
    }

    const uint8 Byte = Runtime->GetMemory().Read<uint8>(Address + static_cast<uint64>(Info.Offset) + Info.ByteOffset);
    return (Byte & Info.FieldMask) != 0;
}

bool FObjectHandle::SetBoolProperty(std::string_view PropertyName, bool Value) const
{
    const FPropertyInfo Info = FindProperty(PropertyName);
    if (!Info.IsValid())
    {
        return false;
    }

    if (!Info.bIsBitfield)
    {
        return Runtime->GetMemory().Write<uint8>(Address + static_cast<uint64>(Info.Offset), Value ? 1 : 0);
    }

    const FRemoteAddress ByteAddress = Address + static_cast<uint64>(Info.Offset) + Info.ByteOffset;

    uint8 Byte = Runtime->GetMemory().Read<uint8>(ByteAddress);
    Byte = Value ? static_cast<uint8>(Byte | Info.FieldMask) : static_cast<uint8>(Byte & ~Info.FieldMask);

    return Runtime->GetMemory().Write<uint8>(ByteAddress, Byte);
}

FObjectHandle FObjectHandle::GetObjectProperty(std::string_view PropertyName) const
{
    const FRemoteAddress PropertyAddress = GetPropertyAddress(PropertyName);
    if (PropertyAddress == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    return FObjectHandle(Runtime, Runtime->GetMemory().ReadPointer(PropertyAddress));
}

bool FObjectHandle::SetObjectProperty(std::string_view PropertyName, const FObjectHandle& Value) const
{
    return SetProperty<FRemoteAddress>(PropertyName, Value.GetAddress());
}

FScriptArrayView FObjectHandle::GetArrayProperty(std::string_view PropertyName) const
{
    FScriptArrayView View;

    const FRemoteAddress PropertyAddress = GetPropertyAddress(PropertyName);
    if (PropertyAddress == InvalidRemoteAddress)
    {
        return View;
    }

    View.Data = Runtime->GetMemory().ReadPointer(PropertyAddress + FUnrealLayout::FScriptArray_Data);
    View.ArrayNum = Runtime->GetMemory().Read<int32>(PropertyAddress + FUnrealLayout::FScriptArray_ArrayNum);
    View.ArrayMax = Runtime->GetMemory().Read<int32>(PropertyAddress + FUnrealLayout::FScriptArray_ArrayMax);

    return View;
}

FObjectHandle FObjectHandle::GetArrayElementAsObject(std::string_view PropertyName, int32 Index) const
{
    const FScriptArrayView View = GetArrayProperty(PropertyName);
    if (!View.IsValidIndex(Index) || View.Data == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    return FObjectHandle(Runtime, Runtime->GetMemory().ReadPointer(View.Data + static_cast<uint64>(Index) * sizeof(FRemoteAddress)));
}

std::string FObjectHandle::GetNameProperty(std::string_view PropertyName) const
{
    const FRemoteAddress PropertyAddress = GetPropertyAddress(PropertyName);
    if (PropertyAddress == InvalidRemoteAddress)
    {
        return std::string();
    }

    return Runtime->GetNameString(Runtime->GetMemory().Read<FName>(PropertyAddress));
}

std::string FObjectHandle::GetStringProperty(std::string_view PropertyName) const
{
    const FRemoteAddress PropertyAddress = GetPropertyAddress(PropertyName);
    if (PropertyAddress == InvalidRemoteAddress)
    {
        return std::string();
    }

    return FStringConv::ToNarrow(Runtime->ReadUnrealString(PropertyAddress));
}

FObjectHandle FObjectHandle::FindFunction(std::string_view FunctionName) const
{
    if (!IsValid())
    {
        return FObjectHandle();
    }

    const FRemoteAddress ClassAddress = Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_Class);
    return FObjectHandle(Runtime, Runtime->FindFunctionInClass(ClassAddress, FunctionName));
}

bool FObjectHandle::InvokeFunction(std::string_view FunctionName) const
{
    return InvokeFunction(FunctionName, nullptr, 0);
}

bool FObjectHandle::InvokeFunction(std::string_view FunctionName, void* Parameters, size_t ParametersSize) const
{
    const FObjectHandle Function = FindFunction(FunctionName);
    if (!Function)
    {
        UE_LOG_VERBOSE("CoreUObject", "Function " + std::string(FunctionName) + " was not found on " + GetName());
        return false;
    }

    return InvokeFunctionHandle(Function, Parameters, ParametersSize);
}

bool FObjectHandle::InvokeFunctionHandle(const FObjectHandle& Function, void* Parameters, size_t ParametersSize) const
{
    if (!IsValid() || !Function)
    {
        return false;
    }

    return Runtime->CallProcessEvent(Address, Function.GetAddress(), Parameters, ParametersSize);
}

FRemoteAddress FObjectHandle::GetVirtualFunction(int32 Index) const
{
    if (!IsValid())
    {
        return InvalidRemoteAddress;
    }

    const FRemoteAddress VirtualTable = Runtime->GetMemory().ReadPointer(Address + FUnrealLayout::UObject_VirtualTable);
    if (VirtualTable == InvalidRemoteAddress)
    {
        return InvalidRemoteAddress;
    }

    return Runtime->GetMemory().ReadPointer(VirtualTable + static_cast<uint64>(Index) * sizeof(FRemoteAddress));
}
