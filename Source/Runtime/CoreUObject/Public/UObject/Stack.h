#pragma once

#include "Runtime/CoreUObject/Public/UObject/UnrealRuntime.h"

namespace FFrameLayout
{
    inline constexpr int32 Node = 0x10;
    inline constexpr int32 Object = 0x18;
    inline constexpr int32 Code = 0x20;
    inline constexpr int32 Locals = 0x28;
}

struct FFrame
{
    FRemoteAddress Address = InvalidRemoteAddress;
    FRemoteAddress Node = InvalidRemoteAddress;
    FRemoteAddress Object = InvalidRemoteAddress;
    FRemoteAddress Code = InvalidRemoteAddress;
    FRemoteAddress Locals = InvalidRemoteAddress;

    static FFrame Read(const FRemoteMemory& Memory, FRemoteAddress FrameAddress);
};

class FNativeFunctionCall
{
public:
    FNativeFunctionCall(const FUnrealRuntime& InRuntime, FRemoteAddress InOriginalFunction, FRemoteAddress InContext, const FFrame& InStack,
        FRemoteAddress InResult);

    const FUnrealRuntime& GetRuntime() const;

    FObjectHandle GetContext() const;

    const FFrame& GetStack() const;

    template<typename ParametersType>
    ParametersType GetParameters() const
    {
        ParametersType Parameters{};
        Runtime->GetMemory().ReadRaw(Stack.Locals, &Parameters, sizeof(ParametersType));
        return Parameters;
    }

    template<typename ResultType>
    bool SetResult(const ResultType& Value) const
    {
        return Result != InvalidRemoteAddress && Runtime->GetMemory().WriteRaw(Result, &Value, sizeof(ResultType));
    }

    template<typename ResultType>
    ResultType GetResult() const
    {
        ResultType Value{};
        if (Result != InvalidRemoteAddress)
        {
            Runtime->GetMemory().ReadRaw(Result, &Value, sizeof(ResultType));
        }

        return Value;
    }

    void CallOriginal() const;

private:
    const FUnrealRuntime* Runtime = nullptr;
    FRemoteAddress OriginalFunction = InvalidRemoteAddress;
    FRemoteAddress Context = InvalidRemoteAddress;
    FFrame Stack;
    FRemoteAddress Result = InvalidRemoteAddress;
};
