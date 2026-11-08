#pragma once

#include "Runtime/CoreUObject/Public/UObject/Stack.h"

enum class ENativeOverrideResult : uint8
{
    ContinueToOriginal,
    Handled
};

using FNativeFunctionHandler = std::function<ENativeOverrideResult(const FNativeFunctionCall&)>;

class FNativeFunctionOverrides
{
public:
    void Initialize(FUnrealRuntime& InRuntime);

    bool Override(std::string_view ClassName, std::string_view FunctionName, FNativeFunctionHandler Handler);

    void RemoveAll();

    int32 GetOverrideCount() const;

private:
    FHookResponse Dispatch(FRemoteAddress Function, FRemoteAddress OriginalFunction, const FNativeFunctionHandler& Handler,
        const FHookInvocation& Invocation) const;

    FUnrealRuntime* Runtime = nullptr;
    std::vector<int32> Channels;
};
