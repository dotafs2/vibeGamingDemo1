#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthEconomyPersistenceTest,"ThreeHearths.Economy.WalletsTradesAndWages",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthEconomyPersistenceTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated economy world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true;
    int32 InitialTotal=V->TreasuryCoins; for(const auto& R:V->Residents) InitialTotal+=R.Coins;

    const FString Work=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("Wage is reserved before work"),V->ReserveWage(0,Work,3));
    TestEqual(TEXT("Reservation leaves the worker unpaid"),V->Residents[0].Coins,12);
    FString Error; const FString ReservedWorld=V->ExportWorldState();
    TestTrue(TEXT("Reserved wage survives reload"),V->ApplyWorldState(ReservedWorld,Error));
    TestTrue(TEXT("Restored reservation settles"),V->SettleWage(0,Work));
    TestFalse(TEXT("The same task cannot receive its wage twice"),V->SettleWage(0,Work));

    const int32 BeforePoorTreasury=V->TreasuryCoins,BeforePayables=V->WagePayables.Num();
    V->TreasuryCoins=1;
    TestFalse(TEXT("Work is legally refused when wage cannot be reserved"),V->ReserveWage(1,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),3));
    TestEqual(TEXT("Rejected reservation changes no ledger"),V->WagePayables.Num(),BeforePayables);
    TestEqual(TEXT("Rejected reservation changes no balance"),V->TreasuryCoins,1); V->TreasuryCoins=BeforePoorTreasury;

    const int32 BeforeWallet=V->Residents[0].Coins,BeforeTreasury=V->TreasuryCoins,BeforeTransactions=V->Transactions.Num();
    TestFalse(TEXT("Invalid negative account is rejected"),V->TransferCoins(TEXT("food_purchase"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),-2,-1,1,TEXT("food"),1));
    TestEqual(TEXT("Invalid transfer changes no wallet"),V->Residents[0].Coins,BeforeWallet);
    TestEqual(TEXT("Invalid transfer changes no treasury"),V->TreasuryCoins,BeforeTreasury);
    TestEqual(TEXT("Invalid transfer changes no ledger"),V->Transactions.Num(),BeforeTransactions);

    TestFalse(TEXT("Food purchase rejects a non-GUID task"),V->TransferCoins(TEXT("food_purchase"),TEXT("not-a-guid"),0,-1,1,TEXT("food"),1));
    TestFalse(TEXT("Food purchase rejects the wrong price"),V->TransferCoins(TEXT("food_purchase"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),0,-1,2,TEXT("food"),1));
    TestFalse(TEXT("Wage reservation rejects a non-GUID task"),V->ReserveWage(0,TEXT("not-a-guid"),2));
    TestFalse(TEXT("Wage reservation rejects an unsupported amount"),V->ReserveWage(0,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),4));
    TestEqual(TEXT("Malformed economy requests change no wallet"),V->Residents[0].Coins,BeforeWallet);
    TestEqual(TEXT("Malformed economy requests change no treasury"),V->TreasuryCoins,BeforeTreasury);
    TestEqual(TEXT("Malformed economy requests change no ledger"),V->Transactions.Num(),BeforeTransactions);
    TestEqual(TEXT("Malformed wage requests create no payable"),V->WagePayables.Num(),BeforePayables);

    const FString Meal=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("Resident buys one real meal"),V->TransferCoins(TEXT("food_purchase"),Meal,0,-1,1,TEXT("food"),1));
    int32 DeliveredForHomes=0;
    for(int32 I=0;I<V->Residents.Num();++I)
    {
        V->PlotOwners[I]=I; V->Residents[I].Plot=I; V->Residents[I].DeliveredWood=V->CostFor(I);
        DeliveredForHomes+=V->Residents[I].DeliveredWood;
        V->Residents[I].BuildProgress=1.f; V->Residents[I].Task=EHearthTask::LifeChoosing;
        V->Residents[I].ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); V->Residents[I].Route.Reset();
    }
    V->WoodStock[0]=36-DeliveredForHomes; V->WoodStock[1]=V->WoodStock[2]=0;
    if(!TestEqual(TEXT("Trade test residents have completed homes"),V->CompletedHomes(),3)) return false;
    V->Manufactured[0]=2; V->Residents[0].PersonalPlanks=2;
    V->Residents[0].Actor->SetActorLocation(FVector(-600,0,8)); V->Residents[1].Actor->SetActorLocation(FVector(0,0,8));
    TestTrue(TEXT("Seller chooses a real visit with the buyer"),V->BeginConversation(0,1,TEXT("出售自己的木板"),false));
    const FVector SellerStart=V->Residents[0].Actor->GetActorLocation();
    for(int32 Step=0;Step<500 && !V->Conversations[0].bMet;++Step) { V->AdvanceSimulation(.05f); V->AdvanceSocial(.05f); }
    if(!TestTrue(TEXT("Seller and buyer meet before negotiating"),V->Conversations[0].bMet)) return false;
    TestTrue(TEXT("Seller opens the conversation"),V->ResolveSocialTurn(0,0,TEXT("最近过得怎么样？"),TEXT("test")));
    TestTrue(TEXT("Buyer answers independently"),V->ResolveSocialTurn(1,0,TEXT("挺好，你找我有什么事？"),TEXT("test")));
    TestTrue(TEXT("Seller speaks the plank offer"),V->ResolveSocialTurn(0,6,TEXT("我有一块自己的木板，两枚钱卖给你。"),TEXT("test")));
    TestEqual(TEXT("Seller reserves one surplus plank and keeps one"),V->Residents[0].PersonalPlanks,1);
    TestTrue(TEXT("Proposal is an actual dialogue intent"),V->Conversations[0].Lines.ContainsByPredicate([](const FHearthDialogueLine& L){ return L.Intent==6; }));
    TestTrue(TEXT("Proposal and reserved good survive reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    TestTrue(TEXT("Buyer explicitly accepts in dialogue"),V->ResolveSocialTurn(1,3,TEXT("两枚钱可以，送来后我付款。"),TEXT("test")));
    TestTrue(TEXT("Accepted offer survives reload before delivery"),V->ApplyWorldState(V->ExportWorldState(),Error));
    TestTrue(TEXT("Seller closes negotiation and starts delivery"),V->ResolveSocialTurn(0,5,TEXT("好，我现在送过来。"),TEXT("test")));
    bool SawTravel=V->Residents[0].Task==EHearthTask::TradeTravel; const int32 TransactionsBeforeDelivery=V->Transactions.Num();
    TestTrue(TEXT("In-transit seller and carried plank survive reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    TestEqual(TEXT("Reload resumes seller delivery travel"),V->Residents[0].Task,EHearthTask::TradeTravel);
    TestEqual(TEXT("Reload keeps exactly one reserved trade plank"),V->TradeOffers[0].ReservedQuantity,1);
    for(int32 Step=0;Step<1000 && V->TradeOffers[0].Status!=TEXT("delivering");++Step)
    { V->AdvanceSimulation(.05f); SawTravel|=V->Residents[0].Task==EHearthTask::TradeTravel; }
    if(!TestEqual(TEXT("Seller reaches the handover phase"),V->TradeOffers[0].Status,FString(TEXT("delivering")))) return false;
    TestTrue(TEXT("Handover-in-progress survives reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    TestEqual(TEXT("Handover reload has not paid early"),V->Transactions.Num(),TransactionsBeforeDelivery);
    for(int32 Step=0;Step<1000 && V->TradeOffers[0].Status!=TEXT("completed");++Step) V->AdvanceSimulation(.05f);
    TestTrue(TEXT("Seller physically enters delivery travel"),SawTravel);
    TestTrue(TEXT("Seller moved while carrying the reserved plank"),!V->Residents[0].Actor->GetActorLocation().Equals(SellerStart,1.f));
    if(!TestTrue(TEXT("A trade record exists after negotiation"),!V->TradeOffers.IsEmpty())) return false;
    TestEqual(TEXT("Trade completes only after delivery"),V->TradeOffers[0].Status,FString(TEXT("completed")));
    TestEqual(TEXT("Buyer owns delivered plank"),V->Residents[1].PersonalPlanks,1);
    TestEqual(TEXT("Seller received price"),V->Residents[0].Coins,16);
    TestEqual(TEXT("Buyer paid price"),V->Residents[1].Coins,10);
    const int32 TransactionsAfterDelivery=V->Transactions.Num(); V->AdvanceEconomy(5.f);
    TestEqual(TEXT("Completed handover cannot settle twice"),V->Transactions.Num(),TransactionsAfterDelivery);
    TestTrue(TEXT("Both residents remember the completed exchange"),V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Memory.Contains(TEXT("交付"))
        && V->Residents[1].Bonds.FindChecked(V->Residents[0].StableId).Memory.Contains(TEXT("交付")));

    ++V->Manufactured[0]; ++V->Residents[1].PersonalPlanks;
    V->Residents[1].Actor->SetActorLocation(FVector(-600,300,8)); V->Residents[2].Actor->SetActorLocation(FVector(0,300,8)); V->Residents[2].Energy=0;
    TestTrue(TEXT("Owner may offer the purchased plank onward"),V->BeginConversation(1,2,TEXT("询问邻居是否需要木板"),false));
    for(int32 Step=0;Step<500 && !V->Conversations.Last().bMet;++Step) { V->AdvanceSimulation(.05f); V->AdvanceSocial(.05f); }
    if(!TestTrue(TEXT("Second seller reaches the intended buyer"),V->Conversations.Last().bMet)) return false;
    V->ResolveSocialTurn(1,0,TEXT("最近好吗？"),TEXT("test")); V->ResolveSocialTurn(2,0,TEXT("有些累，你找我吗？"),TEXT("test"));
    TestTrue(TEXT("Second owner makes a real offer"),V->ResolveSocialTurn(1,6,TEXT("我有一块木板，两枚钱卖给你。"),TEXT("test")));
    V->DecideSocialLocally(2,TEXT("test_local_fallback"));
    TestEqual(TEXT("Buyer may refuse an offer"),V->TradeOffers.Last().Status,FString(TEXT("cancelled")));
    TestTrue(TEXT("Refusal is an actual dialogue intent"),V->Conversations.Last().Lines.ContainsByPredicate([](const FHearthDialogueLine& L){ return L.Intent==4; }));
    TestEqual(TEXT("Refused trade returns the reserved plank"),V->Residents[1].PersonalPlanks,2);

    FHearthWorldImage Saved;
    if(!TestTrue(TEXT("Complete economy state validates"),HearthWorld::Decode(V->ExportWorldState(),Saved,Error))) { AddError(Error); return false; }
    int32 RestoredTotal=Saved.TreasuryCoins; for(const auto& P:Saved.People) RestoredTotal+=P.Person.Coins;
    TestEqual(TEXT("Coins are conserved"),RestoredTotal,InitialTotal);
    TestEqual(TEXT("Produced planks remain conserved across private ownership"),Saved.Manufactured[0],3);

    FHearthWorldImage Shifted=Saved; ++Shifted.People[0].Person.Coins; --Shifted.People[1].Person.Coins;
    FHearthWorldImage Rejected;
    TestFalse(TEXT("Per-account reconciliation rejects balance shifting"),HearthWorld::Decode(HearthWorld::Encode(Shifted),Rejected,Error));
    FHearthWorldImage MissingEntry=Saved; MissingEntry.Transactions.RemoveAt(MissingEntry.Transactions.Num()-1);
    TestFalse(TEXT("Deleting a sale entry is rejected"),HearthWorld::Decode(HearthWorld::Encode(MissingEntry),Rejected,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthLegacyUnfundedWageTest,"ThreeHearths.Economy.LegacyUnfundedWorkMigration",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthLegacyUnfundedWageTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated legacy economy world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true;
    auto& R=V->Residents[0]; R.Plot=0; R.DeliveredWood=V->CostFor(0); R.BuildProgress=1.f; R.Task=EHearthTask::LifeChoosing; V->PlotOwners[0]=0;
    int32 Installed=R.DeliveredWood; for(int32 I=0;I<3 && Installed>0;++I) { const int32 Used=FMath::Min(Installed,V->WoodStock[I]); V->WoodStock[I]-=Used; Installed-=Used; }
    for(int32 X=-8;X<=8;++X) for(int32 Y=-8;Y<=8;++Y) V->LandGrid.Add(FIntPoint(X,Y));
    FHearthSite Tree; Tree.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Tree.Kind=EHearthSiteKind::Tree;
    Tree.Position=FVector(300,300,8); Tree.Approach=FVector(0,300,8); Tree.Stage=2; Tree.Units=Tree.Capacity=18; Tree.bReachable=true; Tree.ReservedBy=0; V->ProductionSites.Add(Tree);
    FHearthDecisionRecord WorkHistory; WorkHistory.Run=V->CurrentRun; WorkHistory.Resident=0; WorkHistory.Status=TEXT("executing");
    R.HistoryIndex=V->DecisionHistory.Add(WorkHistory); R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    R.Task=EHearthTask::ProductionWork; R.ProductionSite=0; R.ProductionOp=10; R.WorkDuration=10.f;
    const FString ActiveTask=R.ActiveTaskId; const int32 ActiveWage=V->WageForOperation(R.ProductionOp);
    if(!TestTrue(TEXT("Legacy worker has a reserved wage before conversion"),V->ReserveWage(0,ActiveTask,ActiveWage))) return false;
    while(V->TreasuryCoins>=3)
    {
        const FString Task=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        if(!V->ReserveWage(1,Task,3) || !V->SettleWage(1,Task)) return false;
    }
    while(V->TreasuryCoins>=2)
    {
        const FString Task=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        if(!V->ReserveWage(1,Task,2) || !V->SettleWage(1,Task)) return false;
    }
    FHearthWorldImage Old; FString Error;
    if(!TestTrue(TEXT("Modern fixture validates before legacy conversion"),HearthWorld::Decode(V->ExportWorldState(),Old,Error))) { AddError(Error); return false; }
    Old.WagePayables.RemoveAll([&](const FHearthWagePayable& P){ return P.TaskId==ActiveTask; });
    FHearthTransaction LegacySpend; LegacySpend.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); LegacySpend.Kind=TEXT("wage");
    LegacySpend.TaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); LegacySpend.From=-1; LegacySpend.To=1; LegacySpend.Amount=ActiveWage; LegacySpend.Item=TEXT("labor"); LegacySpend.Quantity=1;
    Old.Transactions.Add(LegacySpend); Old.People[1].Person.Coins+=ActiveWage;
    FHearthWagePayable LegacyPaid; LegacyPaid.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); LegacyPaid.TaskId=LegacySpend.TaskId;
    LegacyPaid.Worker=1; LegacyPaid.Amount=ActiveWage; LegacyPaid.Status=TEXT("paid"); Old.WagePayables.Add(LegacyPaid);
    FString Legacy=HearthWorld::Encode(Old); Legacy.ReplaceInline(TEXT("\"schema\":5"),TEXT("\"schema\":3"));
    if(!TestTrue(TEXT("Valid schema-3 world migrates"),V->ApplyWorldState(Legacy,Error))) { AddError(Error); return false; }
    auto* Payable=V->WagePayables.FindByPredicate([&](const FHearthWagePayable& P){ return P.TaskId==ActiveTask; });
    if(!TestNotNull(TEXT("Migration creates a payable for unfinished work"),Payable)) return false;
    TestEqual(TEXT("Unfunded unfinished work is not marked owed or paid"),Payable->Status,FString(TEXT("unfunded")));
    const int32 WalletBeforeFunding=V->Residents[0].Coins;
    for(int32 I=0;I<ActiveWage;++I) TestTrue(TEXT("A resident purchase replenishes one treasury coin"),V->TransferCoins(TEXT("food_purchase"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),2,-1,1,TEXT("food"),1));
    V->AdvanceEconomy(.1f);
    TestEqual(TEXT("Funding unfinished work reserves its wage"),Payable->Status,FString(TEXT("reserved")));
    TestEqual(TEXT("Reservation does not pay before completion"),V->Residents[0].Coins,WalletBeforeFunding);
    if(!TestTrue(TEXT("Funded but unfinished migrated work reloads"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
    Payable=V->WagePayables.FindByPredicate([&](const FHearthWagePayable& P){ return P.TaskId==ActiveTask; });
    if(!TestNotNull(TEXT("Reserved migrated wage survives reload"),Payable)) return false;
    TestEqual(TEXT("Reload keeps funded unfinished wage reserved"),Payable->Status,FString(TEXT("reserved")));
    TestEqual(TEXT("Reload still does not pay unfinished worker"),V->Residents[0].Coins,WalletBeforeFunding);
    const int32 Site=V->Residents[0].ProductionSite; V->ReturnTool(0); if(V->ProductionSites.IsValidIndex(Site)) V->ProductionSites[Site].ReservedBy=-1;
    V->Residents[0].Task=EHearthTask::LifeChoosing; V->Residents[0].ProductionSite=-1; V->Residents[0].ProductionOp=-1; V->Residents[0].WorkDuration=0;
    TestTrue(TEXT("Completed migrated work settles once"),V->SettleWage(0,ActiveTask));
    TestEqual(TEXT("Worker receives the wage only after completion"),V->Residents[0].Coins,WalletBeforeFunding+ActiveWage);
    TestFalse(TEXT("Migrated job cannot be paid twice"),V->SettleWage(0,ActiveTask));
    return true;
}
#endif
