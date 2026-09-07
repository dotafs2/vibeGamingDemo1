#pragma once

#include "CoreMinimal.h"

struct THREEHEARTHS_API FHearthPlantPart
{
    FString MeshPath;
    FVector Offset = FVector::ZeroVector;
    FVector Scale = FVector::OneVector;
    float Yaw = 0.0f;
    FLinearColor Color;
};

namespace HearthBotanicalCatalog
{
    THREEHEARTHS_API TArray<FString> Species();
    THREEHEARTHS_API TArray<FHearthPlantPart> Build(const FString& Species, int32 Seed);
    THREEHEARTHS_API float Radius(const FString& Species);
}
