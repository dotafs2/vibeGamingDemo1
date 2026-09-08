#pragma once

#include "CoreMinimal.h"

// Advisory geometry only: these records do not allocate land or advance work.
struct THREEHEARTHS_API FHearthSettlementDistrict
{
    FString Id, Label, Purpose;
    FVector LabelPosition = FVector::ZeroVector;
    TArray<FVector> Boundary;
    FLinearColor Color = FLinearColor::White;
};

struct THREEHEARTHS_API FHearthSettlementOutline
{
    FString Id;
    TArray<FVector> Points;
    bool bClosed = false;
};

struct THREEHEARTHS_API FHearthSettlementPlan
{
    TArray<FHearthSettlementDistrict> Districts;
    TArray<FHearthSettlementOutline> CastleLines;
    FBox Bounds{ForceInit};
};

namespace HearthSettlementPlan
{
    THREEHEARTHS_API FHearthSettlementPlan Build(const FVector& CastleSite, const FString& TemplateId);
    // Bounded [0,8] soft preference; never replaces ownership/access validation.
    THREEHEARTHS_API float SitingPenalty(const FString& Role, const FVector& Position);
}
