#pragma once

#include "CoreMinimal.h"

/** Pure S0 street geometry in actual UE centimetres: east, south, up. */
namespace HearthAincradTownLayout
{
    struct THREEHEARTHS_API FBuilding
    {
        FString Id;
        FString Role;
        FVector CenterCm = FVector::ZeroVector;
        FVector2D FootprintCm = FVector2D::ZeroVector;
        float YawDegrees = 0.f;
        int32 Floors = 1;
        FVector EntranceCm = FVector::ZeroVector;
        FVector LegacyWorkCm = FVector::ZeroVector;
        FVector WorkCm = FVector::ZeroVector;
        bool bHasWorkbench = false;
        FVector WorkbenchCm = FVector::ZeroVector;
        FVector ObserveCm = FVector::ZeroVector;
        FVector SpawnCm = FVector::ZeroVector;
    };

    struct THREEHEARTHS_API FPlan
    {
        FString Id;
        int32 Revision = 0;
        TArray<FBuilding> Buildings;
        FBox ReplacementBounds = FBox(ForceInit);
        TArray<FVector> MainStreet;
    };

    THREEHEARTHS_API FPlan Build();
    THREEHEARTHS_API bool IsReplacementArea(const FVector2D& GeographicEastNorthCm);
    THREEHEARTHS_API const FBuilding* Find(const FPlan& Plan, const FString& Id);
}
