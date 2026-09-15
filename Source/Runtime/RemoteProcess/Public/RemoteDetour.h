#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/RemoteProcess/Public/RemoteAllocation.h"
#include "Runtime/RemoteProcess/Public/RemoteMemory.h"

class FRemoteDetour
{
public:
    bool Prepare(const FRemoteMemory& Memory, FRemoteAllocation& Arena, FRemoteAddress TargetFunction);

    bool Activate(const FRemoteMemory& Memory, FRemoteAddress DetourFunction);

    bool Install(const FRemoteMemory& Memory, FRemoteAllocation& Arena, FRemoteAddress TargetFunction, FRemoteAddress DetourFunction);

    bool Uninstall(const FRemoteMemory& Memory);

    bool IsPrepared() const;

    bool IsInstalled() const;

    FRemoteAddress GetTargetAddress() const;

    FRemoteAddress GetTrampolineAddress() const;

private:
    static bool RelocatePrologue(std::vector<uint8>& PrologueBytes, FRemoteAddress OriginalAddress, FRemoteAddress RelocatedAddress);

    FRemoteAddress TargetAddress = InvalidRemoteAddress;
    FRemoteAddress TrampolineAddress = InvalidRemoteAddress;
    FRemoteAddress PreparedTarget = InvalidRemoteAddress;
    uint32 PreparedPrologueLength = 0;
    std::vector<uint8> OriginalBytes;
};
