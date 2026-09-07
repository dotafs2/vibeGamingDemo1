#pragma once

#include "CoreMinimal.h"

namespace HearthBuildingArchetypes
{
    // Pure recommendation for Resident.BuildingArchetype. Call as:
    // HearthBuildingArchetypes::SelectArchetype(Role, Goal, bKing).
    // Returns rowhouse, shop_house, courtyard_workshop, warehouse, inn, or keep.
    THREEHEARTHS_API FString SelectArchetype(const FString& Role, const FString& Goal, bool bKing);
}
