#pragma once

#include "CoreMinimal.h"
#include "HearthVillage.h"
#include "HearthStructurePlan.h"

struct THREEHEARTHS_API FHearthPlannedConstructionResult
{
    bool bAccepted = false;
    FString Reason;
    TArray<FHearthCottageComponent> Components;
};

namespace HearthPlannedConstructionAdapter
{
    THREEHEARTHS_API FHearthPlannedConstructionResult Convert(const FHearthStructurePlan& Plan, int32 Owner,
        const TArray<FHearthCottageComponent>& Existing);
}
