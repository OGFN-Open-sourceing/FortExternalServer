#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

namespace FLaunchServer
{
    void PrintBanner();

    int Run(int ArgumentCount, char** Arguments);
}
