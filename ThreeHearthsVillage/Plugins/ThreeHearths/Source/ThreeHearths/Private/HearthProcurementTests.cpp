#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSupplyOrderTest,"ThreeHearths.Economy.PublicProcurement",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSupplyOrderTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Procurement world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true;
    auto& R=V->Residents[0]; R.Task=EHearthTask::LifeChoosing; R.Route.Reset();
    V->PublicProject.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    V->PublicProject.Status=TEXT("approved"); V->PublicProject.Completed=0;
    FHearthPublicPart Part; Part.Required[1]=1; V->PublicProject.Parts={Part};
    V->TaxProjectCoins=5; R.PersonalPlanks=0;
    TestFalse(TEXT("Supplier without a privately owned plank is rejected"),V->StartSupplyOrder(0));
    TestEqual(TEXT("Missing ownership leaves tax escrow unchanged"),V->TaxProjectCoins,5);

    R.PersonalPlanks=1;
    if(!TestTrue(TEXT("A real owned plank starts one public supply order"),V->StartSupplyOrder(0))) return false;
    TestFalse(TEXT("Duplicate active order cannot reserve a second plank"),V->StartSupplyOrder(0));
    TestEqual(TEXT("One plank is reserved from the supplier"),R.PersonalPlanks,0);
    TestEqual(TEXT("Supply escrow is protected from general spending"),V->TaxProjectCoins,3);
    const int32 TreasuryBeforeCancel=V->TreasuryCoins;
    TestTrue(TEXT("Cancellation returns the plank and both escrow balances"),V->CancelSupplyOrder(0));
    TestEqual(TEXT("Cancelled order returns the plank exactly once"),R.PersonalPlanks,1);
    TestEqual(TEXT("Cancelled order returns tax escrow exactly"),V->TaxProjectCoins,5);
    TestEqual(TEXT("Cancelled order returns treasury escrow exactly"),V->TreasuryCoins,TreasuryBeforeCancel+2);
    TestTrue(TEXT("Repeated cancellation is harmless"),V->CancelSupplyOrder(0));
    TestEqual(TEXT("Repeated cancellation does not duplicate goods"),R.PersonalPlanks,1);

    if(!TestTrue(TEXT("Supplier can reserve the returned real plank again"),V->StartSupplyOrder(0))) return false;
    FHearthSupplyOrder& Order=V->PublicProject.Orders.Last();
    const int32 TreasuryBeforeSale=V->TreasuryCoins;
    R.Task=EHearthTask::SupplyHandover; R.ActiveTaskId=Order.Id;
    V->TaxRatePercent=30;
    const FString BeforeFailure=V->ExportWorldState();
    TestFalse(TEXT("Tax policy failure leaves delivery fully pending"),V->SettleSupplyOrder(Order));
    TestEqual(TEXT("Failed settlement leaves all state unchanged"),V->ExportWorldState(),BeforeFailure);
    V->TaxRatePercent=25; V->TaxRemainders[0]=75;
    TestTrue(TEXT("Delivery settles sale and tax atomically"),V->SettleSupplyOrder(Order));
    TestEqual(TEXT("One plank reaches public project stock"),V->PublicProject.Stock[1],1);
    TestEqual(TEXT("Supplier receives net proceeds after tax"),R.Coins,13);
    TestEqual(TEXT("Settlement records a single public purchase"),V->Transactions.Num(),2);
    const auto* Sale=V->Transactions.FindByPredicate([](const FHearthTransaction& T){ return T.Kind==TEXT("public_purchase"); });
    TestTrue(TEXT("Purchase transaction exists"),Sale!=nullptr);
    if(Sale) { TestEqual(TEXT("Purchase has the public treasury as payer"),Sale->From,-1); TestEqual(TEXT("Purchase records one plank"),Sale->Quantity,1); }
    TestEqual(TEXT("Sale does not refund spent escrow into treasury"),V->TreasuryCoins,TreasuryBeforeSale+1);
    TestTrue(TEXT("Settling a completed order again is harmless"),V->SettleSupplyOrder(Order));
    TestEqual(TEXT("Duplicate settlement does not add stock"),V->PublicProject.Stock[1],1);
    return true;
}
#endif
