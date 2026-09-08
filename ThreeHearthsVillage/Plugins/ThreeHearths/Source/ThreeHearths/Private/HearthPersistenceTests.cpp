#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace HearthPersistenceTests
{
    UWorld* World(bool bCreatePhysicsScene=false)
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(bCreatePhysicsScene).CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    }
    FString TestPath()
    { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ThreeHearths/Tests")/FGuid::NewGuid().ToString(EGuidFormats::Digits)/TEXT("world.json")); }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPublicProjectPersistenceTest,"ThreeHearths.Persistence.PublicProjectSchema9RoundTrip",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPublicProjectPersistenceTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    // Island planning uses real collision traces even in a persistence test.
    UWorld* World=HearthPersistenceTests::World(true); if(!TestNotNull(TEXT("Isolated public persistence world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140.f,140.f,1.f));
    auto* V=World->SpawnActor<AHearthVillage>(); V->bUseCropoutMap=true; V->BuildEnvironment(); V->ResetVillageState();
    if(!TestTrue(TEXT("Public fixture has a traced base terrain"),V->IsLand(FVector(-1650.f,-1050.f,8.f)))) return false;
    if(!TestTrue(TEXT("Public fixture has ten safe starter plots"),V->TownLayoutError.IsEmpty() && V->Residents.Num()==10)) return false;
    V->PublicProject.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); V->PublicProject.Status=TEXT("unapproved");
    const FString Text=V->ExportWorldState(); FHearthWorldImage Image; FString Error;
    if(!TestTrue(TEXT("Schema 8 public project decodes"),HearthWorld::Decode(Text,Image,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Schema is current 11"),Image.Schema,11); TestEqual(TEXT("Public project ID survives"),Image.PublicProject.Id,V->PublicProject.Id); TestEqual(TEXT("Public project status survives"),Image.PublicProject.Status,FString(TEXT("unapproved")));

    V->ResetVillageState(); V->bApiDisabledThisRun=true;
    auto Id=[] { return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); };
    // Earn project tax through the normal ledger before reserving the public wage.
    for(int32 I=0;I<4;++I)
    {
        const FString Task=Id();
        if(!TestTrue(TEXT("Earn valid income for project tax"),V->ReserveWage(0,Task,3) && V->SettleWage(0,Task))) return false;
    }
    const int32 King=V->Residents.IndexOfByPredicate([](const FHearthResident& R) { return R.bKing; });
    if(!TestTrue(TEXT("Ten-person runtime has its real king"),King!=INDEX_NONE)) return false;
    V->FixedObstacles.Reset(); V->ProductionSites.Reset(); V->LandGrid.Reset();
    V->BuildLandGrid();
    if(!TestTrue(TEXT("Public fixture rebuilds navigation from physical ground"),V->IsClearPoint(FVector(-1650.f,-1050.f,8.f)))) return false;
    FHearthSite PublicSite; PublicSite.StableId=Id(); PublicSite.Kind=EHearthSiteKind::Empty;
    PublicSite.Position=FVector(900.f,0.f,8.f); PublicSite.Approach=FVector(300.f,0.f,8.f);
    PublicSite.Radius=350.f; PublicSite.bExpansion=true; PublicSite.bReachable=true;
    V->ProductionSites.Add(PublicSite);
    V->PublicProject=FHearthPublicProject(); V->PublicProject.Id=Id(); HearthPublicWorks::Populate(V->PublicProject);
    V->PublicProject.Status=TEXT("building"); V->PublicProject.King=King; V->PublicProject.Site=0;
    V->PublicProject.ApprovalHistoryId=Id();
    FHearthDecisionRecord Approval; Approval.Run=V->CurrentRun; Approval.Timestamp=FDateTime::Now().ToString();
    Approval.Resident=King; Approval.At=V->Elapsed; Approval.Kind=TEXT("public_project_policy"); Approval.Source=TEXT("local");
    Approval.Status=TEXT("completed"); Approval.Context=TEXT("ApprovalHistoryId=")+V->PublicProject.ApprovalHistoryId;
    V->DecisionHistory.Add(Approval);
    // Account deterministic quarry output and move it into the physical public depot.
    V->StoneStock+=4; V->Produced[2]+=4; V->StoneStock-=4; V->PublicProject.Stock[0]=4; V->PublicProject.Grants[0]=4;
    const int32 Worker=0;
    const FString PublicTask=Id();
    if(!TestTrue(TEXT("Reserve real tax-funded public wage"),V->ReserveWage(Worker,PublicTask,2,true))) return false;
    auto& AssignedResident=V->Residents[Worker]; AssignedResident.ActiveTaskId=PublicTask;
    AssignedResident.Plot=0; AssignedResident.DeliveredWood=V->CostFor(0); AssignedResident.BuildProgress=1.f; V->PlotOwners[0]=Worker;
    int32 HomeWood=AssignedResident.DeliveredWood;
    for(int32 I=0;I<3 && HomeWood>0;++I) { const int32 Used=FMath::Min(HomeWood,V->WoodStock[I]); V->WoodStock[I]-=Used; HomeWood-=Used; }
    if(!TestTrue(TEXT("Borrow the real shared construction hammer"),V->TryBorrowTool(Worker,5))) return false;
    AssignedResident.Task=EHearthTask::PublicTravel; AssignedResident.LifeAction=1;
    AssignedResident.ProductionSite=-1; AssignedResident.ProductionOp=-1; AssignedResident.ProductionComponentId.Empty();
    AssignedResident.CargoType=-1; AssignedResident.CargoAmount=0; AssignedResident.Route.Reset();
    auto& AssignedPart=V->PublicProject.Parts[0]; AssignedPart.Worker=Worker; AssignedPart.TaskId=PublicTask;
    AssignedPart.Status=TEXT("transporting"); AssignedPart.Reserved[0]=4; V->PublicProject.Stock[0]=0;
    const FString Assigned=V->ExportWorldState();
    const FVector Depot(-1650.f,-1050.f,8.f);
    auto Migrate=[this,V,Worker,&Error](FHearthWorldImage& Out)
    {
        FHearthWorldImage Current;
        // Each legacy fixture starts as a complete current-schema runtime image.
        if(!TestTrue(TEXT("Current runtime fixture validates"),HearthWorld::Decode(V->ExportWorldState(),Current,Error))) return false;
        if(!TestTrue(TEXT("Public worker exists in persisted people"),Current.People.IsValidIndex(Worker))) return false;
        Current.Schema=8; Current.StructurePlans.Reset();
        Current.People[Worker].Person.LifeAction=-1; // early schema-eight value before explicit phases
        const FString Legacy=HearthWorld::Encode(Current);
        if(!TestTrue(TEXT("Schema8 fixture migrates"),HearthWorld::Decode(Legacy,Out,Error))) return false;
        return TestTrue(TEXT("Migrated fixture applies to a live village"),V->ApplyWorldState(Legacy,Error));
    };

    // An early save with cargo and a populated route must migrate to the site-bound phase.
    auto& Loaded=V->Residents[Worker]; Loaded.Actor->SetActorLocation(Depot);
    Loaded.CargoType=2; Loaded.CargoAmount=4; Loaded.LifeAction=2; Loaded.Route={V->ProductionSites[0].Approach};
    V->PublicProject.Parts[0].Reserved[0]=0;
    if(!TestTrue(TEXT("Runtime pickup creates loaded route"),Loaded.CargoAmount>0 && !Loaded.Route.IsEmpty())) return false;
    FHearthWorldImage Migrated;
    if(!Migrate(Migrated)) { AddError(Error); return false; }
    TestEqual(TEXT("Loaded migration infers site-bound phase"),Migrated.People[Worker].Person.LifeAction,2);
    TestTrue(TEXT("Loaded migration keeps physical cargo"),V->Residents[Worker].CargoAmount>0);
    TestTrue(TEXT("Loaded migration keeps site route"),!V->Residents[Worker].Route.IsEmpty());

    // An empty worker already at the depot must pick up instead of being cancelled.
    if(!TestTrue(TEXT("Restore assigned runtime checkpoint"),V->ApplyWorldState(Assigned,Error))) { AddError(Error); return false; }
    auto& AtDepot=V->Residents[Worker]; AtDepot.Actor->SetActorLocation(Depot); AtDepot.Route.Reset();
    if(!Migrate(Migrated)) { AddError(Error); return false; }
    TestEqual(TEXT("Empty depot migration infers depot-bound phase"),Migrated.People[Worker].Person.LifeAction,1);
    TestEqual(TEXT("Depot resume remains public travel"),V->Residents[Worker].Task,EHearthTask::PublicTravel);
    TestEqual(TEXT("Depot resume keeps empty cargo"),V->Residents[Worker].CargoAmount,0);
    TestTrue(TEXT("Depot resume keeps the exact empty route"),V->Residents[Worker].Route.IsEmpty());

    // An empty worker away from the depot and without waypoints must rebuild a route.
    if(!TestTrue(TEXT("Restore assigned runtime checkpoint again"),V->ApplyWorldState(Assigned,Error))) { AddError(Error); return false; }
    auto& Away=V->Residents[Worker];
    const FVector FixtureAway=Depot+FVector(0.f,900.f,0.f);
    if(!TestTrue(TEXT("Away fixture has physical ground and a clear depot path"),V->IsLand(FixtureAway) && V->IsClearPoint(FixtureAway) && V->IsClearSegment(FixtureAway,Depot))) return false;
    if(!TestTrue(TEXT("Current worker actor reaches the explicit away position"),Away.Actor->SetActorLocation(FixtureAway)
        && Away.Actor->GetActorLocation().Equals(FixtureAway,.001f))) return false;
    const FVector AwayPosition=Away.Actor->GetActorLocation(); Away.Route.Reset();
    if(!TestTrue(TEXT("Fixture is physically away from depot"),FVector::Dist2D(AwayPosition,Depot)>280.f)) return false;
    if(!Migrate(Migrated)) { AddError(Error); return false; }
    TestEqual(TEXT("Empty-route migration infers depot-bound phase"),Migrated.People[Worker].Person.LifeAction,1);
    TestEqual(TEXT("Route recovery preserves public travel"),V->Residents[Worker].Task,EHearthTask::PublicTravel);
    TestTrue(TEXT("Empty-route migration preserves missing waypoints for runtime recovery"),V->Residents[Worker].Route.IsEmpty());
    TestEqual(TEXT("Empty-route migration cannot mint cargo"),V->Residents[Worker].CargoAmount,0);
    TestTrue(TEXT("Empty-route migration preserves off-depot position"),V->Residents[Worker].Actor->GetActorLocation().Equals(AwayPosition,.001f));
    // Two independent haulers must survive saving together, with separate tax
    // escrow and material reservations, while the single hammer stays exclusive.
    for(int32 I=0;I<2;++I)
    {
        const FString ExtraIncome=Id();
        if(!TestTrue(TEXT("Additional income supplies the second crew's tax escrow"),V->ReserveWage(1,ExtraIncome,3) && V->SettleWage(1,ExtraIncome))) return false;
    }
    const FString SecondTask=Id();
    if(!TestTrue(TEXT("Second crew reserves another real tax wage"),V->ReserveWage(1,SecondTask,2,true))) return false;
    auto& Second=V->Residents[1];Second.ActiveTaskId=SecondTask;
    Second.Plot=1;Second.DeliveredWood=V->CostFor(1);Second.BuildProgress=1.f;V->PlotOwners[1]=1;
    int32 SecondWood=Second.DeliveredWood;
    for(int32 I=0;I<3 && SecondWood>0;++I){const int32 Used=FMath::Min(SecondWood,V->WoodStock[I]);V->WoodStock[I]-=Used;SecondWood-=Used;}
    Second.Task=EHearthTask::PublicTravel;Second.LifeAction=1;Second.ProductionSite=-1;Second.ProductionOp=-1;
    Second.CargoType=-1;Second.CargoAmount=0;Second.Route.Reset();
    V->Produced[2]+=4;V->PublicProject.Grants[0]+=4;
    auto& SecondPart=V->PublicProject.Parts[1];SecondPart.Worker=1;SecondPart.TaskId=SecondTask;SecondPart.Status=TEXT("transporting");SecondPart.Reserved[0]=4;
    FHearthWorldImage Parallel;
    TestTrue(TEXT("Parallel crews with conserved materials and wages validate for persistence"),HearthWorld::Decode(V->ExportWorldState(),Parallel,Error));
    if(!Error.IsEmpty()) AddInfo(Error);
    if(Parallel.PublicProject.Parts.Num()>1) TestEqual(TEXT("Second crew identity survives encoding"),Parallel.PublicProject.Parts[1].Worker,1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthWorldPersistenceTest,"ThreeHearths.Persistence.ResumeMaterialsAndTasks",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthWorldPersistenceTest::RunTest(const FString&)
{
    UWorld* World=HearthPersistenceTests::World(); if(!TestNotNull(TEXT("Isolated world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    V->bAutonomousLifeEnabled=false;
    // Keep this deterministic when the developer's Saved API config is enabled.
    // AdvanceSimulation does not pump asynchronous HTTP replies.
    V->bApiDisabledThisRun=true;
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); V->WorldPath=HearthPersistenceTests::TestPath();
    Files.CreateDirectoryTree(*FPaths::GetPath(V->WorldPath)); V->WorldLease=MakeShareable(Files.OpenWrite(*(V->WorldPath+TEXT(".lock")))); V->bWorldPersistenceEnabled=true;
    ON_SCOPE_EXIT { V->WorldLease.Reset(); };
    const FString Id=V->WorldId, ResidentId=V->Residents[0].StableId, HouseBlueprint=V->Residents[0].HouseBlueprint;
    TSet<EHearthTask> Resumed;
    const TSet<EHearthTask> Wanted={EHearthTask::ToWood,EHearthTask::Chopping,EHearthTask::ToHome,EHearthTask::Delivering,EHearthTask::Building};
    for(int32 Step=0;Step<16000 && V->CompletedHomes()<3;++Step)
    {
        V->AdvanceSimulation(.05f);
        const auto Task=V->Residents[0].Task;
        if(Wanted.Contains(Task) && !Resumed.Contains(Task))
        {
            const auto Before=V->Residents[0]; const FVector Position=Before.Actor->GetActorLocation(); const int32 Wood=V->AvailableWood();
            if(!TestTrue(TEXT("Save an in-progress home phase"),V->SaveWorld())) { AddError(V->WorldSaveStatus); return false; }
            V->AdvanceSimulation(1.f); V->FoodStock+=99;
            if(!TestTrue(TEXT("Restore complete checkpoint"),V->LoadWorld())) { AddError(V->WorldSaveStatus); return false; }
            TestTrue(TEXT("A saved off switch does not leave a normal restarted village idle"),V->bAutonomousLifeEnabled);
            V->bAutonomousLifeEnabled=false;
            V->bApiDisabledThisRun=true;
            const auto& After=V->Residents[0];
            TestEqual(TEXT("Same resident across reload"),After.StableId,ResidentId); TestEqual(TEXT("Same task identity"),After.ActiveTaskId,Before.ActiveTaskId);
            TestEqual(TEXT("Resident house style survives reload"),After.HouseBlueprint,HouseBlueprint);
            TestEqual(TEXT("Exact task phase"),After.Task,Before.Task); TestEqual(TEXT("Same carried materials"),After.CarriedWood,Before.CarriedWood);
            TestEqual(TEXT("Same delivered materials"),After.DeliveredWood,Before.DeliveredWood); TestEqual(TEXT("Inventory and cargo restored together"),V->AvailableWood(),Wood);
            TestEqual(TEXT("Food restored without minted materials"),V->FoodStock,30); TestEqual(TEXT("Timer resumes"),After.Timer,Before.Timer);
            TestEqual(TEXT("Build progress resumes"),After.BuildProgress,Before.BuildProgress); TestTrue(TEXT("Position resumes"),After.Actor->GetActorLocation().Equals(Position,.001));
            TestEqual(TEXT("Route retains waypoints"),After.Route.Num(),Before.Route.Num()); Resumed.Add(Task);
        }
    }
    TestEqual(TEXT("All five in-progress home phases exercised"),Resumed.Num(),Wanted.Num()); TestEqual(TEXT("All homes finish after repeated reloads"),V->CompletedHomes(),3);
    TestEqual(TEXT("World identity survives"),V->WorldId,Id); TestEqual(TEXT("Only original 27 wood consumed by homes"),V->AvailableWood(),9);
    // A harvest has removed six food from the source but has not reached inventory.
    auto& R=V->Residents[0]; R.Task=EHearthTask::ProductionDeposit; R.Timer=.25f; R.ProductionSite=0; R.ProductionOp=9;
    R.CargoType=0; R.CargoAmount=6; R.WorkDuration=12; R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FHearthSite Site; Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Site.Kind=EHearthSiteKind::Corn;
    Site.Units=6; Site.Capacity=12; Site.Stage=2; Site.ReservedBy=0; V->ProductionSites={Site}; V->Produced[0]=6;
    TestTrue(TEXT("Synthetic harvest reserves its wage"),V->ReserveWage(0,R.ActiveTaskId,V->WageForOperation(R.ProductionOp)));
    R.NextLifeDecision=V->Elapsed+60;
    if(!TestTrue(TEXT("Checkpoint cargo before deposit"),V->SaveWorld())) { AddError(V->WorldSaveStatus); return false; }
    const FString CargoTask=R.ActiveTaskId; V->AdvanceSimulation(.3f); TestEqual(TEXT("First delivery credited"),V->FoodStock,36);
    if(!TestTrue(TEXT("Restart from in-transit checkpoint"),V->LoadWorld())) { AddError(V->WorldSaveStatus); return false; }
    TestEqual(TEXT("Checkpoint inventory has not received cargo"),V->FoodStock,30); TestEqual(TEXT("In-transit quantity restored"),V->Residents[0].CargoAmount,6);
    TestTrue(TEXT("Remaining decision delay uses simulation time"),V->Residents[0].NextLifeDecision>V->Elapsed+58);
    TestEqual(TEXT("Harvest task retained"),V->Residents[0].ActiveTaskId,CargoTask);
    V->AdvanceSimulation(.3f); V->AdvanceProduction(0,1.f);
    TestEqual(TEXT("Deposit is applied exactly once after resume"),V->FoodStock,36); TestEqual(TEXT("Cargo cleared after credit"),V->Residents[0].CargoAmount,0);
    if(!TestTrue(TEXT("Production site retained after resume"),V->ProductionSites.Num()>=1)) return false;
    TestTrue(TEXT("Old checkpoint gains one persistent carpenter workbench"),V->ProductionSites.ContainsByPredicate([](const FHearthSite& S) { return S.Kind==EHearthSiteKind::Carpenter; }));
    TestEqual(TEXT("Site reservation released"),V->ProductionSites[0].ReservedBy,-1); TestEqual(TEXT("Operation completion counted once"),V->ProductionTotals.FindRef(TEXT("harvest")),1);
    TestTrue(TEXT("Save post-delivery checkpoint"),V->SaveWorld()); TestTrue(TEXT("Reload post-delivery checkpoint"),V->LoadWorld()); V->AdvanceProduction(0,1.f);
    TestEqual(TEXT("Completed delivery cannot be replayed"),V->FoodStock,36);
    // Material costs were paid when accepting the planting job; resume does not
    // enter StartProduction a second time and therefore must not debit again.
    R.Task=EHearthTask::ProductionTravel; R.ProductionSite=0; R.ProductionOp=7; R.WorkDuration=25;
    R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); R.Route={R.Actor->GetActorLocation()};
    TestTrue(TEXT("Synthetic planting reserves its wage"),V->ReserveWage(0,R.ActiveTaskId,V->WageForOperation(R.ProductionOp)));
    TestTrue(TEXT("Borrow one shared trowel for the operation"),V->TryBorrowTool(0,7));
    const FString BorrowedToolOperation=R.HeldToolOperationId;
    V->ProductionSites[0].Kind=EHearthSiteKind::Land; V->ProductionSites[0].Units=0; V->ProductionSites[0].Stage=0; V->ProductionSites[0].ReservedBy=0;
    V->FoodStock-=5; V->Spent[0]+=5; V->Spent[1]+=2;
    for(int32 I=0;I<3;++I) if(V->WoodStock[I]>=2) { V->WoodStock[I]-=2; break; }
    TestTrue(TEXT("Checkpoint prepaid travel"),V->SaveWorld()); V->AdvanceSimulation(.05f);
    TestTrue(TEXT("Restore prepaid travel"),V->LoadWorld()); V->AdvanceSimulation(.05f);
    TestEqual(TEXT("Borrowed tool survives restart"),V->Residents[0].HeldToolId,FString(TEXT("tool_trowel")));
    TestEqual(TEXT("Tool remains tied to the same operation"),V->Residents[0].HeldToolOperationId,BorrowedToolOperation);
    TestEqual(TEXT("Travel resumes into work"),V->Residents[0].Task,EHearthTask::ProductionWork);
    V->AdvanceSimulation(12.f); TestTrue(TEXT("Checkpoint partial production work"),V->SaveWorld());
    V->AdvanceSimulation(14.f); TestTrue(TEXT("Restore partial work"),V->LoadWorld()); V->AdvanceSimulation(14.f); V->AdvanceProduction(0,1.f);
    TestEqual(TEXT("Production material food debit is not repeated"),V->FoodStock,31);
    TestEqual(TEXT("Production material wood debit is not repeated"),V->AvailableWood(),7);
    TestEqual(TEXT("Only one planting completion after restore"),V->ProductionTotals.FindRef(TEXT("plant_shrub")),1);
    TestTrue(TEXT("Completed work returns the tool to shared storage"),V->Residents[0].HeldToolId.IsEmpty());
    TestEqual(TEXT("Site conversion resumes"),V->ProductionSites[0].Kind,EHearthSiteKind::Shrub);
    TestTrue(TEXT("Growing site checkpoint"),V->SaveWorld()); V->AdvanceSimulation(20.f); TestTrue(TEXT("Restore growth countdown"),V->LoadWorld());
    TestEqual(TEXT("World growth resumes without another harvest"),V->ProductionSites[0].Growth,120.f);
    // A persistent pending request is an observation, never an instruction to pay again.
    R=V->Residents[0]; R.Task=EHearthTask::LifeChoosing;
    V->PendingDecisions[0].bActive=true; V->PendingDecisions[0].OperationId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("Save unresolved request"),V->SaveWorld()); TestTrue(TEXT("Restore unresolved request"),V->LoadWorld());
    TestEqual(TEXT("No automatic request replay"),V->PendingDecisionCount(),0); TestTrue(TEXT("Paid decisions disabled after uncertainty"),V->bApiDisabledThisRun);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthDerivedMaterialsTest,"ThreeHearths.Production.LogsToConstructionTimber",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthDerivedMaterialsTest::RunTest(const FString&)
{
    UWorld* World=HearthPersistenceTests::World(); if(!TestNotNull(TEXT("Isolated material-chain world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true;
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); V->WorldPath=HearthPersistenceTests::TestPath();
    Files.CreateDirectoryTree(*FPaths::GetPath(V->WorldPath)); V->WorldLease=MakeShareable(Files.OpenWrite(*(V->WorldPath+TEXT(".lock")))); V->bWorldPersistenceEnabled=true;
    ON_SCOPE_EXIT { V->WorldLease.Reset(); };
    auto& R=V->Residents[0]; R.Plot=0; R.BuildProgress=1.f; R.DeliveredWood=V->CostFor(0); V->PlotOwners[0]=0;
    int32 Installed=R.DeliveredWood;
    for(int32 I=0;I<3 && Installed>0;++I) { const int32 Used=FMath::Min(Installed,V->WoodStock[I]); V->WoodStock[I]-=Used; Installed-=Used; }
    const FVector Depot(-250,-400,8); R.Actor->SetActorLocation(Depot);
    V->BuildLandGrid();
    FHearthSite Bench; Bench.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Bench.Kind=EHearthSiteKind::Carpenter;
    Bench.Position=FVector(420,620,8); Bench.Approach=Depot; Bench.Radius=190; Bench.bReachable=true; V->ProductionSites={Bench};
    const int32 InitialLogs=V->AvailableWood();
    TestTrue(TEXT("Carpenter accepts a plank job"),V->StartProduction(0,113,TEXT("test plank chain"),false));
    TestEqual(TEXT("Two source logs are reserved once"),V->AvailableWood(),InitialLogs-2);
    TestEqual(TEXT("Plank job borrows the shared saw"),R.HeldToolId,FString(TEXT("tool_saw")));
    R.Task=EHearthTask::ProductionWork; R.Timer=0; R.Actor->SetActorLocation(Depot);
    V->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Three planks become public in-transit cargo"),R.CargoAmount,3); TestEqual(TEXT("Plank cargo type"),R.CargoType,3);
    TestEqual(TEXT("Worker owns one plank from the four-plank output"),R.PersonalPlanks,1);
    TestTrue(TEXT("Saw returns before delivery"),R.HeldToolId.IsEmpty()); TestEqual(TEXT("No credit before depot deposit"),V->PlankStock,0);
    TestTrue(TEXT("Save in-transit planks"),V->SaveWorld()); TestTrue(TEXT("Restore in-transit planks"),V->LoadWorld());
    TestEqual(TEXT("In-transit planks survive restart"),V->Residents[0].CargoAmount,3); TestEqual(TEXT("Still no premature plank credit"),V->PlankStock,0);
    V->Residents[0].Task=EHearthTask::ProductionDeposit; V->Residents[0].Timer=0; V->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Public planks enter persistent stock exactly once"),V->PlankStock,3); TestEqual(TEXT("Plank completion counted"),V->ProductionTotals.FindRef(TEXT("mill_planks")),1);
    TestTrue(TEXT("Carpenter accepts a beam job"),V->StartProduction(0,114,TEXT("test beam chain"),false));
    TestEqual(TEXT("Three more source logs are reserved"),V->AvailableWood(),InitialLogs-5);
    TestEqual(TEXT("Beam job borrows the shared mallet"),V->Residents[0].HeldToolId,FString(TEXT("tool_mallet")));
    V->Residents[0].Task=EHearthTask::ProductionWork; V->Residents[0].Timer=0; V->Residents[0].Actor->SetActorLocation(Depot); V->AdvanceProduction(0,.1f);
    V->Residents[0].Task=EHearthTask::ProductionDeposit; V->Residents[0].Timer=0; V->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Two beams enter stock"),V->BeamStock,2); TestEqual(TEXT("Beam completion counted"),V->ProductionTotals.FindRef(TEXT("frame_beams")),1);
    FString Error; FHearthWorldImage Image;
    TestTrue(TEXT("Derived material ledger validates"),HearthWorld::Decode(V->ExportWorldState(),Image,Error));
    TestEqual(TEXT("Persisted public plank stock"),Image.Planks,3); TestEqual(TEXT("Persisted private plank stock"),Image.People[0].Person.PersonalPlanks,1); TestEqual(TEXT("Persisted beam stock"),Image.Beams,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthWorldRecoveryTest,"ThreeHearths.Persistence.CorruptionAndExclusiveWorld",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthWorldRecoveryTest::RunTest(const FString&)
{
    UWorld* World=HearthPersistenceTests::World(); if(!TestNotNull(TEXT("Isolated recovery world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); V->WorldPath=HearthPersistenceTests::TestPath();
    Files.CreateDirectoryTree(*FPaths::GetPath(V->WorldPath)); V->WorldLease=MakeShareable(Files.OpenWrite(*(V->WorldPath+TEXT(".lock")))); V->bWorldPersistenceEnabled=true;
    ON_SCOPE_EXIT { V->WorldLease.Reset(); };
    TUniquePtr<IFileHandle> SecondWriter(Files.OpenWrite(*(V->WorldPath+TEXT(".lock"))));
    TestFalse(TEXT("Second process cannot open same world for writing"),SecondWriter.IsValid());
    FString Error; FHearthWorldImage Good;
    TestTrue(TEXT("Decode initial world"),HearthWorld::Decode(V->ExportWorldState(),Good,Error));
    const auto Reject=[this,&Good,&Error,V](const TCHAR* Label,TFunction<void(FHearthWorldImage&)> Change)
    {
        auto Broken=Good; Change(Broken); const FString Before=V->WorldId;
        TestFalse(Label,V->ApplyWorldState(HearthWorld::Encode(Broken),Error)); TestEqual(TEXT("Rejected load leaves running world intact"),V->WorldId,Before);
    };
    Reject(TEXT("Reject duplicated person ID"),[](auto& W) { W.People[1].Person.StableId=W.People[0].Person.StableId; });
    Reject(TEXT("Reject minted inventory"),[](auto& W) { W.Food++; });
    Reject(TEXT("Reject orphan plot owner"),[](auto& W) { W.Owners[1]=2; });
    Reject(TEXT("Reject invalid source reference"),[](auto& W) { W.People[0].Person.Source=99; });
    Reject(TEXT("Reject invalid cargo phase"),[](auto& W) { W.People[0].Person.CargoType=1; W.People[0].Person.CargoAmount=3; W.Wood[0]-=3; });
    Reject(TEXT("Reject forged task enum"),[](auto& W) { W.People[0].Person.Task=static_cast<EHearthTask>(255); });
    Reject(TEXT("Reject invalid house material combination"),[](auto& W) { W.People[0].Person.WallMaterial=TEXT("marble"); });
    Reject(TEXT("Reject duplicated physical tool holder"),[](auto& W) {
        for(int32 I=0;I<2;++I) { auto& R=W.People[I].Person; R.Task=EHearthTask::ProductionWork; R.ProductionSite=0; R.ProductionOp=9; R.HeldToolId=TEXT("tool_hoe"); R.HeldToolOperationId=R.ActiveTaskId; }
    });
    Reject(TEXT("Reject changed map layout"),[](auto& W) { W.Plots[0].X+=100; });
    Reject(TEXT("Reject invalid history reference"),[](auto& W) { W.People[0].Person.HistoryIndex=1234; });
    TestTrue(TEXT("Save first checkpoint"),V->SaveWorld()); V->Elapsed=42; TestTrue(TEXT("Save next checkpoint with backup"),V->SaveWorld());
    const FString Damaged=TEXT("{truncated_world"); FFileHelper::SaveStringToFile(Damaged,*V->WorldPath);
    TestFalse(TEXT("Autosave refuses to overwrite damaged file"),V->SaveWorld());
    FString Preserved; FFileHelper::LoadFileToString(Preserved,*V->WorldPath); TestEqual(TEXT("Damaged original preserved"),Preserved,Damaged);
    TestTrue(TEXT("Restore prior complete backup"),V->LoadWorld()); TestEqual(TEXT("Previous elapsed checkpoint restored"),V->Elapsed,0.f);
    TestTrue(TEXT("Current file valid after recovery"),HearthWorld::Read(V->WorldPath,Preserved,Error));
    TArray<FString> Archives; Files.IterateDirectory(*FPaths::GetPath(V->WorldPath),[&Archives](const TCHAR* Name,bool Directory) { if(!Directory && FString(Name).Contains(TEXT(".archive-"))) Archives.Add(Name); return true; });
    TestTrue(TEXT("Corrupt file retained as named archive"),Archives.Num()>=1);
    FFileHelper::SaveStringToFile(TEXT("broken_current"),*V->WorldPath); FFileHelper::SaveStringToFile(TEXT("broken_backup"),*(V->WorldPath+TEXT(".bak")));
    const FString Before=V->WorldId; TestFalse(TEXT("Both damaged files fail closed"),V->LoadWorld()); TestEqual(TEXT("Failed recovery retains current in-memory world"),V->WorldId,Before);
    const FString CurrentSchema=FString::Printf(TEXT("\"schema\":%d"),Good.Schema);
    TestFalse(TEXT("Unknown schema cannot silently migrate"),HearthWorld::Decode(V->ExportWorldState().Replace(*CurrentSchema,TEXT("\"schema\":999")),Good,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthStructurePlanPersistenceTest,"ThreeHearths.Persistence.StructurePlanSchema9",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthStructurePlanPersistenceTest::RunTest(const FString&)
{
    UWorld* World=HearthPersistenceTests::World(); if(!TestNotNull(TEXT("Structure plan world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->BuildEnvironment(); Village->ResetVillageState(); Village->bApiDisabledThisRun=true;
    FHearthWorldImage Image; FString BaseError; if(!TestTrue(TEXT("Create valid schema9 base"),HearthWorld::Decode(Village->ExportWorldState(),Image,BaseError))) { AddError(BaseError); return false; }
    FHearthStructureFootprint Footprint; Footprint.Size=FVector2D(500,500); Footprint.Origin=FVector(10,20,30); Footprint.Orientation=FRotator(7,13,29);
    FHearthStructureReasonFields Reasons; Reasons.Need=TEXT("shelter"); Reasons.Occupation=TEXT("carpenter"); Reasons.Budget=TEXT("grant"); Reasons.Relationship=TEXT("family"); Reasons.RoadAccess=TEXT("east");
    auto Plan=HearthStructurePlan::MakePlan(TEXT("plan-9"),TEXT("seed-9"),Footprint,Reasons);
    FHearthStructureMaterialRecipe Recipe; Recipe.RecipeId=TEXT("wall-wood"); Recipe.CatalogId=TEXT("wall"); FHearthStructureMaterialQuantity Material; Material.MaterialId=TEXT("plank"); Material.Quantity=2; Recipe.Inputs.Add(Material);
    TestTrue(TEXT("Register serializable recipe"),HearthStructurePlan::RegisterRecipe(Plan,Recipe));
    FHearthStructureComponentSpec Spec; Spec.CatalogId=TEXT("wall"); Spec.SemanticKey=TEXT("wall-a"); Spec.Offset=FVector(0,0,100); Spec.Height=80; Spec.BoundsMin=FVector(-91,-8,16); Spec.BoundsMax=FVector(91,8,224); Spec.RecipeId=Recipe.RecipeId; Spec.Materials=Recipe.Inputs; Spec.MaterialCost=4;
    TestTrue(TEXT("Append serializable upper component"),HearthStructurePlan::AppendComponent(Plan,Spec)); Image.Schema=9; Image.StructurePlans.Add(Plan);
    FString Error; FHearthWorldImage Decoded;
    TestTrue(TEXT("Schema9 structure plan roundtrips"),HearthWorld::Decode(HearthWorld::Encode(Image),Decoded,Error));
    TestEqual(TEXT("Plan ID survives roundtrip"),Decoded.StructurePlans[0].PlanId,FString(TEXT("plan-9")));
    TestEqual(TEXT("Stable seed survives roundtrip"),Decoded.StructurePlans[0].StableSeed,FString(TEXT("seed-9")));
    TestEqual(TEXT("Upper component Z survives roundtrip"),Decoded.StructurePlans[0].Components[0].Offset.Z,100.0);
    TestTrue(TEXT("Authoritative component bounds survive roundtrip"),Decoded.StructurePlans[0].Components[0].BoundsMin.Equals(Spec.BoundsMin) && Decoded.StructurePlans[0].Components[0].BoundsMax.Equals(Spec.BoundsMax));
    TestEqual(TEXT("Recipe material quantity survives roundtrip"),Decoded.StructurePlans[0].Components[0].Materials[0].Quantity,2);
    FHearthWorldImage Legacy=Image; Legacy.Schema=6; Legacy.StructurePlans.Reset(); const FVector LegacyPosition=Legacy.People[0].Position;
    TestTrue(TEXT("Schema6 migration decodes without structure plans"),HearthWorld::Decode(HearthWorld::Encode(Legacy),Decoded,Error));
    TestEqual(TEXT("Schema6 has no implicit structure plans"),Decoded.StructurePlans.Num(),0);
    TestTrue(TEXT("Schema6 resident position is unchanged"),Decoded.People[0].Position.Equals(LegacyPosition,.001f));
    Image.StructurePlans[0].Components[0].Materials[0].Quantity=0;
    TestFalse(TEXT("Invalid structure material quantity is rejected"),HearthWorld::Decode(HearthWorld::Encode(Image),Decoded,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTown3PersistenceTest,"ThreeHearths.Persistence.Town3ThirtyResidentsRoundTrip",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTown3PersistenceTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Append(TEXT(" -HearthNoWorldPersistence"));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    UWorld* World=HearthPersistenceTests::World(true); if(!TestNotNull(TEXT("Isolated Town3 world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    const auto AddSmallBase=[](UWorld* Scene)
    {
        auto* Base=Scene->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
        if(!Base) return Base;
        Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
        auto* Mesh=Base->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        Mesh->SetWorldScale3D(FVector(100.f,100.f,1.f)); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        return Base;
    };
    auto* SmallBase=AddSmallBase(World); if(!TestNotNull(TEXT("Pre-existing small island terrain"),SmallBase)) return false;
    const FTransform SmallTransform=SmallBase->GetActorTransform();
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->bUseCropoutMap=true; Village->bOrganicTownLayout=true; Village->TownLayoutVersion=3;
    TestTrue(TEXT("Small island is physical ground"),Village->IsLand(FVector(0,0,8)));
    TestFalse(TEXT("Small island does not cover the castle"),Village->IsLand(FVector(6500,6500,8)));
    Village->BuildLandGrid(); const int32 SmallGridCount=Village->LandGrid.Num();
    Village->BuildEnvironment(); Village->ResetVillageState(); Village->bApiDisabledThisRun=true;
    TestTrue(TEXT("Town3 preserves the existing island actor and transform"),IsValid(SmallBase) && SmallBase->GetActorTransform().Equals(SmallTransform));
    TestTrue(TEXT("Town3 replaces the cached small-island navigation with traced large ground"),Village->LandGrid.Num()>SmallGridCount);
    auto* LargeBase=Village->GeneratedTown3Terrain.Get();
    if(!TestNotNull(TEXT("Town3 owns a separate terrain actor"),LargeBase)) return false;
    TestTrue(TEXT("Town3 terrain has its own identity and owner"),LargeBase!=SmallBase && LargeBase->GetOwner()==Village && LargeBase->ActorHasTag(TEXT("ThreeHearthsTown3Terrain")));
    TestTrue(TEXT("Town3 terrain has registered 300 m collision bounds"),LargeBase->GetStaticMeshComponent()->IsRegistered()
        && LargeBase->GetStaticMeshComponent()->Bounds.BoxExtent.Equals(FVector(15000,15000,50),1.f));
    TestTrue(TEXT("Large ground reaches the map center and opposite interior corners"),Village->IsLand(FVector(6500,6500,8))
        && Village->IsLand(FVector(-8490,-8490,8)) && Village->IsLand(FVector(21490,21490,8)));
    TestEqual(TEXT("Town3 retains its version after planning"),Village->TownLayoutVersion,3);
    TestEqual(TEXT("Town3 starts with thirty real residents"),Village->Residents.Num(),30);
    if(!TestTrue(TEXT("Town3 planner reports no placement error"),Village->TownLayoutError.IsEmpty())) { AddError(Village->TownLayoutError); return false; }
    TestTrue(TEXT("Town3 generated terrain passes actual land traces"),Village->IsLand(FVector(6500.f,1500.f,8.f)));
    TestFalse(TEXT("A point beyond the generated terrain is not land"),Village->IsLand(FVector(22000.f,6500.f,8.f)));
    TestTrue(TEXT("Town3 navigation covers more than the old island grid"),Village->LandGrid.Num()>45*45);
    TSet<FString> ResidentIds,PlotIds;
    for(int32 I=0;I<Village->Residents.Num();++I)
    {
        ResidentIds.Add(Village->Residents[I].StableId); PlotIds.Add(Village->PlotIds[I]);
        TestTrue(TEXT("Each Town3 resident has an actor and story"),IsValid(Village->Residents[I].Actor) && !Village->Residents[I].InnerStory.IsEmpty());
        TestTrue(TEXT("Each Town3 plot and entrance lie on physical ground"),Village->IsLand(Village->PlotPositions[I]) && Village->IsLand(Village->HomeApproach(I)));
    }
    TestEqual(TEXT("All thirty residents have distinct identities"),ResidentIds.Num(),30);
    TestEqual(TEXT("All thirty plots have distinct identities"),PlotIds.Num(),30);
    const FString Text=Village->ExportWorldState(); FHearthWorldImage Image; FString Error;
    if(!TestTrue(TEXT("Town3 state decodes in an isolated world"),HearthWorld::Decode(Text,Image,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Thirty plots survive persistence"),Image.PlotCount,30);
    TestEqual(TEXT("Town3 version survives persistence"),Image.TownLayoutVersion,3);
    TestEqual(TEXT("Population-sized food ledger is conserved"),Image.Food,300);
    TestEqual(TEXT("Population-sized treasury follows the starter rule"),Image.TreasuryCoins,HearthVillageLimits::Town3TreasuryCoins);
    TestTrue(TEXT("Actual frontage entries survive persistence"),Image.PlotEntrances[0].IsNearlyZero()==false && Image.PlotEntrances[29].IsNearlyZero()==false);
    for(int32 I=0;I<Image.PlotCount;++I)
    {
        TestTrue(TEXT("Each plot position and entry roundtrip exactly"),Image.Plots[I].Equals(Village->PlotPositions[I],.001f) && Image.PlotEntrances[I].Equals(Village->PlotEntrances[I],.001f));
        TestEqual(TEXT("Each resident identity roundtrips"),Image.People[I].Person.StableId,Village->Residents[I].StableId);
    }

    // A fresh world contains only the original small island, not transient Town3 actors.
    const FString ColdPath=HearthPersistenceTests::TestPath();
    auto& TestFiles=FPlatformFileManager::Get().GetPlatformFile();
    ON_SCOPE_EXIT { TestFiles.DeleteFile(*ColdPath); TestFiles.DeleteFile(*(ColdPath+TEXT(".bak"))); };
    if(!TestTrue(TEXT("Write isolated Town3 cold-load fixture"),HearthWorld::Write(ColdPath,Text,Error))) { AddError(Error); return false; }
    {
        const FString FixtureCommandLine=FCommandLine::Get();
        FCommandLine::Set(*FString::Printf(TEXT("-HearthCityV3 -HearthWorld=\"%s\""),*ColdPath));
        ON_SCOPE_EXIT { FCommandLine::Set(*FixtureCommandLine); };
        UWorld* ColdWorld=HearthPersistenceTests::World(true); if(!TestNotNull(TEXT("Separate cold-load physics world"),ColdWorld)) return false;
        ON_SCOPE_EXIT { ColdWorld->DestroyWorld(false); };
        auto* PreservedBase=AddSmallBase(ColdWorld); if(!TestNotNull(TEXT("Cold scene preserves the small island"),PreservedBase)) return false;
        const FTransform PreservedTransform=PreservedBase->GetActorTransform();
        auto* ColdVillage=ColdWorld->SpawnActor<AHearthVillage>(); ColdVillage->BuildEnvironment(); ColdVillage->BuildLandGrid();
        TestTrue(TEXT("Saved Town3 cold load recreates terrain without a layout error"),ColdVillage->TownLayoutError.IsEmpty() && ColdVillage->GeneratedTown3Terrain.IsValid());
        TestTrue(TEXT("Cold Town3 has actual ground at the castle, gate and map corners"),ColdVillage->IsLand(FVector(6500,6500,8))
            && ColdVillage->IsLand(FVector(6500,1500,8)) && ColdVillage->IsLand(FVector(-8490,-8490,8)) && ColdVillage->IsLand(FVector(21490,21490,8)));
        TestTrue(TEXT("Cold scene retains its original island unchanged"),IsValid(PreservedBase) && PreservedBase->GetActorTransform().Equals(PreservedTransform));
        TestTrue(TEXT("Cold load rebuilds the expanded navigation grid"),ColdVillage->LandGrid.Num()>45*45);
        for(int32 I=0;I<Image.PlotCount;++I) TestTrue(TEXT("Cold load preserves each saved plot on real ground"),ColdVillage->PlotPositions[I].Equals(Image.Plots[I],.001f) && ColdVillage->IsLand(Image.Plots[I]));
        auto* ColdBase=ColdVillage->GeneratedTown3Terrain.Get(); ColdVillage->BuildEnvironment(); ColdVillage->BuildLandGrid();
        TestTrue(TEXT("Rebuilding reuses the same owned Town3 terrain"),ColdVillage->GeneratedTown3Terrain.Get()==ColdBase && IsValid(ColdBase));
        TestTrue(TEXT("Rebuilding retains castle access on physical ground"),ColdVillage->IsLand(FVector(6500,1500,8)) && ColdVillage->LandGrid.Num()>45*45);
    }

    // Approval-only fixture: use the real reserved public site and mint no resources or completed work.
    FHearthWorldImage ProjectImage=Image;
    const auto NewId=[] { return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); };
    auto& Project=ProjectImage.PublicProject; Project=FHearthPublicProject();
    Project.Id=NewId(); Project.TemplateId=TEXT("royal_keep_garden_v2");
    Project.King=ProjectImage.People.IndexOfByPredicate([](const FHearthSavedResident& R) { return R.Person.bKing; });
    for(int32 I=0;I<ProjectImage.Sites.Num();++I)
    {
        const auto& S=ProjectImage.Sites[I];
        if(Village->IsRoyalSite(I) && S.Kind==EHearthSiteKind::Empty && S.bExpansion && S.bReachable
            && S.Owner==-1 && S.ReservedBy==-1 && S.BuildPlanId.IsEmpty() && S.Radius>=350.f) { Project.Site=I; break; }
    }
    if(!TestTrue(TEXT("V2 fixture has a king and an existing available royal site"),Project.King>=0 && Project.Site>=0)) return false;
    const auto& PublicSite=ProjectImage.Sites[Project.Site];
    if(!TestTrue(TEXT("V2 public site and entrance have physical ground"),Village->IsLand(PublicSite.Position) && Village->IsLand(PublicSite.Approach))) return false;
    Project.Status=TEXT("building"); Project.ApprovalHistoryId=NewId(); Project.ApprovedAt=ProjectImage.Elapsed;
    HearthPublicWorks::Populate(Project);
    if(!TestEqual(TEXT("Canonical v2 contains all 1276 modules"),Project.Parts.Num(),1276)) return false;
    FHearthDecisionRecord Approval; Approval.Run=ProjectImage.Run; Approval.Timestamp=FDateTime::Now().ToString();
    Approval.Resident=Project.King; Approval.At=ProjectImage.Elapsed; Approval.Kind=TEXT("public_project_policy");
    Approval.Source=TEXT("local"); Approval.Status=TEXT("completed"); Approval.Context=TEXT("ApprovalHistoryId=")+Project.ApprovalHistoryId;
    ProjectImage.History.Add(Approval);
    FHearthDecisionRecord LastResidentHistory=Approval;
    LastResidentHistory.Resident=29; LastResidentHistory.Kind=TEXT("life");
    LastResidentHistory.Context=TEXT("thirtieth resident history regression");
    ProjectImage.History.Add(LastResidentHistory);
    FHearthWorldImage DecodedProject;
    if(!TestTrue(TEXT("Approved canonical v2 roundtrips"),HearthWorld::Decode(HearthWorld::Encode(ProjectImage),DecodedProject,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("All 1276 v2 parts survive persistence"),DecodedProject.PublicProject.Parts.Num(),1276);
    TestTrue(TEXT("History from the thirtieth actual resident survives"),DecodedProject.History.ContainsByPredicate([](const FHearthDecisionRecord& H)
        { return H.Resident==29 && H.Context==TEXT("thirtieth resident history regression"); }));
    auto InvalidPersonHistory=ProjectImage;
    InvalidPersonHistory.History.Last().Resident=30;
    FHearthWorldImage HistoryRejected;
    TestFalse(TEXT("History still rejects a nonexistent resident"),HearthWorld::Decode(HearthWorld::Encode(InvalidPersonHistory),HistoryRejected,Error));
    TestEqual(TEXT("Approval does not complete any module"),DecodedProject.PublicProject.Completed,0);
    TestFalse(TEXT("Every v2 module remains waiting without workers or materials"),DecodedProject.PublicProject.Parts.ContainsByPredicate([](const FHearthPublicPart& P)
    {
        if(P.Status!=TEXT("waiting") || P.Worker!=-1 || !P.TaskId.IsEmpty()) return true;
        for(int32 M=0;M<3;++M) if(P.Reserved[M]!=0 || P.Delivered[M]!=0) return true;
        return false;
    }));
    const int32 Seller=ProjectImage.People.IndexOfByPredicate([](const FHearthSavedResident& R) { return !R.Person.bKing; });
    if(!TestTrue(TEXT("Cancelled orders reference a real resident seller"),Seller>=0)) return false;
    for(int32 I=0;I<101;++I)
    {
        FHearthSupplyOrder Order; Order.Id=NewId(); Order.ProjectId=Project.Id; Order.Seller=Seller;
        Order.Status=TEXT("cancelled"); Order.ReservedQuantity=0; Order.Escrow=0; Order.Remaining=0;
        Order.Origin=TEXT("resident_owned_sawmill_share_or_completed_trade"); Project.Orders.Add(MoveTemp(Order));
    }
    if(!TestTrue(TEXT("More than 100 cancelled public orders roundtrip"),HearthWorld::Decode(HearthWorld::Encode(ProjectImage),DecodedProject,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("All 101 cancelled orders survive persistence"),DecodedProject.PublicProject.Orders.Num(),101);
    TestEqual(TEXT("Cancelled orders preserve treasury"),DecodedProject.TreasuryCoins,Image.TreasuryCoins);
    TestEqual(TEXT("Cancelled orders preserve the seller wallet"),DecodedProject.People[Seller].Person.Coins,Image.People[Seller].Person.Coins);
    for(int32 M=0;M<3;++M)
    {
        TestEqual(TEXT("Approval and cancellation grant no public materials"),DecodedProject.PublicProject.Grants[M],0);
        TestEqual(TEXT("Approval and cancellation create no public stock"),DecodedProject.PublicProject.Stock[M],0);
    }
    FHearthPublicPart ExcessPart=Project.Parts.Last(); ExcessPart.Id+=TEXT(":excess"); Project.Parts.Add(MoveTemp(ExcessPart));
    const FString ExcessText=HearthWorld::Encode(ProjectImage), BeforeDecoded=HearthWorld::Encode(DecodedProject);
    TestFalse(TEXT("A 1277th v2 part is rejected"),HearthWorld::Decode(ExcessText,DecodedProject,Error));
    TestEqual(TEXT("Excess parts leave the decoded image unchanged"),HearthWorld::Encode(DecodedProject),BeforeDecoded);
    const FString BeforeLive=Village->ExportWorldState();
    TestFalse(TEXT("Excess parts reject live import atomically"),Village->ApplyWorldState(ExcessText,Error));
    TestEqual(TEXT("Rejected import leaves the live world unchanged"),Village->ExportWorldState(),BeforeLive);

    // Reproduce scene-only rejection after the pure planner's first thirty choices.
    const FVector Obstructed[2]={Village->PlotPositions[0],Village->PlotPositions[1]};
    for(const FVector& Position:Obstructed)
    {
        auto* Obstacle=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Position+FVector(0,0,60),FRotator::ZeroRotator);
        if(!TestNotNull(TEXT("Physical scene obstacle over a first-choice home"),Obstacle)) return false;
        auto* Mesh=Obstacle->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
        Mesh->SetWorldScale3D(FVector(.8f)); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        TestFalse(TEXT("Actual land trace rejects the obstructed home"),Village->IsLand(Position));
    }
    Village->LandGrid.Reset();
    TestTrue(TEXT("Runtime replaces both physically rejected homes and still requires thirty"),Village->GenerateStarterNeighborhood(HearthTownLayout::VillageRoads(true,3)));
    TestTrue(TEXT("Replacement runtime layout reports no error"),Village->TownLayoutError.IsEmpty());
    for(int32 I=0;I<30;++I)
    {
        TestTrue(TEXT("Replacement plots and entrances pass actual ground checks"),Village->IsLand(Village->PlotPositions[I]) && Village->IsLand(Village->HomeApproach(I)));
        TestTrue(TEXT("Neither obstructed footprint is accepted"),!Village->PlotPositions[I].Equals(Obstructed[0],.01f) && !Village->PlotPositions[I].Equals(Obstructed[1],.01f));
    }
    return true;
}

#endif
