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
    /** True when this appearance was authored by the version 4 composed-massing builder. */
    bool bComposed = false;
    bool bHasCourtyard = false;
    FString LayoutVariant;

    // Version 4 massing metadata.  These are deliberately data-only so siting and
    // persistence code can reason about the authored shape without inspecting parts.
    int32 MassCount = 0;
    TArray<int32> HeightTiers;
    FString ShapeId;
    FVector2D ShapeBoundsMinCm = FVector2D::ZeroVector;
    FVector2D ShapeBoundsMaxCm = FVector2D::ZeroVector;
    TArray<FHearthBuildingAppearancePart> Parts;
};

namespace HearthBuildingAppearance
{
    THREEHEARTHS_API bool Build(const FString& Archetype, const FString& WallMaterial,
        const FString& RoofMaterial, uint32 Seed, bool bTown3, FHearthBuildingAppearance& Out);
    /** Build the authored version 4 organic composition (LayoutId 0..5). */
    THREEHEARTHS_API bool BuildComposed(const FString& Archetype, const FString& WallMaterial,
        const FString& RoofMaterial, uint32 Seed, int32 LayoutId, FHearthBuildingAppearance& Out);
    THREEHEARTHS_API bool Validate(const FHearthBuildingAppearance& Appearance, FString* OutError = nullptr);
}
