#pragma once

#include "Runtime/CoreUObject/Public/UObject/Object.h"

class UNetConnection : public UObject
{
public:
    using UObject::UObject;

    FObjectHandle GetPlayerController() const;

    void SetPlayerController(const FObjectHandle& Controller);

    FObjectHandle GetOwningActor() const;

    FObjectHandle GetDriver() const;

    void SetCurrentNetSpeed(int32 Value);

    bool IsInternalAck() const;

    FRemoteAddress GetPlayerIdAddress() const;

    FRemoteAddress GetRequestUrlAddress() const;

    FRemoteAddress GetClientResponseAddress() const;

    std::string GetRequestUrl() const;
};
