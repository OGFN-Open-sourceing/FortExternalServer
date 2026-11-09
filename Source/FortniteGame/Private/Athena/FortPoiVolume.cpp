#include "FortniteGame/Public/Athena/FortPoiVolume.h"

bool AFortPoiVolume::EnsureBrushComponent() const
{
    if (Object.GetObjectProperty("BrushComponent"))
    {
        return false;
    }

    FObjectHandle Component = Object.GetObjectProperty("PoiCollisionComp");
    if (!Component)
    {
        Component = Object.GetObjectProperty("RootComponent");
    }

    return Component && Object.SetObjectProperty("BrushComponent", Component);
}
