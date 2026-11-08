#pragma once

#include "Runtime/Engine/Public/EngineRuntime.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

class FNetworkHooks
{
public:
    bool Install(FEngineRuntime& InEngineRuntime);

    void Uninstall();

    bool IsInstalled() const;

private:
    bool InstallReplicationTick(const UNetDriver& NetDriver);

    FRemoteAddress EmitServerReplicateActorsRoutine() const;

    FEngineRuntime* EngineRuntime = nullptr;

    bool bInstalled = false;
};
