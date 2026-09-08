#if WITH_DEV_AUTOMATION_TESTS
#include "HearthFreightNavigation.h"
#include "HearthWorldState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightRecoveryGeometryTest,
    "ThreeHearths.FreightNavigation.RecoveryGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightRecoveryGeometryTest::RunTest(const FString&)
{
    using namespace HearthFreightNavigation;
    // Authored horse nose + the planning clearance reaches x=520. A wall
    // beginning at 505 overlaps only the front 15 cm, not the cart behind it.
    const FPose Start{FVector::ZeroVector,0},Back{FVector(-10,0,0),0};
    TArray<FRecoveryObstacle> Walls={ {FVector(530,0,0),FVector2D(25,60),0,0} };
    TestTrue(TEXT("shallow initial nose overlap permits gradual retreat"),CanReverseStep(Start,Back,Walls));
    TestTrue(TEXT("a second step finishes leaving the initial wall"),CanReverseStep(Back,{FVector(-20,0,0),0},Walls));
    TestFalse(TEXT("forward entry is not recovery"),CanReverseStep(Start,{FVector(10,0,0),0},Walls));
    TestFalse(TEXT("lateral sliding is not recovery"),CanReverseStep(Start,{FVector(-10,2,0),0},Walls));
    TestFalse(TEXT("no in-place half turn"),CanReverseStep(Start,{FVector::ZeroVector,180},Walls));
    TestFalse(TEXT("no long escape jump"),CanReverseStep(Start,{FVector(-100,0,0),0},Walls));
    // The old nose overlap is NOT a permit to enter a separate rear wall.
    Walls.Add({FVector(-165,0,0),FVector2D(.5,100),0,0});
    TestFalse(TEXT("thin rear wall blocks despite decreasing front overlap"),CanReverseStep(Start,Back,Walls));
    Walls.Last().HalfSize.X=10;
    TestFalse(TEXT("initial rear overlap must not deepen while the nose escapes"),CanReverseStep(Start,Back,Walls));
    Walls.Pop();
    // Rotate the entire fixture to the reported vehicle heading. This catches
    // implementations that escape along world axes instead of vehicle -X.
    const FRotator Rotation(0,75.03192f,0);
    const FVector Origin(-159.20884,-577.58588,139.34215);
    Walls[0].Center=Origin+Rotation.RotateVector(Walls[0].Center);Walls[0].Yaw=Rotation.Yaw;
    TestTrue(TEXT("oblique recovery preserves the same geometry"),CanReverseStep(
        {Origin,static_cast<float>(Rotation.Yaw)},{Origin+Rotation.RotateVector(FVector(-10,0,0)),static_cast<float>(Rotation.Yaw)},Walls));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightRecoveryBoundsTest,
    "ThreeHearths.FreightNavigation.RecoveryBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightRecoveryBoundsTest::RunTest(const FString&)
{
    using namespace HearthFreightNavigation;
    TArray<FPose> Out={{FVector(1,2,3),90}};
    int32 ReverseChecks=0,ForwardAttempts=0;
    TestFalse(TEXT("no connected forward exit never commits blind backing"),PlanRecovery({FVector::ZeroVector,0},
        [&](const FPose& A,const FPose& B)
        {
            ++ReverseChecks;
            TestTrue(TEXT("each candidate is a small physical reverse step"),B.Position.X<A.Position.X && FVector::Dist2D(A.Position,B.Position)<=10.001f && B.Yaw==A.Yaw);
            return true;
        },
        [&](const FPose&,TArray<FPose>&){++ForwardAttempts;return false;},Out,100000));
    TestTrue(TEXT("failed recovery clears stale output"),Out.IsEmpty());
    TestTrue(TEXT("distance and search attempts stay bounded"),ReverseChecks<=90 && ForwardAttempts<=6);
    int32 VetoChecks=0;
    TestFalse(TEXT("terrain or pedestrian veto terminates the candidate"),PlanRecovery({FVector::ZeroVector,0},
        [&](const FPose&,const FPose&){return ++VetoChecks<3;},
        [](const FPose&,TArray<FPose>&){return true;},Out));
    TestEqual(TEXT("no search beyond the hard veto"),VetoChecks,3);
    TestTrue(TEXT("veto leaves no partial path"),Out.IsEmpty());
    // Use the actual forward planner for the connection. The finite closed
    // region models a newly blocked start; its exit is behind the heading.
    const auto Clear=[](const FPose& P){return !(P.Position.X>-40 && P.Position.X<250 && FMath::Abs(P.Position.Y)<120);};
    TestFalse(TEXT("ordinary Plan still rejects the blocked start"),Plan({FVector::ZeroVector,0},FVector(-700,800,0),Clear,Out));
    TestTrue(TEXT("recovery connects a reverse prefix to a real forward route"),PlanRecovery({FVector::ZeroVector,0},
        [](const FPose& A,const FPose& B){return B.Position.X<A.Position.X && B.Yaw==A.Yaw;},
        [&](const FPose& P,TArray<FPose>& Forward){return Plan(P,FVector(-700,800,0),Clear,Forward,1000);},Out));
    bool bReverse=false,bForward=false;
    for(int32 I=1;I<Out.Num();++I)
    {
        const FVector Delta=Out[I].Position-Out[I-1].Position;
        const double Dot=FVector::DotProduct(Delta,FRotator(0,Out[I-1].Yaw,0).Vector());
        bReverse|=Dot<0;bForward|=Dot>0;
        TestTrue(TEXT("gear transition contains no stationary turn"),Delta.Size2D()>.001 && Delta.Size2D()<=50.01);
    }
    TestTrue(TEXT("connected plan includes both directions"),bReverse && bForward);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightRecoveryTest,
    "ThreeHearths.Medieval.FreightRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightRecoveryTest::RunTest(const FString&)
{
    // Regression input is the portable real-world checkpoint, NOT fabricated
    // points produced by the recovery implementation. This test advances only
    // its freight worker: it is technical verification, not Kimi acceptance.
    const FString Previous=FCommandLine::Get();
    FCommandLine::Set(*(Previous.Replace(TEXT("-HearthCityV3"),TEXT(""))+
        TEXT(" -HearthOrganicVillage -HearthNoWorldPersistence -HearthNoAutonomousLife")));
    ON_SCOPE_EXIT { FCommandLine::Set(*Previous); };
    FString Payload,Error;
    const FString Checkpoint=FPaths::ProjectContentDir()/TEXT("ThreeHearths/Data/MedievalShowcase/world.json");
    if(!TestTrue(TEXT("read portable freight regression world"),HearthWorld::Read(
        Checkpoint,Payload,Error)))
    { AddError(Error);return false; }
    UWorld* World=nullptr;
    ON_SCOPE_EXIT { if(World) World->DestroyWorld(false); };
    const auto CreateVillage=[&]() -> AHearthVillage*
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if(!World) return nullptr;
        auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
        if(!Base) return nullptr;
        Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
        Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
        auto* Village=World->SpawnActor<AHearthVillage>();
        if(!Village) return nullptr;
        Village->bUseCropoutMap=true;
        // BuildIslandVillage reads the checkpoint's retained plot layout.
        // Allow that read only while constructing geometry; reset/apply/ticks
        // still run with persistence disabled and cannot write the source.
        const FString FixtureFlags=FCommandLine::Get();
        FCommandLine::Set(*(FixtureFlags.Replace(TEXT("-HearthNoWorldPersistence"),TEXT(""))
            +TEXT(" -HearthWorld=\"")+Checkpoint+TEXT("\"")));
        Village->BuildEnvironment();
        FCommandLine::Set(*FixtureFlags);
        Village->ResetVillageState();
        return Village;
    };
    auto* V=CreateVillage();
    if(!TestNotNull(TEXT("freight recovery world"),V) || !TestTrue(TEXT("apply original world"),V->ApplyWorldState(Payload,Error)))
    { AddError(Error);return false; }
    const FString Id=TEXT("ED104C19-402A-2AC0-0135-71A2882623D9");
    const auto Order=[&]() -> FHearthFreightOrder& { return V->FreightOrders.Last(); };
    if(!TestTrue(TEXT("original loaded second order exists"),!V->FreightOrders.IsEmpty() && Order().Id==Id
        && Order().bLoaded && Order().CargoQuantity==6 && Order().Phase==2)) return false;
    const FString WorldId=V->WorldId;
    const FVector Original=Order().VehiclePosition;const float Yaw=Order().VehicleYaw;
    const FVector Heading=FRotator(0,Yaw,0).Vector();
    const int32 Beams=V->BeamStock,Granted=V->PublicProject.Grants[2],Delivered=V->PublicProject.Stock[2];
    const int32 OrderCount=V->FreightOrders.Num(),Payables=V->WagePayables.Num();
    const float Odometer=Order().WheelDistanceCm;
    TestFalse(TEXT("reported installed-house start is blocked"),V->IsFreightPoseSafe(Original,Yaw,false));
    // Isolate pedestrian waiting from static recovery. Other workers do not
    // tick in this fixture; park their actors away so they cannot permanently
    // freeze on the tested route. No cart/source/destination or ledger edits.
    const auto Isolate=[&]()
    {
        V->bAutonomousLifeEnabled=false;V->bApiReady=false;V->bApiDisabledThisRun=true;
        for(int32 I=0;I<V->Residents.Num();++I) if(I!=12)
            V->Residents[I].Actor->SetActorLocation(FVector(6500+I*100,6500,200));
    };
    Isolate();
    const auto Step=[&]() { V->Elapsed+=.2f;V->AdvanceFreightResident(12,.2f); };
    for(int32 I=0;I<200 && Order().VehiclePosition.Equals(Original,.001);++I) Step();
    if(!TestTrue(TEXT("original loaded axle really starts backing"),
        FVector::DotProduct(Order().VehiclePosition-Original,Heading)<-1 && Order().VehicleYaw==Yaw)) return false;
    const FVector Pause=Order().VehiclePosition;const float PausedWheel=Order().WheelDistanceCm;
    const FVector Pedestrian=V->Residents[0].Actor->GetActorLocation();
    V->Residents[0].Actor->SetActorLocation(Pause-Heading*175.f);
    for(int32 I=0;I<20;++I) Step();
    TestTrue(TEXT("pedestrian behind cart holds the loaded order still"),Order().VehiclePosition.Equals(Pause,.001));
    TestEqual(TEXT("waiting never spins the mileage"),Order().WheelDistanceCm,PausedWheel);
    V->Residents[0].Actor->SetActorLocation(Pedestrian);
    const FVector RearWall=Pause-Heading*180.f;
    V->FixedObstacles.Add(FVector(RearWall.X,RearWall.Y,20.f));
    TArray<FVector> BlockedRoute;TArray<float> BlockedYaws;
    TestFalse(TEXT("rear wall prevents recovery route, not just the next tick"),
        V->BuildFreightRecoveryRoute(Pause,Yaw,Order().Destination,BlockedRoute,BlockedYaws));
    TestTrue(TEXT("failed retreat exposes no partial route"),BlockedRoute.IsEmpty() && BlockedYaws.IsEmpty());
    Step();
    TestTrue(TEXT("stored reverse route cannot push through a newly built rear wall"),Order().VehiclePosition.Equals(Pause,.001));
    V->FixedObstacles.Pop();
    // Land is an unconditional veto even when the same tick would escape a
    // house overlap. Remove one fixture grid cell and restore it immediately.
    const FVector RearCorner=Pause-Heading*160.f;
    const FIntPoint LandCell(FMath::RoundToInt(RearCorner.X/300.f),FMath::RoundToInt(RearCorner.Y/300.f));
    const bool bHadLand=V->LandGrid.Contains(LandCell);V->LandGrid.Remove(LandCell);
    Step();TestTrue(TEXT("missing ground stops reverse motion"),Order().VehiclePosition.Equals(Pause,.001));
    if(bHadLand) V->LandGrid.Add(LandCell);
    int32 ReverseSteps=0;
    for(int32 I=0;I<12;++I)
    {
        const FVector Before=Order().VehiclePosition;const float DistanceBefore=Order().WheelDistanceCm;
        Step();const FVector Delta=Order().VehiclePosition-Before;
        if(Delta.Size2D()>.001)
        {
            ++ReverseSteps;
            TestTrue(TEXT("real runtime uses low-speed rearward steps"),Delta.Size2D()<=4.01 && FVector::DotProduct(Delta,Heading)<0);
            TestEqual(TEXT("horse body never turns around for reverse"),Order().VehicleYaw,Yaw);
            TestTrue(TEXT("odometer stays positively cumulative"),Order().WheelDistanceCm>DistanceBefore);
        }
    }
    if(!TestTrue(TEXT("recovery is multiple actual ticks"),ReverseSteps>=8)) return false;
    const FVector SavedPosition=Order().VehiclePosition;const float SavedWheel=Order().WheelDistanceCm;
    const TArray<FVector> SavedRoute=Order().VehicleRoute;const TArray<float> SavedYaws=Order().VehicleRouteYaws;
    if(!TestTrue(TEXT("cold fixture still has reverse path remaining"),!SavedRoute.IsEmpty()
        && FVector::DotProduct(SavedRoute[0]-SavedPosition,Heading)<0)) return false;
    const FString Cold=V->ExportWorldState();FHearthWorldImage Image;
    if(!TestTrue(TEXT("mid-reverse snapshot passes existing conservation/schema checks"),HearthWorld::Decode(Cold,Image,Error)))
    { AddError(Error);return false; }
    World->DestroyWorld(false);World=nullptr;V=CreateVillage();
    if(!TestNotNull(TEXT("fresh cold world"),V) || !TestTrue(TEXT("restore mid-reverse without transient state"),V->ApplyWorldState(Cold,Error)))
    { AddError(Error);return false; }
    Isolate();
    TestEqual(TEXT("world GUID survives cold recovery"),V->WorldId,WorldId);
    TestEqual(TEXT("order GUID survives cold recovery"),Order().Id,Id);
    TestTrue(TEXT("saved axle preserved exactly"),Order().VehiclePosition.Equals(SavedPosition,.00001));
    TestTrue(TEXT("remaining reverse route and headings preserved"),Order().VehicleRoute==SavedRoute && Order().VehicleRouteYaws==SavedYaws);
    TestEqual(TEXT("saved odometer preserved"),Order().WheelDistanceCm,SavedWheel);
    Step();
    TestTrue(TEXT("first cold tick continues backing without a turn"),
        FVector::DotProduct(Order().VehiclePosition-SavedPosition,Heading)<0 && Order().VehicleYaw==Yaw);
    for(int32 I=0;I<6000 && Order().Status==TEXT("transporting");++I) Step();
    if(!TestEqual(TEXT("same loaded order ultimately unloads"),Order().Status,FString(TEXT("completed"))))
    {
        AddError(FString::Printf(TEXT("Recovery stopped: pos=%s yaw=%.3f route=%d phase=%d event=%s"),
            *Order().VehiclePosition.ToString(),Order().VehicleYaw,Order().VehicleRoute.Num(),Order().Phase,*V->Residents[12].LatestEvent));
        return false;
    }
    TestEqual(TEXT("no replacement order was created"),V->FreightOrders.Num(),OrderCount);
    TestEqual(TEXT("source stock never deducted again"),V->BeamStock,Beams);
    TestEqual(TEXT("material grant never repeated"),V->PublicProject.Grants[2],Granted);
    TestEqual(TEXT("one unloading credits exactly the six carried beams"),V->PublicProject.Stock[2],Delivered+6);
    TestEqual(TEXT("no second wage reservation"),V->WagePayables.Num(),Payables);
    TestEqual(TEXT("one wage payment for the original order"),V->Transactions.FilterByPredicate(
        [&](const FHearthTransaction& T){return T.Kind==TEXT("wage") && T.TaskId==Id;}).Num(),1);
    TestTrue(TEXT("all travelled distance remains on the same odometer"),Order().WheelDistanceCm>Odometer);
    for(int32 I=0;I<10;++I) Step();
    TestEqual(TEXT("completed order cannot unload twice"),V->PublicProject.Stock[2],Delivered+6);
    if(!TestTrue(TEXT("delivered cold snapshot conserves materials and wages"),HearthWorld::Decode(V->ExportWorldState(),Image,Error)))
    { AddError(Error);return false; }
    return true;
}
#endif
