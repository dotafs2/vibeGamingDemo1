#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HearthVillage.h"
#include "Engine/World.h"
namespace HearthDecision { bool ParsePlan(FString Text,int32& Plot,FString& Reason); bool ParseLifePlan(FString Text,int32& Action,FString& Reason); }
namespace HearthDecision { bool RequiresBudgetGateway(const FString& Base,const FString& Model); bool ReadBudgetDescriptor(const FString& Text,FString& Token,FString& Ledger); }

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
    int32 Plot=-1; FString Reason;
    TestTrue(TEXT("Accept valid Chinese model decision"),HearthDecision::ParsePlan(TEXT("{\"plot_id\":2,\"reason\":\"我想节省木材。\"}"),Plot,Reason));
    TestEqual(TEXT("Preserve chosen plot"),Plot,2);
    TestEqual(TEXT("Preserve returned motivation"),Reason,FString(TEXT("我想节省木材。")));
    TestTrue(TEXT("Tolerate JSON code fences"),HearthDecision::ParsePlan(TEXT("```json\n{\"plot_id\":0,\"reason\":\"树林安静\"}\n```"),Plot,Reason));
    const TArray<FString> Invalid={TEXT("not json"),TEXT("[]"),TEXT("{\"plot_id\":-1,\"reason\":\"x\"}"),TEXT("{\"plot_id\":10,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0.5,\"reason\":\"x\"}"),TEXT("{\"plot_id\":\"0\",\"reason\":\"x\"}"),TEXT("{\"plot_id\":true,\"reason\":\"x\"}"),TEXT("{\"plot_id\":0,\"reason\":\" \"}"),TEXT("{\"plot_id\":0,\"reason\":4}"),TEXT("{\"plot_id\":0}"),TEXT("{\"plot_id\":0,\"reason\":\"x\",\"wood\":999}")};
    for(int32 I=0;I<Invalid.Num();++I) TestFalse(FString::Printf(TEXT("Reject malformed or invented action %d"),I),HearthDecision::ParsePlan(Invalid[I],Plot,Reason));
    TestFalse(TEXT("Reject oversized rationale"),HearthDecision::ParsePlan(TEXT("{\"plot_id\":0,\"reason\":\"")+FString::ChrN(181,TEXT('x'))+TEXT("\"}"),Plot,Reason));
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
    World->DestroyWorld(false);
    return true;
}
#endif
