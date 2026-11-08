#pragma once

#include "FortniteGame/Public/FortniteGameModule.h"
#include "Runtime/Engine/Public/EngineRuntime.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Public/NetworkHooks.h"
#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"
#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Programs/FortExternalServer/Public/ServerSettings.h"
#include "Programs/FortExternalServer/Public/GameLogMirror.h"
#include "Runtime/Online/HTTP/Public/HttpRedirect.h"

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

    const FProcessAttachment& GetProcess() const;

    std::string DescribeStage() const;

private:
    bool ResolveBuildRoot();

    bool AttachToGameProcess();

    bool HasLocalPlayerController();

    bool ListenOnAthenaWorld();

    bool LoadPrimaryModuleImage();

    bool InitialiseRemoteRuntime();

    bool VerifyBuildVersion();

    bool InstallHooks();

    bool WaitForMainMenu();

    bool BeginTravel();

    bool WaitForAthenaWorld();

    void ProcessConsoleInput();

    void PrintStatus() const;

    FServerSettings Settings;
    FGameLogMirror GameLogMirror;
    FHttpRedirect HttpRedirect;
    std::wstring BuildRoot;

    FProcessAttachment Process;
    FRemoteMemory Memory;
    FModuleImage ModuleImage;
    FRemoteAllocation CodeArena;
    FGameThreadBridge Bridge;

    FUnrealRuntime UnrealRuntime;
    FEngineRuntime EngineRuntime;
    FNetworkHooks NetworkHooks;

    EHostStage Stage = EHostStage::Idle;
    bool bStopRequested = false;
    uint64 LastStatusTimestamp = 0;
};
