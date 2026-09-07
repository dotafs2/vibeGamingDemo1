#pragma once

#include "CoreMinimal.h"
#include "HearthStructurePlan.h"

struct THREEHEARTHS_API FHearthResidentBuildingInput
{
    FString ResidentId;
    FString StableSeed;
    FString ExtensionKey = TEXT("resident_extension_1");
    FString Need = TEXT("shelter");
    FString Occupation = TEXT("general");
    // Empty preserves the legacy attached-row planner. Non-empty values select
    // an explicit catalog-backed residential archetype.
    FString Archetype;
    FString WallMaterial = TEXT("timber");
    FString RoofMaterial = TEXT("timber");
    // -1 preserves legacy attached rows; 0 alternates yard wings; 1 right, 2 left, 3 rear.
    int32 GrowthDirection = -1;
    int32 HouseholdSize = 1;
    int32 FriendsNearby = 0;
    // Compatibility cap for the initial plan. Legacy growth-aware goals use
    // up to three rooms; explicit archetypes may use up to six.
    int32 MaxInitialRooms = 2;
    int32 Budget = 12;
    bool bRoadAccessible = true;
    float RoadYaw = 0.f;
    FVector Origin = FVector::ZeroVector;
    // Initial explicit-archetype layout only, in local centimeters. The world
    // anchor stays Origin. AppendExpansion ignores this; Z must remain zero.
    FVector LayoutOffset = FVector::ZeroVector;
    int32 Wood = 0;
    int32 Planks = 0;
    int32 Beams = 0;
    int32 Stone = 0;
    int32 Tiles = 0;
};

struct THREEHEARTHS_API FHearthResidentExpansionProposal
{
    FString ExtensionKey;
    FString Reason;
    FHearthStructurePlan ResultingPlan;
};

struct THREEHEARTHS_API FHearthResidentBuildingPlan
{
    FHearthStructurePlan Plan;
    FHearthResidentExpansionProposal Expansion;
    FString Reason;
    bool bBuildable = false;
};

namespace HearthResidentBuildingPlanner
{
    // Recipe-only queries: no catalog lookup, plan assembly or geometry validation.
    // Minimum is independent of funds/cap: legacy/rowhouse=1, other homes=2,
    // unsupported/keep=0. Maximum respects MaxInitialRooms and returns 0 when
    // the minimum cannot be funded. Road access and siting remain caller checks.
    THREEHEARTHS_API int32 MinimumInitialRooms(const FHearthResidentBuildingInput& Input);
    THREEHEARTHS_API int32 MaxAffordableInitialRooms(const FHearthResidentBuildingInput& Input);
    THREEHEARTHS_API FHearthResidentBuildingPlan Build(const FHearthResidentBuildingInput& Input);
    // Input resources/budget fund only NEW parts. Explicit archetypes preserve
    // the existing footprint transform and stop at six total rooms. Directions
    // 1/2/3 select right/left/rear; -1/0 alternate independent wings. The caller
    // must still validate external land, roads, neighbors and route access.
    THREEHEARTHS_API bool AppendExpansion(FHearthResidentBuildingPlan& Existing, const FHearthResidentBuildingInput& Input);
    // Adds one idempotent, half-open seating canopy to the existing plan. The
    // host plan remains unchanged; the resulting plan is written to Expansion.
    // Input resources pay only for the new canopy components.
    // Empty host-derived plans retain the legacy deck/seat anchor. Roof/ridge
    // offsets use the source mesh datums; consumers must not recenter them.
    THREEHEARTHS_API bool AppendCanopy(FHearthResidentBuildingPlan& Existing, const FHearthResidentBuildingInput& Input);
    THREEHEARTHS_API FHearthStructureValidationContext ValidationContext(const FHearthResidentBuildingInput& Input);
}
