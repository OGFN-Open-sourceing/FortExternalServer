#pragma once

#include "FortniteGame/Public/Athena/FortAthenaMatch.h"
#include "Runtime/Engine/Public/Engine/EngineRuntime.h"
#include "Runtime/Engine/Public/Engine/NetworkHooks.h"
#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"
#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Server/Public/ServerSettings.h"

enum class EHostStage : uint8
{
    Idle,
    AttachingToProcess,
    ResolvingEngine,
    InstallingHooks,
    TravellingToAthena,
    PreparingMatch,
    RunningMatch,
    Stopped
};

class FGameServerHost
{
public:
    bool Start();

    void RunUntilStopped();

    void Stop();

    EHostStage GetStage() const;

    const FServerSettings& GetSettings() const;

    FFortAthenaMatch& GetMatch();

    const FProcessAttachment& GetProcess() const;

    std::string DescribeStage() const;

private:
    static FObjectLayout MakeObjectLayoutFromProfile();

    bool ResolveBuildRoot();

    bool AttachToGameProcess();

    bool LoadPrimaryModuleImage();

    bool InitialiseRemoteRuntime();

    bool VerifyBuildVersion();

    bool InstallHooks();

    bool WaitForMainMenu();

    bool BeginTravel();

    bool WaitForAthenaWorld();

    void OnGameThreadTick();

    void ProcessConsoleInput();

    void PrintStatus() const;

    FServerSettings Settings;
    std::wstring BuildRoot;

    FProcessAttachment Process;
    FRemoteMemory Memory;
    FModuleImage ModuleImage;
    FRemoteAllocation CodeArena;
    FGameThreadBridge Bridge;

    FUnrealRuntime UnrealRuntime;
    FEngineRuntime EngineRuntime;
    FNetworkHooks NetworkHooks;
    FFortAthenaMatch Match;

    EHostStage Stage = EHostStage::Idle;
    bool bStopRequested = false;
    uint64 LastStatusTimestamp = 0;
};
