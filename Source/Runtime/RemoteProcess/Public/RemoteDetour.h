#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

class FRemoteDetour
{
public:
    bool Install(const FRemoteMemory& Memory, FRemoteAllocation& Arena, FRemoteAddress TargetFunction, FRemoteAddress DetourFunction);

    bool Uninstall(const FRemoteMemory& Memory);

    bool IsInstalled() const;

    FRemoteAddress GetTargetAddress() const;

    FRemoteAddress GetTrampolineAddress() const;

private:
    static bool RelocatePrologue(std::vector<uint8>& PrologueBytes, FRemoteAddress OriginalAddress, FRemoteAddress RelocatedAddress);

    FRemoteAddress TargetAddress = InvalidRemoteAddress;
    FRemoteAddress TrampolineAddress = InvalidRemoteAddress;
    std::vector<uint8> OriginalBytes;
};
