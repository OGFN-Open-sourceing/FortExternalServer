#pragma once

#include "FortniteGame/Public/Athena/AthenaMatchSettings.h"
#include "Runtime/CoreUObject/Public/UObject/NativeFunctionOverrides.h"
#include "Runtime/Engine/Public/EngineRuntime.h"

class FFortniteGameModule
{
public:
    static FFortniteGameModule& Get();

    bool StartupModule(FEngineRuntime& InEngineRuntime, const FAthenaMatchSettings& InMatchSettings);

    void ShutdownModule();

    bool TravelToAthena() const;

    FEngineRuntime& GetEngineRuntime() const;

    const FAthenaMatchSettings& GetMatchSettings() const;

private:
    bool RegisterNativeOverrides();

    bool PrepareGameModeDefaults() const;

    FEngineRuntime* EngineRuntime = nullptr;
    FAthenaMatchSettings MatchSettings;
    FNativeFunctionOverrides NativeOverrides;
};
