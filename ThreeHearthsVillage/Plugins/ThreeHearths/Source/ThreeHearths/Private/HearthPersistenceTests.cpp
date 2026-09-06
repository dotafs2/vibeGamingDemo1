#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace HearthPersistenceTests
{
    UWorld* World()
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    }
    FString TestPath()
    { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ThreeHearths/Tests")/FGuid::NewGuid().ToString(EGuidFormats::Digits)/TEXT("world.json")); }
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
    R.NextLifeDecision=V->Elapsed+60;
    if(!TestTrue(TEXT("Checkpoint cargo before deposit"),V->SaveWorld())) { AddError(V->WorldSaveStatus); return false; }
    const FString CargoTask=R.ActiveTaskId; V->AdvanceSimulation(.3f); TestEqual(TEXT("First delivery credited"),V->FoodStock,36);
    if(!TestTrue(TEXT("Restart from in-transit checkpoint"),V->LoadWorld())) { AddError(V->WorldSaveStatus); return false; }
    TestEqual(TEXT("Checkpoint inventory has not received cargo"),V->FoodStock,30); TestEqual(TEXT("In-transit quantity restored"),V->Residents[0].CargoAmount,6);
    TestTrue(TEXT("Remaining decision delay uses simulation time"),V->Residents[0].NextLifeDecision>V->Elapsed+58);
    TestEqual(TEXT("Harvest task retained"),V->Residents[0].ActiveTaskId,CargoTask);
    V->AdvanceSimulation(.3f); V->AdvanceProduction(0,1.f);
    TestEqual(TEXT("Deposit is applied exactly once after resume"),V->FoodStock,36); TestEqual(TEXT("Cargo cleared after credit"),V->Residents[0].CargoAmount,0);
    if(!TestEqual(TEXT("Production site retained after resume"),V->ProductionSites.Num(),1)) return false;
    TestEqual(TEXT("Site reservation released"),V->ProductionSites[0].ReservedBy,-1); TestEqual(TEXT("Operation completion counted once"),V->ProductionTotals.FindRef(TEXT("harvest")),1);
    TestTrue(TEXT("Save post-delivery checkpoint"),V->SaveWorld()); TestTrue(TEXT("Reload post-delivery checkpoint"),V->LoadWorld()); V->AdvanceProduction(0,1.f);
    TestEqual(TEXT("Completed delivery cannot be replayed"),V->FoodStock,36);
    // Material costs were paid when accepting the planting job; resume does not
    // enter StartProduction a second time and therefore must not debit again.
    R.Task=EHearthTask::ProductionTravel; R.ProductionSite=0; R.ProductionOp=7; R.WorkDuration=25;
    R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); R.Route={R.Actor->GetActorLocation()};
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
    TestFalse(TEXT("Unknown schema cannot silently migrate"),HearthWorld::Decode(V->ExportWorldState().Replace(TEXT("\"schema\":3"),TEXT("\"schema\":999")),Good,Error));
    return true;
}
#endif
