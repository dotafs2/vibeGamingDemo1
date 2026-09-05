#include "HearthVillage.h"
#include "HearthMovement.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace HearthPaths
{
    constexpr float Step=300.f;
    FIntPoint Cell(const FVector& P) { return FIntPoint(FMath::RoundToInt(P.X/Step),FMath::RoundToInt(P.Y/Step)); }
    FVector Point(const FIntPoint& C) { return FVector(C.X*Step,C.Y*Step,8); }
}

bool AHearthVillage::IsLand(const FVector& P) const
{
    if(!bUseCropoutMap) return FMath::Abs(P.X)<1120 && FMath::Abs(P.Y)<960;
    FCollisionQueryParams Query; Query.bTraceComplex=true; Query.AddIgnoredActor(this);
    for(const auto& R:Residents) if(IsValid(R.Actor)) Query.AddIgnoredActor(R.Actor);
    FHitResult Hit;
    return GetWorld()->LineTraceSingleByChannel(Hit,P+FVector(0,0,900),P-FVector(0,0,900),ECC_Visibility,Query)
        && Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("ThreeHearthsBaseTerrain")) && FMath::Abs(Hit.ImpactPoint.Z-2.8f)<2.f;
}

void AHearthVillage::BuildLandGrid()
{
    if(!LandGrid.IsEmpty()) return;
    // Bounded once per world: 45 x 45 cells, five terrain probes per cell.
    for(int32 X=-22;X<=22;++X) for(int32 Y=-22;Y<=22;++Y)
    {
        const FVector P(X*HearthPaths::Step,Y*HearthPaths::Step,8);
        if(IsLand(P) && IsLand(P+FVector(120,120,0)) && IsLand(P+FVector(-120,120,0))
            && IsLand(P+FVector(120,-120,0)) && IsLand(P+FVector(-120,-120,0))) LandGrid.Add(FIntPoint(X,Y));
    }
}

bool AHearthVillage::IsClearPoint(const FVector& P) const
{
    if(!LandGrid.Contains(HearthPaths::Cell(P))) return false;
    for(const auto& Obstacle:FixedObstacles)
        if(FMath::Abs(P.X-Obstacle.X)<Obstacle.Z && FMath::Abs(P.Y-Obstacle.Y)<Obstacle.Z) return false;
    // Empty expansion plots reserve their future footprint, keeping permanent paths between buildings.
    for(const auto& Site:ProductionSites)
        if(FMath::Abs(P.X-Site.Position.X)<Site.Radius+25 && FMath::Abs(P.Y-Site.Position.Y)<Site.Radius+25) return false;
    return true;
}

bool AHearthVillage::IsClearSegment(const FVector& A,const FVector& B) const
{
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/80.f));
    for(int32 I=0;I<=Steps;++I) if(!IsClearPoint(FMath::Lerp(A,B,static_cast<float>(I)/Steps))) return false;
    return true;
}

bool AHearthVillage::FindProductionPath(const FVector& Start,const FVector& End,TArray<FVector>& Out) const
{
    Out.Reset(); if(!IsClearPoint(Start) || !IsClearPoint(End)) return false;
    auto Attach=[this](const FVector& P,FIntPoint& Found)
    {
        float Best=FLT_MAX; bool Valid=false; const auto C=HearthPaths::Cell(P);
        for(int32 X=-2;X<=2;++X) for(int32 Y=-2;Y<=2;++Y)
        {
            const FIntPoint Candidate=C+FIntPoint(X,Y); const FVector Position=HearthPaths::Point(Candidate);
            const float Distance=FVector::DistSquared2D(Position,P);
            if(LandGrid.Contains(Candidate) && Distance<Best && IsClearSegment(P,Position)) { Best=Distance; Found=Candidate; Valid=true; }
        }
        return Valid;
    };
    FIntPoint From,To; if(!Attach(Start,From) || !Attach(End,To)) return false;
    TArray<FIntPoint> Open={From}; TSet<FIntPoint> Closed;
    TMap<FIntPoint,float> Cost; TMap<FIntPoint,FIntPoint> Parent; Cost.Add(From,0);
    const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)};
    bool Found=false;
    for(int32 Iteration=0;!Open.IsEmpty() && Iteration<2025;++Iteration)
    {
        int32 Best=0; float BestScore=FLT_MAX;
        for(int32 I=0;I<Open.Num();++I)
        {
            const auto C=Open[I]; const float Score=Cost[C]+FMath::Abs(C.X-To.X)+FMath::Abs(C.Y-To.Y);
            if(Score<BestScore) { BestScore=Score; Best=I; }
        }
        const auto Current=Open[Best]; Open.RemoveAtSwap(Best); if(Current==To) { Found=true; break; }
        Closed.Add(Current);
        for(const auto& D:Directions)
        {
            const auto Next=Current+D; if(Closed.Contains(Next) || !LandGrid.Contains(Next)) continue;
            if(!IsClearSegment(HearthPaths::Point(Current),HearthPaths::Point(Next))) continue;
            const float NewCost=Cost[Current]+1;
            if(!Cost.Contains(Next) || NewCost<Cost[Next])
            { Cost.Add(Next,NewCost); Parent.Add(Next,Current); Open.AddUnique(Next); }
        }
    }
    if(!Found) return false;
    TArray<FVector> Reverse; FIntPoint C=To;
    for(int32 Guard=0;Guard<2025;++Guard)
    { Reverse.Add(HearthPaths::Point(C)); if(C==From) break; const auto* P=Parent.Find(C); if(!P) return false; C=*P; }
    for(int32 I=Reverse.Num()-1;I>=0;--I) Out.Add(Reverse[I]);
    Out.Add(End);
    // Validate the final route against the actual terrain too, including exact endpoint connectors.
    FVector Previous=Start;
    for(const FVector& Point:Out)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Previous,Point)/120.f));
        for(int32 I=0;I<=Steps;++I) if(!IsLand(FMath::Lerp(Previous,Point,static_cast<float>(I)/Steps))) { Out.Reset(); return false; }
        Previous=Point;
    }
    return true;
}

bool AHearthVillage::ChooseSiteApproach(int32 Index)
{
    auto& Site=ProductionSites[Index]; float Best=FLT_MAX; bool Found=false;
    const FVector Depot(-1650,-1050,8);
    const auto C=HearthPaths::Cell(Site.Position);
    for(int32 X=-3;X<=3;++X) for(int32 Y=-3;Y<=3;++Y)
    {
        const FVector P=HearthPaths::Point(C+FIntPoint(X,Y)); const float D=FVector::Dist2D(P,Site.Position);
        if(D<Site.Radius+30 || D>750 || !IsClearPoint(P)) continue;
        const float Score=D+FVector::Dist2D(P,Depot)*0.06f;
        if(Score<Best) { Site.Approach=P; Best=Score; Found=true; }
    }
    TArray<FVector> Route; Site.bReachable=Found && FindProductionPath(Depot,Site.Approach,Route);
    return Site.bReachable;
}

bool AHearthVillage::FindActivityRoute(int32 Index,const FVector& Target,TArray<FVector>& Route) const
{
    if(!Residents.IsValidIndex(Index) || !IsValid(Residents[Index].Actor)) return false;
    TArray<FVector,TInlineAllocator<8>> Reserved;
    for(int32 I=0;I<Residents.Num();++I) if(I!=Index && IsValid(Residents[I].Actor))
    {
        Reserved.Add(Residents[I].Actor->GetActorLocation());
        if(!Residents[I].Route.IsEmpty()) Reserved.Add(Residents[I].Route.Last());
    }
    // Reserve distinct standing positions at shared destinations, including visits and deposits.
    for(int32 Candidate=0;Candidate<17;++Candidate)
    {
        FVector Stand=Target;
        if(Candidate>0)
        {
            const double Angle=((Candidate-1+Index*3)%8)*UE_DOUBLE_PI/4.;
            const double Radius=Candidate<=8?120.:240.;
            Stand+=FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        }
        if(!HearthMovement::SegmentClear(Stand,Stand,Reserved) || !IsClearSegment(Stand,Target)) continue;
        if(FindProductionPath(Residents[Index].Actor->GetActorLocation(),Stand,Route)) return true;
    }
    Route.Reset(); return false;
}
