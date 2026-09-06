#if WITH_DEV_AUTOMATION_TESTS
#include "HearthResidentBuildingPlanner.h"
#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPlannedConstructionRuntimeTest,
    "ThreeHearths.Production.ResidentPlanRuntimeExecution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthPlannedConstructionRuntimeTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated resident construction world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->BuildEnvironment(); Village->ResetVillageState();
    Village->bAutonomousLifeEnabled=false; Village->bApiDisabledThisRun=true;
    auto& Resident=Village->Residents[0];
    Village->PlotOwners[0]=0; Resident.Plot=0; Resident.DeliveredWood=Village->CostFor(0);
    Resident.BuildProgress=1.f; Resident.Task=EHearthTask::LifeChoosing; Resident.Role=TEXT("carpenter"); Resident.Coins=100;
    Resident.Actor->SetActorLocation(FVector(-400,0,8)); Resident.Route.Reset();

    Village->ProductionSites.Reset(); Village->LandGrid.Reset();
    for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) Village->LandGrid.Add(FIntPoint(X,Y));
    FHearthSite Site; Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Site.Kind=EHearthSiteKind::Land;
    Site.Position=FVector(400,0,8); Site.Approach=FVector(0,0,8); Site.bReachable=true;
    Village->ProductionSites.Add(Site);
    Village->StoneStock=100; Village->PlankStock=100; Village->BeamStock=100;
    Village->Produced[2]=100; Village->Manufactured[0]=100; Village->Manufactured[1]=100;

    const int32 Action=105;
    const int32 StoneBefore=Village->StoneStock;
    FHearthResidentBuildingInput DiagnosticInput; DiagnosticInput.ResidentId=Resident.StableId;
    DiagnosticInput.StableSeed=Resident.StableId+TEXT("|")+Resident.Personality+TEXT("|")+Resident.Role;
    DiagnosticInput.Need=TEXT("shelter"); DiagnosticInput.Occupation=Resident.Role; DiagnosticInput.Budget=Resident.Coins;
    DiagnosticInput.bRoadAccessible=true; DiagnosticInput.RoadYaw=180.f; DiagnosticInput.Origin=Village->ProductionSites[0].Position;
    DiagnosticInput.Stone=Village->StoneStock; DiagnosticInput.Planks=Village->PlankStock; DiagnosticInput.Beams=Village->BeamStock;
    const auto DiagnosticPlan=HearthResidentBuildingPlanner::Build(DiagnosticInput);
    AddInfo(FString::Printf(TEXT("Planner diagnostic: buildable=%s components=%d reason=%s"),
        DiagnosticPlan.bBuildable?TEXT("true"):TEXT("false"),DiagnosticPlan.Plan.Components.Num(),*DiagnosticPlan.Reason));
    TestTrue(TEXT("Runtime Op5 invokes the resident planner and accepts the first component"),
        Village->StartProduction(0,Action,TEXT("resident chooses a catalog house"),false));
    if(!TestEqual(TEXT("Runtime owns one authoritative structure plan"),Village->StructurePlans.Num(),1)) return false;
    const auto& Plan=Village->StructurePlans[0]; const auto& RuntimeSite=Village->ProductionSites[0];
    TestEqual(TEXT("Site references the authoritative resident plan"),RuntimeSite.BuildPlanId,Plan.PlanId);
    TestTrue(TEXT("Resident plan id records resident ownership"),Plan.PlanId.Contains(Resident.StableId));
    TestEqual(TEXT("Adapter creates one executable record per planned component"),RuntimeSite.CottageComponents.Num(),Plan.Components.Num());
    TestEqual(TEXT("First planned component is reserved for its choosing resident"),RuntimeSite.CottageComponents[0].ReservedBy,0);
    TestEqual(TEXT("Only the first real material unit is reserved"),Village->StoneStock,StoneBefore-RuntimeSite.CottageComponents[0].MaterialAmount);
    FHearthWorldImage SavedDuringFirstHaul; FString SaveError;
    TestTrue(TEXT("Schema9 accepts a native non-GUID plan and dynamic component count"),
        HearthWorld::Decode(Village->ExportWorldState(),SavedDuringFirstHaul,SaveError));
    TestEqual(TEXT("In-progress native plan survives the save image"),SavedDuringFirstHaul.StructurePlans.Num(),1);
    TestEqual(TEXT("Reserved component list survives the save image"),SavedDuringFirstHaul.Sites[0].CottageComponents.Num(),Plan.Components.Num());

    for(int32 Step=0;Step<1000 && Resident.CargoAmount==0;++Step) Village->AdvanceSimulation(.05f);
    TestTrue(TEXT("Worker reaches the depot and physically picks up planned material"),Resident.CargoAmount>0);
    const FString ComponentId=Resident.ProductionComponentId;
    for(int32 Step=0;Step<1000 && Resident.Task==EHearthTask::ProductionTravel;++Step) Village->AdvanceSimulation(.05f);
    if(!TestEqual(TEXT("Worker carries the planned material back to its construction site"),Resident.Task,EHearthTask::ProductionWork)) return false;
    Resident.Timer=0; Village->AdvanceProduction(0,.05f);
    const auto* Installed=Village->ProductionSites[0].CottageComponents.FindByPredicate([&](const FHearthCottageComponent& C){ return C.Id==ComponentId; });
    TestTrue(TEXT("The exact transported component is installed"),Installed && Installed->Status==TEXT("completed"));
    TestEqual(TEXT("Piecewise execution advances by exactly one component"),Village->ProductionSites[0].Units,1);
    TestEqual(TEXT("Installed material leaves resident cargo"),Resident.CargoAmount,0);
    return true;
}
#endif
