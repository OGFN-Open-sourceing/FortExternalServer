#pragma once

#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"

class FObjectWrapper
{
public:
    FObjectWrapper() = default;

    explicit FObjectWrapper(const FObjectHandle& InObject)
        : Object(InObject)
    {
    }

    const FObjectHandle& GetObject() const
    {
        return Object;
    }

    FRemoteAddress GetAddress() const
    {
        return Object.GetAddress();
    }

    const FUnrealRuntime& GetRuntime() const
    {
        return Object.GetRuntime();
    }

    bool IsValid() const
    {
        return Object.IsValid();
    }

    explicit operator bool() const
    {
        return IsValid();
    }

    bool operator==(const FObjectWrapper& Other) const
    {
        return Object == Other.Object;
    }

    bool operator!=(const FObjectWrapper& Other) const
    {
        return Object != Other.Object;
    }

    std::string GetName() const
    {
        return Object.GetName();
    }

protected:
    FObjectHandle Object;
};
