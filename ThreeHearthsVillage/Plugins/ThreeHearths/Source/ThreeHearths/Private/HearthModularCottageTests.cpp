#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
    V->StoneStock=4; V->Produced[2]=4; V->PlankStock=20; V->Manufactured[0]=20; V->BeamStock=21; V->Manufactured[1]=21;
    const int32 Action=105, InitialWallet=V->Residents[0].Coins, Wage=V->WageForOperation(5);
    FString PlanId,Error; FHearthWorldImage Baseline;
    if(!TestTrue(TEXT("Seeded component inventory is ledger-consistent"),HearthWorld::Decode(V->ExportWorldState(),Baseline,Error))) { AddError(Error); return false; }

    V->ProductionSites[0].bReachable=false; const int32 TreasuryBeforeFailure=V->TreasuryCoins;
    TestFalse(TEXT("Unreachable construction refuses before reserving resources or wages"),V->StartProduction(0,Action,TEXT("test"),false));
    TestEqual(TEXT("Failed route leaves treasury unchanged"),V->TreasuryCoins,TreasuryBeforeFailure);
    TestTrue(TEXT("Failed route creates no build plan"),V->ProductionSites[0].BuildPlanId.IsEmpty());
    V->ProductionSites[0].bReachable=true;
    V->StoneStock=0; const int32 PayablesBeforeMissing=V->WagePayables.Num();
    TestFalse(TEXT("Missing first component material refuses before creating the plan"),V->StartProduction(0,Action,TEXT("test"),false));
    TestEqual(TEXT("Missing material creates no payable"),V->WagePayables.Num(),PayablesBeforeMissing); V->StoneStock=4;

    for(int32 ExpectedComponent=1;ExpectedComponent<=45;++ExpectedComponent)
    {
        if(!TestTrue(*FString::Printf(TEXT("Component %d starts"),ExpectedComponent),V->StartProduction(0,Action,TEXT("逐件搭建木结构小住宅"),false))) return false;
        if(ExpectedComponent==1)
        {
            PlanId=V->ProductionSites[0].BuildPlanId; FGuid Parsed;
            TestTrue(TEXT("Construction creates a stable GUID plan"),FGuid::Parse(PlanId,Parsed) && Parsed.IsValid());
            TestEqual(TEXT("Plan contains every independently persisted component"),V->ProductionSites[0].CottageComponents.Num(),45);
        }
        else TestEqual(TEXT("Every stage keeps the same plan identity"),V->ProductionSites[0].BuildPlanId,PlanId);
        TestEqual(TEXT("The resident who chose the plan remains its owner"),V->ProductionSites[0].Owner,0);
        TestEqual(TEXT("Construction first travels empty-handed to public inventory"),V->Residents[0].CargoAmount,0);
        if(ExpectedComponent==2 && !TestTrue(TEXT("Partial same-stage plan and reserved next piece survive reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
        for(int32 Step=0;Step<1000 && V->Residents[0].CargoAmount==0;++Step) V->AdvanceSimulation(.05f);
        if(!TestTrue(TEXT("Worker physically picks up the reserved component"),V->Residents[0].CargoAmount>0)) return false;
        if(ExpectedComponent==1)
        {
            const FVector Depot(-250,-520,8);
            TestTrue(TEXT("Pickup occurs at public inventory before the construction site"),FVector::Dist2D(V->Residents[0].Actor->GetActorLocation(),Depot)<260.f
                && FVector::Dist2D(V->Residents[0].Actor->GetActorLocation(),V->ProductionSites[0].Approach)>150.f);
        }
        if(ExpectedComponent==2)
        {
            const int32 StockBeforeCancel=V->StoneStock; const int32 TreasuryBeforeCancel=V->TreasuryCoins; const int32 PayablesBeforeCancel=V->WagePayables.Num();
            TestTrue(TEXT("Player/system may cancel after pickup"),V->CancelProduction(0));
            TestEqual(TEXT("Cancellation returns the uninstalled stone"),V->StoneStock,StockBeforeCancel+1);
            TestEqual(TEXT("Cancellation returns the unearned reserved wage"),V->TreasuryCoins,TreasuryBeforeCancel+Wage);
            TestEqual(TEXT("Cancellation removes the unpaid payable"),V->WagePayables.Num(),PayablesBeforeCancel-1);
            TestEqual(TEXT("Already installed component remains installed"),V->ProductionSites[0].Units,1);
            if(!TestTrue(TEXT("Cancelled component can be resumed"),V->StartProduction(0,Action,TEXT("恢复施工"),false))) return false;
            for(int32 Step=0;Step<1000 && V->Residents[0].CargoAmount==0;++Step) V->AdvanceSimulation(.05f);
        }
        if(ExpectedComponent==10 && !TestTrue(TEXT("In-transit individual component survives reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
        for(int32 Step=0;Step<1000 && V->Residents[0].Task==EHearthTask::ProductionTravel;++Step) V->AdvanceSimulation(.05f);
        if(!TestEqual(TEXT("Worker reaches component installation"),V->Residents[0].Task,EHearthTask::ProductionWork)) return false;
        if(ExpectedComponent==10 && !TestTrue(TEXT("Partially completed frame and installing member survive reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
        V->Residents[0].Timer=0; V->AdvanceProduction(0,.05f);
        TestEqual(TEXT("Exactly one physical component completes"),V->ProductionSites[0].Units,ExpectedComponent);
        TestEqual(TEXT("Installed material is no longer cargo"),V->Residents[0].CargoAmount,0);
        TestEqual(TEXT("Each installed component pays gross wage less accumulated income tax"),V->Residents[0].Coins,InitialWallet+Wage*ExpectedComponent-(Wage*ExpectedComponent*V->TaxRatePercent/100));
        if(ExpectedComponent<45) TestFalse(TEXT("A partial cottage plan cannot be overwritten by a farm"),V->IsProductionAllowed(1,101));
        if(ExpectedComponent==8)
        {
            TSharedPtr<FJsonObject> LegacyRoot; const FString Current=V->ExportWorldState();
            const auto Reader=TJsonReaderFactory<>::Create(Current);
            if(!TestTrue(TEXT("Schema-five cottage snapshot parses for migration fixture"),FJsonSerializer::Deserialize(Reader,LegacyRoot) && LegacyRoot.IsValid())) return false;
            LegacyRoot->SetNumberField(TEXT("schema"),4);
            for(const auto& Value:LegacyRoot->GetArrayField(TEXT("sites"))) if(const auto Object=Value->AsObject()) Object->RemoveField(TEXT("cottage_components"));
            for(const auto& Value:LegacyRoot->GetArrayField(TEXT("people"))) if(const auto Object=Value->AsObject()) Object->RemoveField(TEXT("ProductionComponentId"));
            FString LegacyText; const auto Writer=TJsonWriterFactory<>::Create(&LegacyText); FJsonSerializer::Serialize(LegacyRoot.ToSharedRef(),Writer);
            if(!TestTrue(TEXT("Schema-four staged cottage migrates into component records"),V->ApplyWorldState(LegacyText,Error))) { AddError(Error); return false; }
            TestEqual(TEXT("Migration recreates all component records"),V->ProductionSites[0].CottageComponents.Num(),45);
            TestEqual(TEXT("Migration preserves the eight installed stage-one pieces"),V->ProductionSites[0].Units,8);
        }
        if(ExpectedComponent==10) TestEqual(TEXT("Reload resumes within the frame stage, not after it"),V->ProductionSites[0].Stage,1);
    }
    TArray<FVector> EntranceRoute;
    V->Residents[0].Actor->SetActorLocation(FVector(-900,0,8));
    TestTrue(TEXT("Completed cottage keeps a traversable route to its entrance approach"),V->FindActivityRoute(0,V->ProductionSites[0].Approach,EntranceRoute));
    TestTrue(TEXT("The entrance approach stays outside the four-metre footprint"),FVector::Dist2D(V->ProductionSites[0].Position,V->ProductionSites[0].Approach)>=300.f);
    V->UpdateSiteVisual(0);
    TestEqual(TEXT("Visual assembly contains all 45 independently installed modules"),V->ProductionSites[0].Meshes.Num(),45);
    for(const auto& Weak:V->ProductionSites[0].Meshes) if(auto* Mesh=Weak.Get())
    {
        TestNotNull(TEXT("Installed component uses a native VillageKit mesh"),Mesh->GetStaticMesh().Get());
        TestTrue(TEXT("Installed component keeps its authored materials"),Mesh->GetNumMaterials()>0);
        TestEqual(TEXT("Modular cottage does not block resident paths"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    }
    TestEqual(TEXT("Final site becomes a real house"),V->ProductionSites[0].Kind,EHearthSiteKind::House);
    TestEqual(TEXT("Four foundation blocks consume four stone"),V->Spent[2],4);
    TestEqual(TEXT("Posts and beams consume twenty-one beams"),V->ManufacturedSpent[1],21);
    TestEqual(TEXT("Floors, timber walls and timber roof consume twenty planks"),V->ManufacturedSpent[0],20);
    TestEqual(TEXT("All cottage component stocks are consumed"),V->StoneStock+V->BeamStock+V->PlankStock,0);
    const int32 WageTransactions=V->Transactions.FilterByPredicate([](const FHearthTransaction& T){ return T.Kind==TEXT("wage"); }).Num();
    TestEqual(TEXT("Forty-five component jobs settle exactly forty-five wages"),WageTransactions,45);
    const int32 TxBefore=V->Transactions.Num(); V->AdvanceEconomy(5.f); TestEqual(TEXT("Completed cottage cannot settle twice"),V->Transactions.Num(),TxBefore);
    const FString Production=V->GetProductionState();
    TestTrue(TEXT("Snapshot exposes the stable build plan"),Production.Contains(PlanId));
    TestTrue(TEXT("Snapshot exposes completed modular components"),Production.Contains(TEXT("\"status\": \"completed\"")) || Production.Contains(TEXT("\"status\":\"completed\"")));
    FHearthWorldImage Restored; if(!TestTrue(TEXT("Completed modular cottage validates as a saved world"),HearthWorld::Decode(V->ExportWorldState(),Restored,Error))) AddError(Error);
    return true;
}
#endif
