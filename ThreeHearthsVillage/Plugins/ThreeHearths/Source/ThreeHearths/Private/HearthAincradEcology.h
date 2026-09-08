#pragma once

#include "CoreMinimal.h"
#include "HearthOrganicTerrain.h"

class AActor;

// Transient scenery only: no resource/site IDs, inventory, collision or navigation.
namespace HearthAincradEcology
{
    constexpr int32 SpeciesCount = 8;
    struct FClearing
    {
        FVector2D Center = FVector2D::ZeroVector;
        FVector2D HalfSize = FVector2D(500,500);
        float Yaw = 0;
    };
    struct FInput
    {
        HearthOrganicTerrain::FSettings Terrain;
        TArray<FClearing> Clearings;
        // Runtime replaces these conservative test dimensions with native bounds.
        double MeshRadius[SpeciesCount] = {350,350,130,130,100,100,100,100};
    };
    struct FPlacement
    {
        FVector2D XY = FVector2D::ZeroVector;
        float Scale = 1, Yaw = 0;
        int32 Species = 0, Region = 0;
        double Radius = 0;
    };
    struct FPlan
    {
        TArray<FPlacement> Placements;
        int32 Candidates = 0, ClearanceRejected = 0, SpacingRejected = 0;
        int32 Trees = 0, Understory = 0;
    };

    const TCHAR* AssetPath(int32 Species);
    FPlan BuildPlan(const FInput& Input);
    // Owner must be the freshly generated terrain, not AHearthVillage: the old
    // production initializer scans village-owned tree components as obstacles.
    void Build(AActor& TerrainOwner, const FInput& Input,
        TFunctionRef<float(const FVector&)> GroundHeight);
}
