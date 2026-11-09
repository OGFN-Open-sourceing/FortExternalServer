#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class AFortPoiVolume : public AActor
{
public:
    using AActor::AActor;

    bool EnsureBrushComponent() const;
};
