#pragma once

#include "Runtime/RemoteProcess/Public/GameThreadBridge.h"

#include <string>

class FHttpRedirect
{
public:
    bool Install(FGameThreadBridge& InBridge, FRemoteAddress InRequestVirtualTable, FRemoteAddress InSetURL, const std::wstring& BackendUrl, uint64 TimeoutMilliseconds);

    void Uninstall();

    static constexpr int32 SetURLVirtualIndex = 10;

private:
    FRemoteAddress EmitSetURLOverride(FRemoteAddress DomainTable, FRemoteAddress BackendText) const;

    bool WriteVirtualSlot(FRemoteAddress Value) const;

    FGameThreadBridge* Bridge = nullptr;
    FRemoteAddress RequestVirtualTable = InvalidRemoteAddress;
    FRemoteAddress OriginalSetURL = InvalidRemoteAddress;
    bool bInstalled = false;
};
