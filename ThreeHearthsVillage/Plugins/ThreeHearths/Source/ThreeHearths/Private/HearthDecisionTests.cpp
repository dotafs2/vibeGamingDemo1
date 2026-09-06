#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HearthVillage.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
namespace HearthDecision { bool ParsePlan(FString Text,int32& Plot,int32& HouseStyle,FString& Reason); bool ParseLifePlan(FString Text,int32& Action,FString& Reason); }
namespace HearthDecision { bool RequiresBudgetGateway(const FString& Base,const FString& Model); bool ReadBudgetDescriptor(const FString& Text,FString& Token,FString& Ledger); }
namespace HearthDecision { bool RequestExceededSimulationDeadline(double CurrentSimulationTime,double StartedSimulationTime,float TimeoutSeconds); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthBudgetRoutingTest,"ThreeHearths.Decisions.PersistentBudgetRouting",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthBudgetRoutingTest::RunTest(const FString&)
{
    TestTrue(TEXT("Official Kimi cannot bypass budget"),HearthDecision::RequiresBudgetGateway(TEXT("https://api.moonshot.cn/v1"),TEXT("kimi-k2.6")));
    TestTrue(TEXT("Kimi loopback redirects still require budget"),HearthDecision::RequiresBudgetGateway(TEXT("http://127.0.0.1:18000/v1"),TEXT("kimi-k2.6")));
    TestTrue(TEXT("Unknown paid HTTPS provider cannot bypass budget"),HearthDecision::RequiresBudgetGateway(TEXT("https://other.invalid/v1"),TEXT("other")));
    TestFalse(TEXT("Offline fake remains available without real provider"),HearthDecision::RequiresBudgetGateway(TEXT("http://127.0.0.1:18000/v1"),TEXT("offline-test")));
    FString Token,Ledger;
    const FString Descriptor=TEXT("{\"schema_version\":1,\"base_url\":\"http://127.0.0.1:18766/v1\",\"model\":\"kimi-k2.6\",\"allocation_cap_cny\":95,\"ledger_id\":\"00112233-4455-6677-8899-aabbccddeeff\",\"api_key\":\"")+FString::ChrN(64,TEXT('a'))+TEXT("\",\"policy_sha256\":\"")+FString::ChrN(64,TEXT('b'))+TEXT("\"}");
    TestTrue(TEXT("Accept canonical local descriptor"),HearthDecision::ReadBudgetDescriptor(Descriptor,Token,Ledger));
    TestFalse(TEXT("Reject larger budget"),HearthDecision::ReadBudgetDescriptor(Descriptor.Replace(TEXT(":95,"),TEXT(":100,")),Token,Ledger));
    TestFalse(TEXT("Reject other local endpoint"),HearthDecision::ReadBudgetDescriptor(Descriptor.Replace(TEXT(":18766/"),TEXT(":18000/")),Token,Ledger));
    TestFalse(TEXT("Reject truncated descriptor"),HearthDecision::ReadBudgetDescriptor(Descriptor.Left(100),Token,Ledger));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthDecisionValidationTest,"ThreeHearths.Decisions.ValidateModelOutput",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthDecisionValidationTest::RunTest(const FString&)
{
    int32 Plot=-1,HouseStyle=-1; FString Reason;
    TestTrue(TEXT("Accept valid Chinese model decision"),HearthDecision::ParsePlan(TEXT("{\"plot_id\":2,\"house_style_id\":1,\"reason\":\"我想节省木材。\"}"),Plot,HouseStyle,Reason));
    TestEqual(TEXT("Preserve chosen plot"),Plot,2);
    TestEqual(TEXT("Preserve chosen house style"),HouseStyle,1);
    TestEqual(TEXT("Preserve returned motivation"),Reason,FString(TEXT("我想节省木材。")));
    TestTrue(TEXT("Tolerate JSON code fences"),HearthDecision::ParsePlan(TEXT("```json\n{\"plot_id\":0,\"house_style_id\":2,\"reason\":\"树林安静\"}\n```"),Plot,HouseStyle,Reason));
    const TArray<FString> Invalid={TEXT("not json"),TEXT("[]"),TEXT("{\"plot_id\":-1,\"house_style_id\":0,\"reason\":\"x\"}"),TEXT("{\"plot_id\":10,\"house_style_id\":0,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0.5,\"house_style_id\":0,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0,\"house_style_id\":3,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0,\"house_style_id\":\"1\",\"reason\":\"x\"}"),TEXT("{\"plot_id\":0,\"house_style_id\":1,\"reason\":\" \"}"),TEXT("{\"plot_id\":0,\"house_style_id\":1,\"reason\":4}"),TEXT("{\"plot_id\":0,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0,\"house_style_id\":1,\"reason\":\"x\",\"wood\":999}")};
    for(int32 I=0;I<Invalid.Num();++I) TestFalse(FString::Printf(TEXT("Reject malformed or invented action %d"),I),HearthDecision::ParsePlan(Invalid[I],Plot,HouseStyle,Reason));
    TestFalse(TEXT("Reject oversized rationale"),HearthDecision::ParsePlan(TEXT("{\"plot_id\":0,\"house_style_id\":1,\"reason\":\"")+FString::ChrN(181,TEXT('x'))+TEXT("\"}"),Plot,HouseStyle,Reason));
    TestTrue(TEXT("Accept valid life choice"),HearthDecision::ParseLifePlan(TEXT("{\"action_id\":5,\"reason\":\"我去拜访伯恩。\"}"),Plot,Reason));
    TestEqual(TEXT("Preserve life target"),Plot,5);
    TestFalse(TEXT("Do not interpret home plan as life plan"),HearthDecision::ParseLifePlan(TEXT("{\"plot_id\":0,\"reason\":\"休息\"}"),Plot,Reason));
    TestFalse(TEXT("Reject out-of-range activity"),HearthDecision::ParseLifePlan(TEXT("{\"action_id\":10001,\"reason\":\"收获\"}"),Plot,Reason));
    TestFalse(TEXT("Reject resource mutations"),HearthDecision::ParseLifePlan(TEXT("{\"action_id\":1,\"reason\":\"观察\",\"wood\":99}"),Plot,Reason));
    TestFalse(TEXT("Reject fractional activity"),HearthDecision::ParseLifePlan(TEXT("{\"action_id\":0.5,\"reason\":\"休息\"}"),Plot,Reason));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthParallelCapacityTest,"ThreeHearths.Decisions.OneSlotPerResident",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthParallelCapacityTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Create isolated decision test world"),World)) return false;
    auto* Village=World->SpawnActor<AHearthVillage>();
    if(!TestNotNull(TEXT("Create village"),Village)) { World->DestroyWorld(false); return false; }
    Village->ApiBackend=TEXT("openai_compatible");
    Village->Residents.SetNum(5); Village->PendingDecisions.SetNum(5);
    for(int32 I=0;I<3;++I) Village->PendingDecisions[I].bActive=true;
    TestEqual(TEXT("Capacity follows the number of residents"),Village->DecisionConcurrencyLimit(),5);
    TestFalse(TEXT("A resident cannot have a second request"),Village->HasDecisionCapacity(0));
    TestTrue(TEXT("Resident four is independent of the first three"),Village->HasDecisionCapacity(3));
    TestTrue(TEXT("Resident five is independent of the first three"),Village->HasDecisionCapacity(4));
    Village->PendingDecisions[3].bActive=true; Village->PendingDecisions[4].bActive=true;
    TestEqual(TEXT("Five residents can each own a pending request"),Village->PendingDecisionCount(),5);
    for(int32 I=0;I<5;++I) TestFalse(TEXT("No duplicate resident request"),Village->HasDecisionCapacity(I));
    Village->PendingDecisions[2]=FHearthPendingDecision();
    TestTrue(TEXT("A resident's reply frees only their own slot"),Village->HasDecisionCapacity(2));
    TestFalse(TEXT("Another resident still has their pending request"),Village->HasDecisionCapacity(3));
    Village->Residents.SetNum(11); Village->PendingDecisions.SetNum(11);
    for(int32 I=0;I<11;++I) Village->PendingDecisions[I]=FHearthPendingDecision();
    TestEqual(TEXT("Society concurrency is capped at ten"),Village->DecisionConcurrencyLimit(),10);
    for(int32 I=0;I<10;++I)
    {
        TestTrue(TEXT("Each of ten residents has independent capacity"),Village->HasDecisionCapacity(I));
        Village->PendingDecisions[I].bActive=true;
    }
    TestFalse(TEXT("An eleventh request waits for capacity"),Village->HasDecisionCapacity(10));
    for(auto& Pending:Village->PendingDecisions) Pending=FHearthPendingDecision();
    Village->Residents[0].Task=EHearthTask::Settled;
    Village->Elapsed=1.f; Village->ApiTimeout=30.f;
    Village->PendingDecisions[0].bActive=true;
    Village->PendingDecisions[0].StartedAt=-100000.0;
    Village->PendingDecisions[0].StartedAtSimulation=0.0;
    Village->ConsumeDecision();
    TestTrue(TEXT("Wall time alone cannot expire a simulation-clock request"),Village->PendingDecisions[0].bActive);
    Village->Elapsed=31.1f;
    Village->ConsumeDecision();
    TestTrue(TEXT("Accelerated simulation keeps the real request tracked"),Village->PendingDecisions[0].bActive);
    TestTrue(TEXT("Accelerated simulation releases only gameplay waiting"),Village->PendingDecisions[0].bGameplayReleased);
    TestFalse(TEXT("Simulation deadline alone does not disable the paid API"),Village->bApiDisabledThisRun);
    const EHearthTask TaskAfterFallback=Village->Residents[0].Task;
    Village->PendingDecisions[0].bReturned=true;
    Village->PendingDecisions[0].Choice=0;
    Village->ConsumeDecision();
    TestFalse(TEXT("Real completion finally frees the resident's request slot"),Village->PendingDecisions[0].bActive);
    TestEqual(TEXT("A late choice cannot rewrite the resident's newer task"),Village->Residents[0].Task,TaskAfterFallback);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthDecisionSimulationClockTest,"ThreeHearths.Decisions.CooldownUsesSimulationClock",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthDecisionSimulationClockTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Create isolated timing world"),World)) return false;
    auto* Village=World->SpawnActor<AHearthVillage>();
    if(!TestNotNull(TEXT("Create timing village"),Village)) { World->DestroyWorld(false); return false; }
    Village->Residents.SetNum(1);
    Village->Residents[0].Task=EHearthTask::LifeChoosing;
    Village->Residents[0].SpeechRemaining=4.5f;
    Village->bAutonomousLifeEnabled=false;
    Village->SimulationSpeed=3.f;
    Village->Tick(.2f);
    TestTrue(TEXT("Three-times speed advances the simulation clock three times faster"),FMath::IsNearlyEqual(Village->Elapsed,.6f,.001f));
    TestTrue(TEXT("Social turn timing follows the same simulation clock"),FMath::IsNearlyEqual(Village->Residents[0].SpeechRemaining,3.9f,.001f));
    Village->bSimulationPaused=true;
    Village->Tick(.2f);
    TestTrue(TEXT("Pause freezes the simulation clock"),FMath::IsNearlyEqual(Village->Elapsed,.6f,.001f));
    Village->bSimulationPaused=false;
    Village->Residents[0].NextLifeDecision=6.0;
    Village->Elapsed=0.f;
    TestEqual(TEXT("Disabled autonomy is reported instead of looking scheduler-stalled"),Village->StatusFor(0),FString(TEXT("自主生活已关闭")));
    Village->bAutonomousLifeEnabled=true;
    TestEqual(TEXT("Cooldown starts on simulation clock"),Village->StatusFor(0),FString(TEXT("稍作休息 · 6 秒")));
    Village->Elapsed=3.f;
    TestEqual(TEXT("Advancing simulated time reduces cooldown"),Village->StatusFor(0),FString(TEXT("稍作休息 · 3 秒")));
    Village->Elapsed=6.f;
    TestEqual(TEXT("Cooldown becomes schedulable at simulated deadline"),Village->StatusFor(0),FString(TEXT("等待下一步调度")));
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthDecisionSimulationTimeoutTest,"ThreeHearths.Decisions.RequestTimeoutUsesSimulationClock",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthDecisionSimulationTimeoutTest::RunTest(const FString&)
{
    TestFalse(TEXT("Request remains valid before its simulated deadline"),HearthDecision::RequestExceededSimulationDeadline(29.9,0.0,30.f));
    TestTrue(TEXT("Accelerated simulation expires a request at the same world-time deadline"),HearthDecision::RequestExceededSimulationDeadline(30.1,0.0,30.f));
    TestFalse(TEXT("Pause does not age a request when the simulation clock is unchanged"),HearthDecision::RequestExceededSimulationDeadline(12.0,12.0,30.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthToolTaskBindingTest,"ThreeHearths.Residents.ToolTaskBinding",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthToolTaskBindingTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Create isolated tool test world"),World)) return false;
    auto* Resident=World->SpawnActor<AHearthVillager>();
    if(!TestNotNull(TEXT("Create resident"),Resident)) { World->DestroyWorld(false); return false; }
    auto* ReusableTool=Resident->Tool.Get();
    Resident->EquippedToolId=TEXT("tool_pickaxe"); Resident->SetMotion(EHearthTask::ProductionWork,1.f,11);
    TestEqual(TEXT("Mining equips one pickaxe component"),Resident->EquippedToolId,FString(TEXT("tool_pickaxe")));
    TestTrue(TEXT("Equipped tool is visible"),Resident->Tool->IsVisible());
    TestEqual(TEXT("Borrowed identity loads the matching physical mesh"),Resident->Tool->GetStaticMesh()->GetName(),FString(TEXT("tool_pickaxe")));
    TestEqual(TEXT("Tool remains attached to right hand"),Resident->Tool->GetAttachSocketName(),FName(TEXT("hand_r")));
    Resident->SetMotion(EHearthTask::ProductionTravel,1.f,11);
    TestTrue(TEXT("Travel removes the work tool"),Resident->EquippedToolId.IsEmpty());
    TestFalse(TEXT("Travel hides the reusable component"),Resident->Tool->IsVisible());
    Resident->EquippedToolId=TEXT("tool_hammer"); Resident->SetMotion(EHearthTask::ProductionWork,1.f,5);
    TestEqual(TEXT("Production construction equips its borrowed hammer"),Resident->EquippedToolId,FString(TEXT("tool_hammer")));
    TestEqual(TEXT("Task switch replaces the physical mesh"),Resident->Tool->GetStaticMesh()->GetName(),FString(TEXT("tool_hammer")));
    TestTrue(TEXT("Task changes reuse the same tool component"),Resident->Tool.Get()==ReusableTool);
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthToolOwnershipTest,"ThreeHearths.Residents.ExclusiveToolOwnership",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthToolOwnershipTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Create isolated ownership world"),World)) return false;
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->Residents.SetNum(2);
    for(int32 I=0;I<2;++I) Village->Residents[I].ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TestTrue(TEXT("First resident borrows the single shared hoe"),Village->TryBorrowTool(0,9));
    TestFalse(TEXT("Second resident cannot hold the same physical hoe"),Village->TryBorrowTool(1,9));
    Village->ProductionSites.SetNum(1);
    Village->Residents[1].ProductionSite=0;
    Village->Residents[1].ProductionOp=9;
    Village->Residents[1].Task=EHearthTask::ProductionWork;
    Village->Residents[1].Timer=4.5f;
    Village->Residents[1].WorkDuration=10.f;
    Village->AdvanceProduction(1,0.5f);
    TestEqual(TEXT("A restored work task waits instead of progressing without its tool"),Village->Residents[1].Timer,5.f);
    TestTrue(TEXT("Waiting resident records the shared-tool reason"),Village->Residents[1].LatestEvent.Contains(TEXT("等待归还")));
    Village->ReturnTool(0);
    Village->AdvanceProduction(1,0.5f);
    TestEqual(TEXT("Neighbor automatically borrows it after return"),Village->Residents[1].HeldToolId,FString(TEXT("tool_hoe")));
    TestTrue(TEXT("Previous holder stays empty"),Village->Residents[0].HeldToolId.IsEmpty());
    World->DestroyWorld(false); return true;
}
#endif
