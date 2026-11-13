#include "Programs/FortExternalServer/Public/GameServerHost.h"
#include "FortniteGame/Public/Versioning/FortBuildProfile.h"
#include "Runtime/Core/Public/Misc/Paths.h"

#include <cmath>
#include <conio.h>

namespace
{
    FRemoteAddress ToImageAddress(const FModuleImage& Image, uint32 RelativeOffset)
    {
        return RelativeOffset != 0 ? Image.GetBaseAddress() + RelativeOffset : InvalidRemoteAddress;
    }

    FUnrealGlobals MakeUnrealGlobals(const FModuleImage& Image, const FEngineOffsetProfile& Offsets)
    {
        FUnrealGlobals Globals;
        Globals.ObjectArray = ToImageAddress(Image, Offsets.GObjects);
        Globals.NameToString = ToImageAddress(Image, Offsets.AppendString);
        Globals.ProcessEvent = ToImageAddress(Image, Offsets.ProcessEvent);
        Globals.ProcessEventVirtualIndex = Offsets.ProcessEventIdx;
        Globals.MemoryRealloc = ToImageAddress(Image, Offsets.Realloc);
        Globals.StaticFindObject = ToImageAddress(Image, Offsets.StaticFindObject);
        Globals.StaticLoadObject = ToImageAddress(Image, Offsets.StaticLoadObject);
        Globals.NameConstructor = ToImageAddress(Image, Offsets.FNameConstructor);
        return Globals;
    }

    FEngineFunctions MakeEngineFunctions(const FModuleImage& Image, const FEngineFunctionProfile& Offsets)
    {
        FEngineFunctions Functions;
        Functions.NetDriverTickFlush = ToImageAddress(Image, Offsets.NetDriverTickFlush);
        Functions.EngineTick = ToImageAddress(Image, Offsets.EngineTick);
        Functions.AbilitySystemGiveAbility = ToImageAddress(Image, Offsets.AbilitySystemGiveAbility);
        Functions.AbilitySystemInternalTryActivateAbility = ToImageAddress(Image, Offsets.AbilitySystemInternalTryActivateAbility);
        Functions.GameplayAbilitySpecConstructor = ToImageAddress(Image, Offsets.GameplayAbilitySpecConstructor);
        Functions.EngineCreateNetDriver = ToImageAddress(Image, Offsets.EngineCreateNetDriver);
        Functions.NetDriverSetWorld = ToImageAddress(Image, Offsets.NetDriverSetWorld);
        Functions.GameSessionKickPlayer = ToImageAddress(Image, Offsets.GameSessionKickPlayer);
        return Functions;
    }

    FEngineNetworkLayout MakeNetworkLayout(const FNetworkLayoutProfile& Profile)
    {
        FEngineNetworkLayout Layout;
        Layout.NetDriverReplicationDriver = Profile.NetDriverReplicationDriver;
        Layout.ServerReplicateActorsVirtualIndex = Profile.ServerReplicateActorsVirtualIndex;
        Layout.InitListenVirtualIndex = Profile.InitListenVirtualIndex;
        return Layout;
    }

    constexpr size_t CodeArenaBytes = 0x400000;
    constexpr int32 BootObjectCountThreshold = 40000;
    constexpr uint64 BootTimeoutMilliseconds = 600000;
    constexpr uint64 MainMenuTimeoutMilliseconds = 300000;
    constexpr uint64 AthenaWorldTimeoutMilliseconds = 300000;
    constexpr uint64 StatusIntervalMilliseconds = 15000;
    constexpr uint64 PumpWarmupTimeoutMilliseconds = 60000;
    constexpr uint32 EngineScanIntervalMilliseconds = 50;
    constexpr uint64 GameThreadHoldTimeoutMilliseconds = 30000;
    constexpr uint64 AttachTimeoutMilliseconds = 180000;
    constexpr uint64 ImageReadyTimeoutMilliseconds = 120000;
    constexpr size_t MinimumExecutablePopulationPercent = 98;
}

bool FGameServerHost::ResolveBuildRoot()
{
    const std::wstring ExecutableDirectory = FPaths::GetExecutableDirectory();

    BuildRoot = FPaths::FindBuildRoot(ExecutableDirectory);

    if (BuildRoot.empty())
    {
        UE_LOG_ERROR("Host", "FortExternalServer.exe must sit in the build folder that contains FortniteGame and Engine");
        return false;
    }

    UE_LOG_DISPLAY("Host", "Build root resolved to " + FStringConv::ToNarrow(BuildRoot));
    return true;
}

bool FGameServerHost::AttachToGameProcess()
{
    Stage = EHostStage::AttachingToProcess;

    FWindowsPlatform::EnableDebugPrivilege();

    const std::wstring ExecutablePath = Settings.GetGameExecutablePath(BuildRoot);
    const std::wstring ExecutableName = Settings.GetGameExecutableName();

    if (Settings.ShouldAttachToRunningProcess())
    {
        GameLogMirror.Start(FGameLogMirror::GetDefaultLogFilePath(), false);
        return Process.AttachToRunning(ExecutableName, AttachTimeoutMilliseconds, Settings.ShouldTrackProcessId());
    }

    if (!FPaths::FileExists(ExecutablePath))
    {
        UE_LOG_ERROR("Host", "The game executable was not found at " + FStringConv::ToNarrow(ExecutablePath));
        return false;
    }

    const std::wstring WorkingDirectory = FPaths::GetParentDirectory(ExecutablePath);
    const std::wstring Arguments = FStringConv::ToWide(Settings.BuildLaunchArguments());

    GameLogMirror.Start(FGameLogMirror::GetDefaultLogFilePath(), true);

    if (!Process.LaunchAndAttach(ExecutablePath, Arguments, WorkingDirectory, false, Settings.IsHeadless(), Settings.ShouldTrackProcessId()))
    {
        return false;
    }

    return true;
}

bool FGameServerHost::LoadPrimaryModuleImage()
{
    Memory.Initialize(&Process);

    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + BootTimeoutMilliseconds;

    const FRemoteModuleInfo* PrimaryModule = nullptr;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (!Process.IsAlive())
        {
            UE_LOG_ERROR("Host", "The game process exited before it finished booting");
            return false;
        }

        if (Process.RefreshModules())
        {
            PrimaryModule = Process.GetPrimaryModule();
            if (PrimaryModule != nullptr && PrimaryModule->ImageSize > 0)
            {
                break;
            }
        }

        FWindowsPlatform::SleepMilliseconds(500);
    }

    if (PrimaryModule == nullptr)
    {
        UE_LOG_ERROR("Host", "The game module never appeared in the target process");
        return false;
    }

    UE_LOG_DISPLAY("Host", "Game module at " + FStringConv::ToHex(PrimaryModule->BaseAddress) + " size " + std::to_string(PrimaryModule->ImageSize));

    const uint64 ImageDeadline = FWindowsPlatform::GetTimeMilliseconds() + ImageReadyTimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < ImageDeadline)
    {
        if (!Process.IsAlive())
        {
            UE_LOG_ERROR("Host", "The game process exited while its image was being read");
            return false;
        }

        Memory.InvalidateCache();

        if (!ModuleImage.Load(Memory, *PrimaryModule))
        {
            UE_LOG_ERROR("Host", "Failed to read the game module image");
            return false;
        }

        if (ModuleImage.IsExecutableImagePopulated(MinimumExecutablePopulationPercent))
        {
            UE_LOG_DISPLAY("Host", "Game module image is fully mapped");
            return true;
        }

        UE_LOG_DISPLAY("Host", "Waiting for the game module image to finish mapping");
        FWindowsPlatform::SleepMilliseconds(500);
    }

    UE_LOG_ERROR("Host", "The game module image never became fully readable");
    return false;
}

bool FGameServerHost::InitialiseRemoteRuntime()
{
    Stage = EHostStage::ResolvingEngine;

    if (!CodeArena.Reserve(Process, Memory, ModuleImage.GetBaseAddress(), CodeArenaBytes))
    {
        return false;
    }

    if (!Bridge.Initialize(Memory, CodeArena, Process.GetMainThreadId()))
    {
        UE_LOG_ERROR("Host", "Failed to initialise the game thread bridge");
        return false;
    }

    const FFortBuildProfile& RedirectProfile = GetActiveBuildProfile();
    if (!HttpRedirect.Install(Bridge, ToImageAddress(ModuleImage, RedirectProfile.EngineOffsets.HttpRequestVirtualTable),
            ToImageAddress(ModuleImage, RedirectProfile.EngineFunctions.HttpRequestSetURL), Settings.GetBackendUrl(), BootTimeoutMilliseconds))
    {
        return false;
    }

    const FFortBuildProfile& Profile = GetActiveBuildProfile();

    UnrealRuntime.SetObjectArrayLayout(Profile.ObjectLayout.bChunkedObjectArray, Profile.ObjectLayout.ObjectsPerChunk);

    if (!UnrealRuntime.Initialize(Bridge, ModuleImage, MakeUnrealGlobals(ModuleImage, Profile.EngineOffsets)))
    {
        return false;
    }

    UE_LOG_DISPLAY("Host", "Waiting for the object array to populate");

    if (!UnrealRuntime.WaitForObjectArray(BootObjectCountThreshold, BootTimeoutMilliseconds))
    {
        UE_LOG_ERROR("Host", "The game never populated its object array");
        return false;
    }

    FEngineGlobals EngineGlobals;
    EngineGlobals.GWorld = ToImageAddress(ModuleImage, Profile.EngineOffsets.GWorld);

    if (!EngineRuntime.Initialize(UnrealRuntime, EngineGlobals, MakeEngineFunctions(ModuleImage, Profile.EngineFunctions), MakeNetworkLayout(Profile.NetworkLayout)))
    {
        return false;
    }

    return true;
}

bool FGameServerHost::VerifyBuildVersion()
{
    if (!UnrealRuntime.RefreshVersionInfo())
    {
        UE_LOG_WARNING("Host", "The engine version string could not be read from the game");
        return !Settings.ShouldEnforceVersionMatch();
    }

    const FEngineVersionInfo& VersionInfo = UnrealRuntime.GetVersionInfo();
    const int32 ExpectedChangelist = GetActiveBuildProfile().Changelist;

    UE_LOG_DISPLAY("Host", "Detected changelist " + std::to_string(VersionInfo.Changelist) + " on engine " + std::to_string(VersionInfo.EngineVersion));

    if (!Settings.ShouldEnforceVersionMatch())
    {
        return true;
    }

    if (VersionInfo.Changelist != 0 && VersionInfo.Changelist != ExpectedChangelist)
    {
        UE_LOG_ERROR("Host", "This build reports changelist " + std::to_string(VersionInfo.Changelist) + " but the server targets " + std::to_string(ExpectedChangelist));
        UE_LOG_ERROR("Host", "Set bSkipVersionCheck in Configuration.h to run anyway");
        return false;
    }

    return true;
}

FObjectHandle FGameServerHost::FindEngineThroughWorld(int32& OutEngineTickIndex) const
{
    const FEngineOffsetProfile& Offsets = GetActiveBuildProfile().EngineOffsets;
    if (Offsets.GWorld == 0 || Offsets.WorldOwningGameInstance < 0)
    {
        return FObjectHandle();
    }

    const FRemoteMemory& RemoteMemory = UnrealRuntime.GetMemory();
    const FRemoteAddress World = RemoteMemory.ReadPointer(ToImageAddress(ModuleImage, Offsets.GWorld));
    const FRemoteAddress GameInstance = World != InvalidRemoteAddress ? RemoteMemory.ReadPointer(World + static_cast<uint64>(Offsets.WorldOwningGameInstance)) : InvalidRemoteAddress;
    const FRemoteAddress Engine = GameInstance != InvalidRemoteAddress ? RemoteMemory.ReadPointer(GameInstance + FUnrealLayout::UObject_Outer) : InvalidRemoteAddress;

    if (Engine == InvalidRemoteAddress || Engine == 0)
    {
        return FObjectHandle();
    }

    OutEngineTickIndex = Bridge.FindVirtualIndex(Engine, EngineRuntime.GetFunctions().EngineTick);
    return OutEngineTickIndex == InvalidIndex ? FObjectHandle() : UnrealRuntime.MakeHandle(Engine);
}

bool FGameServerHost::InstallHooks()
{
    Stage = EHostStage::InstallingHooks;

    int32 EngineTickIndex = InvalidIndex;
    FObjectHandle Engine;
    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + BootTimeoutMilliseconds;

    while (!Engine)
    {
        if (!Process.IsAlive() || FWindowsPlatform::GetTimeMilliseconds() >= Deadline)
        {
            UE_LOG_ERROR("Host", "No engine object implements UFortEngine::Tick");
            return false;
        }

        Engine = FindEngineThroughWorld(EngineTickIndex);
        if (!Engine && GetActiveBuildProfile().EngineOffsets.WorldOwningGameInstance < 0)
        {
            Engine = UnrealRuntime.FindInstanceImplementingVirtual(EngineRuntime.GetFunctions().EngineTick, EngineTickIndex);
        }

        if (!Engine)
        {
            FWindowsPlatform::SleepMilliseconds(EngineScanIntervalMilliseconds);
        }
    }

    EngineRuntime.SetEngine(Engine);

    if (!Bridge.InstallGameThreadPump(Engine.GetAddress(), EngineTickIndex))
    {
        return false;
    }

    UE_LOG_DISPLAY("Host", "Waiting for the game thread to reach the bridge pump");

    if (!Bridge.WaitForGameThreadPump(PumpWarmupTimeoutMilliseconds))
    {
        UE_LOG_ERROR("Host", "The game thread never reached the bridge pump through UFortEngine::Tick");
        return false;
    }

    UE_LOG_DISPLAY("Host", "Game thread pump is live");
    return true;
}

bool FGameServerHost::WaitForMainMenu()
{
    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + MainMenuTimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (!Process.IsAlive())
        {
            return false;
        }

        Bridge.ServicePendingHooks();

        if (HasLocalPlayerController())
        {
            UE_LOG_DISPLAY("Host", "The game reached the front end");
            return true;
        }

        FWindowsPlatform::SleepMilliseconds(500);
    }

    UE_LOG_ERROR("Host", "The game never produced a local player controller");
    return false;
}

bool FGameServerHost::BeginTravel()
{
    Stage = EHostStage::TravellingToAthena;
    const FGameThreadHoldScope Hold(Bridge, GameThreadHoldTimeoutMilliseconds);
    return FFortniteGameModule::Get().TravelToAthena();
}

bool FGameServerHost::HasLocalPlayerController()
{
    const FGameThreadHoldScope Hold(Bridge, GameThreadHoldTimeoutMilliseconds);
    const FObjectHandle LocalPlayer = EngineRuntime.GetLocalPlayer(0);
    return LocalPlayer && LocalPlayer.GetObjectProperty("PlayerController");
}

bool FGameServerHost::ListenOnAthenaWorld()
{
    const FGameThreadHoldScope Hold(Bridge, GameThreadHoldTimeoutMilliseconds);
    const UWorld World = EngineRuntime.GetWorld();
    if (!World || World.GetName() != Settings.GetMatch().MapName || !World.GetAuthorityGameMode())
    {
        return false;
    }

    return World.GetNetDriver() || EngineRuntime.Listen(World, Settings.GetPort());
}

bool FGameServerHost::WaitForAthenaWorld()
{
    Stage = EHostStage::PreparingMatch;

    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + AthenaWorldTimeoutMilliseconds;

    while (FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        if (!Process.IsAlive())
        {
            return false;
        }

        Bridge.ServicePendingHooks();

        if (ListenOnAthenaWorld())
        {
            UE_LOG_DISPLAY("Host", "Athena world is live and listening");
            const FGameThreadHoldScope Hold(Bridge, GameThreadHoldTimeoutMilliseconds);
            return NetworkHooks.Install(EngineRuntime) && FFortniteGameModule::Get().PrepareMatch();
        }

        FWindowsPlatform::SleepMilliseconds(500);
    }

    UE_LOG_ERROR("Host", "The Athena world never finished loading");
    return false;
}

bool FGameServerHost::Start()
{
    if (!ResolveBuildRoot())
    {
        return false;
    }

    Settings.Resolve();

    FServerLog::Initialize(FPaths::Combine(BuildRoot, Settings.GetLogFileRelativePath()), Settings.GetLogVerbosity());

    FWindowsPlatform::SetConsoleTitleText(L"FortExternalServer");

    Settings.LogResolvedConfiguration();

    if (!AttachToGameProcess())
    {
        return false;
    }

    if (!LoadPrimaryModuleImage())
    {
        return false;
    }

    if (!InitialiseRemoteRuntime())
    {
        return false;
    }

    if (!InstallHooks())
    {
        return false;
    }

    if (!VerifyBuildVersion())
    {
        return false;
    }

    if (!WaitForMainMenu())
    {
        return false;
    }

    if (!FFortniteGameModule::Get().StartupModule(EngineRuntime, Settings.GetMatch()))
    {
        return false;
    }

    if (!BeginTravel())
    {
        return false;
    }

    if (!WaitForAthenaWorld())
    {
        return false;
    }

    UNetDriver NetDriver = EngineRuntime.GetWorld().GetNetDriver();
    if (NetDriver)
    {
        if (Settings.GetMaxTickRate() > 0)
        {
            NetDriver.SetNetServerMaxTickRate(Settings.GetMaxTickRate());
        }

        if (Settings.GetMaxClientRate() > 0)
        {
            NetDriver.SetMaxClientRate(Settings.GetMaxClientRate());
        }
    }

    Stage = EHostStage::RunningMatch;
    UE_LOG_DISPLAY("Host", "Server is running, press H for the command list");
    return true;
}

void FGameServerHost::RunUntilStopped()
{
    LastStatusTimestamp = FWindowsPlatform::GetTimeMilliseconds();

    while (!bStopRequested)
    {
        if (!Process.IsAlive())
        {
            UE_LOG_ERROR("Host", "The game process exited");
            break;
        }

        Bridge.ServicePendingHooks();
        ProcessConsoleInput();

        const uint64 Now = FWindowsPlatform::GetTimeMilliseconds();
        if (Now - LastStatusTimestamp >= StatusIntervalMilliseconds)
        {
            LastStatusTimestamp = Now;
            PrintStatus();
        }

        FWindowsPlatform::YieldThread();
    }

    Stage = EHostStage::Stopped;
    CloseTrackedProcess();
    FFortniteGameModule::Get().ShutdownModule();
    NetworkHooks.Uninstall();
    HttpRedirect.Uninstall();
    GameLogMirror.Stop();
    FServerLog::Shutdown();
}

void FGameServerHost::Stop()
{
    bStopRequested = true;
}

void FGameServerHost::CloseTrackedProcess()
{
    if (Settings.ShouldTrackProcessId() && Process.IsAlive())
    {
        UE_LOG_DISPLAY("Host", "Closing tracked process id " + std::to_string(Process.GetProcessId()));
        Process.Terminate();
    }
}

EHostStage FGameServerHost::GetStage() const
{
    return Stage;
}

const FServerSettings& FGameServerHost::GetSettings() const
{
    return Settings;
}

const FProcessAttachment& FGameServerHost::GetProcess() const
{
    return Process;
}

std::string FGameServerHost::DescribeStage() const
{
    switch (Stage)
    {
    case EHostStage::AttachingToProcess:
        return "Attaching to the game process";
    case EHostStage::ResolvingEngine:
        return "Resolving engine globals";
    case EHostStage::InstallingHooks:
        return "Installing listen server hooks";
    case EHostStage::TravellingToAthena:
        return "Travelling to Athena";
    case EHostStage::PreparingMatch:
        return "Preparing the match";
    case EHostStage::RunningMatch:
        return "Running";
    case EHostStage::Stopped:
        return "Stopped";
    default:
        return "Idle";
    }
}

void FGameServerHost::PrintStatus() const
{
    if (Stage != EHostStage::RunningMatch)
    {
        return;
    }

    UE_LOG_VERBOSE("Host", "Connections " + std::to_string(EngineRuntime.GetWorld().GetNetDriver().GetClientConnectionCount()));
}

void FGameServerHost::ProcessConsoleInput()
{
    if (!Settings.AreConsoleCommandsEnabled() || _kbhit() == 0)
    {
        return;
    }

    const int Key = _getch();

    switch (Key)
    {
    case 'h':
    case 'H':
        UE_LOG_DISPLAY("Console", "S status, D dump objects, Q quit");
        break;

    case 's':
    case 'S':
        UE_LOG_DISPLAY("Console", DescribeStage() + ", connections " + std::to_string(EngineRuntime.GetWorld().GetNetDriver().GetClientConnectionCount()));
        break;

    case 'd':
    case 'D':
        UnrealRuntime.DumpObjectsToFile(FPaths::Combine(BuildRoot, L"Saved\\ObjectDump.txt"));
        break;

    case 'q':
    case 'Q':
        UE_LOG_DISPLAY("Console", "Shutting down");
        Stop();
        break;

    default:
        break;
    }
}
