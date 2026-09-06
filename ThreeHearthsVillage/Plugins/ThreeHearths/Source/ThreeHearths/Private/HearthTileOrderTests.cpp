#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTileOrderTest,"ThreeHearths.Society.TileOrderNegotiation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTileOrderTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated tile-order world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    V->bAutonomousLifeEnabled=false; V->bApiDisabledThisRun=true; V->ClayStock=8; V->ProducedClay=8;
    V->LandGrid.Reset(); for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) V->LandGrid.Add(FIntPoint(X,Y));
    for(int32 I=0;I<2;++I)
    {
        V->PlotOwners[I]=I; V->Residents[I].Plot=I; V->Residents[I].DeliveredWood=V->CostFor(I);
        V->Residents[I].BuildProgress=1.f; V->Residents[I].Task=EHearthTask::LifeChoosing; V->Residents[I].Route.Reset();
    }
    int32 InstalledWood=V->Residents[0].DeliveredWood+V->Residents[1].DeliveredWood;
    for(int32 I=0;I<3 && InstalledWood>0;++I) { const int32 Used=FMath::Min(InstalledWood,V->WoodStock[I]); V->WoodStock[I]-=Used; InstalledWood-=Used; }
    V->Residents[0].Role=TEXT("商人"); V->Residents[0].RoofMaterial=TEXT("terracotta"); V->Residents[0].PersonalTiles=0; V->Residents[0].Coins=12;
    V->Residents[1].Role=TEXT("陶工"); V->Residents[1].Energy=75.f;
    FHearthSite Kiln; Kiln.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Kiln.Kind=EHearthSiteKind::TileKiln;
    Kiln.Position=FVector(300,300,8); Kiln.Approach=FVector(0,300,8); Kiln.bReachable=true; V->ProductionSites.Add(Kiln);

    auto Meet=[this,V](int32 First,int32 Second)
    {
        V->Residents[First].Actor->SetActorLocation(FVector(-300,0,8)); V->Residents[Second].Actor->SetActorLocation(FVector(0,0,8));
        if(!TestTrue(TEXT("Eligible tile-order parties begin a conversation"),V->BeginConversation(First,Second,TEXT("商量制瓦"),false))) return false;
        auto& Chat=V->Conversations.Last();
        for(int32 Step=0;Step<500 && !Chat.bMet;++Step) { V->AdvanceSimulation(.05f); V->AdvanceSocial(.01f); }
        return TestTrue(TEXT("Tile-order parties meet before negotiating"),Chat.bMet);
    };

    if(!Meet(0,1)) return false;
    V->ResolveSocialTurn(0,0,TEXT("最近窑火怎么样？"),TEXT("test"));
    V->ResolveSocialTurn(1,0,TEXT("窑已经备好了，你需要陶瓦吗？"),TEXT("test"));
    TestTrue(TEXT("Customer with a tile-roof need may commission the potter"),V->AvailableSocialIntents(0).Contains(7));
    V->Residents[0].PersonalTiles=12; TestFalse(TEXT("A complete room roof removes the commission intent"),V->AvailableSocialIntents(0).Contains(7)); V->Residents[0].PersonalTiles=0;
    V->Residents[1].Energy=24.f; TestFalse(TEXT("A potter without work capacity cannot receive a commission"),V->AvailableSocialIntents(0).Contains(7)); V->Residents[1].Energy=75.f;
    auto& CustomerTrust=V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Trust; const float TrustedValue=CustomerTrust;
    CustomerTrust=34.f; TestFalse(TEXT("Customer refuses to commission a distrusted potter"),V->AvailableSocialIntents(0).Contains(7)); CustomerTrust=TrustedValue;
    V->Residents[0].Coins=3; TestFalse(TEXT("Unaffordable fixed price removes the commission intent"),V->AvailableSocialIntents(0).Contains(7)); V->Residents[0].Coins=12;
    TestTrue(TEXT("Customer makes a fixed, executable tile commission"),V->ResolveSocialTurn(0,7,TEXT("四枚钱请你把四份陶土制成六片陶瓦。"),TEXT("test")));
    const int32 CoinsBeforeReject=V->Residents[0].Coins,ClayBeforeReject=V->ClayStock;
    const float TrustBeforeReject=V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Trust;
    TestTrue(TEXT("Potter may reject the commission"),V->ResolveSocialTurn(1,4,TEXT("抱歉，这一炉我暂时接不了。"),TEXT("test")));
    TestEqual(TEXT("Rejected quote reserves no customer money"),V->Residents[0].Coins,CoinsBeforeReject);
    TestEqual(TEXT("Rejected quote consumes no village clay"),V->ClayStock,ClayBeforeReject);
    TestTrue(TEXT("Rejected tile cooperation has a durable order record"),V->TileOrders.ContainsByPredicate([](const FHearthTileOrder& O){return O.Status==TEXT("rejected");}));
    TestTrue(TEXT("Rejection leaves a persistent relationship consequence"),V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Trust<TrustBeforeReject
        && V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Memory.Contains(TEXT("制瓦")));

    if(!Meet(1,0)) return false;
    V->ResolveSocialTurn(1,0,TEXT("我可以替你烧一箱陶瓦。"),TEXT("test"));
    V->ResolveSocialTurn(0,0,TEXT("正好屋顶需要备用瓦。"),TEXT("test"));
    TestTrue(TEXT("Potter may quote a customer with recorded demand"),V->AvailableSocialIntents(1).Contains(8));
    TestTrue(TEXT("Potter makes the fixed tile quote"),V->ResolveSocialTurn(1,8,TEXT("四枚钱，四份陶土烧成六片陶瓦。"),TEXT("test")));
    TestTrue(TEXT("Customer can accept only while the order remains feasible"),V->AvailableSocialIntents(0).Contains(3));
    const int32 OrdersBeforeAccept=V->TileOrders.Num();
    TestTrue(TEXT("Acceptance starts the real tile order"),V->ResolveSocialTurn(0,3,TEXT("好，按这份订单做。"),TEXT("test")));
    TestEqual(TEXT("Acceptance creates exactly one order"),V->TileOrders.Num(),OrdersBeforeAccept+1);
    const auto& Accepted=V->TileOrders.Last();
    TestTrue(TEXT("Accepted conversation is driven by a non-terminal order"),Accepted.Status!=TEXT("proposed") && Accepted.Status!=TEXT("rejected") && Accepted.Status!=TEXT("cancelled") && Accepted.Status!=TEXT("completed"));
    TestEqual(TEXT("Accepted order records the conversation source"),Accepted.ConversationId,V->Conversations.Last().Id);
    TestEqual(TEXT("Accepted order records fixed clay terms"),Accepted.ClayQuantity,4);
    TestEqual(TEXT("Accepted order records fixed tile output"),Accepted.TileQuantity,6);
    TestEqual(TEXT("Accepted order records fixed price"),Accepted.Price,4);

    FHearthWorldImage ActiveImage; FString Error;
    if(!TestTrue(TEXT("Accepted escrow and order survive schema-10 validation"),HearthWorld::Decode(V->ExportWorldState(),ActiveImage,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Reload image keeps one active tile order"),ActiveImage.TileOrders.Num(),2);
    TestEqual(TEXT("Reload image keeps the four-coin escrow"),ActiveImage.TileOrders.Last().Escrow,4);

    V->CloseConversation(V->Conversations.Last(),TEXT("双方确认订单，陶工开始履约。"));
    const int32 KilnIndex=V->ProductionSites.IndexOfByPredicate([](const FHearthSite& S){return S.Kind==EHearthSiteKind::TileKiln;});
    V->Residents[1].Actor->SetActorLocation(FVector(-400,0,8));
    TArray<FVector> KilnRoute;
    TestTrue(TEXT("Order potter has a physical route to the kiln"),V->FindActivityRoute(1,V->ProductionSites[KilnIndex].Approach,KilnRoute));
    TestTrue(TEXT("Kiln action passes current feasibility checks"),V->IsProductionAllowed(1,100+KilnIndex*16+15));
    TestTrue(TEXT("Accepted order can immediately claim its kiln execution unit"),V->StartProduction(1,100+KilnIndex*16+15,TEXT("履行已接受的制瓦订单。"),false));
    for(int32 Step=0;Step<6000 && V->TileOrders.Last().Status!=TEXT("completed");++Step) V->AdvanceSimulation(.05f);
    TestEqual(TEXT("Accepted order reaches delivery and settlement"),V->TileOrders.Last().Status,FString(TEXT("completed")));
    TestEqual(TEXT("Customer owns the six produced tiles"),V->Residents[0].PersonalTiles,6);
    TestEqual(TEXT("Order escrow is empty after delivery"),V->TileOrders.Last().Escrow,0);
    TestEqual(TEXT("Exactly one tile sale is recorded"),V->Transactions.FilterByPredicate([](const FHearthTransaction& T){return T.Kind==TEXT("tile_order");}).Num(),1);
    TestEqual(TEXT("Exactly one tax assessment is tied to tile income"),V->TaxAssessments.FilterByPredicate([V](const FHearthTaxAssessment& A)
    {
        return V->Transactions.ContainsByPredicate([&](const FHearthTransaction& T){return T.Kind==TEXT("tile_order") && T.Id==A.SourceTransactionId;});
    }).Num(),1);
    const int32 CustomerCoinsAfterDelivery=V->Residents[0].Coins;
    TestTrue(TEXT("Repeated settlement is idempotent"),V->SettleTileOrder(V->TileOrders.Last()));
    TestEqual(TEXT("Repeated settlement cannot charge the customer twice"),V->Residents[0].Coins,CustomerCoinsAfterDelivery);

    const int32 BeforeCancel=V->Residents[0].Coins,BeforeCancelClay=V->ClayStock;
    TestTrue(TEXT("A second six-tile batch may be commissioned for the other roof slope"),V->StartTileOrder(0,1,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)));
    const FString CancelId=V->TileOrders.Last().Id;
    V->TileOrders.Last().Remaining=.01f; V->Residents[1].Task=EHearthTask::Settled; V->AdvanceTileOrders(.02f);
    TestEqual(TEXT("Unstarted order times out on the simulation clock"),V->TileOrders.Last().Status,FString(TEXT("cancelled")));
    TestTrue(TEXT("Cancelling an already cancelled order remains idempotent"),V->CancelTileOrder(CancelId));
    TestEqual(TEXT("Cancellation refunds escrow exactly once"),V->Residents[0].Coins,BeforeCancel);
    TestEqual(TEXT("Cancellation consumes no clay"),V->ClayStock,BeforeCancelClay);

    const auto Json=V->GetSocialState(0);
    TestTrue(TEXT("Social state exposes tile orders"),Json.Contains(TEXT("tile_orders")));
    TestTrue(TEXT("Social state exposes material and ownership source"),Json.Contains(TEXT("村庄陶土")) && Json.Contains(TEXT("客户个人陶瓦")));
    return true;
}
#endif
