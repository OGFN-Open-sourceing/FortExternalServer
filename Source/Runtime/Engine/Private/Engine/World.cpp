#include "Runtime/Engine/Public/Engine/World.h"

FVector AActor::GetActorLocation() const
{
    struct FGetActorLocationParameters
    {
        FVector ReturnValue;
    } Parameters;

    if (!Object.InvokeFunction("K2_GetActorLocation", &Parameters, sizeof(Parameters)))
    {
        return FVector();
    }

    return Parameters.ReturnValue;
}

FRotator AActor::GetActorRotation() const
{
    struct FGetActorRotationParameters
    {
        FRotator ReturnValue;
    } Parameters;

    if (!Object.InvokeFunction("K2_GetActorRotation", &Parameters, sizeof(Parameters)))
    {
        return FRotator();
    }

    return Parameters.ReturnValue;
}

bool AActor::SetActorLocation(const FVector& Location, bool bSweep)
{
    struct FSetActorLocationParameters
    {
        FVector NewLocation;
        bool bSweep = false;
        uint8 Padding[7] = {};
        uint8 SweepHitResult[0x88] = {};
        bool bTeleport = false;
        uint8 TeleportPadding[7] = {};
        bool ReturnValue = false;
    } Parameters;

    Parameters.NewLocation = Location;
    Parameters.bSweep = bSweep;

    if (!Object.InvokeFunction("K2_SetActorLocation", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return Parameters.ReturnValue;
}

bool AActor::TeleportTo(const FVector& Location, const FRotator& Rotation)
{
    struct FTeleportToParameters
    {
        FVector DestLocation;
        FRotator DestRotation;
        bool ReturnValue = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.DestLocation = Location;
    Parameters.DestRotation = Rotation;

    if (!Object.InvokeFunction("K2_TeleportTo", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return Parameters.ReturnValue;
}

bool AActor::DestroyActor()
{
    struct FDestroyActorParameters
    {
        bool ReturnValue = false;
        uint8 Padding[7] = {};
    } Parameters;

    if (!Object.InvokeFunction("K2_DestroyActor", &Parameters, sizeof(Parameters)))
    {
        return false;
    }

    return true;
}

void AActor::ForceNetUpdate()
{
    Object.InvokeFunction("ForceNetUpdate");
}

void AActor::SetOwner(const AActor& NewOwner)
{
    Object.SetObjectProperty("Owner", NewOwner.GetObject());
    Object.InvokeFunction("OnRep_Owner");
}

AActor AActor::GetOwner() const
{
    return AActor(Object.GetObjectProperty("Owner"));
}

void AActor::SetReplicates(bool bReplicates)
{
    struct FSetReplicatesParameters
    {
        bool bInReplicates = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bInReplicates = bReplicates;
    Object.InvokeFunction("SetReplicates", &Parameters, sizeof(Parameters));
}

void AActor::SetActorHiddenInGame(bool bHidden)
{
    struct FSetActorHiddenInGameParameters
    {
        bool bNewHidden = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bNewHidden = bHidden;
    Object.InvokeFunction("SetActorHiddenInGame", &Parameters, sizeof(Parameters));
}

void AActor::SetNetCullDistanceSquared(float Value)
{
    Object.SetProperty<float>("NetCullDistanceSquared", Value);
}

void AActor::SetActorTickEnabled(bool bEnabled)
{
    struct FSetActorTickEnabledParameters
    {
        bool bEnabled = false;
        uint8 Padding[7] = {};
    } Parameters;

    Parameters.bEnabled = bEnabled;
    Object.InvokeFunction("SetActorTickEnabled", &Parameters, sizeof(Parameters));
}

FObjectHandle UNetConnection::GetPlayerController() const
{
    return Object.GetObjectProperty("PlayerController");
}

void UNetConnection::SetPlayerController(const FObjectHandle& Controller)
{
    Object.SetObjectProperty("PlayerController", Controller);
}

FObjectHandle UNetConnection::GetOwningActor() const
{
    return Object.GetObjectProperty("OwningActor");
}

void UNetConnection::SetCurrentNetSpeed(int32 Value)
{
    Object.SetProperty<int32>("CurrentNetSpeed", Value);
}

bool UNetConnection::IsInternalAck() const
{
    return Object.GetBoolProperty("InternalAck", false);
}

FRemoteAddress UNetConnection::GetPlayerIdAddress() const
{
    return Object.GetPropertyAddress("PlayerId");
}

FRemoteAddress UNetConnection::GetRequestUrlAddress() const
{
    return Object.GetPropertyAddress("RequestURL");
}

FRemoteAddress UNetConnection::GetClientResponseAddress() const
{
    return Object.GetPropertyAddress("ClientResponse");
}

std::string UNetConnection::GetRequestUrl() const
{
    return Object.GetStringProperty("RequestURL");
}

FScriptArrayView UNetDriver::GetClientConnections() const
{
    return Object.GetArrayProperty("ClientConnections");
}

UNetConnection UNetDriver::GetClientConnection(int32 Index) const
{
    return UNetConnection(Object.GetArrayElementAsObject("ClientConnections", Index));
}

int32 UNetDriver::GetClientConnectionCount() const
{
    return GetClientConnections().ArrayNum;
}

FObjectHandle UNetDriver::GetReplicationDriver() const
{
    return Object.GetObjectProperty("ReplicationDriver");
}

FObjectHandle UNetDriver::GetServerConnection() const
{
    return Object.GetObjectProperty("ServerConnection");
}

void UNetDriver::SetNetServerMaxTickRate(int32 Value)
{
    Object.SetProperty<int32>("NetServerMaxTickRate", Value);
}

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

    const FUnrealRuntime& Runtime = Object.GetRuntime();
    const FRemoteAddress SpawnActorAddress = Runtime.GetGlobals().SpawnActor;

    if (SpawnActorAddress == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    struct FActorSpawnParameters
    {
        FRemoteAddress Name = InvalidRemoteAddress;
        FRemoteAddress Template = InvalidRemoteAddress;
        FRemoteAddress Owner = InvalidRemoteAddress;
        FRemoteAddress Instigator = InvalidRemoteAddress;
        FRemoteAddress OverrideLevel = InvalidRemoteAddress;
        uint32 SpawnCollisionHandlingOverride = 0;
        uint8 Flags = 0;
        uint8 Padding[3] = {};
        FRemoteAddress ObjectFlags = 0;
    } SpawnParameters;

    SpawnParameters.Owner = Owner.GetAddress();

    const FRemoteAddress TransformStorage = Runtime.AcquireScratch(sizeof(FTransform));
    const FRemoteAddress SpawnParametersStorage = Runtime.AcquireScratch(sizeof(FActorSpawnParameters));

    if (TransformStorage == InvalidRemoteAddress || SpawnParametersStorage == InvalidRemoteAddress)
    {
        return FObjectHandle();
    }

    Runtime.GetMemory().WriteRaw(TransformStorage, &Transform, sizeof(FTransform));
    Runtime.GetMemory().WriteRaw(SpawnParametersStorage, &SpawnParameters, sizeof(FActorSpawnParameters));

    const uint64 Result =
        Runtime.GetBridge().CallFunction(SpawnActorAddress, { Object.GetAddress(), ClassHandle.GetAddress(), TransformStorage, SpawnParametersStorage });

    return Runtime.MakeHandle(Result);
}

FObjectHandle UWorld::GetTransientPackage() const
{
    if (!IsValid())
    {
        return FObjectHandle();
    }

    return Object.GetRuntime().FindObject("Package /Engine/Transient");
}

FObjectHandle UWorld::BeginDeferredSpawnActor(const FObjectHandle& ClassHandle, const FTransform& Transform) const
{
    if (!IsValid() || !ClassHandle)
    {
        return FObjectHandle();
    }

    const FUnrealRuntime& Runtime = Object.GetRuntime();

    const FObjectHandle GameplayStatics = Runtime.GetClassDefaultObject("GameplayStatics");
    if (!GameplayStatics)
    {
        return FObjectHandle();
    }

    struct FBeginDeferredActorSpawnParameters
    {
        FRemoteAddress WorldContextObject = InvalidRemoteAddress;
        FRemoteAddress ActorClass = InvalidRemoteAddress;
        FTransform SpawnTransform;
        uint8 CollisionHandlingOverride = 0;
        uint8 Padding[7] = {};
        FRemoteAddress Owner = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    Parameters.WorldContextObject = Object.GetAddress();
    Parameters.ActorClass = ClassHandle.GetAddress();
    Parameters.SpawnTransform = Transform;

    if (!GameplayStatics.InvokeFunction("BeginDeferredActorSpawnFromClass", &Parameters, sizeof(Parameters)))
    {
        return FObjectHandle();
    }

    return Runtime.MakeHandle(Parameters.ReturnValue);
}

bool UWorld::FinishSpawningActor(const FObjectHandle& Actor, const FTransform& Transform) const
{
    if (!IsValid() || !Actor)
    {
        return false;
    }

    const FObjectHandle GameplayStatics = Object.GetRuntime().GetClassDefaultObject("GameplayStatics");
    if (!GameplayStatics)
    {
        return false;
    }

    struct FFinishSpawningActorParameters
    {
        FRemoteAddress Actor = InvalidRemoteAddress;
        FTransform SpawnTransform;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    Parameters.Actor = Actor.GetAddress();
    Parameters.SpawnTransform = Transform;

    return GameplayStatics.InvokeFunction("FinishSpawningActor", &Parameters, sizeof(Parameters));
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
