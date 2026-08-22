#include "Server/Public/GameServerHost.h"
#include "Runtime/Core/Public/Misc/Paths.h"

#include <cmath>
#include <conio.h>

namespace
{
    constexpr size_t CodeArenaBytes = 0x100000;
    constexpr int32 BootObjectCountThreshold = 40000;
    constexpr uint64 BootTimeoutMilliseconds = 600000;
    constexpr uint64 MainMenuTimeoutMilliseconds = 300000;
    constexpr uint64 AthenaWorldTimeoutMilliseconds = 300000;
    constexpr uint64 StatusIntervalMilliseconds = 15000;
    constexpr uint64 PumpWarmupTimeoutMilliseconds = 60000;
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

    if (Settings.GetProcess().bAttachToRunningProcess)
    {
        return Process.AttachToRunning(ExecutableName, Settings.GetProcess().AttachTimeoutSeconds * 1000);
    }

    if (!FPaths::FileExists(ExecutablePath))
    {
        UE_LOG_ERROR("Host", "The game executable was not found at " + FStringConv::ToNarrow(ExecutablePath));
        return false;
    }

    const std::wstring WorkingDirectory = FPaths::GetParentDirectory(ExecutablePath);
    const std::wstring Arguments = FStringConv::ToWide(Settings.BuildLaunchArguments());

    if (!Process.LaunchAndAttach(ExecutablePath, Arguments, WorkingDirectory, false))
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

    if (!ModuleImage.Load(Memory, *PrimaryModule))
    {
        UE_LOG_ERROR("Host", "Failed to read the game module image");
        return false;
    }

    return true;
}

bool FGameServerHost::InitialiseRemoteRuntime()
{
    Stage = EHostStage::ResolvingEngine;

    if (!CodeArena.Reserve(Process, Memory, ModuleImage.GetBaseAddress(), CodeArenaBytes))
    {
        return false;
    }

    if (!Bridge.Initialize(Memory, CodeArena))
    {
        UE_LOG_ERROR("Host", "Failed to initialise the game thread bridge");
        return false;
    }

    if (!UnrealRuntime.Initialize(Bridge, ModuleImage))
    {
        return false;
    }

    UE_LOG_DISPLAY("Host", "Waiting for the object array to populate");

    if (!UnrealRuntime.WaitForObjectArray(BootObjectCountThreshold, BootTimeoutMilliseconds))
    {
        UE_LOG_ERROR("Host", "The game never populated its object array");
        return false;
    }

    if (!EngineRuntime.Initialize(UnrealRuntime))
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
        return !Settings.GetEngineExpectations().bEnforceVersionMatch;
    }

    const FEngineVersionInfo& VersionInfo = UnrealRuntime.GetVersionInfo();
    const FEngineExpectations& Expected = Settings.GetEngineExpectations();

    UE_LOG_DISPLAY("Host", "Detected changelist " + std::to_string(VersionInfo.Changelist) + " on engine " + std::to_string(VersionInfo.EngineVersion));

    if (!Expected.bEnforceVersionMatch)
    {
        return true;
    }

    if (VersionInfo.Changelist != 0 && VersionInfo.Changelist != Expected.Changelist)
    {
        UE_LOG_ERROR("Host", "This build reports changelist " + std::to_string(VersionInfo.Changelist) + " but the server targets " + std::to_string(Expected.Changelist));
        UE_LOG_ERROR("Host", "Pass -SkipVersionCheck to run anyway");
        return false;
    }

    return true;
}

bool FGameServerHost::InstallHooks()
{
    Stage = EHostStage::InstallingHooks;

    NetworkHooks.SetFrameTickDelegate([this]() { OnGameThreadTick(); });
    NetworkHooks.SetFrameTickInterval(Settings.GetRuntime().FrameTickIntervalMilliseconds);

    if (!NetworkHooks.Install(EngineRuntime))
    {
        return false;
    }

    UE_LOG_DISPLAY("Host", "Waiting for the game thread to reach the bridge pump");

    if (!Bridge.WaitForGameThreadPump(PumpWarmupTimeoutMilliseconds))
    {
        UE_LOG_ERROR("Host", "The game thread never reached the bridge pump, the net driver tick hook is not firing");
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

        const FObjectHandle LocalPlayer = EngineRuntime.GetLocalPlayer(0);
        if (LocalPlayer && LocalPlayer.GetObjectProperty("PlayerController"))
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

    Match.Initialize(EngineRuntime, NetworkHooks, Settings.GetMatch());

    return Match.TravelToAthena();
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

        EngineRuntime.InvalidateCachedEngine();

        const UWorld World = EngineRuntime.GetWorld();
        if (World && World.GetAuthorityGameMode() && World.GetGameState())
        {
            UE_LOG_DISPLAY("Host", "Athena world is live");
            return Match.PrepareMatch();
        }

        FWindowsPlatform::SleepMilliseconds(500);
    }

    UE_LOG_ERROR("Host", "The Athena world never finished loading");
    return false;
}

void FGameServerHost::OnGameThreadTick()
{
    if (Stage == EHostStage::RunningMatch)
    {
        Match.Tick();
    }
}

bool FGameServerHost::Start()
{
    if (!ResolveBuildRoot())
    {
        return false;
    }

    Settings.Load(BuildRoot);

    FServerLog::Initialize(FPaths::Combine(BuildRoot, Settings.GetLogging().LogFileRelativePath), Settings.GetLogging().Verbosity);

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

    if (!BeginTravel())
    {
        return false;
    }

    if (!WaitForAthenaWorld())
    {
        return false;
    }

    if (Settings.GetRuntime().bDumpObjectsOnStart)
    {
        UnrealRuntime.DumpObjectsToFile(FPaths::Combine(BuildRoot, L"Saved\\ObjectDump.txt"));
    }

    const UNetDriver NetDriver = EngineRuntime.GetWorld().GetNetDriver();
    if (NetDriver && Settings.GetRuntime().MaxTickRate > 0)
    {
        UNetDriver MutableNetDriver = NetDriver;
        MutableNetDriver.SetNetServerMaxTickRate(Settings.GetRuntime().MaxTickRate);
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
    NetworkHooks.Uninstall();
    FServerLog::Shutdown();
}

void FGameServerHost::Stop()
{
    bStopRequested = true;
}

EHostStage FGameServerHost::GetStage() const
{
    return Stage;
}

const FServerSettings& FGameServerHost::GetSettings() const
{
    return Settings;
}

FFortAthenaMatch& FGameServerHost::GetMatch()
{
    return Match;
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

    UE_LOG_VERBOSE("Host", "Players " + std::to_string(Match.GetConnectedPlayerCount()) + ", alive " + std::to_string(Match.GetAlivePlayerCount()) + ", phase " +
        std::to_string(ToUnderlying(Match.GetGamePhase())));
}

void FGameServerHost::ProcessConsoleInput()
{
    if (!Settings.GetRuntime().bEnableConsoleCommands || _kbhit() == 0)
    {
        return;
    }

    const int Key = _getch();

    switch (Key)
    {
    case 'h':
    case 'H':
        UE_LOG_DISPLAY("Console", "S status, P players, B start match now, E end match, D dump objects, Q quit");
        break;

    case 's':
    case 'S':
        UE_LOG_DISPLAY("Console", DescribeStage() + ", players " + std::to_string(Match.GetConnectedPlayerCount()) + ", alive " +
            std::to_string(Match.GetAlivePlayerCount()) + ", phase " + std::to_string(ToUnderlying(Match.GetGamePhase())));
        break;

    case 'p':
    case 'P':
    {
        const std::vector<FTrackedPlayer> Players = Match.GetTrackedPlayers();
        UE_LOG_DISPLAY("Console", std::to_string(Players.size()) + " connected players");

        for (const FTrackedPlayer& Player : Players)
        {
            UE_LOG_DISPLAY("Console", Player.PlayerName + " team " + std::to_string(Player.TeamIndex) + (Player.bEliminated ? " eliminated" : " alive"));
        }

        break;
    }

    case 'b':
    case 'B':
        Match.ForceStartMatch();
        break;

    case 'e':
    case 'E':
        Match.ForceEndMatch();
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
