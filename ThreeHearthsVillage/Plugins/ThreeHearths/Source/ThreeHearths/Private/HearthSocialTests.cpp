#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSocialIntegrationTest,"ThreeHearths.Society.TwoWayPromisesAndPersistence",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSocialIntegrationTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated social world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false;
    // The tight deterministic simulation loop does not pump asynchronous HTTP.
    V->bApiDisabledThisRun=true;
    for(int32 Step=0;Step<16000 && V->CompletedHomes()<3;++Step) V->AdvanceSimulation(.05f);
    V->AdvanceSimulation(.05f);
    if(!TestEqual(TEXT("Settled test residents"),V->CompletedHomes(),3)) return false;
    auto& Files=FPlatformFileManager::Get().GetPlatformFile();
    V->WorldPath=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ThreeHearths/Tests")/FGuid::NewGuid().ToString(EGuidFormats::Digits)/TEXT("social-world.json"));
    Files.CreateDirectoryTree(*FPaths::GetPath(V->WorldPath)); V->WorldLease=MakeShareable(Files.OpenWrite(*(V->WorldPath+TEXT(".lock")))); V->bWorldPersistenceEnabled=true;
    ON_SCOPE_EXIT { V->WorldLease.Reset(); };
    auto Begin=[this,V]()
    {
        V->Residents[0].Actor->SetActorLocation(FVector(-300,0,8)); V->Residents[1].Actor->SetActorLocation(FVector(0,0,8));
        V->Residents[0].SocialNeed=80; V->Residents[1].SocialNeed=80;
        if(!TestTrue(TEXT("Start a real meeting with an idle neighbor"),V->BeginConversation(0,1,TEXT("想聊聊近况"),false))) return false;
        TestFalse(TEXT("A third resident cannot interrupt the same partner"),V->BeginConversation(2,1,TEXT("插话"),false));
        const int32 At=V->Conversations.Num()-1;
        TestTrue(TEXT("No remote conversation lines before meeting"),V->Conversations[At].Lines.IsEmpty());
        for(int32 Step=0;Step<500 && !V->Conversations[At].bMet;++Step) { V->AdvanceSimulation(.05f); V->AdvanceSocial(.01f); }
        return TestTrue(TEXT("Both participants are physically present"),V->Conversations[At].bMet);
    };
    if(!Begin()) return false;
    const FString ChatId=V->Conversations[0].Id;
    TestFalse(TEXT("Wrong speaker cannot take a turn"),V->ResolveSocialTurn(1,0,TEXT("抢先说话"),TEXT("test")));
    TestTrue(TEXT("First resident speaks"),V->ResolveSocialTurn(0,0,TEXT("你好，近来怎么样？"),TEXT("test")));
    TestFalse(TEXT("Speaker must wait for the other reply"),V->ResolveSocialTurn(0,0,TEXT("继续抢话"),TEXT("test")));
    TestTrue(TEXT("Other resident answers independently"),V->ResolveSocialTurn(1,0,TEXT("我过得不错，谢谢你过来。"),TEXT("test")));
    TestTrue(TEXT("Conversation benefits both people's social need"),V->Residents[0].SocialNeed<80 && V->Residents[1].SocialNeed<80);
    TestTrue(TEXT("Meal invitation is a proposal"),V->ResolveSocialTurn(0,1,TEXT("一起去吃饭吧？"),TEXT("test")));
    TestEqual(TEXT("Invitation does not spend food"),V->FoodStock,30);
    TestTrue(TEXT("Recipient can choose acceptance"),V->ResolveSocialTurn(1,3,TEXT("好，我们一起去。"),TEXT("test")));
    TestEqual(TEXT("Two actual meal commitments"),V->Commitments.Num(),2);
    TestEqual(TEXT("Promise is not completed just by saying yes"),V->Commitments[0].Status,FString(TEXT("promised")));
    TestTrue(TEXT("Checkpoint before promises execute"),V->SaveWorld()); const auto Before=V->Residents[1].Bonds;
    if(!TestTrue(TEXT("Resume unfinished dialogue and promises"),V->LoadWorld())) { AddError(V->WorldSaveStatus); return false; }
    TestEqual(TEXT("Same conversation ID"),V->Conversations[0].Id,ChatId); TestEqual(TEXT("No duplicated dialogue lines"),V->Conversations[0].Lines.Num(),4);
    TestEqual(TEXT("No duplicated promises"),V->Commitments.Num(),2);
    TestEqual(TEXT("Relationship counters do not replay"),V->Residents[1].Bonds.FindChecked(V->Residents[0].StableId).Meetings,Before.FindChecked(V->Residents[0].StableId).Meetings);
    TestTrue(TEXT("Acknowledgement starts agreed real activities"),V->ResolveSocialTurn(0,5,TEXT("走吧，边吃边聊。"),TEXT("test")));
    TestTrue(TEXT("Conversation closes once"),V->Conversations[0].bClosed);
    TestEqual(TEXT("Meal promise becomes in-progress task"),V->Commitments[0].Status,FString(TEXT("active")));
    TestTrue(TEXT("Active promised tasks checkpoint"),V->SaveWorld()); TestTrue(TEXT("Active promised tasks restore"),V->LoadWorld());
    for(int32 Step=0;Step<10000 && V->Commitments.ContainsByPredicate([](const auto& P) { return P.Status==TEXT("active"); });++Step) V->AdvanceSimulation(.05f);
    TestEqual(TEXT("Only actual meals consume two food"),V->FoodStock,28); TestEqual(TEXT("Food consumption accounted"),V->Spent[0],2);
    TestEqual(TEXT("Both promises completed by actual eating"),V->Commitments.FilterByPredicate([](const auto& P) { return P.Status==TEXT("fulfilled"); }).Num(),2);
    const float Trust=V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Trust;
    TestTrue(TEXT("Fulfilment raises trust"),Trust>50);
    TestTrue(TEXT("Completed promises save"),V->SaveWorld()); TestTrue(TEXT("Completed promises restore"),V->LoadWorld());
    V->CompleteCommitments(1,true,TEXT("重复回调"));
    TestEqual(TEXT("Completed promise cannot credit trust twice"),V->Residents[0].Bonds.FindChecked(V->Residents[1].StableId).Trust,Trust);
    if(!Begin()) return false;
    V->ResolveSocialTurn(0,0,TEXT("又见面了。"),TEXT("test")); V->ResolveSocialTurn(1,0,TEXT("你好。"),TEXT("test"));
    V->ResolveSocialTurn(0,1,TEXT("再一起吃点东西吗？"),TEXT("test"));
    TestTrue(TEXT("Recipient can decline without being forced into task"),V->ResolveSocialTurn(1,4,TEXT("抱歉，我想先休息。"),TEXT("test")));
    TestEqual(TEXT("Decline creates no extra promise"),V->Commitments.Num(),2); TestEqual(TEXT("Decline creates no food consumption"),V->FoodStock,28);
    TestEqual(TEXT("Declining participant returns to own decisions"),V->Residents[1].Task,EHearthTask::LifeChoosing);
    // A synthetic clear ground grid isolates the real production/deposit state
    // machine here. Actual island routes receive a separate PIE acceptance.
    for(int32 X=-8;X<=8;++X) for(int32 Y=-8;Y<=8;++Y) V->LandGrid.Add(FIntPoint(X,Y));
    FHearthSite Tree; Tree.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Tree.Kind=EHearthSiteKind::Tree; Tree.Position=FVector(-700,300,8); Tree.Approach=FVector(-900,300,8);
    Tree.Radius=150; Tree.Stage=2; Tree.Units=Tree.Capacity=18; Tree.bReachable=true; const int32 TreeSite=V->ProductionSites.Add(Tree);
    if(!Begin()) return false;
    V->ResolveSocialTurn(0,0,TEXT("村里需要些木材。"),TEXT("test")); V->ResolveSocialTurn(1,0,TEXT("有什么我能帮忙的吗？"),TEXT("test"));
    TestTrue(TEXT("Ask for one available real gathering task"),V->ResolveSocialTurn(0,2,TEXT("可以帮忙采集一趟吗？"),TEXT("test")));
    TestTrue(TEXT("Neighbor accepts gathering commitment"),V->ResolveSocialTurn(1,3,TEXT("我来采集并运回来。"),TEXT("test")));
    const int32 WoodBefore=V->AvailableWood();
    V->ResolveSocialTurn(0,5,TEXT("谢谢，回来再聊。"),TEXT("test"));
    TestEqual(TEXT("Accepted help starts production travel"),V->Residents[1].Task,EHearthTask::ProductionTravel);
    TestEqual(TEXT("Accepting help does not generate wood"),V->AvailableWood(),WoodBefore);
    TestTrue(TEXT("Help task checkpoint"),V->SaveWorld()); TestTrue(TEXT("Help task restore"),V->LoadWorld());
    for(int32 Step=0;Step<16000 && V->Commitments.Last().Status==TEXT("active");++Step) V->AdvanceSimulation(.05f);
    TestEqual(TEXT("Real gathering delivers six wood"),V->AvailableWood(),WoodBefore+6);
    TestEqual(TEXT("Source loses exactly six wood"),V->ProductionSites[TreeSite].Units,12);
    TestEqual(TEXT("Help fulfils only after inventory deposit"),V->Commitments.Last().Status,FString(TEXT("fulfilled")));
    if(!Begin()) return false;
    auto& Reply=V->PendingDecisions[0]; Reply.bActive=true; Reply.bReturned=true; Reply.bSocial=true; Reply.bLife=true;
    Reply.ConversationId=V->Residents[0].ConversationId; Reply.AllowedActions={0}; Reply.Choice=0;
    Reply.Reason=TEXT("谢谢你上次帮忙搬回木材。"); Reply.Tokens=42; Reply.Latency=.25; Reply.bHasUsage=true;
    FHearthDecisionRecord Record; Record.Kind=TEXT("social_turn"); Record.Resident=0; Record.Source=TEXT("api");
    const int32 ReceiptHistory=V->DecisionHistory.Add(Record); Reply.HistoryIndex=ReceiptHistory;
    V->ConsumeDecision();
    TestEqual(TEXT("Social history preserves returned token usage"),V->DecisionHistory[ReceiptHistory].Tokens,42);
    TestTrue(TEXT("Social usage is marked available"),V->DecisionHistory[ReceiptHistory].bHasUsage);
    TestEqual(TEXT("Social history preserves response latency"),V->DecisionHistory[ReceiptHistory].Latency,.25);
    TestEqual(TEXT("Valid social reply is recorded complete"),V->DecisionHistory[ReceiptHistory].Status,FString(TEXT("completed")));
    V->ResolveSocialTurn(1,5,TEXT("下次再聊。"),TEXT("test"));
    if(!Begin()) return false;
    V->PendingDecisions[0].bActive=true; V->PendingDecisions[0].bSocial=true; V->PendingDecisions[0].bLife=true;
    V->PendingDecisions[0].OperationId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("Uncertain paid social response can checkpoint"),V->SaveWorld()); TestTrue(TEXT("Uncertain social response restores"),V->LoadWorld());
    TestEqual(TEXT("No paid social request replay"),V->PendingDecisionCount(),0); TestTrue(TEXT("Unknown paid result disables more paid requests"),V->bApiDisabledThisRun);
    FHearthWorldImage Good; FString Error;
    TestTrue(TEXT("Whole social world validates"),HearthWorld::Decode(V->ExportWorldState(),Good,Error));
    auto Broken=Good; Broken.Conversations.Last().SecondId=Broken.People[2].Person.StableId;
    TestFalse(TEXT("Forged partner reference is rejected"),HearthWorld::Decode(HearthWorld::Encode(Broken),Good,Error));
    Broken=Good; Broken.People[0].Person.Bonds.Add(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens),FHearthBond());
    TestFalse(TEXT("Unknown remembered resident is rejected"),HearthWorld::Decode(HearthWorld::Encode(Broken),Good,Error));
    return true;
}
#endif
