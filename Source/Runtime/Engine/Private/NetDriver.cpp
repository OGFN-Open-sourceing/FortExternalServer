#include "Runtime/Engine/Classes/Engine/NetDriver.h"

FScriptArrayView UNetDriver::GetClientConnections() const
{
    return Object.GetArrayProperty("ClientConnections");
}

UNetConnection UNetDriver::GetClientConnection(int32 Index) const
{
    return UNetConnection(Object.GetArrayElementAsObject("ClientConnections", Index));
}

int32 UNetDriver::GetClientConnectionCount() const
{
    return GetClientConnections().ArrayNum;
}

FObjectHandle UNetDriver::GetReplicationDriver() const
{
    return Object.GetObjectProperty("ReplicationDriver");
}

FObjectHandle UNetDriver::GetServerConnection() const
{
    return Object.GetObjectProperty("ServerConnection");
}

void UNetDriver::SetNetServerMaxTickRate(int32 Value)
{
    Object.SetProperty<int32>("NetServerMaxTickRate", Value);
}

int32 UNetDriver::GetMaxClientRate() const
{
    return Object.GetProperty<int32>("MaxClientRate", 0);
}

void UNetDriver::SetMaxClientRate(int32 Value)
{
    Object.SetProperty<int32>("MaxClientRate", Value);
    Object.SetProperty<int32>("MaxInternetClientRate", Value);
}
