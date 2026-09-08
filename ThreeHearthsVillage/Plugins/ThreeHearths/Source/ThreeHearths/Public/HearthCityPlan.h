#pragma once

#include "CoreMinimal.h"
#include "HearthTownLayout.h"

struct FHearthCityLandmark
{
    FString Id;
    FString Kind;
    FVector Position = FVector::ZeroVector;
    FVector Approach = FVector::ZeroVector;
    float Radius = 0.f;
};

struct FHearthCityDistrict
{
    FString Id;
    FString Kind;
    FVector Center = FVector::ZeroVector;
    float Radius = 0.f;
};

struct FHearthCityPlan
{
    int32 LayoutVersion = 2;
    int32 RecommendedPopulation = 10;
    FVector2D MapMin = FVector2D(-6000.f, -6000.f);
    FVector2D MapMax = FVector2D(5700.f, 5700.f);
    TArray<FHearthTownRoadSegment> Roads;
    TArray<FHearthCityLandmark> Landmarks;
    TArray<FHearthCityDistrict> Districts;
};

namespace HearthCityPlan
{
    THREEHEARTHS_API FHearthCityPlan Build();
    THREEHEARTHS_API FHearthCityPlan BuildVersion3();
    THREEHEARTHS_API FHearthCityPlan BuildVersion4();
    THREEHEARTHS_API FHearthCityPlan BuildForVersion(int32 LayoutVersion);
}
