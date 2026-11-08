#include "FortniteGame/Public/Athena/FortPoiManager.h"

#include <vector>

int32 AFortPoiManager::RemoveVolumesWithoutBrush() const
{
    return RemoveVolumesWithoutBrush("SmallTickUpdatedPoi") + RemoveVolumesWithoutBrush("AllPoiVolumes");
}

int32 AFortPoiManager::RemoveVolumesWithoutBrush(std::string_view ArrayName) const
{
    const FUnrealRuntime& Runtime = Object.GetRuntime();
    const FRemoteMemory& Memory = Runtime.GetMemory();
    const FScriptArrayView Volumes = Object.GetArrayProperty(ArrayName);

    if (Volumes.Data == InvalidRemoteAddress || Volumes.ArrayNum <= 0)
    {
        return 0;
    }

    std::vector<FRemoteAddress> Elements(static_cast<size_t>(Volumes.ArrayNum));
    if (!Memory.ReadRaw(Volumes.Data, Elements.data(), Elements.size() * sizeof(FRemoteAddress)))
    {
        return 0;
    }

    std::vector<FRemoteAddress> Kept;
    for (const FRemoteAddress Volume : Elements)
    {
        const FObjectHandle VolumeHandle = Runtime.MakeHandle(Volume);
        if (VolumeHandle && VolumeHandle.GetObjectProperty("BrushComponent"))
        {
            Kept.push_back(Volume);
        }
    }

    const int32 Removed = Volumes.ArrayNum - static_cast<int32>(Kept.size());
    if (Removed == 0)
    {
        return 0;
    }

    if (!Kept.empty())
    {
        Memory.WriteRaw(Volumes.Data, Kept.data(), Kept.size() * sizeof(FRemoteAddress));
    }

    Memory.Write<int32>(Object.GetPropertyAddress(ArrayName) + sizeof(FRemoteAddress), static_cast<int32>(Kept.size()));
    return Removed;
}
