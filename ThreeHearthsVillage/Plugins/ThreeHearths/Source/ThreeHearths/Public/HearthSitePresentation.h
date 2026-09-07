#pragma once

#include "CoreMinimal.h"

struct FHearthGroundPatch
{
    FVector Offset = FVector::ZeroVector;
    FVector Scale = FVector::OneVector;
    float Yaw = 0.f;
};

namespace HearthSitePresentation
{
    THREEHEARTHS_API TArray<FHearthGroundPatch> CropGround(const FString& StableId, float Radius);
}
