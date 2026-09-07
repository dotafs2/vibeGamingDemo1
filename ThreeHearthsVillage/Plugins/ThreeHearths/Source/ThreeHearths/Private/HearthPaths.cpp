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

float HearthCottage::WalkRadius(const FHearthCottageComponent& Part)
{
    if(Part.AssetId!=TEXT("floor_timber_2m")) return 0;
    // A named outdoor deck is traversable. Indoor floor bays retain the old
    // blocking behavior until their interior routing is implemented.
    if(Part.Id.EndsWith(TEXT(":component:canopy_deck"))) return 0;
    const float R=FMath::DegreesToRadians(Part.Yaw);
    return 100*(FMath::Abs(FMath::Sin(R))+FMath::Abs(FMath::Cos(R)))+35;
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
    // Town2 keeps the original bounded probe. Town3 uses the generated 300 m
    // square centered at (6500,6500), still with a fixed 102 x 102 probe
    // window and five corners per cell so this cannot become an unbounded scan.
    const int32 MinCell=TownLayoutVersion>=3?-29:-22;
    const int32 MaxCell=TownLayoutVersion>=3?72:22;
    for(int32 X=MinCell;X<=MaxCell;++X) for(int32 Y=MinCell;Y<=MaxCell;++Y)
    {
        const FVector P(X*HearthPaths::Step,Y*HearthPaths::Step,8);
        if(IsLand(P) && IsLand(P+FVector(120,120,0)) && IsLand(P+FVector(-120,120,0))
            && IsLand(P+FVector(120,-120,0)) && IsLand(P+FVector(-120,-120,0))) LandGrid.Add(FIntPoint(X,Y));
    }
}

bool AHearthVillage::IsSiteWalkObstacle(const FHearthSite& Site) const
{
    if(!Site.BuildPlanId.IsEmpty()) return true;
    if(!PublicProject.Id.IsEmpty() && PublicProject.Status!=TEXT("unapproved") && PublicProject.Status!=TEXT("cancelled")
        && ProductionSites.IsValidIndex(PublicProject.Site) && &Site==&ProductionSites[PublicProject.Site]) return true;
    return Site.Kind!=EHearthSiteKind::Empty && Site.Kind!=EHearthSiteKind::Land;
}

bool AHearthVillage::IsClearPoint(const FVector& P) const
{
    if(!LandGrid.Contains(HearthPaths::Cell(P))) return false;
    for(const auto& Obstacle:FixedObstacles)
        if(FMath::Abs(P.X-Obstacle.X)<Obstacle.Z && FMath::Abs(P.Y-Obstacle.Y)<Obstacle.Z) return false;
    // Ownership reserves building rights, not a physical wall across vacant ground.
    for(const auto& Site:ProductionSites)
        if(IsSiteWalkObstacle(Site)
            && FMath::Abs(P.X-Site.Position.X)<Site.Radius+25 && FMath::Abs(P.Y-Site.Position.Y)<Site.Radius+25) return false;
    for(const auto& S:ProductionSites) for(const auto& C:S.CottageComponents)
    {
        const float Radius=HearthCottage::WalkRadius(C); if(Radius<=0) continue;
        const FVector Center=S.Position+C.Offset;
        if(FMath::Abs(P.X-Center.X)<Radius && FMath::Abs(P.Y-Center.Y)<Radius) return false;
    }
    return true;
}

bool AHearthVillage::IsClearSegment(const FVector& A,const FVector& B) const
{
    // Sparse probes can miss a corner that a later 12 cm movement step hits,
    // producing an endless replan to the same invalid shortcut. Use exact boxes
    // and every crossed grid cell for both planning and execution.
    const auto MayLeaveContainingBox=[](const FVector& Start,const FVector& End,const FVector& Center,float Radius)
    {
        const FVector2D From(Start.X-Center.X,Start.Y-Center.Y),Delta(End.X-Start.X,End.Y-Start.Y);
        const bool bStartsInside=FMath::Abs(From.X)<Radius && FMath::Abs(From.Y)<Radius;
        return bStartsInside && FVector2D::DotProduct(From,Delta)>=0.f;
    };
    for(const auto& O:FixedObstacles) if(HearthMovement::SegmentHitsBox(A,B,O,O.Z) && !MayLeaveContainingBox(A,B,O,O.Z)) return false;
    for(const auto& S:ProductionSites)
        if(IsSiteWalkObstacle(S)
            && HearthMovement::SegmentHitsBox(A,B,S.Position,S.Radius+25) && !MayLeaveContainingBox(A,B,S.Position,S.Radius+25)) return false;
    for(const auto& S:ProductionSites) for(const auto& C:S.CottageComponents)
    {
        const float Radius=HearthCottage::WalkRadius(C); if(Radius<=0) continue;
        const FVector Center=S.Position+C.Offset;
        if(HearthMovement::SegmentHitsBox(A,B,Center,Radius) && !MayLeaveContainingBox(A,B,Center,Radius)) return false;
    }
    return HearthMovement::GridSegmentClear(A,B,HearthPaths::Step,[this](FIntPoint Cell) { return LandGrid.Contains(Cell); });
}

bool AHearthVillage::FindProductionPath(const FVector& Start,const FVector& End,TArray<FVector>& Out) const
{
    Out.Reset(); if(!IsClearPoint(Start) || !IsClearPoint(End)) return false;
    if(Start.Equals(End,.01)) return true;
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
    TArray<FVector> PreferredRoute;
    const float ExistingEntryDistance=FVector::Dist2D(Site.Approach,Site.Position);
    if(ExistingEntryDistance>=Site.Radius+30 && ExistingEntryDistance<=FMath::Max(750.f,Site.Radius+250.f) && IsClearPoint(Site.Approach) && FindProductionPath(Depot,Site.Approach,PreferredRoute))
    { Site.bReachable=true; return true; }
    FVector RoadPoint=Site.Position; double RoadDistance=DBL_MAX;
    for(const auto& Road:HearthTownLayout::VillageRoads(bOrganicTownLayout,TownLayoutVersion))
    {
        const FVector Q=FMath::ClosestPointOnSegment(Site.Position,Road.A,Road.B);
        const double D=FVector::DistSquared2D(Q,Site.Position);
        if(D<RoadDistance){RoadDistance=D;RoadPoint=Q;}
    }
    const FVector StreetEntry=Site.Position+(RoadPoint-Site.Position).GetSafeNormal2D()*(Site.Radius+90);
    if(IsClearPoint(StreetEntry) && FindProductionPath(Depot,StreetEntry,PreferredRoute))
    { Site.Approach=StreetEntry; Site.bReachable=true; return true; }
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
