#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthProjectFinanceTest,"ThreeHearths.Economy.ProtectedProjectFundsAndAtomicIncome",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthProjectFinanceTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Finance world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bApiDisabledThisRun=true;
    auto Id=[] { return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); };
    TestFalse(TEXT("Initial 500 general coins cannot fund a tax project"),V->ReserveWage(1,Id(),2,true));
    for(int32 I=0;I<4;++I) { const auto Task=Id(); TestTrue(TEXT("Reserve real income"),V->ReserveWage(0,Task,3)); TestTrue(TEXT("Settle real income"),V->SettleWage(0,Task)); }
    TestEqual(TEXT("12 gross income yields three tax coins"),V->TaxProjectCoins,3);
    const int32 General=V->GeneralFunds(); const FString Project=Id();
    TestTrue(TEXT("Tax-backed wage reserves actual tax coins"),V->ReserveWage(1,Project,2,true));
    TestEqual(TEXT("General cash does not change on tax reservation"),V->GeneralFunds(),General);
    TestEqual(TEXT("Tax cash leaves available balance for escrow"),V->TaxProjectCoins,1);
    FString Error; TestTrue(TEXT("Tax escrow reloads"),V->ApplyWorldState(V->ExportWorldState(),Error));
    TestTrue(TEXT("Cancelling escrow returns tax cash"),V->CancelWage(Project));
    TestFalse(TEXT("Escrow cannot be refunded twice"),V->CancelWage(Project));
    TestEqual(TEXT("Cancellation restores original tax fund"),V->TaxProjectCoins,3);
    const FString Paid=Id(); TestTrue(TEXT("Re-reserve using a new task"),V->ReserveWage(1,Paid,2,true));
    TestTrue(TEXT("Earned project wage settles"),V->SettleWage(1,Paid));
    TestFalse(TEXT("Earned wage cannot be refunded"),V->CancelWage(Paid));
    TestTrue(TEXT("Paid tax-funded wage reconciles on reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    const FString Stable=V->ExportWorldState();
    V->TaxRatePercent=30; const auto Work=Id(); TestTrue(TEXT("Funding can precede a settlement failure"),V->ReserveWage(2,Work,3));
    const FString Before=V->ExportWorldState();
    TestFalse(TEXT("Unsupported policy fails before wage mutation"),V->SettleWage(2,Work));
    TestEqual(TEXT("Failed policy leaves all world state unchanged"),V->ExportWorldState(),Before);
    TestTrue(TEXT("Restore supported policy fixture"),V->ApplyWorldState(Stable,Error));
    const auto CapacityTask=Id(); TestTrue(TEXT("Reserve capacity test wage"),V->ReserveWage(0,CapacityTask,3));
    // Worker zero already has a zero remainder: a 3-coin wage needs one transaction.
    V->TaxRemainders[0]=75;
    V->Transactions.SetNum(99999);
    const int32 Wallet=V->Residents[0].Coins,Treasury=V->TreasuryCoins,Taxes=V->TaxAssessments.Num();
    TestFalse(TEXT("Only one free transaction slot cannot hold income plus tax"),V->SettleWage(0,CapacityTask));
    TestEqual(TEXT("Capacity failure preserves wallet"),V->Residents[0].Coins,Wallet);
    TestEqual(TEXT("Capacity failure preserves treasury"),V->TreasuryCoins,Treasury);
    TestEqual(TEXT("Capacity failure preserves tax assessments"),V->TaxAssessments.Num(),Taxes);
    TestEqual(TEXT("Capacity failure preserves wage reservation"),V->WagePayables.Last().Status,FString(TEXT("reserved")));
    TestFalse(TEXT("Trade also refuses incomplete income/tax capacity"),V->TransferCoins(TEXT("plank_trade"),Id(),1,0,2,TEXT("plank"),1));
    TestEqual(TEXT("Trade capacity failure preserves seller"),V->Residents[0].Coins,Wallet);
    TestTrue(TEXT("Return to valid fixture"),V->ApplyWorldState(Stable,Error));
    const int32 Cash=V->TreasuryCoins; V->TaxProjectCoins=Cash;
    TestFalse(TEXT("Ordinary worker cannot spend protected project cash"),V->ReserveWage(0,Id(),2));
    FHearthWagePayable Unfunded; Unfunded.Id=Id(); Unfunded.TaskId=Id(); Unfunded.Worker=2; Unfunded.Amount=2; Unfunded.Status=TEXT("owed");
    V->WagePayables.Add(Unfunded); V->AdvanceEconomy(.1f);
    TestEqual(TEXT("Deferred wage also cannot spend protected cash"),V->WagePayables.Last().Status,FString(TEXT("owed")));
    TestEqual(TEXT("Protected cash stays in treasury"),V->TreasuryCoins,Cash);
    const FString BeforeDuplicateTax=V->ExportWorldState();
    TestTrue(TEXT("A completed wage has a tax assessment"),V->TaxAssessments.Num()>0);
    const FHearthTaxAssessment CompletedAssessment=V->TaxAssessments.Last();
    V->CommitIncomeTax(CompletedAssessment);
    TestEqual(TEXT("Replayed tax assessment leaves all state unchanged"),V->ExportWorldState(),BeforeDuplicateTax);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPrivateHouseFinanceTest,"ThreeHearths.Economy.PrivateHouseWageEscrow",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPrivateHouseFinanceTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Private finance world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bApiDisabledThisRun=true;
    auto Id=[] { return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); };
    FString Error; FHearthWorldImage Image;
    // Exhaust general funds through real, assessed wages. Protected tax cash remains.
    while(V->GeneralFunds()>=2)
    {
        const int32 Amount=V->GeneralFunds()==2?2:3; const FString Task=Id();
        if(!V->ReserveWage(2,Task,Amount) || !V->SettleWage(2,Task)) return false;
    }
    TestEqual(TEXT("Fixture spends all general funds without inventing coins"),V->GeneralFunds(),0);
    V->PlotOwners[0]=0; V->Residents[0].Plot=0; V->Residents[0].DeliveredWood=V->CostFor(0); V->WoodStock[0]-=V->Residents[0].DeliveredWood;
    V->Residents[0].BuildProgress=1.f; V->Residents[0].Task=EHearthTask::LifeChoosing;
    const TArray<FHearthSite> OriginalSites=V->ProductionSites; V->ProductionSites.Reset();
    FHearthSite PrivatePlot; PrivatePlot.StableId=Id(); PrivatePlot.Kind=EHearthSiteKind::Empty; PrivatePlot.Position=FVector(300,0,8); PrivatePlot.Approach=FVector(100,0,8); PrivatePlot.bReachable=true; V->ProductionSites.Add(PrivatePlot);
    FHearthSite PublicQuarry; PublicQuarry.StableId=Id(); PublicQuarry.Kind=EHearthSiteKind::Stone; PublicQuarry.Position=FVector(-300,0,8); PublicQuarry.Approach=FVector(-100,0,8); PublicQuarry.bReachable=true; PublicQuarry.Units=10; V->ProductionSites.Add(PublicQuarry);
    const int32 EmptySite=V->ProductionSites.IndexOfByPredicate([](const FHearthSite& Site){return Site.Kind==EHearthSiteKind::Empty&&Site.bReachable;});
    const int32 StoneSite=V->ProductionSites.IndexOfByPredicate([](const FHearthSite& Site){return Site.Kind==EHearthSiteKind::Stone&&Site.bReachable&&Site.Units>0;});
    TestTrue(TEXT("A resident may fund their own plot preparation after public funds run out"),EmptySite>=0&&V->IsProductionAllowed(0,100+EmptySite*16));
    TestFalse(TEXT("An unfunded public production job is removed before local choice"),StoneSite>=0&&V->IsProductionAllowed(0,100+StoneSite*16+11));
    V->Residents[0].Role=TEXT("木匠"); V->Residents[0].SocialNeed=80.f;
    TestEqual(TEXT("Local policy chooses the executable private plot instead of an unfunded public job"),V->ChooseProductionLocally(0),100+EmptySite*16);
    V->ProductionSites=OriginalSites;
    const int32 Treasury=V->TreasuryCoins,TaxFund=V->TaxProjectCoins,Owner=V->Residents[0].Coins,Worker=V->Residents[1].Coins;
    TestFalse(TEXT("Public wage refuses empty general fund"),V->ReserveWage(1,Id(),2));
    TestFalse(TEXT("Private funding cannot also claim tax money"),V->ReserveWage(1,Id(),2,true,0));
    TestFalse(TEXT("Private funding rejects an unknown owner"),V->ReserveWage(1,Id(),2,false,99));
    const FString Cancelled=Id();
    TestTrue(TEXT("House owner funds neighbor's component even with empty general fund"),V->ReserveWage(1,Cancelled,2,false,0));
    TestEqual(TEXT("Escrow removes real owner coins"),V->Residents[0].Coins,Owner-2);
    TestEqual(TEXT("Escrow leaves worker unpaid"),V->Residents[1].Coins,Worker);
    TestEqual(TEXT("Private escrow leaves protected treasury untouched"),V->TreasuryCoins,Treasury);
    if(!TestTrue(TEXT("Private escrow and exact funder survive schema9 reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Cancelling after reload refunds original owner"),V->CancelWage(Cancelled));
    TestEqual(TEXT("Refund returns to owner wallet"),V->Residents[0].Coins,Owner);
    TestEqual(TEXT("Refund does not become village income"),V->TreasuryCoins,Treasury);
    TestFalse(TEXT("Refund cannot run twice"),V->CancelWage(Cancelled));
    for(int32 Part=0;Part<2;++Part)
    {
        const FString Task=Id(); TestTrue(TEXT("Reserve next private component"),V->ReserveWage(1,Task,2,false,0));
        TestTrue(TEXT("Private component wage settles"),V->SettleWage(1,Task));
        TestFalse(TEXT("Private component cannot be paid twice"),V->SettleWage(1,Task));
        const auto* Wage=V->Transactions.FindByPredicate([&](const auto& T){return T.Kind==TEXT("wage")&&T.TaskId==Task;});
        TestTrue(TEXT("Income ledger identifies the actual house owner"),Wage&&Wage->From==0&&Wage->To==1);
    }
    TestEqual(TEXT("Owner pays four gross coins"),V->Residents[0].Coins,Owner-4);
    TestEqual(TEXT("Worker receives four coins less one income-tax coin"),V->Residents[1].Coins,Worker+3);
    TestEqual(TEXT("Only assessed tax reaches protected fund"),V->TaxProjectCoins,TaxFund+1);
    if(!TestTrue(TEXT("Private paid wages reconcile both wallets and tax on reload"),V->ApplyWorldState(V->ExportWorldState(),Error))) { AddError(Error); return false; }
    const FString Self=Id(); const int32 SelfWallet=V->Residents[0].Coins;
    TestTrue(TEXT("Owner may reserve own labor"),V->ReserveWage(0,Self,2,false,0));
    TestTrue(TEXT("Owner's completed labor releases their escrow"),V->SettleWage(0,Self));
    TestEqual(TEXT("Self labor does not create money"),V->Residents[0].Coins,SelfWallet);
    if(!TestTrue(TEXT("Self-funded labor is a valid conserved transaction"),HearthWorld::Decode(V->ExportWorldState(),Image,Error))) { AddError(Error); return false; }
    auto Corrupt=Image; Corrupt.WagePayables.Last().Funder=1;
    TestFalse(TEXT("A forged wage payer is rejected"),HearthWorld::Decode(HearthWorld::Encode(Corrupt),Image,Error));
    // A public payable without the optional field is still the historical treasury case.
    V->ResetVillageState(); const FString LegacyTask=Id(); TestTrue(TEXT("Reserve historical public wage"),V->ReserveWage(1,LegacyTask,2));
    TSharedPtr<FJsonObject> LegacyRoot;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V->ExportWorldState()),LegacyRoot)) return false;
    for(const auto& Entry:LegacyRoot->GetArrayField(TEXT("wage_payables"))) Entry->AsObject()->RemoveField(TEXT("funder"));
    FString Legacy; FJsonSerializer::Serialize(LegacyRoot.ToSharedRef(),TJsonWriterFactory<>::Create(&Legacy));
    if(!TestTrue(TEXT("Earlier schema9 treasury escrow remains loadable"),HearthWorld::Decode(Legacy,Image,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Missing payer defaults to treasury"),Image.WagePayables.Last().Funder,-1);
    return true;
}
#endif
