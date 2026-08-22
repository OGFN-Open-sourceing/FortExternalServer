#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FName
{
    int32 ComparisonIndex = 0;
    int32 Number = 0;

    constexpr bool IsNone() const
    {
        return ComparisonIndex == 0;
    }

    constexpr bool operator==(const FName& Other) const
    {
        return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number;
    }

    constexpr bool operator!=(const FName& Other) const
    {
        return !(*this == Other);
    }
};

struct FScriptArrayView
{
    FRemoteAddress Data = InvalidRemoteAddress;
    int32 ArrayNum = 0;
    int32 ArrayMax = 0;

    bool IsValidIndex(int32 Index) const
    {
        return Index >= 0 && Index < ArrayNum;
    }
};

struct FScriptInterfaceView
{
    FRemoteAddress ObjectPointer = InvalidRemoteAddress;
    FRemoteAddress InterfacePointer = InvalidRemoteAddress;
};

struct FWeakObjectPtrView
{
    int32 ObjectIndex = InvalidIndex;
    int32 ObjectSerialNumber = 0;
};

struct FUniqueNetIdReplView
{
    uint8 Storage[0x28] = {};
};

enum class ENetMode : uint8
{
    Standalone = 0,
    DedicatedServer = 1,
    ListenServer = 2,
    Client = 3
};

enum class ENetRole : uint8
{
    None = 0,
    SimulatedProxy = 1,
    AutonomousProxy = 2,
    Authority = 3
};

enum class ENetworkMessageType : uint8
{
    Hello = 0,
    Welcome = 1,
    Upgrade = 2,
    Challenge = 3,
    Netspeed = 4,
    Login = 5,
    Failure = 6,
    Join = 9,
    JoinSplit = 10,
    Skip = 11,
    Abort = 12,
    PCSwap = 15,
    ActorChannelFailure = 16,
    DebugText = 17,
    NetGUIDAssign = 18,
    SecurityViolation = 19,
    GameSpecific = 20,
    EncryptionAck = 21,
    BeaconWelcome = 25,
    BeaconJoin = 26,
    BeaconAssignGUID = 27,
    BeaconNetGUIDAck = 28
};
