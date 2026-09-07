#include "HearthVillage.h"
#include "HearthMovement.h"
#include "HearthCityPlan.h"

FVector AHearthVillage::HomeApproach(int32 Plot) const
{
    if(Plot<0 || Plot>=HousingPlotCount()) return FVector::ZeroVector;
    if(TownLayoutVersion>=3)
    {
        const FVector Entry=PlotEntrances[Plot];
        if(!Entry.IsNearlyZero())
        {
            const FVector Outward=(Entry-PlotPositions[Plot]).GetSafeNormal2D();
            return Entry+Outward*120.f;
        }
        // Old Town3 saves may not have an entry field. Keep their recorded
        // position/yaw and use a short safe exterior offset until the next
        // layout rebuild derives the exact frontage point.
        return PlotPositions[Plot]+FRotator(0,PlotYaws[Plot]-90.f,0).RotateVector(FVector(220.f,0,0));
    }
    return PlotPositions[Plot]+FRotator(0,PlotYaws[Plot],0).RotateVector(FVector(bUseCropoutMap && TownLayoutVersion>0?-370.f:-245.f,0,0));
}

bool AHearthVillage::GenerateStarterNeighborhood(const TArray<FHearthTownRoadSegment>& Roads)
{
    FHearthTownLayoutInput Input; Input.Roads=Roads; Input.bOrganic=true; Input.Seed=7919; Input.LayoutVersion=TownLayoutVersion;
    Input.RequestedHomes=TownLayoutVersion>=3?HearthVillageLimits::Town3Population:32;
    Input.CandidateSpacing=TownLayoutVersion>=3?760.f:Input.CandidateSpacing;
    Input.IslandMin=TownLayoutVersion>=3?FVector2D(-8500.f,-8500.f):FVector2D(-4700,-4600);
    Input.IslandMax=TownLayoutVersion>=3?FVector2D(21500.f,21500.f):FVector2D(1500,4500);
    Input.Markets={FVector(-1100,-1050,8)}; Input.Workpoints={FVector(-2250,-1050,8),FVector(-2800,-1050,8)};
    auto Block=[&](FVector P,float Radius){FHearthTownRect R;R.Center=P;R.HalfExtent=FVector2D(Radius);R.Clearance=100;Input.TerrainBlockers.Add(R);};
    Block(FVector(-1100,-1050,8),330); Block(FVector(-2250,-1050,8),190); Block(FVector(-2800,-1050,8),210);
    for(int32 I=0;I<4;++I) Block(FVector(-1460+(I%2)*650,-3150+(I/2)*650,8),245);
    for(int32 I=0;I<3;++I)
    { Block(FVector(-4300,-1900+I*1900,8),160); Block(FVector(-3600,-1900+I*1900,8),260); Block(FVector(-1000+I*800,3100,8),145); }
    Block(FVector(-2600,3100,8),160);
    Block(FVector(-3300,-2800,8),120); Block(FVector(-2900,-3400,8),120); Block(FVector(-3700,-3400,8),120);
    if(TownLayoutVersion>=2) for(const auto& Landmark:HearthCityPlan::BuildForVersion(TownLayoutVersion).Landmarks)
        if(Landmark.Kind==TEXT("castle")) Block(Landmark.Position,Landmark.Radius);
    BuildLandGrid();
    const auto HasSafeGround=[this](const FHearthTownFootprint& Home)
    {
        const FVector P=Home.Center;
        // Town3 appearance fronts are local -Y; keep the stored yaw aligned
        // with the actual frontage entry used by the planner.
        const float Yaw=FRotator::NormalizeAxis((Home.Door-P).Rotation().Yaw+90.f);
        const FVector Entry=TownLayoutVersion>=3
            ? Home.Door+(Home.Door-P).GetSafeNormal2D()*120.f
            : P+FRotator(0,Yaw,0).RotateVector(FVector(-370,0,0));
        const float HalfWidth=TownLayoutVersion>=3?560.f:250.f;
        const float HalfDepth=TownLayoutVersion>=3?650.f:250.f;
        if(!IsLand(P) || !IsLand(P+FVector(HalfWidth,HalfDepth,0)) || !IsLand(P-FVector(HalfWidth,HalfDepth,0))
            || !IsLand(P+FVector(-HalfWidth,HalfDepth,0)) || !IsLand(P+FVector(HalfWidth,-HalfDepth,0)) || !IsLand(Entry)) return false;
        const FIntPoint Cell(FMath::RoundToInt(Entry.X/300),FMath::RoundToInt(Entry.Y/300));
        return LandGrid.Contains(Cell);
    };
    int32 GroundChecked=0,GroundRejected=0;
    if(TownLayoutVersion>=3) Input.IsCandidateUsable=[&](const FHearthTownFootprint& Home)
    { ++GroundChecked; const bool Safe=HasSafeGround(Home); GroundRejected+=!Safe; return Safe; };
    const auto Plan=HearthTownLayout::Build(Input);
    TArray<FVector> Centers; TArray<float> Yaws; TArray<FVector> Entries;
    for(const auto& Home:Plan.Homes)
    {
        if(Centers.Num()>=HousingPlotCount()) break;
        if(!HasSafeGround(Home)) continue;
        Centers.Add(Home.Center); Yaws.Add(FRotator::NormalizeAxis((Home.Door-Home.Center).Rotation().Yaw+90.f)); Entries.Add(Home.Door);
    }
    if(TownLayoutVersion>=3) UE_LOG(LogTemp,Display,TEXT("TOWN3_STARTER_CANDIDATES checked=%d ground_or_grid_rejected=%d geometric_homes=%d accepted=%d required=%d"),
        GroundChecked,GroundRejected,Plan.Homes.Num(),Centers.Num(),HousingPlotCount());
    if(Centers.Num()!=HousingPlotCount())
    {
        TownLayoutError=FString::Printf(TEXT("Town3 requires %d safe plots; planner produced %d"),HousingPlotCount(),Centers.Num());
        return false;
    }
    for(int32 I=0;I<Centers.Num();++I) {PlotPositions[I]=Centers[I];PlotYaws[I]=Yaws[I];PlotEntrances[I]=Entries[I];}
    return true;
}

FHearthResidentSitingResult AHearthVillage::EvaluateResidentSite(int32 Index,const FVector& Position) const
{
    if(!Residents.IsValidIndex(Index)) return {};
    const auto& R=Residents[Index]; FHearthResidentSitingInput Input;
    Input.Role=R.Role; Input.Personality=R.Personality; Input.Goal=R.DesignGoal; Input.bKing=R.bKing;
    Input.Market=FVector(-1650,-1050,8); Input.Current=R.Actor?R.Actor->GetActorLocation():Input.Market;
    Input.Home=R.Plot>=0?PlotPositions[R.Plot]:Input.Current;
    for(const auto& S:ProductionSites)
    {
        const bool Relevant=(R.Role.Contains(TEXT("陶")) && (S.Kind==EHearthSiteKind::TileKiln || S.Kind==EHearthSiteKind::ClayPit))
            || (R.Role.Contains(TEXT("木")) && S.Kind==EHearthSiteKind::Carpenter)
            || (R.Role.Contains(TEXT("农")) && S.Kind>=EHearthSiteKind::Corn && S.Kind<=EHearthSiteKind::Pumpkin)
            || (R.Role.Contains(TEXT("石")) && S.Kind==EHearthSiteKind::Stone);
        if(Relevant && S.bReachable) Input.Workpoints.Add(S.Approach);
    }
    for(const auto& Other:Residents)
    {
        const auto* Bond=R.Bonds.Find(Other.StableId);
        if(Bond && Bond->Meetings>0 && Bond->Affinity>10 && Other.Plot>=0) Input.Friends.Add(PlotPositions[Other.Plot]);
    }
    return HearthResidentSiting::Evaluate(Input,Position);
}
