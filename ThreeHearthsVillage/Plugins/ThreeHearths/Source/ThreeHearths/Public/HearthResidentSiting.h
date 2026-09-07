#pragma once

#include "CoreMinimal.h"

struct FHearthResidentSitingInput
{
    FString Role;
    FString Personality;
    FString Goal;
    FVector Current = FVector::ZeroVector;
    FVector Home = FVector::ZeroVector;
    FVector Market = FVector::ZeroVector;
    TArray<FVector> Workpoints;
    TArray<FVector> Friends;
    bool bKing = false;
};

struct FHearthResidentSitingResult
{
    float Penalty = 0.f;
    FString Reason;
};

namespace HearthResidentSiting
{
    THREEHEARTHS_API FHearthResidentSitingResult Evaluate(const FHearthResidentSitingInput& Input, const FVector& Candidate);
}
