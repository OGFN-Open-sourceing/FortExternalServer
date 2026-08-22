#include "Launch/Public/LaunchServer.h"
#include "Runtime/Core/Public/Misc/CommandLine.h"
#include "Server/Public/GameServerHost.h"

#include <cstdio>

namespace
{
    FGameServerHost* GActiveHost = nullptr;

    BOOL WINAPI HandleConsoleControlEvent(DWORD ControlType)
    {
        switch (ControlType)
        {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (GActiveHost != nullptr)
            {
                GActiveHost->Stop();
            }

            return TRUE;

        default:
            return FALSE;
        }
    }
}

namespace FLaunchServer
{
    void PrintBanner()
    {
        FWindowsPlatform::EnableVirtualTerminalProcessing();

        std::printf("\x1b[96m");
        std::printf("FortExternalServer\n");
        std::printf("External Fortnite 3.6 Chapter 1 Season 3 game server\n");
        std::printf("\x1b[0m\n");
    }

    int Run(int ArgumentCount, char** Arguments)
    {
        FCommandLine::Initialize(ArgumentCount, Arguments);

        PrintBanner();

        FGameServerHost Host;
        GActiveHost = &Host;

        ::SetConsoleCtrlHandler(HandleConsoleControlEvent, TRUE);

        if (!Host.Start())
        {
            UE_LOG_FATAL("Launch", "The server failed to start during stage " + Host.DescribeStage());
            FServerLog::Shutdown();

            std::printf("\nPress enter to close.\n");
            std::getchar();

            GActiveHost = nullptr;
            return 1;
        }

        Host.RunUntilStopped();

        GActiveHost = nullptr;
        return 0;
    }
}

int wmain(int ArgumentCount, wchar_t** Arguments)
{
    std::vector<std::string> NarrowStorage;
    NarrowStorage.reserve(static_cast<size_t>(ArgumentCount));

    for (int Index = 0; Index < ArgumentCount; ++Index)
    {
        NarrowStorage.push_back(FStringConv::ToNarrow(Arguments[Index]));
    }

    std::vector<char*> NarrowArguments;
    NarrowArguments.reserve(NarrowStorage.size());

    for (std::string& Argument : NarrowStorage)
    {
        NarrowArguments.push_back(Argument.data());
    }

    return FLaunchServer::Run(ArgumentCount, NarrowArguments.data());
}
