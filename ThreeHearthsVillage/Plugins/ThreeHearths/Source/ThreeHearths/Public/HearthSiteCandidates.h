#pragma once

#include "CoreMinimal.h"
#include "HearthTownLayout.h"

struct FHearthSiteCandidateBlocker
{
    FVector Center = FVector::ZeroVector;
    float Radius = 0.f;
};

struct FHearthReplacementCandidate
{
    FVector Position = FVector::ZeroVector;
    FVector Approach = FVector::ZeroVector;
    float Yaw = 0.f;
    int32 RoadIndex = -1;
    int32 SampleIndex = -1;
    int32 Side = 0;
    FString StableKey;
};

struct FHearthReplacementCandidateInput
{
    TArray<FHearthTownRoadSegment> Roads;
    TArray<FHearthSiteCandidateBlocker> Occupied;
    FVector ResidentAnchor = FVector::ZeroVector;
    FVector PublicSite = FVector::ZeroVector;
    int32 Seed = 1;
    int32 MaxCandidates = 32;
    int32 SamplesPerRoad = 4;
    float PlotRadius = 260.f;
    float RoadClearance = 80.f;
};

namespace HearthSiteCandidates
{
    THREEHEARTHS_API TArray<FHearthReplacementCandidate> Generate(const FHearthReplacementCandidateInput& Input);
}
