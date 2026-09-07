#pragma once

#include "CoreMinimal.h"

/** A single native VillageKit piece in starter-building local coordinates.
 * Local +X is the gable width, local -Y is the frontage, and +Z is up.
 * Scale is intentionally kept at one for every authored kit piece.
 */
struct THREEHEARTHS_API FHearthBuildingAppearancePart
{
    FString AssetId;
    FString AssetPath;
    FString Role;
    FVector Offset = FVector::ZeroVector;
    float Yaw = 0.f;
    FVector Scale = FVector::OneVector;
};

struct THREEHEARTHS_API FHearthBuildingAppearance
{
    FString Archetype;
    FVector2D CoreFootprintCm = FVector2D::ZeroVector;
    FVector2D OccupiedFootprintCm = FVector2D::ZeroVector;
    FVector2D EntranceOffsetCm = FVector2D::ZeroVector;
    float EntranceYaw = 0.f;
    float FloorHeightCm = 280.f;
    float RoofRidgeHeightCm = 0.f;
    int32 Floors = 1;
    int32 RoofSpanCount = 1;
    int32 StairCount = 0;
    bool bTown3 = false;
    bool bHasCourtyard = false;
    FString LayoutVariant;
    TArray<FHearthBuildingAppearancePart> Parts;
};

namespace HearthBuildingAppearance
{
    THREEHEARTHS_API bool Build(const FString& Archetype, const FString& WallMaterial,
        const FString& RoofMaterial, uint32 Seed, bool bTown3, FHearthBuildingAppearance& Out);
    THREEHEARTHS_API bool Validate(const FHearthBuildingAppearance& Appearance, FString* OutError = nullptr);
}
