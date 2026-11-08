#include "Runtime/Engine/Public/EngineRuntime.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include <cstring>

std::vector<std::pair<FRemoteAddress, const char*>> FEngineFunctions::GetAllEntries() const
{
    return { { NetDriverTickFlush, "UNetDriver::TickFlush" }, { EngineTick, "UFortEngine::Tick" },
        { AbilitySystemGiveAbility, "UAbilitySystemComponent::GiveAbility" },
        { AbilitySystemInternalTryActivateAbility, "UAbilitySystemComponent::InternalTryActivateAbility" },
        { GameplayAbilitySpecConstructor, "FGameplayAbilitySpec::FGameplayAbilitySpec" }, { EngineCreateNetDriver, "UEngine::CreateNetDriver" },
        { NetDriverSetWorld, "UNetDriver::SetWorld" }, { GameSessionKickPlayer, "AFortGameSession::KickPlayer" } };
}

bool FEngineRuntime::Initialize(FUnrealRuntime& InUnrealRuntime, const FEngineGlobals& InGlobals, const FEngineFunctions& InFunctions, const FEngineNetworkLayout& InNetworkLayout)
{
    UnrealRuntime = &InUnrealRuntime;
    Globals = InGlobals;
    Functions = InFunctions;
    NetworkLayout = InNetworkLayout;
    CachedEngineAddress = InvalidRemoteAddress;

    if (Globals.GWorld == InvalidRemoteAddress)
    {
        UE_LOG_WARNING("Engine", "GWorld is missing from the build profile, the world is resolved through the engine instead");
    }

    for (const auto& [Address, Label] : Functions.GetAllEntries())
    {
        if (Address == InvalidRemoteAddress)
        {
            UE_LOG_VERBOSE("Engine", std::string(Label) + " has no offset in the build profile");
            continue;
        }

        UE_LOG_DISPLAY("Engine", std::string(Label) + " at " + FStringConv::ToHex(Address));
    }

    return true;
}

bool FEngineRuntime::IsInitialized() const
{
    return UnrealRuntime != nullptr;
}

const FEngineFunctions& FEngineRuntime::GetFunctions() const
{
    return Functions;
}

const FEngineNetworkLayout& FEngineRuntime::GetNetworkLayout() const
{
    return NetworkLayout;
}

FUnrealRuntime& FEngineRuntime::GetUnrealRuntime() const
{
    return *UnrealRuntime;
}

FObjectHandle FEngineRuntime::GetEngine() const
{
    if (CachedEngineAddress != InvalidRemoteAddress)
    {
        const FObjectHandle Cached = UnrealRuntime->MakeHandle(CachedEngineAddress);
        if (Cached)
        {
            return Cached;
        }
    }

    const FObjectHandle EngineClass = UnrealRuntime->FindClass("FortniteGame.FortEngine");
    if (EngineClass)
    {
        const std::vector<FObjectHandle> Instances = UnrealRuntime->FindObjectsOfClass(EngineClass, 1);
        if (!Instances.empty())
        {
            CachedEngineAddress = Instances.front().GetAddress();
            return Instances.front();
        }
    }

    const std::vector<FObjectHandle> ByName = UnrealRuntime->FindObjectsByNamePrefix("FortEngine_", 1);
    if (!ByName.empty())
    {
        CachedEngineAddress = ByName.front().GetAddress();
        return ByName.front();
    }

    return FObjectHandle();
}

void FEngineRuntime::SetEngine(const FObjectHandle& Engine)
{
    CachedEngineAddress = Engine.GetAddress();
}

FObjectHandle FEngineRuntime::GetGameViewportClient() const
{
    const FObjectHandle Engine = GetEngine();
    if (!Engine)
    {
        return FObjectHandle();
    }

    return Engine.GetObjectProperty("GameViewport");
}

UWorld FEngineRuntime::GetWorld() const
{
    if (Globals.GWorld != InvalidRemoteAddress)
    {
        const FObjectHandle World = UnrealRuntime->MakeHandle(UnrealRuntime->GetMemory().ReadPointer(Globals.GWorld));
        if (World)
        {
            return UWorld(World);
        }
    }

    const FObjectHandle Viewport = GetGameViewportClient();
    if (Viewport)
    {
        const FObjectHandle World = Viewport.GetObjectProperty("World");
        if (World)
        {
            return UWorld(World);
        }
    }

    const FObjectHandle GameInstance = GetGameInstance();
    if (GameInstance)
    {
        const FObjectHandle World = GameInstance.GetObjectProperty("WorldContext");
        if (World)
        {
            return UWorld(World);
        }
    }

    return UWorld();
}

FObjectHandle FEngineRuntime::GetGameInstance() const
{
    const FObjectHandle Engine = GetEngine();
    if (!Engine)
    {
        return FObjectHandle();
    }

    return Engine.GetObjectProperty("GameInstance");
}

FObjectHandle FEngineRuntime::GetLocalPlayer(int32 Index) const
{
    const FObjectHandle GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return FObjectHandle();
    }

    return GameInstance.GetArrayElementAsObject("LocalPlayers", Index);
}

bool FEngineRuntime::RemoveLocalPlayers() const
{
    const FObjectHandle GameInstance = GetGameInstance();
    const FRemoteAddress LocalPlayers = GameInstance ? GameInstance.GetPropertyAddress("LocalPlayers") : InvalidRemoteAddress;
    if (LocalPlayers == InvalidRemoteAddress)
    {
        return false;
    }

    return UnrealRuntime->GetMemory().Write<int32>(LocalPlayers + FUnrealLayout::FScriptArray_ArrayNum, 0);
}

bool FEngineRuntime::Listen(const UWorld& World, int32 Port) const
{
    if (Functions.EngineCreateNetDriver == InvalidRemoteAddress || Functions.NetDriverSetWorld == InvalidRemoteAddress ||
        NetworkLayout.InitListenVirtualIndex == InvalidIndex)
    {
        UE_LOG_ERROR("Engine", "UEngine::CreateNetDriver, UNetDriver::SetWorld or UNetDriver::InitListen is missing from the build profile");
        return false;
    }

    if (World.GetNetDriver())
    {
        UE_LOG_ERROR("Engine", "NetDriver already exists");
        return false;
    }

    const FObjectHandle Engine = GetEngine();
    if (!Engine)
    {
        return false;
    }

    FGameThreadBridge& Bridge = UnrealRuntime->GetBridge();
    const FRemoteMemory& Memory = UnrealRuntime->GetMemory();

    const FName GameNetDriverName = UnrealRuntime->CreateName("GameNetDriver");
    uint64 PackedName = 0;
    std::memcpy(&PackedName, &GameNetDriverName, sizeof(PackedName));

    const FObjectHandle NetDriver = UnrealRuntime->MakeHandle(Bridge.CallFunction(Functions.EngineCreateNetDriver, { Engine.GetAddress(), World.GetAddress(), PackedName }));
    if (!NetDriver)
    {
        UE_LOG_ERROR("Engine", "Error creating network driver.");
        return false;
    }

    NetDriver.WritePropertyRaw("NetDriverName", &GameNetDriverName, sizeof(GameNetDriverName));
    World.GetObject().SetObjectProperty("NetDriver", NetDriver);
    Bridge.CallFunction(Functions.NetDriverSetWorld, { NetDriver.GetAddress(), World.GetAddress() });

    const FScriptArrayView LevelCollections = World.GetObject().GetArrayProperty("LevelCollections");
    for (int32 Index = 0; Index < LevelCollections.ArrayNum; ++Index)
    {
        const FRemoteAddress Collection = LevelCollections.Data + static_cast<uint64>(Index) * FLevelCollectionLayout::Size;
        const auto Type = static_cast<ELevelCollectionType>(Memory.Read<uint8>(Collection + FLevelCollectionLayout::CollectionType));
        if (Type == ELevelCollectionType::DynamicSourceLevels || Type == ELevelCollectionType::StaticLevels)
        {
            Memory.Write<FRemoteAddress>(Collection + FLevelCollectionLayout::NetDriver, NetDriver.GetAddress());
        }
    }

    FRemoteAllocation& Arena = Bridge.GetArena();
    const FRemoteAddress ListenURL = Arena.AllocateZeroed(FURLLayout::Size);
    const FRemoteAddress Error = Arena.AllocateZeroed(sizeof(FScriptArrayView));
    Memory.Write<int32>(ListenURL + FURLLayout::Port, Port);

    const FRemoteAddress InitListen = NetDriver.GetVirtualFunction(NetworkLayout.InitListenVirtualIndex);
    const bool bListening = (Bridge.CallFunction(InitListen, { NetDriver.GetAddress(), World.GetAddress() + FWorldLayout::NetworkNotify, ListenURL, 0, Error }) & 0xFF) != 0;

    if (!bListening)
    {
        UE_LOG_ERROR("Engine", "Error initializing network layer.");
        World.GetObject().SetObjectProperty("NetDriver", FObjectHandle());
        return false;
    }

    UE_LOG_DISPLAY("Engine", "GameNetDriver " + NetDriver.GetName() + " listening on port " + std::to_string(Port));
    return true;
}

FObjectHandle FEngineRuntime::SpawnObject(const FObjectHandle& ClassHandle, const FObjectHandle& Outer) const
{
    const FObjectHandle GameplayStatics = UnrealRuntime->GetClassDefaultObject("GameplayStatics");
    if (!GameplayStatics || !ClassHandle)
    {
        return FObjectHandle();
    }

    struct FSpawnObjectParameters
    {
        FRemoteAddress ObjectClass = InvalidRemoteAddress;
        FRemoteAddress Outer = InvalidRemoteAddress;
        FRemoteAddress ReturnValue = InvalidRemoteAddress;
    } Parameters;

    Parameters.ObjectClass = ClassHandle.GetAddress();
    Parameters.Outer = Outer.GetAddress();

    if (!GameplayStatics.InvokeFunction("SpawnObject", &Parameters, sizeof(Parameters)))
    {
        return FObjectHandle();
    }

    return UnrealRuntime->MakeHandle(Parameters.ReturnValue);
}
