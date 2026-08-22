#pragma once

#include "Runtime/Core/Public/HAL/PlatformDetection.h"

#if FORT_PLATFORM_WINDOWS

namespace FWindowsPlatform
{
    bool EnableDebugPrivilege();
}

#endif
