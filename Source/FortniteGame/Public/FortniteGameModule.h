#pragma once

#include "FortniteGame/Public/Athena/AthenaMatchSettings.h"
#include "Runtime/CoreUObject/Public/UObject/NativeFunctionOverrides.h"
#include "Runtime/Engine/Public/NetworkHooks.h"

class FFortniteGameModule
{
public:
    static FFortniteGameModule& Get();

    bool StartupModule(FEngineRuntime& InEngineRuntime, const FNetworkHooks& InNetworkHooks, const FAthenaMatchSettings& InMatchSettings);

    void ShutdownModule();

    bool TravelToAthena() const;

    FEngineRuntime& GetEngineRuntime() const;

    const FAthenaMatchSettings& GetMatchSettings() const;

    const FNetworkHooks& GetNetworkHooks() const;

private:
    bool RegisterNativeOverrides();

    bool PrepareGameModeDefaults() const;

    FEngineRuntime* EngineRuntime = nullptr;
    const FNetworkHooks* NetworkHooks = nullptr;
    FAthenaMatchSettings MatchSettings;
    FNativeFunctionOverrides NativeOverrides;
};
