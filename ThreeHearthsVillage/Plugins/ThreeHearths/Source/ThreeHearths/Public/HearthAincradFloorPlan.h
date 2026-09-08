#pragma once

#include "CoreMinimal.h"

namespace HearthAincradFloorPlan
{
    struct THREEHEARTHS_API FRegion
    {
        FString Id;
        FString ChineseLabel;
        FString Kind;
        FVector2D CenterCm = FVector2D::ZeroVector;
        float RadiusCm = 0.f;
        bool bSafeZone = false;
        FString CanonicalNote;
    };

    struct THREEHEARTHS_API FRoute
    {
        FString FromRegionId;
        FString ToRegionId;
        TArray<FVector2D> WaypointsCm;
    };

    struct THREEHEARTHS_API FPlan
    {
        float FloorRadiusCm = 0.f;
        FVector2D TownWallCenterCm = FVector2D::ZeroVector;
        float TownWallRadiusCm = 0.f;
        TArray<FRegion> Regions;
        TArray<FRoute> Routes;
        FString SourcePolicy;
    };

    THREEHEARTHS_API FPlan Build();
    THREEHEARTHS_API float HeightAt(const FVector2D& XYCm);
    THREEHEARTHS_API bool IsInsideFloor(const FVector2D& XYCm);
}
