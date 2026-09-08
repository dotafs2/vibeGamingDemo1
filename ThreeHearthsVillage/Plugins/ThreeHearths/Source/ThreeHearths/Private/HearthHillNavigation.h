#pragma once

#include "CoreMinimal.h"
#include "HearthFreightNavigation.h"

// Derived navigation only. No ownership, material or persisted state lives here.
namespace HearthHillNavigation
{
    bool InHill(const FVector& Point,float Margin=0.f);
    bool TouchesHill(const FVector& A,const FVector& B);
    bool OnAscent(const FVector& Point);
    bool AscentSegment(const FVector& A,const FVector& B);
    bool Accessible(const FVector& Point);
    // False means both endpoints are on the same flat region; use the ordinary
    // planner. Otherwise Guide follows only the required portion of the ascent.
    bool MakeGuide(const FVector& Start,const FVector& Goal,TArray<FVector>& Guide);
    // Ordinary forward search attaches the guide's ends. On the known road,
    // bounded curvature following samples every <=50cm and honors every veto.
    bool PlanFreight(HearthFreightNavigation::FPose Start,const FVector& Goal,
        TFunctionRef<bool(const HearthFreightNavigation::FPose&)> Clear,
        TArray<HearthFreightNavigation::FPose>& Out,int32 MaxNodes=6000);
}
