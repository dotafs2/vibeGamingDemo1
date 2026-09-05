#if WITH_DEV_AUTOMATION_TESTS
#include "HearthMovement.h"
#include "HearthVillage.h"
#include "Engine/World.h"
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
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
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
    World->DestroyWorld(false);
    return true;
}
#endif
