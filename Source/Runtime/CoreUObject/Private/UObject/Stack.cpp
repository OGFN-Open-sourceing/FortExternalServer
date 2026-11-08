#include "Runtime/CoreUObject/Public/UObject/Stack.h"

FFrame FFrame::Read(const FRemoteMemory& Memory, FRemoteAddress FrameAddress)
{
    FFrame Frame;
    Frame.Address = FrameAddress;

    if (FrameAddress == InvalidRemoteAddress || FrameAddress == 0)
    {
        return Frame;
    }

    Frame.Node = Memory.ReadPointer(FrameAddress + FFrameLayout::Node);
    Frame.Object = Memory.ReadPointer(FrameAddress + FFrameLayout::Object);
    Frame.Code = Memory.ReadPointer(FrameAddress + FFrameLayout::Code);
    Frame.Locals = Memory.ReadPointer(FrameAddress + FFrameLayout::Locals);
    return Frame;
}

FNativeFunctionCall::FNativeFunctionCall(const FUnrealRuntime& InRuntime, FRemoteAddress InOriginalFunction, FRemoteAddress InContext, const FFrame& InStack,
    FRemoteAddress InResult)
    : Runtime(&InRuntime)
    , OriginalFunction(InOriginalFunction)
    , Context(InContext)
    , Stack(InStack)
    , Result(InResult)
{
}

const FUnrealRuntime& FNativeFunctionCall::GetRuntime() const
{
    return *Runtime;
}

FObjectHandle FNativeFunctionCall::GetContext() const
{
    return Runtime->MakeHandle(Context);
}

const FFrame& FNativeFunctionCall::GetStack() const
{
    return Stack;
}

void FNativeFunctionCall::CallOriginal() const
{
    Runtime->GetBridge().CallFunction(OriginalFunction, { Context, Stack.Address, Result });
}
