#if WITH_DEV_AUTOMATION_TESTS
#include "HearthMovement.h"
#include "HearthVillage.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMovementGeometryTest,"ThreeHearths.Movement.SweptSeparation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthMovementGeometryTest::RunTest(const FString&)
{
    const TArray<FVector> People={FVector(0,0,8)};
    TestFalse(TEXT("A long movement cannot tunnel through a villager"),HearthMovement::SegmentClear(FVector(-400,0,8),FVector(400,0,8),People));
    TestTrue(TEXT("A parallel lane has sufficient clearance"),HearthMovement::SegmentClear(FVector(-400,120,8),FVector(400,120,8),People));
    TestFalse(TEXT("An occupied destination is unavailable"),HearthMovement::SegmentClear(FVector(0,0,8),FVector(0,0,8),People));
    TestTrue(TEXT("An existing overlap can separate"),HearthMovement::SegmentClear(FVector(20,0,8),FVector(40,0,8),People));
    TestFalse(TEXT("An existing overlap cannot cross through its neighbor"),HearthMovement::SegmentClear(FVector(20,0,8),FVector(-40,0,8),People));
    TArray<FVector> Route;
    auto NorthOnly=[](const FVector& A,const FVector& B) { return A.Y>=-1 && B.Y>=-1; };
    TestTrue(TEXT("Find a detour on the available side of a wall"),HearthMovement::FindDetour(FVector(-300,0,8),FVector(300,0,8),People,NorthOnly,Route));
    FVector Previous(-300,0,8);
    for(const FVector& Waypoint:Route)
    {
        TestTrue(TEXT("Every detour segment maintains separation"),HearthMovement::SegmentClear(Previous,Waypoint,People));
        TestTrue(TEXT("Detour stays on the accessible side"),NorthOnly(Previous,Waypoint));
        Previous=Waypoint;
    }
    auto Narrow=[](const FVector& A,const FVector& B) { return FMath::Abs(A.Y)<40 && FMath::Abs(B.Y)<40; };
    TestFalse(TEXT("Wait when a narrow passage has no room to pass"),HearthMovement::FindDetour(FVector(-300,0,8),FVector(300,0,8),People,Narrow,Route));
    const FVector Stuck(-1213.16,1339.14,8),Door(-1245,950,8),House(-1000,950,8);
    TestTrue(TEXT("Real island house corner missed by 80 cm probes is blocked"),HearthMovement::SegmentHitsBox(Stuck,Door,House,230));
    TestTrue(TEXT("Corner collision is independent of travel direction"),HearthMovement::SegmentHitsBox(Door,Stuck,House,230));
    TestFalse(TEXT("Door standing lane remains outside house"),HearthMovement::SegmentHitsBox(FVector(-1245,1400,8),Door,House,230));
    auto Allowed=[](FIntPoint C) { return C!=FIntPoint(1,0); };
    TestFalse(TEXT("A sub-centimeter crossing into a blocked grid corner is detected"),HearthMovement::GridSegmentClear(FVector(0,-.2,0),FVector(300,300,0),300,Allowed));
    TestTrue(TEXT("A clear parallel grid segment is allowed"),HearthMovement::GridSegmentClear(FVector(0,0,0),FVector(0,600,0),300,Allowed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMovementIntegrationTest,"ThreeHearths.Movement.ResidentTraffic",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthMovementIntegrationTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Create isolated movement test world"),World)) return false;
    auto* Village=World->SpawnActor<AHearthVillage>();
    if(!TestNotNull(TEXT("Spawn village"),Village)) { World->DestroyWorld(false); return false; }
    for(int32 I=0;I<3;++I)
    {
        FHearthResident Person;
        Person.Actor=World->SpawnActor<AHearthVillager>();
        Person.MoveSpeed=240;
        Village->Residents.Add(Person);
    }
    auto Scenario=[this,Village](const TCHAR* Label,const TArray<FVector>& Starts,const TArray<FVector>& Goals,float Dt)
    {
        for(int32 I=0;I<3;++I)
        {
            auto& Person=Village->Residents[I]; Person.Actor->SetActorLocation(Starts[I]);
            Person.Route={Goals[I]}; Person.MoveRetry=0; Person.bMovementBlocked=false;
        }
        bool Complete=false;
        double Closest=DBL_MAX;
        for(int32 Step=0;Step<1200 && !Complete;++Step)
        {
            Complete=true;
            for(int32 I=0;I<3;++I)
            {
                Complete=Village->MoveResident(I,Dt) && Complete;
                for(int32 A=0;A<3;++A) for(int32 B=A+1;B<3;++B)
                    Closest=FMath::Min(Closest,FVector::Dist2D(Village->Residents[A].Actor->GetActorLocation(),Village->Residents[B].Actor->GetActorLocation()));
            }
        }
        TestTrue(FString(Label)+TEXT(": every walker arrives without deadlock"),Complete);
        TestTrue(FString(Label)+TEXT(": separated after every individual movement"),Closest>=HearthMovement::Separation-0.01);
    };
    Scenario(TEXT("Head-on passing"),{FVector(-400,0,8),FVector(400,0,8),FVector(0,700,8)},
        {FVector(400,0,8),FVector(-400,0,8),FVector(0,700,8)},0.05f);
    Scenario(TEXT("Three-way crossing"),{FVector(-400,0,8),FVector(400,0,8),FVector(0,-400,8)},
        {FVector(400,0,8),FVector(-400,0,8),FVector(0,400,8)},0.05f);
    Scenario(TEXT("Stationary worker"),{FVector(-400,0,8),FVector(0,0,8),FVector(0,700,8)},
        {FVector(400,0,8),FVector(0,0,8),FVector(0,700,8)},0.05f);
    Scenario(TEXT("Oversized step"),{FVector(-400,0,8),FVector(0,0,8),FVector(0,700,8)},
        {FVector(400,0,8),FVector(0,0,8),FVector(0,700,8)},3.f);
    Village->Residents[0].Actor->SetActorLocation(FVector(-109,0,8)); Village->Residents[0].Route={FVector(400,0,8)};
    Village->Residents[1].Actor->SetActorLocation(FVector(0,0,8)); Village->Residents[1].Route.Reset();
    Village->Residents[2].Actor->SetActorLocation(FVector(0,700,8));
    Village->Residents[1].Task=EHearthTask::LifeChoosing; Village->Residents[1].BuildProgress=1;
    Village->PendingDecisions.SetNum(3); Village->PendingDecisions[1].bActive=true;
    TestFalse(TEXT("Yielding never interrupts an independent pending decision"),Village->TryYieldFor(0));
    Village->PendingDecisions[1].bActive=false; Village->Residents[1].Task=EHearthTask::ProductionWork;
    TestFalse(TEXT("Yielding never takes over an ongoing production job"),Village->TryYieldFor(0));
    Village->Residents[1].Task=EHearthTask::LifeChoosing;
    TestTrue(TEXT("An idle neighbor can step aside when asked by blocked traffic"),Village->TryYieldFor(0));
    TestFalse(TEXT("The short avoidance movement cannot receive a new job midway"),Village->CanAssignActivity(1));
    const int32 HistoryBefore=Village->DecisionHistory.Num();
    bool Arrived=false; double Clearance=DBL_MAX;
    for(int32 Step=0;Step<1000 && !Arrived;++Step)
    {
        const bool Walker=Village->MoveResident(0,.05f),Neighbor=Village->MoveResident(1,.05f);
        Clearance=FMath::Min(Clearance,FVector::Dist2D(Village->Residents[0].Actor->GetActorLocation(),Village->Residents[1].Actor->GetActorLocation()));
        Arrived=Walker && Neighbor;
    }
    TestTrue(TEXT("Both finish the avoidance without teleporting or overlap"),Arrived && Clearance>=HearthMovement::Separation-.01);
    TestEqual(TEXT("Yield does not create a new paid or production decision"),Village->DecisionHistory.Num(),HistoryBefore);
    TestEqual(TEXT("Idle neighbor keeps its own planning state"),Village->Residents[1].Task,EHearthTask::LifeChoosing);

    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140.f,140.f,1.f));
    Village->bUseCropoutMap=true;
    for(int32 X=-22;X<=22;++X) for(int32 Y=-22;Y<=22;++Y) Village->LandGrid.Add(FIntPoint(X,Y));
    // Exact north-street deadlock from the 10-NPC run: two 285 cm exclusion
    // boxes leave only 30 cm between them, and both walkers inside need to exit
    // past the builder at the mouth. All three have live, opposing tasks.
    Village->FixedObstacles={FVector(-1690,3000,285),FVector(-1690,3600,285),FVector(-2570,3000,285),FVector(-2570,3600,285)};
    FHearthSite DistantSite; DistantSite.Position=FVector(5000,5000,8); DistantSite.Radius=145;
    Village->ProductionSites.Add(DistantSite);
    for(float Dt:{.05f,.2f,3.f})
    {
        const TArray<FVector> Starts={FVector(-2027.549,3300,8),FVector(-1920,3300,8),FVector(-1812,3300,8)};
        const TArray<FVector> Goals={FVector(-1680,3300,8),FVector(-2010,-1050,8),FVector(-1530,-1339.706,8)};
        for(int32 I=0;I<3;++I)
        {
            auto& R=Village->Residents[I]; R.Actor->SetActorLocation(Starts[I]);
            R.MoveRetry=0; R.bYieldingForTraffic=false; R.Task=I==0?EHearthTask::ProductionTravel:EHearthTask::LifeTravel;
            R.ActiveTaskId=FString::Printf(TEXT("north-street-task-%d"),I);
            R.Route.Reset();
            if(I<2) R.Route.Add(FVector(-1800,3300,8));
            if(I>0)
            {
                for(int32 Y=3300;Y>=-300;Y-=300) R.Route.Add(FVector(-2100,Y,8));
                R.Route.Append({FVector(-1800,-300,8),FVector(-1800,-600,8),FVector(-1800,-900,8),FVector(-1800,-1200,8)});
            }
            R.Route.Add(Goals[I]);
        }
        auto& Builder=Village->Residents[0]; Builder.CargoType=3; Builder.CargoAmount=1;
        Builder.ProductionComponentId=TEXT("roof_slope_timber_2m"); Builder.HeldToolId=TEXT("tool_hammer");
        Builder.HeldToolOperationId=Builder.ActiveTaskId;
        bool Complete=false,ClearGeometry=true,SimulatedReload=false; double MinSeparation=DBL_MAX;
        for(int32 Step=0;Step<4000 && !Complete;++Step)
        {
            Complete=true;
            for(int32 I=0;I<3;++I)
            {
                Complete=Village->MoveResident(I,Dt) && Complete;
                ClearGeometry=Village->IsClearPoint(Village->Residents[I].Actor->GetActorLocation()) && ClearGeometry;
                // The persisted route must also recover if a reload drops the
                // transient traffic courtesy state while standing in the bay.
                auto& R=Village->Residents[I];
                if(Dt==.2f && !SimulatedReload && R.bYieldingForTraffic && R.Actor->GetActorLocation().Equals(R.TrafficYieldTarget,.01))
                { R.bYieldingForTraffic=false; R.TrafficYieldWaiters.Reset(); SimulatedReload=true; }
                for(int32 A=0;A<3;++A) for(int32 B=A+1;B<3;++B)
                    MinSeparation=FMath::Min(MinSeparation,FVector::Dist2D(Village->Residents[A].Actor->GetActorLocation(),Village->Residents[B].Actor->GetActorLocation()));
            }
        }
        TestTrue(FString::Printf(TEXT("Real north-street traffic resolves at dt %.2f"),Dt),Complete);
        if(!Complete) for(const auto& R:Village->Residents) AddInfo(FString::Printf(TEXT("Traffic dt=%g pos=%s next=%s yield=%d route=%d"),Dt,*R.Actor->GetActorLocation().ToString(),R.Route.IsEmpty()?TEXT("none"):*R.Route[0].ToString(),R.bYieldingForTraffic,R.Route.Num()));
        TestTrue(TEXT("Narrow-street yielding never overlaps another resident"),MinSeparation>=HearthMovement::Separation-.01);
        TestTrue(TEXT("Every movement update ends outside building exclusions"),ClearGeometry);
        if(Dt==.2f) TestTrue(TEXT("Regression exercised missing transient courtesy state after reload"),SimulatedReload);
        for(int32 I=0;I<3;++I)
        {
            const auto& R=Village->Residents[I];
            TestTrue(TEXT("Original destination is reached"),R.Actor->GetActorLocation().Equals(Goals[I],.01));
            TestEqual(TEXT("Avoidance preserves the original task"),R.Task,I==0?EHearthTask::ProductionTravel:EHearthTask::LifeTravel);
            TestEqual(TEXT("Avoidance preserves the task identity"),R.ActiveTaskId,FString::Printf(TEXT("north-street-task-%d"),I));
        }
        TestEqual(TEXT("Builder still carries the paid component material"),Builder.CargoAmount,1);
        TestEqual(TEXT("Builder keeps component ownership"),Builder.ProductionComponentId,FString(TEXT("roof_slope_timber_2m")));
        TestEqual(TEXT("Builder keeps the borrowed hammer"),Builder.HeldToolId,FString(TEXT("tool_hammer")));
        TestEqual(TEXT("Hammer operation remains attached to the construction task"),Builder.HeldToolOperationId,Builder.ActiveTaskId);
    }
    Village->FixedObstacles.Reset(); Village->ProductionSites.Reset();
    FHearthSite SpawnBlocker; SpawnBlocker.Position=FVector(-2250,-1050,8); SpawnBlocker.Radius=190;
    Village->ProductionSites.Add(SpawnBlocker);
    Village->Residents[0].Actor->SetActorLocation(FVector(-2100,-850,8));
    Village->Residents[1].Actor->SetActorLocation(FVector(0,1000,8));
    Village->Residents[2].Actor->SetActorLocation(FVector(0,-1000,8));
    Village->Residents[0].Route={FVector(-3520,-120,8)};
    bool Escaped=false;
    for(int32 Step=0;Step<1200 && !Escaped;++Step) Escaped=Village->MoveResident(0,.05f);
    TestTrue(TEXT("A resident restored inside a later production exclusion can walk out and finish the original route"),Escaped);
    TestTrue(TEXT("Production-exclusion escape reaches the original destination"),Village->Residents[0].Actor->GetActorLocation().Equals(FVector(-3520,-120,8),.01));
    TArray<FVector> SamePointRoute;
    TestTrue(TEXT("A resident already at a clear activity point has a valid completed path"),Village->FindProductionPath(FVector(0,0,8),FVector(0,0,8),SamePointRoute));
    TestTrue(TEXT("A completed same-point path contains no artificial out-and-back loop"),SamePointRoute.IsEmpty());
    World->DestroyWorld(false);
    return true;
}
#endif
