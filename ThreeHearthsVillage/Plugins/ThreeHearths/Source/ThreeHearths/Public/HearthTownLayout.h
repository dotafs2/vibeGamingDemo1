#pragma once

#include "CoreMinimal.h"

/** Pure, engine-independent inputs for planning a continuous residential street. */
struct FHearthTownRoadSegment
{
    FVector A = FVector::ZeroVector;
    FVector B = FVector::ZeroVector;
    float Width = 180.f;
};

struct FHearthTownRect
{
    FVector Center = FVector::ZeroVector;
    FVector2D HalfExtent = FVector2D(120.f, 100.f);
    float Clearance = 60.f;
};

struct FHearthTownFootprint
{
    FString Id;
    FVector Center = FVector::ZeroVector;
    FVector Door = FVector::ZeroVector;
    FVector2D HalfExtent = FVector2D(120.f, 100.f);
    float Yaw = 0.f;
    bool bExisting = false;
};

struct FHearthTownExpansion
{
    FString Id;
    FVector Center = FVector::ZeroVector;
    FVector Door = FVector::ZeroVector;
    FVector2D HalfExtent = FVector2D(120.f, 100.f);
    float Yaw = 0.f;
    int32 ParentIndex = -1;
};

struct FHearthTownLayoutInput
{
    FVector2D IslandMin = FVector2D(-6000.f, -6000.f);
    FVector2D IslandMax = FVector2D(5700.f, 5700.f);
    TArray<FHearthTownRoadSegment> Roads;
    TArray<FVector> Markets;
    TArray<FVector> Friends;
    TArray<FVector> Workpoints;
    TArray<FHearthTownRect> TerrainBlockers;
    TArray<FHearthTownFootprint> ExistingBuildings;
    int32 RequestedHomes = 6;
    int32 Seed = 1;
};

struct FHearthTownLayoutPlan
{
    TArray<FHearthTownFootprint> Homes;
    TArray<FHearthTownExpansion> Expansions;
    TArray<FVector> StreetNodes;
    FVector CourtyardCenter = FVector::ZeroVector;
    bool bHasCourtyard = false;
};

namespace HearthTownLayout
{
    THREEHEARTHS_API FHearthTownLayoutPlan Build(const FHearthTownLayoutInput& Input);
    THREEHEARTHS_API bool IsValid(const FHearthTownLayoutPlan& Plan, const FHearthTownLayoutInput& Input);
}
