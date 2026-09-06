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
    FString WallMaterial = TEXT("timber");
    FString RoofMaterial = TEXT("timber");
    int32 HouseholdSize = 1;
    int32 FriendsNearby = 0;
    int32 Budget = 12;
    bool bRoadAccessible = true;
    float RoadYaw = 0.f;
    FVector Origin = FVector::ZeroVector;
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
    THREEHEARTHS_API FHearthResidentBuildingPlan Build(const FHearthResidentBuildingInput& Input);
    THREEHEARTHS_API bool AppendExpansion(FHearthResidentBuildingPlan& Existing, const FHearthResidentBuildingInput& Input);
    THREEHEARTHS_API FHearthStructureValidationContext ValidationContext(const FHearthResidentBuildingInput& Input);
}
