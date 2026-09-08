#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HearthVillage.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthGuardReflectionRuntimeTest,
    "ThreeHearths.Decisions.GuardReflectionPreservesDuty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthGuardReflectionRuntimeTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("reflection fixture world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->Residents.SetNum(13); V->PendingDecisions.SetNum(13);
    V->bWorldPersistenceEnabled=false; V->bSimulationPaused=false; V->Elapsed=10000.f;
    auto& R=V->Residents[10]; R.ServiceRoleKey=TEXT("gatekeeper"); R.Task=EHearthTask::LifeActivity;
    R.LifeAction=60; R.ActiveTaskId=TEXT("same-paid-shift"); R.Timer=320.f; R.Coins=2;
    R.ConversationId=TEXT("a-newer-real-visitor"); R.Speech=TEXT("请问你找谁？"); R.Reason=TEXT("原来的心事");
    FHearthServiceDutyRecord Duty; Duty.Resident=10; Duty.TaskId=R.ActiveTaskId; Duty.Status=TEXT("active"); Duty.DutySeconds=280.f;
    V->ServiceDuties.Add(Duty);
    FHearthPendingDecision Pending; Pending.bActive=true; Pending.bLife=true; Pending.bDaydream=true;
    Pending.StartedAtSimulation=0; Pending.HistoryIndex=V->DecisionHistory.Add(FHearthDecisionRecord());
    V->PendingDecisions[10]=Pending;
    V->ConsumeDecision();
    TestFalse(TEXT("accelerated simulation cannot turn reflection into a life fallback"),V->PendingDecisions[10].bGameplayReleased);
    TestTrue(TEXT("guard remains on existing duty while HTTP waits"),R.Task==EHearthTask::LifeActivity && R.LifeAction==60);
    V->PendingDecisions[10].bReturned=true; V->PendingDecisions[10].Choice=0;
    V->PendingDecisions[10].Reason=TEXT("等轮休了，希望能和老朋友聊聊。");
    V->ConsumeDecision();
    TestEqual(TEXT("reflection is recorded"),R.Reason,FString(TEXT("等轮休了，希望能和老朋友聊聊。")));
    TestEqual(TEXT("a newer visitor is not overwritten"),R.ConversationId,FString(TEXT("a-newer-real-visitor")));
    TestEqual(TEXT("private reflection is never broadcast as dialogue"),R.Speech,FString(TEXT("请问你找谁？")));
    TestEqual(TEXT("shift identity unchanged"),R.ActiveTaskId,FString(TEXT("same-paid-shift")));
    TestEqual(TEXT("reflection cannot complete duty time"),V->ServiceDuties[0].DutySeconds,280.f);
    TestEqual(TEXT("reflection cannot change wages"),R.Coins,2);
    TestEqual(TEXT("reflection cannot release duty timer"),R.Timer,320.f);
    TestFalse(TEXT("finished reflection releases its real HTTP slot"),V->IsDecisionPending(10));
    const FString Previous=R.Reason;
    Pending.bReturned=true; Pending.Choice=1; Pending.Reason=TEXT("擅自离岗并获得金钱");
    V->PendingDecisions[10]=Pending; V->ConsumeDecision();
    TestEqual(TEXT("an unoffered action cannot replace the thought"),R.Reason,Previous);
    TestEqual(TEXT("invalid reflection has a failed receipt"),V->DecisionHistory[Pending.HistoryIndex].Status,FString(TEXT("failed")));
    TestFalse(TEXT("offline reflection does not invent an API call"),V->RequestGuardDaydream(10));
    TestTrue(TEXT("offline guard still has the same duty"),R.Task==EHearthTask::LifeActivity && R.ActiveTaskId==TEXT("same-paid-shift"));
    return true;
}
#endif
