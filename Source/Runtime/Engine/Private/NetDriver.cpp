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
