#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/CoreUObject/Public/UObject/UnrealTypes.h"

class FUnrealRuntime;

struct FPropertyInfo
{
    FRemoteAddress PropertyAddress = InvalidRemoteAddress;
    int32 Offset = InvalidIndex;
    int32 ElementSize = 0;
    int32 ArrayDim = 1;
    uint64 PropertyFlags = 0;
    bool bIsBitfield = false;
    uint8 ByteOffset = 0;
    uint8 FieldMask = 0xFF;

    bool IsValid() const
    {
        return Offset >= 0;
    }
};

class FObjectHandle
{
public:
    FObjectHandle() = default;

    FObjectHandle(const FUnrealRuntime* InRuntime, FRemoteAddress InAddress);

    bool IsValid() const;

    explicit operator bool() const;

    bool operator==(const FObjectHandle& Other) const;

    bool operator!=(const FObjectHandle& Other) const;

    FRemoteAddress GetAddress() const;

    const FUnrealRuntime& GetRuntime() const;

    FName GetFName() const;

    std::string GetName() const;

    std::string GetFullName() const;

    std::string GetPathName() const;

    FObjectHandle GetClass() const;

    FObjectHandle GetOuter() const;

    int32 GetInternalIndex() const;

    bool IsA(const FObjectHandle& ClassHandle) const;

    bool IsA(std::string_view ClassPath) const;

    bool HasProperty(std::string_view PropertyName) const;

    FPropertyInfo FindProperty(std::string_view PropertyName) const;

    FRemoteAddress GetPropertyAddress(std::string_view PropertyName) const;

    bool ReadPropertyRaw(std::string_view PropertyName, void* Destination, size_t Size) const;

    bool WritePropertyRaw(std::string_view PropertyName, const void* Source, size_t Size) const;

    template<typename ValueType>
    ValueType GetProperty(std::string_view PropertyName, ValueType DefaultValue = ValueType()) const
    {
        ValueType Value = DefaultValue;
        if (!ReadPropertyRaw(PropertyName, &Value, sizeof(ValueType)))
        {
            return DefaultValue;
        }

        return Value;
    }

    template<typename ValueType>
    bool SetProperty(std::string_view PropertyName, const ValueType& Value) const
    {
        return WritePropertyRaw(PropertyName, &Value, sizeof(ValueType));
    }

    bool GetBoolProperty(std::string_view PropertyName, bool DefaultValue = false) const;

    bool SetBoolProperty(std::string_view PropertyName, bool Value) const;

    FObjectHandle GetObjectProperty(std::string_view PropertyName) const;

    bool SetObjectProperty(std::string_view PropertyName, const FObjectHandle& Value) const;

    FScriptArrayView GetArrayProperty(std::string_view PropertyName) const;

    FObjectHandle GetArrayElementAsObject(std::string_view PropertyName, int32 Index) const;

    std::string GetNameProperty(std::string_view PropertyName) const;

    std::string GetStringProperty(std::string_view PropertyName) const;

    FObjectHandle FindFunction(std::string_view FunctionName) const;

    bool InvokeFunction(std::string_view FunctionName) const;

    bool InvokeFunction(std::string_view FunctionName, void* Parameters, size_t ParametersSize) const;

    bool InvokeFunctionHandle(const FObjectHandle& Function, void* Parameters, size_t ParametersSize) const;

    FRemoteAddress GetVirtualFunction(int32 Index) const;

    FRemoteAddress GetFieldAddress(int32 Offset) const;

    template<typename ValueType>
    ValueType ReadField(int32 Offset, ValueType DefaultValue = ValueType()) const;

    template<typename ValueType>
    bool WriteField(int32 Offset, const ValueType& Value) const;

private:
    const FUnrealRuntime* Runtime = nullptr;
    FRemoteAddress Address = InvalidRemoteAddress;
};
