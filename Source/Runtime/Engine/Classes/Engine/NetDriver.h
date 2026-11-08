#pragma once

#include "Runtime/Engine/Classes/Engine/NetConnection.h"

class UNetDriver : public UObject
{
public:
    using UObject::UObject;

    FScriptArrayView GetClientConnections() const;

    UNetConnection GetClientConnection(int32 Index) const;

    int32 GetClientConnectionCount() const;

    FObjectHandle GetReplicationDriver() const;

    FObjectHandle GetServerConnection() const;

    void SetNetServerMaxTickRate(int32 Value);
};
