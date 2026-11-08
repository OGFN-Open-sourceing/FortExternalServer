#include "Runtime/Engine/Classes/Engine/NetConnection.h"

FObjectHandle UNetConnection::GetPlayerController() const
{
    return Object.GetObjectProperty("PlayerController");
}

void UNetConnection::SetPlayerController(const FObjectHandle& Controller)
{
    Object.SetObjectProperty("PlayerController", Controller);
}

FObjectHandle UNetConnection::GetOwningActor() const
{
    return Object.GetObjectProperty("OwningActor");
}

void UNetConnection::SetCurrentNetSpeed(int32 Value)
{
    Object.SetProperty<int32>("CurrentNetSpeed", Value);
}

bool UNetConnection::IsInternalAck() const
{
    return Object.GetBoolProperty("InternalAck", false);
}

FRemoteAddress UNetConnection::GetPlayerIdAddress() const
{
    return Object.GetPropertyAddress("PlayerId");
}

FRemoteAddress UNetConnection::GetRequestUrlAddress() const
{
    return Object.GetPropertyAddress("RequestURL");
}

FRemoteAddress UNetConnection::GetClientResponseAddress() const
{
    return Object.GetPropertyAddress("ClientResponse");
}

std::string UNetConnection::GetRequestUrl() const
{
    return Object.GetStringProperty("RequestURL");
}
