#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

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
    return true;
}
#endif
