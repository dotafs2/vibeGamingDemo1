#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "HearthFreightVisual.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthFreightRuntimeTest,"ThreeHearths.Medieval.FreightRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthFreightRuntimeTest::RunTest(const FString&)
{
    const FString Previous=FCommandLine::Get();
    FCommandLine::Set(*(Previous.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthOrganicVillage -HearthNoWorldPersistence -HearthNoAutonomousLife")));
    ON_SCOPE_EXIT { FCommandLine::Set(*Previous); };
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("freight world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* V=World->SpawnActor<AHearthVillage>();
    V->bUseCropoutMap=true;V->BuildEnvironment();V->ResetVillageState();V->bAutonomousLifeEnabled=false;
    if(!TestEqual(TEXT("13 residents"),V->Residents.Num(),13)) return false;
    // The pedestrian blocker deliberately follows authored floor cells, so
    // this fixture places an installed-looking layer in otherwise clear land
    // and verifies that the freight footprint also honors the rendered mesh
    // bounds. This catches wall/trim overhangs that extend beyond a 138 cm
    // floor-cell proxy without changing ordinary resident navigation.
    FVector ClearProbe=FVector::ZeroVector; bool bFoundClearProbe=false;
    for(const FIntPoint& Cell:V->LandGrid)
    {
        const FVector Candidate(Cell.X*300.f,Cell.Y*300.f,0.f);
        if(V->IsFreightPoseSafe(Candidate,0.f,false)) { ClearProbe=Candidate;ClearProbe.Z=V->GroundHeightAt(ClearProbe)+5.2f;bFoundClearProbe=true;break; }
    }
    if(!TestTrue(TEXT("clear freight mesh fixture probe"),bFoundClearProbe)) return false;
    UStaticMeshComponent* FreightMesh=NewObject<UStaticMeshComponent>(V);
    if(!TestNotNull(TEXT("freight mesh fixture"),FreightMesh)) return false;
    FreightMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    FreightMesh->SetMobility(EComponentMobility::Movable);
    FreightMesh->SetWorldLocation(ClearProbe+FVector(305.f,0.f,0.f));
    FreightMesh->SetWorldScale3D(FVector(2.f,2.f,1.f));
    V->AddInstanceComponent(FreightMesh);FreightMesh->RegisterComponent();FreightMesh->UpdateBounds();
    V->StarterArchitectureMeshes[0].Add(FreightMesh);
    const bool bMeshBlocked=!V->IsFreightPoseSafe(ClearProbe,0.f,false);
    V->StarterArchitectureMeshes[0].Remove(FreightMesh);FreightMesh->DestroyComponent();
    if(!TestTrue(TEXT("actual installed mesh blocks freight footprint"),bMeshBlocked)) return false;
    for(auto& R:V->Residents) R.NextLifeDecision=80000;
    if(!TestTrue(TEXT("canonical public project approved"),V->ApprovePublicProject(3))) return false;
    for(auto& R:V->Residents) R.NextLifeDecision=80000;
    // A test fixture of six already produced beams; both stock and cumulative
    // production are declared. Runtime dispatch must deduct this same stock.
    V->BeamStock+=6;V->Manufactured[1]+=6;
    auto Prepare=[&]()
    {
        auto& R=V->Residents[12];R.Task=EHearthTask::LifeChoosing;R.ActiveTaskId.Empty();R.Route.Reset();
        R.Hunger=10;R.Energy=90;R.NextLifeDecision=0;R.Timer=0;R.LifeAction=0;
    };
    Prepare();
    const int32 StockBefore=V->BeamStock,GrantsBefore=V->PublicProject.Grants[2];
    if(!TestTrue(TEXT("real beam shortage dispatches carter"),V->StartFreightOrder())) return false;
    const FString Id=V->FreightOrders.Last().Id;
    const FVector Initial=V->FreightOrders.Last().VehiclePosition;
    TestEqual(TEXT("source deducted"),V->BeamStock,StockBefore-6);
    TestEqual(TEXT("source grant recorded once"),V->PublicProject.Grants[2],GrantsBefore+6);
    TestEqual(TEXT("unloaded cart is visibly empty"),V->GetFreightVisualState().CargoQuantity,0);
    FString Error;FHearthWorldImage Image;
    if(!TestTrue(TEXT("reserved save conserves all material"),HearthWorld::Decode(V->ExportWorldState(),Image,Error))) { AddError(Error);return false; }
    for(int32 I=0;I<3000 && !V->FreightOrders.Last().bLoaded;++I)
    {
        V->AdvanceSimulation(.2f);
        if(!V->FreightOrders.Last().bCarterAttached)
            if(!TestTrue(TEXT("walking to cart never drags or teleports it"),V->FreightOrders.Last().VehiclePosition.Equals(Initial,.01f))) return false;
    }
    if(!TestTrue(TEXT("carter stopped and loaded beams"),V->FreightOrders.Last().bLoaded)) return false;
    TestEqual(TEXT("loaded cart shows exact quantity"),V->GetFreightVisualState().CargoQuantity,6);
    const FString Loaded=V->ExportWorldState();
    if(!TestTrue(TEXT("loaded cold state validates"),HearthWorld::Decode(Loaded,Image,Error))) { AddError(Error);return false; }
    if(!TestTrue(TEXT("loaded cold state applies"),V->ApplyWorldState(Loaded,Error))) { AddError(Error);return false; }
    TestEqual(TEXT("order identity preserved"),V->FreightOrders.Last().Id,Id);
    const FVector PausedPosition=V->FreightOrders.Last().VehiclePosition;
    V->Residents[12].Hunger=66;
    V->AdvanceSimulation(.2f);
    if(!TestTrue(TEXT("loaded needs pause retains cargo"),V->FreightOrders.Last().bPausedForNeeds && V->FreightOrders.Last().bLoaded)) return false;
    const FString Paused=V->ExportWorldState();
    if(!TestTrue(TEXT("food break save validates"),HearthWorld::Decode(Paused,Image,Error))) { AddError(Error);return false; }
    if(!TestTrue(TEXT("food break cold restore"),V->ApplyWorldState(Paused,Error))) { AddError(Error);return false; }
    for(int32 I=0;I<25;++I) V->AdvanceSimulation(.2f);
    TestTrue(TEXT("loaded cart stays parked while driver seeks food"),V->FreightOrders.Last().VehiclePosition.Equals(PausedPosition,.01f));
    // The central hill adds about 430m of real climbing road. At 60cm/s the
    // previous 600-second horizon ends before arrival; include the complete
    // trip and a real meal/rest stop without changing speed or needs.
    for(int32 I=0;I<10000 && V->FreightOrders.Last().Status==TEXT("transporting");++I) V->AdvanceSimulation(.2f);
    if(!TestEqual(TEXT("real transport completes"),V->FreightOrders.Last().Status,FString(TEXT("completed"))))
    {
        AddInfo(FString::Printf(TEXT("phase=%d loaded=%d paused=%d attached=%d route=%d pos=%s target=%s hunger=%.1f energy=%.1f task=%d decision=%s"),
            V->FreightOrders.Last().Phase,V->FreightOrders.Last().bLoaded,V->FreightOrders.Last().bPausedForNeeds,V->FreightOrders.Last().bCarterAttached,V->FreightOrders.Last().VehicleRoute.Num(),
            *V->FreightOrders.Last().VehiclePosition.ToString(),*V->FreightOrders.Last().Destination.ToString(),V->Residents[12].Hunger,V->Residents[12].Energy,(int32)V->Residents[12].Task,*V->Residents[12].LatestEvent));
        return false;
    }
    TestEqual(TEXT("public work receives six actual beams"),V->PublicProject.Stock[2],6);
    TestEqual(TEXT("wage settled once"),V->Transactions.FilterByPredicate([&](const FHearthTransaction& T){return T.Kind==TEXT("wage") && T.TaskId==Id;}).Num(),1);
    TestTrue(TEXT("driver bought real food"),V->Transactions.ContainsByPredicate([](const FHearthTransaction& T){return T.Kind==TEXT("food_purchase") && T.From==12;}));
    TestTrue(TEXT("actual odometer advanced"),V->FreightOrders.Last().WheelDistanceCm>1000);
    if(!TestTrue(TEXT("delivered save validates"),HearthWorld::Decode(V->ExportWorldState(),Image,Error))) { AddError(Error);return false; }
    const FVector Parked=V->FreightOrders.Last().VehiclePosition;const float Distance=V->FreightOrders.Last().WheelDistanceCm;
    V->BeamStock+=6;V->Manufactured[1]+=6;Prepare();
    if(!TestTrue(TEXT("next order dispatched"),V->StartFreightOrder())) return false;
    TestTrue(TEXT("next order keeps parked vehicle"),V->FreightOrders.Last().VehiclePosition.Equals(Parked,.01f));
    TestEqual(TEXT("odometer is cumulative across orders"),V->FreightOrders.Last().WheelDistanceCm,Distance);
    TestTrue(TEXT("unloaded reservation cancels"),V->CancelFreightOrder(12));
    TestEqual(TEXT("cancelled unpicked beams return"),V->BeamStock,StockBefore);
    TestFalse(TEXT("cancel cannot settle twice"),V->CancelFreightOrder(12));
    if(!TestTrue(TEXT("cancelled save validates"),HearthWorld::Decode(V->ExportWorldState(),Image,Error))) { AddError(Error);return false; }
    return true;
}
#endif
