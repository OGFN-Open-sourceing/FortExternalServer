#include "Runtime/Engine/Public/NetworkHooks.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

namespace
{
    constexpr int32 RoutineFrameSize = 0x28;
}

bool FNetworkHooks::Install(FEngineRuntime& InEngineRuntime)
{
    if (bInstalled)
    {
        return true;
    }

    EngineRuntime = &InEngineRuntime;

    if (!InstallReplicationTick())
    {
        return false;
    }

    bInstalled = true;
    return true;
}

void FNetworkHooks::Uninstall()
{
    if (!bInstalled || EngineRuntime == nullptr)
    {
        return;
    }

    EngineRuntime->GetUnrealRuntime().GetBridge().RemoveAllHooks();
    bInstalled = false;
}

bool FNetworkHooks::IsInstalled() const
{
    return bInstalled;
}

FRemoteAddress FNetworkHooks::EmitServerReplicateActorsRoutine() const
{
    const FEngineNetworkLayout& Layout = EngineRuntime->GetNetworkLayout();

    FX64Emitter Emitter;
    const FX64Emitter::FLabelId Skip = Emitter.CreateLabel();

    Emitter.SubRegisterImmediate(EX64Register::Rsp, RoutineFrameSize);
    Emitter.LoadRegisterFromMemory(EX64Register::Rcx, EX64Register::Rcx, Layout.NetDriverReplicationDriver);
    Emitter.TestRegisterRegister(EX64Register::Rcx, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, Skip);
    Emitter.LoadRegisterFromMemory(EX64Register::Rax, EX64Register::Rcx, 0);
    Emitter.LoadRegisterFromMemory(EX64Register::Rax, EX64Register::Rax, Layout.ServerReplicateActorsVirtualIndex * static_cast<int32>(sizeof(FRemoteAddress)));
    Emitter.CallRegister(EX64Register::Rax);
    Emitter.BindLabel(Skip);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, RoutineFrameSize);
    Emitter.EmitReturn();

    return EngineRuntime->GetUnrealRuntime().GetBridge().EmitRoutine(Emitter);
}

bool FNetworkHooks::InstallReplicationTick()
{
    const FEngineFunctions& Functions = EngineRuntime->GetFunctions();
    const FEngineNetworkLayout& Layout = EngineRuntime->GetNetworkLayout();

    if (Functions.NetDriverTickFlush == InvalidRemoteAddress || Layout.ServerReplicateActorsVirtualIndex == InvalidIndex)
    {
        UE_LOG_ERROR("Network", "UNetDriver::TickFlush or the replication graph slot is missing from the build profile");
        return false;
    }

    const FRemoteAddress Routine = EmitServerReplicateActorsRoutine();
    if (Routine == InvalidRemoteAddress)
    {
        UE_LOG_ERROR("Network", "Failed to emit the replication routine");
        return false;
    }

    if (!EngineRuntime->GetUnrealRuntime().GetBridge().InstallPrologueCall(Functions.NetDriverTickFlush, Routine))
    {
        return false;
    }

    UE_LOG_DISPLAY("Network", "UNetDriver::TickFlush now drives UReplicationDriver::ServerReplicateActors");
    return true;
}

void FNetworkHooks::DisableGarbageCollection() const
{
    const FRemoteAddress CollectGarbage = EngineRuntime->GetFunctions().CollectGarbage;
    if (CollectGarbage == InvalidRemoteAddress)
    {
        return;
    }

    EngineRuntime->GetUnrealRuntime().GetBridge().InstallConstantHook(CollectGarbage, 0);
}
