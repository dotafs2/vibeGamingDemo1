#pragma once

#include "CoreMinimal.h"

/**
 * Pure inputs and outputs for the organic Town4 ground.  The implementation
 * intentionally has no dependency on AHearthVillage or on an Actor/component;
 * callers can feed the generated arrays to ProceduralMeshComponent,
 * DynamicMeshComponent, or an offline acceptance tool.
 */
namespace HearthOrganicTerrain
{
    constexpr int32 TerrainVersion = 4;
    constexpr int32 DefaultSeed = 7919;
    constexpr float DefaultTransition = 400.f;

    struct FBounds
    {
        FVector2D Min = FVector2D(-8500.f, -8500.f);
        FVector2D Max = FVector2D(21500.f, 21500.f);

        bool IsValid() const
        {
            return FMath::IsFinite(Min.X) && FMath::IsFinite(Min.Y)
                && FMath::IsFinite(Max.X) && FMath::IsFinite(Max.Y)
                && Max.X > Min.X && Max.Y > Min.Y;
        }
        bool Contains(const FVector2D& P) const
        {
            return IsValid() && P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y;
        }
    };

    /** A rotated rectangle which is smoothly blended into the natural land. */
    struct FFlattenZone
    {
        FVector2D Center = FVector2D::ZeroVector;
        FVector2D HalfSize = FVector2D(250.f, 250.f);
        float YawDegrees = 0.f;
        float Elevation = 0.f;
        float Transition = DefaultTransition;
    };

    /** A road profile is explicit about the elevation at each centerline node. */
    struct FRoadNode
    {
        FVector2D Position = FVector2D::ZeroVector;
        float Elevation = 0.f;
    };

    /** Polyline centerline whose grade is blended smoothly over Width+Transition. */
    struct FRoadCenterline
    {
        TArray<FRoadNode> Nodes;
        float Width = 160.f;
        float Transition = DefaultTransition;
    };

    struct FSettings
    {
        int32 Seed = DefaultSeed;
        FBounds Bounds;
        int32 GridQuadsX = 100;
        int32 GridQuadsY = 100;
        TArray<FFlattenZone> FlattenZones;
        TArray<FRoadCenterline> Roads;
    };

    struct FHeightSample
    {
        float Height = 0.f;
        FVector Normal = FVector::UpVector;
        float SlopeDegrees = 0.f;
        bool bInsideBounds = false;
    };

    /** Output arrays are directly consumable by a procedural/dynamic mesh component. */
    struct FGrid
    {
        TArray<FVector> Vertices;
        TArray<int32> Indices;
        TArray<FVector> Normals;
        TArray<FVector2D> UV0;
        TArray<FLinearColor> VertexColors;
        int32 VertexColumns = 0;
        int32 VertexRows = 0;

        void Reset()
        {
            Vertices.Reset(); Indices.Reset(); Normals.Reset(); UV0.Reset(); VertexColors.Reset();
            VertexColumns = 0; VertexRows = 0;
        }
    };

    THREEHEARTHS_API FBounds DefaultBounds();

    /** Height in centimetres. Inputs outside Bounds are clamped to the edge. */
    THREEHEARTHS_API float HeightAt(const FVector2D& XY, const FSettings& Settings = FSettings());

    /** Central-difference normal and slope in degrees, with finite-boundary reporting. */
    THREEHEARTHS_API FHeightSample NormalOrSlopeAt(const FVector2D& XY, const FSettings& Settings = FSettings());

    /** Generate a finite regular grid, including normals, UVs, colours and triangle indices. */
    THREEHEARTHS_API bool GenerateGrid(const FSettings& Settings, FGrid& OutGrid);

    /** Useful to callers that need to reject steep path samples before mesh creation. */
    THREEHEARTHS_API bool IsWithinBounds(const FVector2D& XY, const FSettings& Settings);
}
