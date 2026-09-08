#if WITH_DEV_AUTOMATION_TESTS
#include "HearthPersonalObservation.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPersonalObservationEyeTest,
    "ThreeHearths.Design.PersonalEyeGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthPersonalObservationEyeTest::RunTest(const FString&)
{
    // Independent human-sized fixture: feet at zero, bare head top 170cm.
    // The head reference origin is below the eyes and has its own bone axes.
    const FBox Body(FVector(-35,-20,0),FVector(35,20,170));
    const FTransform Head(FRotator(0,30,0),FVector(0,0,135));
    const FVector Offset=HearthPersonalObservation::EyeInHeadSpace(Body,Head);
    TestTrue(TEXT("eye offset reconstructs the native face position"),Head.TransformPosition(Offset).Equals(FVector(0,6.8,156.4),.001));
    const FTransform BodyWorld(FRotator(0,-90,0),FVector(1100,1200,3500),FVector(1.2));
    const FVector Eye=(Head*BodyWorld).TransformPosition(Offset);
    TestTrue(TEXT("component scale and terrain elevation carry the eyes without a target camera"),Eye.Equals(FVector(1108.16,1200,3687.68),.001));
    const FTransform BentHead(FRotator(20,30,0),FVector(0,0,115));
    TestFalse(TEXT("live head movement changes the eye origin instead of fixing a standing camera"),
        (BentHead*BodyWorld).TransformPosition(Offset).Equals(Eye,.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPersonalObservationContextTest,
    "ThreeHearths.Design.PersonalObservationKnowledge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthPersonalObservationContextTest::RunTest(const FString&)
{
    FHearthResident R;R.StableId=TEXT("resident-a");R.Name=TEXT("林恩");R.Role=TEXT("木匠");
    R.InnerStory=TEXT("我想守住自己的积蓄。");R.DesignGoal=TEXT("希望有一个庭院。");
    R.Coins=7;R.PersonalPlanks=2;R.PersonalTiles=3;
    R.LatestEvent=TEXT("GLOBAL_STOCK_SECRET");R.HouseBlueprint=TEXT("UNSEEN_ROOMS_SECRET");
    R.DesignFeedback=TEXT("OLD_OVERHEAD_EVIDENCE");R.LastVisualSignature=TEXT("old-hash");
    const auto Context=HearthPersonalObservation::Context(R,TEXT("resident-a:fp1:test"),TEXT("view.png"),TEXT("home-a"),false,true);
    TestEqual(TEXT("dispatcher can recognize personal image scope"),Context->GetStringField(TEXT("observation_scope")),FString(TEXT("resident_first_person")));
    TestEqual(TEXT("personal money is retained"),Context->GetNumberField(TEXT("my_coins")),7.0);
    TestEqual(TEXT("only owned plank quantity is retained"),Context->GetNumberField(TEXT("my_owned_planks")),2.0);
    for(const TCHAR* Field:{TEXT("planks"),TEXT("beams"),TEXT("stone"),TEXT("tiles"),TEXT("my_plans"),TEXT("target_center"),
        TEXT("exterior_storeys"),TEXT("native_exterior_features"),TEXT("royal_completed_parts"),TEXT("castle_actual_progress"),TEXT("settlement_guidance")})
        TestFalse(FString::Printf(TEXT("personal context excludes %s"),Field),Context->HasField(Field));
    FString Json;FJsonSerializer::Serialize(Context,TJsonWriterFactory<>::Create(&Json));
    TestFalse(TEXT("unrelated live world event is not appended"),Json.Contains(TEXT("GLOBAL_STOCK_SECRET")));
    TestFalse(TEXT("unseen layout is not appended"),Json.Contains(TEXT("UNSEEN_ROOMS_SECRET")));
    TestFalse(TEXT("old overhead claims are not reused as first-person memory"),Json.Contains(TEXT("OLD_OVERHEAD_EVIDENCE")));
    TestTrue(TEXT("unseen target permits explicit deferral separate from satisfaction"),Context->GetStringField(TEXT("options")).Contains(TEXT("choose 6")));
    TestTrue(TEXT("art charter is a preference rather than observed fact"),Context->GetStringField(TEXT("shared_cultural_preference")).Contains(TEXT("不是眼前事实")));
    R.VisualInspectionId=TEXT("inspection-once"); R.VisualInspectionTargetId=TEXT("home-a");
    TestFalse(TEXT("A walking intention is not an arrival memory"),HearthPersonalObservation::Context(R,TEXT("rev"),TEXT("view.png"),TEXT("home-a"),false,true)->HasField(TEXT("physical_inspection_memory")));
    R.bVisualInspectionArrived=true;
    const auto InspectedContext=HearthPersonalObservation::Context(R,TEXT("resident-a:fp1:test"),TEXT("view.png"),TEXT("home-a"),false,true);
    TestTrue(TEXT("completed arrival is remembered without claiming visibility"),InspectedContext->GetStringField(TEXT("physical_inspection_memory")).Contains(TEXT("不证明它在当前图像中可见")));
    TestFalse(TEXT("arrival at one target cannot identify a different target"),HearthPersonalObservation::Context(R,TEXT("rev"),TEXT("view.png"),TEXT("home-b"),false,true)->HasField(TEXT("physical_inspection_memory")));
    return true;
}
#endif
