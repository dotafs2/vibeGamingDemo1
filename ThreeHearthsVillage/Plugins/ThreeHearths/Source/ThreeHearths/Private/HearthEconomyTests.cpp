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

    const FString Meal=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("Resident buys one real meal"),V->TransferCoins(TEXT("food_purchase"),Meal,0,-1,1,TEXT("food"),1));
    V->Manufactured[0]=1; V->Residents[0].PersonalPlanks=1; V->NextTradeAt=0; V->Elapsed=10;
    V->AdvanceEconomy(.1f);
    TestEqual(TEXT("Seller reserves owned goods"),V->Residents[0].PersonalPlanks,0);
    TestEqual(TEXT("Trade starts as proposal"),V->TradeOffers[0].Status,FString(TEXT("proposed")));
    TestTrue(TEXT("Proposal and reserved good survive reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    V->AdvanceEconomy(2.1f); TestEqual(TEXT("Buyer explicitly accepts"),V->TradeOffers[0].Status,FString(TEXT("accepted")));
    TestTrue(TEXT("Accepted delivery survives reload"),V->ApplyWorldState(V->ExportWorldState(),Error));
    V->AdvanceEconomy(4.1f);
    TestEqual(TEXT("Trade completes after delivery"),V->TradeOffers[0].Status,FString(TEXT("completed")));
    TestEqual(TEXT("Buyer owns delivered plank"),V->Residents[1].PersonalPlanks,1);
    TestEqual(TEXT("Seller received price"),V->Residents[0].Coins,16);
    TestEqual(TEXT("Buyer paid price"),V->Residents[1].Coins,10);
    V->Residents[0].Mood=0; V->Residents[2].Mood=0; V->NextTradeAt=0; V->AdvanceEconomy(.1f); V->AdvanceEconomy(2.1f);
    TestEqual(TEXT("Buyer may refuse an offer"),V->TradeOffers.Last().Status,FString(TEXT("cancelled")));
    TestEqual(TEXT("Refused trade returns the reserved plank"),V->Residents[1].PersonalPlanks,1);

    FHearthWorldImage Saved;
    TestTrue(TEXT("Complete economy state validates"),HearthWorld::Decode(V->ExportWorldState(),Saved,Error));
    int32 RestoredTotal=Saved.TreasuryCoins; for(const auto& P:Saved.People) RestoredTotal+=P.Person.Coins;
    TestEqual(TEXT("Coins are conserved"),RestoredTotal,InitialTotal);
    TestEqual(TEXT("Produced planks remain conserved across private ownership"),Saved.Manufactured[0],1);

    FHearthWorldImage Shifted=Saved; ++Shifted.People[0].Person.Coins; --Shifted.People[1].Person.Coins;
    FHearthWorldImage Rejected;
    TestFalse(TEXT("Per-account reconciliation rejects balance shifting"),HearthWorld::Decode(HearthWorld::Encode(Shifted),Rejected,Error));
    FHearthWorldImage MissingEntry=Saved; MissingEntry.Transactions.RemoveAt(MissingEntry.Transactions.Num()-1);
    TestFalse(TEXT("Deleting a sale entry is rejected"),HearthWorld::Decode(HearthWorld::Encode(MissingEntry),Rejected,Error));
    return true;
}
#endif
