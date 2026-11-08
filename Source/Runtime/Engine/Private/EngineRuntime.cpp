#include "Runtime/Engine/Public/EngineRuntime.h"
#include "Runtime/Engine/Classes/Engine/World.h"

std::vector<std::pair<FRemoteAddress, const char*>> FEngineFunctions::GetAllEntries() const
{
    return { { NetDriverTickFlush, "UNetDriver::TickFlush" }, { CollectGarbage, "CollectGarbage" },
        { AbilitySystemGiveAbility, "UAbilitySystemComponent::GiveAbility" },
        { AbilitySystemInternalTryActivateAbility, "UAbilitySystemComponent::InternalTryActivateAbility" },
        { GameplayAbilitySpecConstructor, "FGameplayAbilitySpec::FGameplayAbilitySpec" } };
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

void FEngineRuntime::InvalidateCachedEngine()
{
    CachedEngineAddress = InvalidRemoteAddress;
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
