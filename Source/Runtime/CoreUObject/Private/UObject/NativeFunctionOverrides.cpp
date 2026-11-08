#include "Runtime/CoreUObject/Public/UObject/NativeFunctionOverrides.h"

void FNativeFunctionOverrides::Initialize(FUnrealRuntime& InRuntime)
{
    Runtime = &InRuntime;
}

bool FNativeFunctionOverrides::Override(std::string_view ClassName, std::string_view FunctionName, FNativeFunctionHandler Handler)
{
    if (Runtime == nullptr || !Handler)
    {
        return false;
    }

    const FObjectHandle DefaultObject = Runtime->GetClassDefaultObject(ClassName);
    const FObjectHandle Function = DefaultObject ? DefaultObject.FindFunction(FunctionName) : FObjectHandle();

    if (!Function)
    {
        UE_LOG_ERROR("Overrides", std::string(ClassName) + "::" + std::string(FunctionName) + " was not found");
        return false;
    }

    const FRemoteAddress ExecSlot = Function.GetAddress() + FUnrealLayout::UFunction_ExecFunction;
    const FRemoteAddress OriginalFunction = Runtime->GetMemory().ReadPointer(ExecSlot);
    const FRemoteAddress FunctionAddress = Function.GetAddress();

    const int32 Channel = Runtime->GetBridge().InstallPointerHook(ExecSlot, EHookDispatchMode::Blocking,
        [this, FunctionAddress, OriginalFunction, Handler = std::move(Handler)](const FHookInvocation& Invocation)
        { return Dispatch(FunctionAddress, OriginalFunction, Handler, Invocation); });

    if (Channel == InvalidIndex)
    {
        UE_LOG_ERROR("Overrides", "Failed to override " + std::string(ClassName) + "::" + std::string(FunctionName));
        return false;
    }

    Channels.push_back(Channel);
    UE_LOG_DISPLAY("Overrides", "Overrode " + std::string(ClassName) + "::" + std::string(FunctionName));
    return true;
}

void FNativeFunctionOverrides::RemoveAll()
{
    if (Runtime == nullptr)
    {
        return;
    }

    for (const int32 Channel : Channels)
    {
        Runtime->GetBridge().RemoveHook(Channel);
    }

    Channels.clear();
}

int32 FNativeFunctionOverrides::GetOverrideCount() const
{
    return static_cast<int32>(Channels.size());
}

FHookResponse FNativeFunctionOverrides::Dispatch(FRemoteAddress Function, FRemoteAddress OriginalFunction, const FNativeFunctionHandler& Handler,
    const FHookInvocation& Invocation) const
{
    const FFrame Stack = FFrame::Read(Runtime->GetMemory(), Invocation.IntegerArguments[1]);

    if (Stack.Node != Function)
    {
        return FHookResponse::ContinueToOriginal();
    }

    const FNativeFunctionCall Call(*Runtime, OriginalFunction, Invocation.IntegerArguments[0], Stack, Invocation.IntegerArguments[2]);

    if (Handler(Call) == ENativeOverrideResult::Handled)
    {
        return FHookResponse::OverrideWith(0);
    }

    return FHookResponse::ContinueToOriginal();
}
