#pragma once

#include "Runtime/Engine/Public/EngineRuntime.h"

class FNetworkHooks
{
public:
    bool Install(FEngineRuntime& InEngineRuntime);

    void Uninstall();

    bool IsInstalled() const;

    void DisableGarbageCollection() const;

private:
    bool InstallReplicationTick();

    FRemoteAddress EmitServerReplicateActorsRoutine() const;

    FEngineRuntime* EngineRuntime = nullptr;

    bool bInstalled = false;
};
