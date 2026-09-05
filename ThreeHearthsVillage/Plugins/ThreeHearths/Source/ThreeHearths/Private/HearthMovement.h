#pragma once
#include "CoreMinimal.h"

namespace HearthMovement
{
    constexpr float Separation=100.f;
    // Test the whole movement segment, including when recovering from an old overlap.
    bool SegmentClear(const FVector& Start,const FVector& End,TConstArrayView<FVector> People);
    bool FindDetour(const FVector& Start,const FVector& Goal,TConstArrayView<FVector> People,
        TFunctionRef<bool(const FVector&,const FVector&)> ClearGround,TArray<FVector>& Out);
}
