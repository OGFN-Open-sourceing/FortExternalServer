#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

FObjectHandle UWorld::GetGameState() const
{
    return Object.GetObjectProperty("GameState");
}

FObjectHandle UWorld::GetAuthorityGameMode() const
{
    return Object.GetObjectProperty("AuthorityGameMode");
}

UNetDriver UWorld::GetNetDriver() const
{
    return UNetDriver(Object.GetObjectProperty("NetDriver"));
}

FObjectHandle UWorld::GetOwningGameInstance() const
{
    return Object.GetObjectProperty("OwningGameInstance");
}

std::vector<FObjectHandle> UWorld::GetAllActorsOfClass(const FObjectHandle& ClassHandle) const
{
    std::vector<FObjectHandle> Results;

    if (!IsValid() || !ClassHandle)
    {
        return Results;
    }

    const FUnrealRuntime& Runtime = Object.GetRuntime();

    const FObjectHandle GameplayStatics = Runtime.GetClassDefaultObject("GameplayStatics");
    if (!GameplayStatics)
    {
        return Results;
    }

    struct FGetAllActorsOfClassParameters
    {
        FRemoteAddress WorldContextObject = InvalidRemoteAddress;
        FRemoteAddress ActorClass = InvalidRemoteAddress;
        FRemoteAddress OutActorsData = InvalidRemoteAddress;
        int32 OutActorsNum = 0;
        int32 OutActorsMax = 0;
    } Parameters;

    Parameters.WorldContextObject = Object.GetAddress();
    Parameters.ActorClass = ClassHandle.GetAddress();

    if (!GameplayStatics.InvokeFunction("GetAllActorsOfClass", &Parameters, sizeof(Parameters)))
    {
        return Results;
    }

    Results.reserve(static_cast<size_t>(std::max(Parameters.OutActorsNum, 0)));

    for (int32 Index = 0; Index < Parameters.OutActorsNum; ++Index)
    {
        const FRemoteAddress ActorAddress = Runtime.GetMemory().ReadPointer(Parameters.OutActorsData + static_cast<uint64>(Index) * sizeof(FRemoteAddress));
        if (ActorAddress == InvalidRemoteAddress)
        {
            continue;
        }

        Results.push_back(Runtime.MakeHandle(ActorAddress));
    }

    if (Parameters.OutActorsData != InvalidRemoteAddress)
    {
        Runtime.GetBridge().CallFunction(Runtime.GetGlobals().MemoryRealloc, { Parameters.OutActorsData, 0, 0 });
    }

    return Results;
}

FObjectHandle UWorld::SpawnActor(const FObjectHandle& ClassHandle, const FTransform& Transform, const FObjectHandle& Owner) const
{
    if (!IsValid() || !ClassHandle)
    {
        return FObjectHandle();
    }

    return UGameplayStatics::SpawnActor(*this, ClassHandle, Transform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, AActor(Owner)).GetObject();
}

bool UWorld::ExecuteConsoleCommand(const std::string& Command) const
{
    if (!IsValid())
    {
        return false;
    }

    const FUnrealRuntime& Runtime = Object.GetRuntime();

    const FObjectHandle KismetSystemLibrary = Runtime.GetClassDefaultObject("KismetSystemLibrary");
    if (!KismetSystemLibrary)
    {
        return false;
    }

    const std::wstring WideCommand = FStringConv::ToWide(Command);
    const FRemoteAddress CommandStorage = Runtime.AllocateTransientWideString(WideCommand);

    struct FExecuteConsoleCommandParameters
    {
        FRemoteAddress WorldContextObject = InvalidRemoteAddress;
        FRemoteAddress CommandData = InvalidRemoteAddress;
        int32 CommandNum = 0;
        int32 CommandMax = 0;
        FRemoteAddress SpecificPlayer = InvalidRemoteAddress;
    } Parameters;

    Parameters.WorldContextObject = Object.GetAddress();
    Parameters.CommandData = CommandStorage;
    Parameters.CommandNum = static_cast<int32>(WideCommand.size() + 1);
    Parameters.CommandMax = Parameters.CommandNum;

    return KismetSystemLibrary.InvokeFunction("ExecuteConsoleCommand", &Parameters, sizeof(Parameters));
}
