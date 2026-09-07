#if WITH_DEV_AUTOMATION_TESTS
#include "HearthTownLayout.h"
#include "HearthVillage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTownLayoutTest, "ThreeHearths.Layout.ContinuousStreetPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTownLayoutTest::RunTest(const FString&)
{
    FHearthTownLayoutInput Input;
    Input.IslandMin = FVector2D(-3200.f, -2500.f); Input.IslandMax = FVector2D(3200.f, 2500.f);
    Input.Roads = {
        { FVector(-2800.f, -700.f, 8.f), FVector(2800.f, -700.f, 8.f), 180.f },
        { FVector(-1800.f, -2200.f, 8.f), FVector(-1800.f, 2100.f, 8.f), 180.f },
        { FVector(900.f, -1900.f, 8.f), FVector(2400.f, -400.f, 8.f), 180.f }
    };
    Input.Markets = { FVector(0.f, -400.f, 8.f) };
    Input.Friends = { FVector(-500.f, -350.f, 8.f) };
    Input.Workpoints = { FVector(800.f, 700.f, 8.f) };
    Input.RequestedHomes = 6; Input.Seed = 17;

    const FHearthTownLayoutPlan Plan = HearthTownLayout::Build(Input);
    TestTrue(TEXT("Planner produces a valid bounded street plan"), HearthTownLayout::IsValid(Plan, Input));
    TestTrue(TEXT("Plan has multiple homes"), Plan.Homes.Num() >= 3);
    TestTrue(TEXT("Plan exposes a shared courtyard for a block"), Plan.bHasCourtyard);
    TestTrue(TEXT("Plan records street endpoints for access"), Plan.StreetNodes.Num() >= 6);

    TSet<int32> RoundedYaw;
    for (const FHearthTownFootprint& Home : Plan.Homes) RoundedYaw.Add(FMath::RoundToInt(Home.Yaw));
    TestTrue(TEXT("Three road directions create distinct orientations"), RoundedYaw.Num() >= 3);
    for (const FHearthTownFootprint& Home : Plan.Homes)
    {
        TestTrue(TEXT("Every door remains near its frontage street"), FVector::Dist2D(Home.Center, Home.Door) >= 90.f && FVector::Dist2D(Home.Center, Home.Door) <= 300.f);
    }

    FHearthTownLayoutInput WithExisting = Input;
    WithExisting.ExistingBuildings.Add({ TEXT("old_house"), FVector(2850.f, 2100.f, 8.f), FVector(2600.f, 2100.f, 8.f), FVector2D(250.f, 240.f), 0.f, true });
    WithExisting.RequestedHomes = 6;
    const FHearthTownLayoutPlan Before = HearthTownLayout::Build(WithExisting);
    TestTrue(TEXT("Existing occupancy remains valid while adding a block"), HearthTownLayout::IsValid(Before, WithExisting));
    TestTrue(TEXT("A future expansion slot is planned"), Before.Expansions.Num() > 0);
    TestEqual(TEXT("Existing footprint remains first and unchanged"), Before.Homes[0].Id, FString(TEXT("old_house")));
    TestTrue(TEXT("Expansion references an existing planned home"), Before.Expansions[0].ParentIndex >= 1);
    for (const FHearthTownFootprint& Home : Plan.Homes)
        if (!Home.bExisting)
            TestTrue(TEXT("Candidate ID remains stable when an existing building is inserted"), Before.Homes.ContainsByPredicate([&](const FHearthTownFootprint& Other) { return Other.Id == Home.Id; }));
    for (const FHearthTownExpansion& Expansion : Plan.Expansions)
        TestTrue(TEXT("Expansion ID remains stable when an existing building is inserted"), Before.Expansions.ContainsByPredicate([&](const FHearthTownExpansion& Other) { return Other.Id == Expansion.Id; }));

    const FHearthTownLayoutPlan Repeat = HearthTownLayout::Build(Input);
    TestEqual(TEXT("Same seed gives same home count"), Repeat.Homes.Num(), Plan.Homes.Num());
    for (int32 I = 0; I < Plan.Homes.Num() && I < Repeat.Homes.Num(); ++I)
    {
        TestTrue(TEXT("Same seed gives deterministic centers"), Plan.Homes[I].Center.Equals(Repeat.Homes[I].Center, .01f));
        TestTrue(TEXT("Same seed gives deterministic orientation"), FMath::IsNearlyEqual(Plan.Homes[I].Yaw, Repeat.Homes[I].Yaw));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTownLayoutRuntimeTest, "ThreeHearths.Layout.RuntimeContinuousStreetSites", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTownLayoutRuntimeTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated runtime layout world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140.f,140.f,1.f));

    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState();
    TArray<const FHearthSite*> Homes;
    for(const FHearthSite& Site:Village->ProductionSites) if(Site.Kind==EHearthSiteKind::Empty && Site.bExpansion && !Village->IsRoyalSite(static_cast<int32>(&Site-Village->ProductionSites.GetData()))) Homes.Add(&Site);
    TestTrue(TEXT("Runtime initialization creates at least six planner supplied residential sites"),Homes.Num()>=6);
    TestTrue(TEXT("Runtime status reports the continuous street layout"),Village->ProductionStatus.Contains(TEXT("连续街区住宅位")));

    int32 Reachable=0,NearRoad=0;
    for(const FHearthSite* Home:Homes)
    {
        Reachable+=Home->bReachable;
        bool Near=false;
        for(const auto& Road:HearthTownLayout::VillageRoads(Village->bOrganicTownLayout,Village->TownLayoutVersion))
            if(FVector::Dist2D(Home->Position,FMath::ClosestPointOnSegment(Home->Position,Road.A,Road.B))<=Road.Width*.5f+400) Near=true;
        NearRoad+=Near;
    }
    TestEqual(TEXT("Every planned residential site passes runtime path validation"),Reachable,Homes.Num());
    TestEqual(TEXT("Every planned home fronts a real rendered street segment"),NearRoad,Homes.Num());

    int32 LargestStreetChain=0;
    for(int32 Seed=0;Seed<Homes.Num();++Seed)
    {
        TSet<int32> Connected; Connected.Add(Seed);
        TArray<int32> Open={Seed};
        while(!Open.IsEmpty())
        {
            const int32 J=Open.Pop(EAllowShrinking::No);
            for(int32 I=0;I<Homes.Num();++I) if(!Connected.Contains(I)
                && FVector::Dist2D(Homes[I]->Position,Homes[J]->Position)<=(Village->bOrganicTownLayout?1200.f:620.f))
            { Connected.Add(I); Open.Add(I); }
        }
        LargestStreetChain=FMath::Max(LargestStreetChain,Connected.Num());
    }
    if(!Village->bOrganicTownLayout) TestTrue(TEXT("Legacy sites retain their adjacent street chain"),LargestStreetChain>=6);
    else
    {
        const auto Roads=HearthTownLayout::VillageRoads(true,Village->TownLayoutVersion);
        TSet<int32> ConnectedRoads; ConnectedRoads.Add(0);
        for(int32 Pass=0;Pass<Roads.Num();++Pass) for(int32 I=0;I<Roads.Num();++I)
            for(int32 J=0;J<Roads.Num();++J) if(ConnectedRoads.Contains(J) && (Roads[I].A.Equals(Roads[J].A) || Roads[I].A.Equals(Roads[J].B) || Roads[I].B.Equals(Roads[J].A) || Roads[I].B.Equals(Roads[J].B))) ConnectedRoads.Add(I);
        TestEqual(TEXT("All organically separated home clusters share a connected street network"),ConnectedRoads.Num(),Roads.Num());
    }
    float ClosestSameFrontage=FLT_MAX;
    for(int32 I=0;I<Homes.Num();++I) for(int32 J=I+1;J<Homes.Num();++J)
        if(FMath::Abs(Homes[I]->Position.X-Homes[J]->Position.X)<1.f)
            ClosestSameFrontage=FMath::Min(ClosestSameFrontage,FMath::Abs(Homes[I]->Position.Y-Homes[J]->Position.Y));
    TestTrue(TEXT("Legacy equal-frontage roofs retain their planned clearance"),ClosestSameFrontage>=580.f);

    for(int32 I=0;I<Homes.Num();++I) for(int32 J=I+1;J<Homes.Num();++J)
        TestTrue(TEXT("Staggered runtime plots retain roof clearance"),FVector::Dist2D(Homes[I]->Position,Homes[J]->Position)>=560.f);
    const FVector ExpectedFarm(-1460,-3150,8),ExpectedMine(-1000,3100,8),ExpectedCarpenter(-2250,-1050,8);
    TestTrue(TEXT("Existing farm location remains unchanged"),Village->ProductionSites.ContainsByPredicate([&](const FHearthSite& S){return S.Kind>=EHearthSiteKind::Corn&&S.Kind<=EHearthSiteKind::Pumpkin&&S.Position.Equals(ExpectedFarm,.1f);}));
    TestTrue(TEXT("Existing resource location remains unchanged"),Village->ProductionSites.ContainsByPredicate([&](const FHearthSite& S){return S.Kind==EHearthSiteKind::Stone&&S.Position.Equals(ExpectedMine,.1f);}));
    TestTrue(TEXT("Existing workpoint remains unchanged"),Village->ProductionSites.ContainsByPredicate([&](const FHearthSite& S){return S.Kind==EHearthSiteKind::Carpenter&&S.Position.Equals(ExpectedCarpenter,.1f);}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTown3LayoutGeometryTest, "ThreeHearths.Layout.Town3ThirtyFrontageHomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTown3LayoutGeometryTest::RunTest(const FString&)
{
    FHearthTownLayoutInput Input; Input.LayoutVersion=3; Input.bOrganic=true; Input.Seed=7919; Input.RequestedHomes=30; Input.CandidateSpacing=760.f;
    Input.IslandMin=FVector2D(-8500.f,-8500.f); Input.IslandMax=FVector2D(21500.f,21500.f); Input.Roads=HearthTownLayout::VillageRoads(true,3);
    FHearthTownRect CastleReserve; CastleReserve.Center=FVector(6500,6500,8); CastleReserve.HalfExtent=FVector2D(5000); CastleReserve.Clearance=100;
    Input.TerrainBlockers.Add(CastleReserve);
    const FHearthTownLayoutPlan Plan=HearthTownLayout::Build(Input);
    TestTrue(TEXT("Town3 planner supplies all thirty homes on the street graph"),Plan.Homes.Num()>=30);
    TestTrue(TEXT("Town3 planner remains geometrically valid"),HearthTownLayout::IsValid(Plan,Input));
    int32 FrontageAligned=0;
    for(const FHearthTownFootprint& Home:Plan.Homes) if(!Home.bExisting)
    {
        const float DoorYaw=(Home.Door-Home.Center).Rotation().Yaw;
        const float LocalMinusYYaw=Home.Yaw-90.f;
        float Delta=FMath::Abs(FRotator::NormalizeAxis(DoorYaw-LocalMinusYYaw));
        if(Delta<=8.f) ++FrontageAligned;
    }
    TestTrue(TEXT("Town3 house rotations follow their actual frontage entrances"),FrontageAligned>=30);
    if(Plan.Homes.Num()<2) return false;
    const FVector RejectedA=Plan.Homes[0].Center,RejectedB=Plan.Homes[1].Center;
    FHearthTownLayoutInput Filtered=Input;
    Filtered.IsCandidateUsable=[RejectedA,RejectedB](const FHearthTownFootprint& Home)
    { return !Home.Center.Equals(RejectedA,.01f) && !Home.Center.Equals(RejectedB,.01f); };
    const auto Replacements=HearthTownLayout::Build(Filtered);
    TestEqual(TEXT("Two rejected first-choice footprints are replaced within the same candidate pool"),Replacements.Homes.Num(),30);
    TestTrue(TEXT("Replacement homes retain road, clearance and castle reserve checks"),HearthTownLayout::IsValid(Replacements,Filtered));
    for(const auto& Home:Replacements.Homes) TestTrue(TEXT("Every selected home passes scene eligibility"),Filtered.IsCandidateUsable(Home));
    Filtered.IsCandidateUsable=[](const FHearthTownFootprint&) { return false; };
    TestEqual(TEXT("Exhausted eligible candidates never fall back to unsafe homes"),HearthTownLayout::Build(Filtered).Homes.Num(),0);
    return true;
}
#endif
