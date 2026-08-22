#include "FortniteGame/Public/AI/FortAthenaAI.h"
#include "FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "FortniteGame/Public/Inventory/FortInventory.h"
#include "FortniteGame/Public/Player/FortPlayerControllerAthena.h"

namespace
{
    constexpr int32 AITeamIndex = 78;
}

void FFortAIDirector::Initialize(FEngineRuntime& InEngineRuntime, const FPlayerBootstrapSettings& InPlayerSettings)
{
    EngineRuntime = &InEngineRuntime;
    PlayerSettings = InPlayerSettings;

    AbilityGrantor.Initialize(InEngineRuntime);
    Reset();
}

void FFortAIDirector::Reset()
{
    SpawnedAI.clear();
    bLoggedUnsupported = false;
    bSubsystemsReady = false;
}

void FFortAIDirector::SetSpawnLocationProvider(std::function<FVector()> Provider)
{
    SpawnLocationProvider = std::move(Provider);
}

bool FFortAIDirector::IsSupported() const
{
    if (EngineRuntime == nullptr || !GetActiveBuildProfile().AI.IsSupported())
    {
        return false;
    }

    return ResolveBotPawnClass().IsValid();
}

std::string FFortAIDirector::DescribeSupport() const
{
    const FAIProfile& AI = GetActiveBuildProfile().AI;

    if (!AI.IsSupported())
    {
        return "This build profile declares no bot classes, so bots and bosses are unavailable";
    }

    if (!ResolveBotPawnClass())
    {
        return "The bot pawn class " + AI.BotPawnClassPath + " was not found in the running build";
    }

    return "Bot support is available";
}

FObjectHandle FFortAIDirector::ResolveBotPawnClass() const
{
    const FAIProfile& AI = GetActiveBuildProfile().AI;

    if (AI.BotPawnClassPath.empty() || EngineRuntime == nullptr)
    {
        return FObjectHandle();
    }

    return EngineRuntime->GetUnrealRuntime().FindOrLoadObject(AI.BotPawnClassPath);
}

FObjectHandle FFortAIDirector::ResolveBotControllerClass() const
{
    const FAIProfile& AI = GetActiveBuildProfile().AI;

    if (AI.BotControllerClassPath.empty() || EngineRuntime == nullptr)
    {
        return FObjectHandle();
    }

    return EngineRuntime->GetUnrealRuntime().FindOrLoadObject(AI.BotControllerClassPath);
}

bool FFortAIDirector::SetupSubsystems()
{
    if (bSubsystemsReady)
    {
        return true;
    }

    if (EngineRuntime == nullptr)
    {
        return false;
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return false;
    }

    const FUnrealRuntime& Runtime = EngineRuntime->GetUnrealRuntime();
    const FAIProfile& AI = GetActiveBuildProfile().AI;

    AFortGameModeAthena GameMode(World.GetAuthorityGameMode());
    if (!GameMode)
    {
        return false;
    }

    const FObjectHandle GameState = World.GetGameState();

    const FObjectHandle ServerBotManagerClass = Runtime.FindClass(AI.ServerBotManagerClassPath);
    if (ServerBotManagerClass)
    {
        const FObjectHandle BotManager = EngineRuntime->SpawnObject(ServerBotManagerClass, World.GetTransientPackage());

        if (BotManager)
        {
            BotManager.SetObjectProperty("CachedGameMode", GameMode.GetObject());
            BotManager.SetObjectProperty("CachedGameState", GameState);

            const FObjectHandle BotMutatorClass = Runtime.FindClass(AI.BotMutatorClassPath);
            if (BotMutatorClass)
            {
                const FObjectHandle BotMutator = World.SpawnActor(BotMutatorClass, FTransform(), FObjectHandle());
                if (BotMutator)
                {
                    BotMutator.SetObjectProperty("ServerBotManagerClass", ServerBotManagerClass);
                    BotMutator.SetObjectProperty("CachedGameMode", GameMode.GetObject());
                    BotMutator.SetObjectProperty("CachedGameState", GameState);

                    BotManager.SetObjectProperty("CachedBotMutator", BotMutator);
                }
            }

            GameMode.GetObject().SetObjectProperty("ServerBotManager", BotManager);
            UE_LOG_DISPLAY("AI", "Server bot manager is ready");
        }
    }
    else
    {
        UE_LOG_WARNING("AI", "Server bot manager class " + AI.ServerBotManagerClassPath + " was not found");
    }

    const FObjectHandle AIDirectorClass = Runtime.FindClass(AI.AIDirectorClassPath);
    if (AIDirectorClass)
    {
        const FObjectHandle Director = World.SpawnActor(AIDirectorClass, FTransform(), FObjectHandle());
        if (Director)
        {
            Director.InvokeFunction("Activate");
            GameMode.GetObject().SetObjectProperty("AIDirector", Director);
            AIDirectorHandle = Director.GetAddress();

            UE_LOG_DISPLAY("AI", "AI director is active");
        }
    }
    else
    {
        UE_LOG_WARNING("AI", "AI director class " + AI.AIDirectorClassPath + " was not found");
    }

    bSubsystemsReady = true;
    return true;
}

FVector FFortAIDirector::ChooseSpawnLocation(const FFortAIDefinition& Definition) const
{
    if (Definition.HasExplicitSpawnLocation())
    {
        return Definition.SpawnLocation;
    }

    if (SpawnLocationProvider)
    {
        return SpawnLocationProvider();
    }

    return GetActiveBuildProfile().FallbackSpawnLocation;
}

void FFortAIDirector::RegisterWithDirector(const FObjectHandle& Pawn) const
{
    if (AIDirectorHandle == InvalidRemoteAddress || !Pawn || EngineRuntime == nullptr)
    {
        return;
    }

    const FObjectHandle Director = EngineRuntime->GetUnrealRuntime().MakeHandle(AIDirectorHandle);
    if (!Director)
    {
        return;
    }

    Director.InvokeFunction("ForceNetUpdate");
}

void FFortAIDirector::ApplyDefaultAbilitySets(const FObjectHandle& Pawn) const
{
    if (!Pawn)
    {
        return;
    }

    const FScriptArrayView AbilitySets = Pawn.GetArrayProperty("DefaultGameplayAbilitySets");

    for (int32 Index = 0; Index < AbilitySets.ArrayNum; ++Index)
    {
        const FObjectHandle AbilitySet = Pawn.GetArrayElementAsObject("DefaultGameplayAbilitySets", Index);
        if (!AbilitySet)
        {
            continue;
        }

        struct FAddFortAbilitySetParameters
        {
            FRemoteAddress AbilitySet = InvalidRemoteAddress;
        } Parameters;

        Parameters.AbilitySet = AbilitySet.GetAddress();
        Pawn.InvokeFunction("AddFortAbilitySet", &Parameters, sizeof(Parameters));
    }
}

FSpawnedAI FFortAIDirector::SpawnAI(const FFortAIDefinition& Definition, int32 TeamIndex) const
{
    FSpawnedAI Result;

    if (!IsSupported())
    {
        if (!bLoggedUnsupported)
        {
            bLoggedUnsupported = true;
            UE_LOG_WARNING("AI", DescribeSupport());
        }

        return Result;
    }

    const UWorld World = EngineRuntime->GetWorld();
    if (!World)
    {
        return Result;
    }

    const FVector SpawnLocation = ChooseSpawnLocation(Definition);
    const FTransform SpawnTransform = FTransform::FromLocation(SpawnLocation);

    const FObjectHandle PawnHandle = World.BeginDeferredSpawnActor(ResolveBotPawnClass(), SpawnTransform);
    if (!PawnHandle)
    {
        UE_LOG_ERROR("AI", "Failed to begin spawning the bot pawn for " + Definition.DisplayName);
        return Result;
    }

    PawnHandle.SetProperty<uint8>("Team", static_cast<uint8>(AITeamIndex));

    if (!World.FinishSpawningActor(PawnHandle, SpawnTransform))
    {
        UE_LOG_ERROR("AI", "Failed to finish spawning the bot pawn for " + Definition.DisplayName);
        return Result;
    }

    PawnHandle.SetProperty<uint8>("Team", static_cast<uint8>(AITeamIndex));

    AFortPlayerPawnAthena Pawn(PawnHandle);
    Pawn.SetNetCullDistanceSquared(0.0f);
    Pawn.RemoveMinimumClamps();
    Pawn.SetMaxHealth(std::max(Definition.Health, PlayerSettings.MaxHealth));
    Pawn.SetMaxShield(std::max(Definition.Shield, PlayerSettings.MaxShield));
    Pawn.SetHealth(Definition.Health);
    Pawn.SetShield(Definition.Shield);
    Pawn.SetCanBeDamaged(true);

    if (!PlayerSettings.bHealthRegenEnabled)
    {
        Pawn.DisableHealthRegeneration();
    }

    ApplyDefaultAbilitySets(PawnHandle);
    AbilityGrantor.ApplyAthenaAbilities(Pawn);
    RegisterWithDirector(PawnHandle);

    const FObjectHandle ControllerHandle = PawnHandle.GetObjectProperty("Controller");

    if (ControllerHandle)
    {
        const AFortPlayerControllerAthena Controller(ControllerHandle);

        AFortPlayerStateAthena PlayerState = Controller.GetPlayerState();
        if (PlayerState)
        {
            PlayerState.SetTeamIndex(TeamIndex, true);
            PlayerState.SetHasFinishedLoading(true);
            PlayerState.SetHasStartedPlaying(true);
        }

        const std::vector<std::string>& Loadout = Definition.Loadout.empty() ? GetActiveBuildProfile().AI.BotLoadout : Definition.Loadout;

        if (!Loadout.empty())
        {
            const FFortInventory Inventory(Controller);
            if (Inventory.IsValid())
            {
                Inventory.ApplyDefaultLoadout(Loadout);
            }
        }
    }
    else
    {
        UE_LOG_WARNING("AI", "The bot pawn for " + Definition.DisplayName + " was spawned without an AI controller");
    }

    Result.ControllerAddress = ControllerHandle.GetAddress();
    Result.PawnAddress = PawnHandle.GetAddress();
    Result.DisplayName = Definition.DisplayName;
    Result.TeamIndex = TeamIndex;
    Result.bIsBoss = Definition.bIsBoss;

    UE_LOG_DISPLAY("AI", "Spawned " + std::string(Definition.bIsBoss ? "boss " : "bot ") + Definition.DisplayName + " on team " + std::to_string(TeamIndex));
    return Result;
}

int32 FFortAIDirector::SpawnPlayerBots(int32 Count, int32 FirstTeamIndex)
{
    if (Count <= 0)
    {
        return 0;
    }

    if (!IsSupported())
    {
        UE_LOG_WARNING("AI", DescribeSupport());
        return 0;
    }

    SetupSubsystems();

    int32 Spawned = 0;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        FFortAIDefinition Definition;
        Definition.DisplayName = "Bot " + std::to_string(Index + 1);
        Definition.Health = PlayerSettings.MaxHealth;
        Definition.Shield = PlayerSettings.StartingShield;

        const FSpawnedAI Entry = SpawnAI(Definition, FirstTeamIndex + Index);
        if (Entry.PawnAddress == InvalidRemoteAddress)
        {
            break;
        }

        SpawnedAI.push_back(Entry);
        ++Spawned;
    }

    UE_LOG_DISPLAY("AI", "Spawned " + std::to_string(Spawned) + " of " + std::to_string(Count) + " requested player bots");
    return Spawned;
}

int32 FFortAIDirector::SpawnBosses()
{
    const std::vector<FFortAIDefinition>& Bosses = GetActiveBuildProfile().AI.Bosses;

    if (Bosses.empty())
    {
        UE_LOG_WARNING("AI", "This build profile declares no bosses");
        return 0;
    }

    if (!IsSupported())
    {
        UE_LOG_WARNING("AI", DescribeSupport());
        return 0;
    }

    SetupSubsystems();

    int32 Spawned = 0;

    for (const FFortAIDefinition& Boss : Bosses)
    {
        if (!Boss.HasExplicitSpawnLocation())
        {
            UE_LOG_WARNING("AI", "Boss " + Boss.DisplayName + " has no spawn location in the build profile and will use a warmup spawn point");
        }

        const FSpawnedAI Entry = SpawnAI(Boss, 1);
        if (Entry.PawnAddress == InvalidRemoteAddress)
        {
            continue;
        }

        SpawnedAI.push_back(Entry);
        ++Spawned;
    }

    UE_LOG_DISPLAY("AI", "Spawned " + std::to_string(Spawned) + " of " + std::to_string(Bosses.size()) + " bosses");
    return Spawned;
}

void FFortAIDirector::UpdateEliminationState()
{
    if (EngineRuntime == nullptr)
    {
        return;
    }

    for (FSpawnedAI& Entry : SpawnedAI)
    {
        if (Entry.bEliminated)
        {
            continue;
        }

        const AFortPlayerPawnAthena Pawn(EngineRuntime->GetUnrealRuntime().MakeHandle(Entry.PawnAddress));
        if (!Pawn)
        {
            Entry.bEliminated = true;
            continue;
        }

        if (Pawn.GetHealth() > 0.0f)
        {
            continue;
        }

        Entry.bEliminated = true;
        UE_LOG_DISPLAY("AI", Entry.DisplayName + " was eliminated");
    }
}

int32 FFortAIDirector::GetAliveCount() const
{
    int32 Alive = 0;

    for (const FSpawnedAI& Entry : SpawnedAI)
    {
        if (!Entry.bEliminated)
        {
            ++Alive;
        }
    }

    return Alive;
}

int32 FFortAIDirector::GetSpawnedCount() const
{
    return static_cast<int32>(SpawnedAI.size());
}

const std::vector<FSpawnedAI>& FFortAIDirector::GetSpawnedAI() const
{
    return SpawnedAI;
}
