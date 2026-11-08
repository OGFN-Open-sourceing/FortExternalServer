#pragma once

#include "Runtime/Engine/Classes/GameFramework/Actor.h"

class AFortPoiManager : public AActor
{
public:
    using AActor::AActor;

    int32 RemoveVolumesWithoutBrush() const;

private:
    int32 RemoveVolumesWithoutBrush(std::string_view ArrayName) const;
};
