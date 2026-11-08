#pragma once

#include "FortniteGame/Public/FortTypes.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

class AFortPlaylistAthena : public UObject
{
public:
    using UObject::UObject;

    int32 GetPlaylistId() const;

    int32 GetMaxSquadSize() const;

    int32 GetMaxTeamCount() const;

    void SetMaxSquadSize(int32 Value);

    void SetMaxTeamCount(int32 Value);

    void SetRespawnType(EAthenaRespawnType Type);

    void SetRespawnLocation(EAthenaRespawnLocation Location);

    void SetFriendlyFireType(EFriendlyFireType Type);

    void SetDownedButNotOutEnabled(bool bEnabled);
};
