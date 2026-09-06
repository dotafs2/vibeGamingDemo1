#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthModularCottageTest,"ThreeHearths.Production.ModularCottageLifecycle",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthModularCottageTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated modular cottage world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true;

    int32 InstalledWood=0;
    for(int32 I=0;I<V->Residents.Num();++I)
    {
        auto& Person=V->Residents[I]; V->PlotOwners[I]=I; Person.Plot=I; Person.DeliveredWood=V->CostFor(I); InstalledWood+=Person.DeliveredWood;
        Person.BuildProgress=1.f; Person.Task=EHearthTask::LifeChoosing; Person.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        Person.Route.Reset(); Person.Actor->SetActorLocation(FVector(-800-I*400,I*500,8));
    }
    for(int32 I=0;I<3 && InstalledWood>0;++I) { const int32 Used=FMath::Min(InstalledWood,V->WoodStock[I]); V->WoodStock[I]-=Used; InstalledWood-=Used; }
    V->ProductionSites.Reset(); V->LandGrid.Reset();
    for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) V->LandGrid.Add(FIntPoint(X,Y));
    FHearthSite Site; Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Site.Kind=EHearthSiteKind::Land;
    Site.Position=FVector(400,0,8); Site.Approach=FVector(0,0,8); Site.bReachable=true; V->ProductionSites.Add(Site);
    V->Residents[0].Actor->SetActorLocation(FVector(-400,0,8));
    V->StoneStock=2; V->Produced[2]=2; V->PlankStock=5; V->Manufactured[0]=5; V->BeamStock=2; V->Manufactured[1]=2;
    const int32 Action=105, InitialWallet=V->Residents[0].Coins, Wage=V->WageForOperation(5);
    FString PlanId,Error; FHearthWorldImage Baseline;
    if(!TestTrue(TEXT("Seeded component inventory is ledger-consistent"),HearthWorld::Decode(V->ExportWorldState(),Baseline,Error))) { AddError(Error); return false; }

    V->ProductionSites[0].bReachable=false; const int32 TreasuryBeforeFailure=V->TreasuryCoins;
    TestFalse(TEXT("Unreachable construction refuses before reserving resources or wages"),V->StartProduction(0,Action,TEXT("test"),false));
    TestEqual(TEXT("Failed route leaves treasury unchanged"),V->TreasuryCoins,TreasuryBeforeFailure);
    TestTrue(TEXT("Failed route creates no build plan"),V->ProductionSites[0].BuildPlanId.IsEmpty());
    V->ProductionSites[0].bReachable=true;

    for(int32 ExpectedStage=1;ExpectedStage<=4;++ExpectedStage)
    {
        if(ExpectedStage==3)
        {
            const int32 Saved=V->PlankStock; V->PlankStock=0; const int32 Payables=V->WagePayables.Num();
            TestFalse(TEXT("Missing component material refuses the stage"),V->StartProduction(0,Action,TEXT("test"),false));
            TestEqual(TEXT("Material refusal creates no payable"),V->WagePayables.Num(),Payables); V->PlankStock=Saved;
        }
        if(!TestTrue(*FString::Printf(TEXT("Stage %d starts"),ExpectedStage),V->StartProduction(0,Action,TEXT("逐件搭建小住宅"),false))) return false;
        if(ExpectedStage==1) { PlanId=V->ProductionSites[0].BuildPlanId; FGuid Parsed; TestTrue(TEXT("Construction creates a stable GUID plan"),FGuid::Parse(PlanId,Parsed) && Parsed.IsValid()); }
        else TestEqual(TEXT("Every stage keeps the same plan identity"),V->ProductionSites[0].BuildPlanId,PlanId);
        TestEqual(TEXT("The resident who chose the plan remains its owner"),V->ProductionSites[0].Owner,0);
        TestEqual(TEXT("Construction starts by transporting a reserved component"),V->Residents[0].Task,EHearthTask::ProductionTravel);
        if(!TestTrue(TEXT("Transporting component survives reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
        for(int32 Step=0;Step<1000 && V->Residents[0].Task==EHearthTask::ProductionTravel;++Step) V->AdvanceSimulation(.05f);
        if(!TestEqual(TEXT("Worker reaches component installation"),V->Residents[0].Task,EHearthTask::ProductionWork)) return false;
        if(!TestTrue(TEXT("Installing component survives reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
        V->Residents[0].Timer=0; V->AdvanceProduction(0,.05f);
        TestEqual(TEXT("Exactly one component layer completes"),V->ProductionSites[0].Stage,ExpectedStage);
        TestEqual(TEXT("Installed material is no longer cargo"),V->Residents[0].CargoAmount,0);
        TestEqual(TEXT("One wage is paid per installed layer"),V->Residents[0].Coins,InitialWallet+Wage*ExpectedStage);
        if(ExpectedStage<4) TestFalse(TEXT("A partial cottage plan cannot be overwritten by a farm"),V->IsProductionAllowed(1,101));
        V->UpdateSiteVisual(0); const int32 Counts[]={8,29,39,45};
        TestEqual(TEXT("Visual assembly retains every earlier module"),V->ProductionSites[0].Meshes.Num(),Counts[ExpectedStage-1]);
        for(const auto& Weak:V->ProductionSites[0].Meshes) if(auto* Mesh=Weak.Get())
        {
            TestNotNull(TEXT("Installed component uses a native VillageKit mesh"),Mesh->GetStaticMesh().Get());
            TestTrue(TEXT("Installed component keeps its authored materials"),Mesh->GetNumMaterials()>0);
            TestEqual(TEXT("Modular cottage does not block resident paths"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        }
    }
    TestEqual(TEXT("Final site becomes a real house"),V->ProductionSites[0].Kind,EHearthSiteKind::House);
    TestEqual(TEXT("Foundation consumes two stone"),V->Spent[2],2);
    TestEqual(TEXT("Frame consumes two beams"),V->ManufacturedSpent[1],2);
    TestEqual(TEXT("Walls and roof consume five planks"),V->ManufacturedSpent[0],5);
    TestEqual(TEXT("All cottage component stocks are consumed"),V->StoneStock+V->BeamStock+V->PlankStock,0);
    const int32 WageTransactions=V->Transactions.FilterByPredicate([](const FHearthTransaction& T){ return T.Kind==TEXT("wage"); }).Num();
    TestEqual(TEXT("Four component jobs settle exactly four wages"),WageTransactions,4);
    const int32 TxBefore=V->Transactions.Num(); V->AdvanceEconomy(5.f); TestEqual(TEXT("Completed cottage cannot settle twice"),V->Transactions.Num(),TxBefore);
    const FString Production=V->GetProductionState();
    TestTrue(TEXT("Snapshot exposes the stable build plan"),Production.Contains(PlanId));
    TestTrue(TEXT("Snapshot exposes completed modular components"),Production.Contains(TEXT("\"status\": \"completed\"")) || Production.Contains(TEXT("\"status\":\"completed\"")));
    FHearthWorldImage Restored; if(!TestTrue(TEXT("Completed modular cottage validates as a saved world"),HearthWorld::Decode(V->ExportWorldState(),Restored,Error))) AddError(Error);
    return true;
}
#endif
