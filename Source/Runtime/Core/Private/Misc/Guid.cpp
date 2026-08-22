#include "Runtime/Core/Public/Misc/Guid.h"
#include "Runtime/Core/Public/HAL/WindowsPlatform.h"

#include <cstdio>

std::string FGuid::ToString() const
{
    char Buffer[33] = {};
    std::snprintf(Buffer, sizeof(Buffer), "%08X%08X%08X%08X", A, B, C, D);
    return std::string(Buffer);
}

FGuid FGuid::NewGuid()
{
    GUID PlatformGuid = {};
    if (::CoCreateGuid(&PlatformGuid) != S_OK)
    {
        FGuid Fallback;
        Fallback.A = static_cast<uint32>(FWindowsPlatform::GetTimeMilliseconds());
        Fallback.B = ::GetCurrentThreadId();
        Fallback.C = ::GetTickCount();
        Fallback.D = static_cast<uint32>(reinterpret_cast<uint64>(&Fallback));
        return Fallback;
    }

    FGuid Result;
    Result.A = PlatformGuid.Data1;
    Result.B = (static_cast<uint32>(PlatformGuid.Data2) << 16) | PlatformGuid.Data3;
    Result.C = (static_cast<uint32>(PlatformGuid.Data4[0]) << 24) | (static_cast<uint32>(PlatformGuid.Data4[1]) << 16) |
        (static_cast<uint32>(PlatformGuid.Data4[2]) << 8) | static_cast<uint32>(PlatformGuid.Data4[3]);
    Result.D = (static_cast<uint32>(PlatformGuid.Data4[4]) << 24) | (static_cast<uint32>(PlatformGuid.Data4[5]) << 16) |
        (static_cast<uint32>(PlatformGuid.Data4[6]) << 8) | static_cast<uint32>(PlatformGuid.Data4[7]);
    return Result;
}
