#if WITH_DEV_AUTOMATION_TESTS
#include "HearthResidentBuildingPlanner.h"
#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

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
    Village->WoodStock[0]-=Resident.DeliveredWood;
    Resident.BuildProgress=1.f; Resident.Task=EHearthTask::LifeChoosing; Resident.Role=TEXT("carpenter");
    Resident.Actor->SetActorLocation(FVector(-400,0,8)); Resident.Route.Reset();

    Village->ProductionSites.Reset(); Village->LandGrid.Reset();
    for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) Village->LandGrid.Add(FIntPoint(X,Y));
    FHearthSite Site; Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Site.Kind=EHearthSiteKind::Land;
    Site.Position=FVector(400,0,8); Site.Approach=FVector(0,0,8); Site.bReachable=true;
    Village->ProductionSites.Add(Site);
    Village->StoneStock=100; Village->PlankStock=100; Village->BeamStock=100;
    Village->Produced[2]=100; Village->Manufactured[0]=100; Village->Manufactured[1]=100;
    TestFalse(TEXT("Full village treasury cannot subsidize an unaffordable private house"),Village->StartProduction(0,105,TEXT("unaffordable private plan"),false));
    TestEqual(TEXT("Rejected private plan preserves owner savings"),Resident.Coins,12);
    TestTrue(TEXT("Rejected private plan creates no wage or structure"),Village->WagePayables.IsEmpty()&&Village->StructurePlans.IsEmpty());
    // Owners save real assessed earnings; construction no longer draws wages from the village.
    for(int32 Worker=0;Worker<2;++Worker) for(int32 Job=0;Job<(Worker==0?50:10);++Job)
    {
        const FString Task=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        if(!Village->ReserveWage(Worker,Task,3) || !Village->SettleWage(Worker,Task)) return false;
    }
    while(Village->GeneralFunds()>=2)
    {
        const FString Task=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); const int32 Amount=Village->GeneralFunds()==2?2:3;
        if(!Village->ReserveWage(2,Task,Amount) || !Village->SettleWage(2,Task)) return false;
    }
    TestEqual(TEXT("Private construction fixture has no general village funds"),Village->GeneralFunds(),0);

    const int32 Action=105;
    const int32 StoneBefore=Village->StoneStock;
    FHearthResidentBuildingInput DiagnosticInput; DiagnosticInput.ResidentId=Resident.StableId;
    DiagnosticInput.StableSeed=Resident.StableId+TEXT("|")+Resident.Personality+TEXT("|")+Resident.Role;
    DiagnosticInput.Need=TEXT("shelter"); DiagnosticInput.Occupation=Resident.Role; DiagnosticInput.Budget=Resident.Coins/Village->WageForOperation(5);
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
    TestEqual(TEXT("First component wage is backed by its owner"),Village->WagePayables.Last().Funder,0);
    TSharedPtr<FJsonObject> ProductionEvidence;
    TestTrue(TEXT("Runtime production evidence is valid JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Village->GetProductionState()),ProductionEvidence));
    const TArray<TSharedPtr<FJsonValue>>* EvidencePlans=nullptr;
    TestTrue(TEXT("Runtime evidence exposes authoritative structure plans"),ProductionEvidence.IsValid()&&ProductionEvidence->TryGetArrayField(TEXT("structure_plans"),EvidencePlans)&&EvidencePlans&&EvidencePlans->Num()==1);
    if(EvidencePlans&&EvidencePlans->Num()==1)
    {
        const auto EvidencePlan=(*EvidencePlans)[0]->AsObject(); const TSharedPtr<FJsonObject>* Reasons=nullptr;
        TestEqual(TEXT("Evidence plan maps to its live site"),static_cast<int32>(EvidencePlan->GetNumberField(TEXT("site_id"))),0);
        TestEqual(TEXT("Evidence contains every component transform"),EvidencePlan->GetArrayField(TEXT("components")).Num(),Plan.Components.Num());
        TestTrue(TEXT("Evidence persists live NPC reasons"),EvidencePlan->TryGetObjectField(TEXT("reasons"),Reasons)&&Reasons&&(*Reasons)->HasField(TEXT("need"))&&(*Reasons)->HasField(TEXT("occupation")));
    }
    FHearthWorldImage SavedDuringFirstHaul; FString SaveError;
    if(!TestTrue(TEXT("Schema9 accepts a native non-GUID plan and dynamic component count"),
        HearthWorld::Decode(Village->ExportWorldState(),SavedDuringFirstHaul,SaveError)))
    { AddError(SaveError); return false; }
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

    auto CompleteCurrentPlan=[&](AHearthVillage* Active)
    {
        for(int32 PartGuard=0;PartGuard<100;++PartGuard)
        {
            auto& ActiveSite=Active->ProductionSites[0];
            if(!ActiveSite.CottageComponents.ContainsByPredicate([](const auto& C){return C.Status!=TEXT("completed");}))
                return ActiveSite.Kind==EHearthSiteKind::House;
            auto& Worker=Active->Residents[0];
            if(Worker.Task==EHearthTask::LifeChoosing && !Active->StartProduction(0,Action,TEXT("continue resident plan"),false))
            { AddError(TEXT("Could not reserve next component: ")+Worker.LatestEvent); return false; }
            for(int32 Step=0;Step<3000 && Worker.Task==EHearthTask::ProductionTravel;++Step) Active->AdvanceSimulation(.05f);
            if(Worker.Task!=EHearthTask::ProductionWork)
            { AddError(FString::Printf(TEXT("Worker did not reach install state: task=%d route=%d cargo=%d event=%s"),static_cast<int32>(Worker.Task),Worker.Route.Num(),Worker.CargoAmount,*Worker.LatestEvent)); return false; }
            Worker.Timer=0; Active->AdvanceProduction(0,.05f);
        }
        return false;
    };
    auto Preserves=[&](const TArray<FHearthCottageComponent>& Before,const TArray<FHearthCottageComponent>& After)
    {
        for(const auto& Old:Before) if(!After.ContainsByPredicate([&](const auto& Current)
            {return Current.Id==Old.Id&&Current.AssetId==Old.AssetId&&Current.Offset==Old.Offset&&Current.Yaw==Old.Yaw&&Current.Stage==Old.Stage&&Current.MaterialType==Old.MaterialType&&Current.MaterialAmount==Old.MaterialAmount&&Current.Owner==Old.Owner&&Current.Status==Old.Status;})) return false;
        return true;
    };

    if(!TestTrue(TEXT("NPC completes every remaining base component through real haul/install work"),CompleteCurrentPlan(Village))) return false;
    const TArray<FHearthCottageComponent> BaseParts=Village->ProductionSites[0].CottageComponents;
    const int32 BaseRooms=Village->StructurePlans[0].Rooms.Num();
    FHearthSite NeighborSite; NeighborSite.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); NeighborSite.Kind=EHearthSiteKind::Land; NeighborSite.Position=FVector(800,600,8); NeighborSite.Approach=FVector(400,600,8); NeighborSite.bReachable=true; Village->ProductionSites.Add(NeighborSite);
    auto& NeighborBuilder=Village->Residents[1]; NeighborBuilder.BuildProgress=1.f; NeighborBuilder.Task=EHearthTask::LifeChoosing; NeighborBuilder.SocialNeed=80.f; NeighborBuilder.Role=TEXT("陶工"); NeighborBuilder.Actor->SetActorLocation(FVector(0,600,8)); NeighborBuilder.Route.Reset();
    TestFalse(TEXT("A neighbor may not decide an extension for somebody else's completed home"),Village->IsProductionAllowed(1,Action));
    const int32 PreferredAction=Village->ChooseProductionLocally(1); const int32 PreferredSite=(PreferredAction-100)/16,PreferredOperation=(PreferredAction-100)%16;
    TestEqual(TEXT("Local policy starts another household before repeatedly extending the first"),PreferredSite,1);
    TestEqual(TEXT("Neighborhood priority still selects resident construction"),PreferredOperation,5);
    Village->Residents[0].SocialNeed=85.f;
    if(!TestTrue(TEXT("Later social need starts the first material-backed expansion"),Village->StartProduction(0,Action,TEXT("household needs another room"),false))) return false;
    TestEqual(TEXT("First runtime expansion appends one room"),Village->StructurePlans[0].Rooms.Num(),BaseRooms+1);
    TestTrue(TEXT("First runtime expansion preserves every installed base field"),Preserves(BaseParts,Village->ProductionSites[0].CottageComponents));
    const FString FirstExpansionSave=Village->ExportWorldState();
    auto* Reloaded=World->SpawnActor<AHearthVillage>(); Reloaded->BuildEnvironment(); Reloaded->ResetVillageState(); Reloaded->bApiDisabledThisRun=true;
    FString ReloadError;
    if(!TestTrue(TEXT("Partially built first expansion reloads"),Reloaded->ApplyWorldState(FirstExpansionSave,ReloadError))) { AddError(ReloadError); return false; }
    Reloaded->bAutonomousLifeEnabled=false;
    Reloaded->LandGrid.Reset(); for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) Reloaded->LandGrid.Add(FIntPoint(X,Y));
    TestEqual(TEXT("Reload keeps first expansion room"),Reloaded->StructurePlans[0].Rooms.Num(),BaseRooms+1);
    TestTrue(TEXT("Reload keeps all base component fields"),Preserves(BaseParts,Reloaded->ProductionSites[0].CottageComponents));
    if(!TestTrue(TEXT("NPC completes the first expansion after reload"),CompleteCurrentPlan(Reloaded))) return false;

    const TArray<FHearthCottageComponent> FirstExpansionParts=Reloaded->ProductionSites[0].CottageComponents;
    Reloaded->Residents[0].SocialNeed=20.f; Reloaded->Residents[0].Hunger=85.f;
    if(!TestTrue(TEXT("A later urgent need starts the second material-backed expansion"),Reloaded->StartProduction(0,Action,TEXT("food access changes the household need"),false))) return false;
    TestEqual(TEXT("Second runtime expansion appends another room"),Reloaded->StructurePlans[0].Rooms.Num(),BaseRooms+2);
    TestTrue(TEXT("Second runtime expansion preserves the entire first expansion"),Preserves(FirstExpansionParts,Reloaded->ProductionSites[0].CottageComponents));
    FHearthWorldImage SecondExpansionImage; FString SecondExpansionError;
    if(!TestTrue(TEXT("Partially built second expansion is schema9-valid"),HearthWorld::Decode(Reloaded->ExportWorldState(),SecondExpansionImage,SecondExpansionError)))
    { AddError(SecondExpansionError); return false; }
    TestEqual(TEXT("Second expansion persistence keeps all three rooms"),SecondExpansionImage.StructurePlans[0].Rooms.Num(),BaseRooms+2);
    return true;
}
#endif
