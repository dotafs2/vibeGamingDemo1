#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthServiceRuntimeTest,
    "ThreeHearths.Medieval.ServiceDutyRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthServiceRuntimeTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthOrganicVillage -HearthNoWorldPersistence -HearthNoAutonomousLife")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("isolated service world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState(); Village->bAutonomousLifeEnabled=false;
    if(!TestTrue(TEXT("service fixture has the v4 roster"),Village->IsOrganicVillage() && Village->Residents.Num()==13)) return false;
    for(int32 I=10;I<13;++I)
    {
        const FVector Anchor=Village->GetServiceDutyAnchor(I);
        TestTrue(TEXT("service duty anchor is on generated ground"),FMath::Abs(Anchor.Z-(Village->GroundHeightAt(Anchor)+5.2f))<.1f);
        Village->Residents[I].NextLifeDecision=80000;
    }
    for(int32 I=0;I<10;++I) Village->Residents[I].NextLifeDecision=80000;
    TArray<UStaticMeshComponent*> Facilities; Village->GetComponents(Facilities);
    for(auto* Part:Facilities)
    {
        if(!Part->ComponentHasTag(TEXT("HearthSourceModule=gate_pier"))) continue;
        const FVector Center=Part->Bounds.Origin;
        TestFalse(TEXT("gate piers stay outside the king's angled home"),Village->OrganicBlocksPoint(Center));
    }

    auto Prepare=[&](int32 Index,float Hunger)
    {
        auto& R=Village->Residents[Index];
        R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.LifeAction=0; R.Route.Reset(); R.Timer=0.f;
        R.Energy=90.f; R.Hunger=Hunger; R.NextLifeDecision=0.0; R.Actor->SetActorLocation(Village->GetServiceDutyAnchor(Index));
    };
    auto RunActive=[&](int32 Index,int32 MaxSeconds)
    {
        for(int32 Step=0;Step<MaxSeconds;++Step)
        {
            const bool bActive=Village->ServiceDuties.ContainsByPredicate([Index](const FHearthServiceDutyRecord& D){return D.Resident==Index && D.Status==TEXT("active");});
            if(!bActive) break;
            Village->AdvanceSimulation(1.f);
        }
        if(Index!=12 && Village->Residents[Index].ActiveTaskId.IsEmpty())
            Village->Residents[Index].NextLifeDecision=Village->Elapsed+80000;
    };

    const int32 TreasuryBefore=Village->TreasuryCoins;
    Prepare(10,10.f);
    if(!TestTrue(TEXT("gatekeeper starts a funded local duty"),Village->StartServiceDuty(10))) return false;
    const FString GateTask=Village->Residents[10].ActiveTaskId;
    TestEqual(TEXT("duty reserves exactly one three coin wage"),Village->TreasuryCoins,TreasuryBefore-3);
    TestEqual(TEXT("duty uses the imported ninety centimetre per second walk speed"),Village->Residents[10].MoveSpeed,90.f);

    FString Error; const FString ActiveDutyPayload=Village->ExportWorldState(); FHearthWorldImage ActiveImage;
    if(!TestTrue(TEXT("active duty cold image validates"),HearthWorld::Decode(ActiveDutyPayload,ActiveImage,Error))) { AddError(Error); return false; }
    if(!TestTrue(TEXT("active duty cold reload succeeds"),Village->ApplyWorldState(ActiveDutyPayload,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("cold reload keeps the active duty task"),Village->Residents[10].ActiveTaskId,GateTask);
    Village->Residents[10].NextLifeDecision=80000;
    RunActive(10,700);
    const FHearthServiceDutyRecord* GateDuty=Village->ServiceDuties.FindByPredicate([&GateTask](const FHearthServiceDutyRecord& D){return D.TaskId==GateTask;});
    if(!TestNotNull(TEXT("gate duty record survives a real simulation shift"),GateDuty)) return false;
    TestEqual(TEXT("gate duty completes after six hundred simulated seconds"),GateDuty->Status,FString(TEXT("completed")));
    TestEqual(TEXT("completed duty pays the worker once"),Village->Residents[10].Coins,3);
    TestEqual(TEXT("completed duty uses the configured shift length"),GateDuty->DutySeconds,GateDuty->ShiftSeconds);
    const int32 TransactionCount=Village->Transactions.Num(); const int32 CoinsAfter=Village->Residents[10].Coins;
    const FString CompletedPayload=Village->ExportWorldState(); FHearthWorldImage Decoded;
    if(!TestTrue(TEXT("completed duty cold image validates"),HearthWorld::Decode(CompletedPayload,Decoded,Error))) { AddError(Error); return false; }
    if(!TestTrue(TEXT("completed duty cold reload succeeds"),Village->ApplyWorldState(CompletedPayload,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("cold reload does not replay the wage"),Village->Residents[10].Coins,CoinsAfter);
    TestEqual(TEXT("cold reload does not duplicate the wage transaction"),Village->Transactions.Num(),TransactionCount);

    Prepare(11,10.f);
    if(!TestTrue(TEXT("guard starts a patrol duty"),Village->StartServiceDuty(11))) return false;
    const FString GuardTask=Village->Residents[11].ActiveTaskId;
    RunActive(11,750);
    const FHearthServiceDutyRecord* GuardDuty=Village->ServiceDuties.FindByPredicate([&GuardTask](const FHearthServiceDutyRecord& D){return D.TaskId==GuardTask;});
    if(!TestNotNull(TEXT("guard patrol record survives the simulation"),GuardDuty)) return false;
    TestEqual(TEXT("guard completes its full duty"),GuardDuty->Status,FString(TEXT("completed")));
    TestEqual(TEXT("guard visits all three patrol points"),GuardDuty->PatrolPoint,2);
    TestTrue(TEXT("guard returns to life choice after patrol"),Village->Residents[11].Task==EHearthTask::LifeChoosing && Village->Residents[11].ActiveTaskId.IsEmpty());

    // Completed colleagues must not take another shift during this resident's
    // food scenario. Their live work was already tested above.
    Village->Residents[10].NextLifeDecision=Village->Elapsed+80000;
    Village->Residents[11].NextLifeDecision=Village->Elapsed+80000;
    Prepare(12,35.f); Village->Residents[12].Coins=0;
    const int32 PoorTreasuryBefore=Village->TreasuryCoins; const int32 FoodBefore=Village->FoodStock;
    if(!TestTrue(TEXT("zero coin carter receives a funded bootstrap shift"),Village->StartServiceDuty(12))) return false;
    const FString PoorTask=Village->Residents[12].ActiveTaskId;
    const FHearthServiceDutyRecord* PoorDuty=Village->ServiceDuties.FindByPredicate([&PoorTask](const FHearthServiceDutyRecord& D){return D.TaskId==PoorTask;});
    if(!TestNotNull(TEXT("bootstrap duty record exists"),PoorDuty)) return false;
    TestTrue(TEXT("bootstrap wage is paid exactly once before food need"),PoorDuty->bWagePaid && Village->Residents[12].Coins==3 && Village->TreasuryCoins==PoorTreasuryBefore-3);
    Village->Residents[12].NextLifeDecision=80000;
    RunActive(12,750);
    PoorDuty=Village->ServiceDuties.FindByPredicate([&PoorTask](const FHearthServiceDutyRecord& D){return D.TaskId==PoorTask;});
    if(!TestNotNull(TEXT("zero coin worker has a completed shift"),PoorDuty)) return false;
    TestEqual(TEXT("zero coin worker completes the bootstrap shift"),PoorDuty->Status,FString(TEXT("completed")));
    bool BoughtFood=false;
    for(int32 Step=0;Step<360 && !BoughtFood;++Step)
    {
        Village->AdvanceSimulation(1.f);
        BoughtFood=Village->Transactions.ContainsByPredicate([](const FHearthTransaction& T){return T.Kind==TEXT("food_purchase") && T.From==12;});
    }
    TestTrue(TEXT("zero coin worker can buy real food after the completed shift"),BoughtFood && Village->FoodStock==FoodBefore-1 && Village->Residents[12].Coins==2);
    if(!BoughtFood || Village->FoodStock!=FoodBefore-1 || Village->Residents[12].Coins!=2) AddError(FString::Printf(TEXT("Food scenario bought=%d food_before=%d food_now=%d task=%d action=%d coins=%d hunger=%.2f next=%.2f elapsed=%.2f route=%d"),BoughtFood,FoodBefore,Village->FoodStock,int32(Village->Residents[12].Task),Village->Residents[12].LifeAction,Village->Residents[12].Coins,Village->Residents[12].Hunger,Village->Residents[12].NextLifeDecision,Village->Elapsed,Village->Residents[12].Route.Num()));

    Prepare(10,10.f); const int32 TreasuryBeforeCancel=Village->TreasuryCoins;
    if(!TestTrue(TEXT("a later duty can still reserve wages"),Village->StartServiceDuty(10))) return false;
    const FString CancelTask=Village->Residents[10].ActiveTaskId;
    if(!TestTrue(TEXT("cancelling an unpaid duty returns its reservation"),Village->CancelServiceDuty(10))) return false;
    TestEqual(TEXT("cancel restores the treasury balance"),Village->TreasuryCoins,TreasuryBeforeCancel);
    TestFalse(TEXT("cancelled duty cannot settle later"),Village->SettleWage(10,CancelTask));
    // Finance fixture: pay recorded wages from the remaining general purse,
    // assessing real income tax, until only protected tax cash remains.
    // No balances or collected-tax counters are assigned directly.
    while(Village->GeneralFunds()>=2)
    {
        const FString Work=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        const int32 Amount=Village->GeneralFunds()>=3?3:2;
        if(!Village->ReserveWage(0,Work,Amount) || !Village->SettleWage(0,Work)) { AddError(TEXT("tax income fixture failed")); return false; }
    }
    if(!TestTrue(TEXT("recorded wages leave an unfunded general purse but real tax money"),Village->GeneralFunds()<2 && Village->TaxProjectCoins>=3)) return false;
    Prepare(10,10.f); const int32 TaxBefore=Village->TaxProjectCoins;
    if(!TestTrue(TEXT("public guard may work using already collected tax cash"),Village->StartServiceDuty(10))) return false;
    const FString TaxCancelTask=Village->Residents[10].ActiveTaskId;
    const auto* TaxWage=Village->WagePayables.FindByPredicate([&TaxCancelTask](const FHearthWagePayable& W){return W.TaskId==TaxCancelTask;});
    TestTrue(TEXT("tax funding is visible in the wage reservation"),TaxWage && TaxWage->bTaxFunded);
    TestEqual(TEXT("reserved public wage debits exactly three existing tax coins"),Village->TaxProjectCoins,TaxBefore-3);
    TestTrue(TEXT("cancel tax-funded shift"),Village->CancelServiceDuty(10));
    TestEqual(TEXT("cancel returns tax cash to its original pool"),Village->TaxProjectCoins,TaxBefore);
    Prepare(10,10.f);
    if(!TestTrue(TEXT("a new funded shift can follow cancellation"),Village->StartServiceDuty(10))) return false;
    const FString TaxPaidTask=Village->Residents[10].ActiveTaskId;
    RunActive(10,750);
    TestEqual(TEXT("tax-funded shift settles once"),Village->Transactions.FilterByPredicate([&TaxPaidTask](const FHearthTransaction& T){return T.Kind==TEXT("wage") && T.TaskId==TaxPaidTask;}).Num(),1);
    FHearthWorldImage TaxImage;
    if(!TestTrue(TEXT("tax-funded service and cancellation preserve full world conservation"),HearthWorld::Decode(Village->ExportWorldState(),TaxImage,Error))) AddError(Error);
    return true;
}
#endif
