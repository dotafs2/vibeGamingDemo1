#include "HearthResidentSiting.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentSitingPreferenceTest,
    "ThreeHearths.Residents.SitingPreferences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthResidentSitingPreferenceTest::RunTest(const FString& Parameters)
{
    FHearthResidentSitingInput Craft;
    Craft.Role = TEXT("木匠");
    Craft.Current = FVector(0.f, 0.f, 0.f);
    Craft.Home = Craft.Current;
    Craft.Workpoints = { FVector(0.f, 0.f, 0.f) };
    Craft.Market = FVector(1000.f, 0.f, 0.f);

    FHearthResidentSitingInput Merchant = Craft;
    Merchant.Role = TEXT("商人");
    Merchant.Friends.Empty();

    const FVector Work = FVector(0.f, 0.f, 0.f);
    const FVector Frontage = FVector(1000.f, 0.f, 0.f);
    const FHearthResidentSitingResult CraftAtWork = HearthResidentSiting::Evaluate(Craft, Work);
    const FHearthResidentSitingResult CraftAtFrontage = HearthResidentSiting::Evaluate(Craft, Frontage);
    const FHearthResidentSitingResult MerchantAtWork = HearthResidentSiting::Evaluate(Merchant, Work);
    const FHearthResidentSitingResult MerchantAtFrontage = HearthResidentSiting::Evaluate(Merchant, Frontage);
    TestTrue(TEXT("Craft resident prefers a workpoint"), CraftAtWork.Penalty < CraftAtFrontage.Penalty);
    TestTrue(TEXT("Merchant prefers market frontage"), MerchantAtFrontage.Penalty < MerchantAtWork.Penalty);
    TestTrue(TEXT("Different roles produce different preference ordering"), CraftAtWork.Penalty < MerchantAtWork.Penalty
        && MerchantAtFrontage.Penalty < CraftAtFrontage.Penalty);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentSitingRobustnessTest,
    "ThreeHearths.Residents.SitingRobustness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthResidentSitingRobustnessTest::RunTest(const FString& Parameters)
{
    FHearthResidentSitingInput Input;
    Input.Role = TEXT("农民");
    Input.Personality = TEXT("热心农民 · 喜欢邻居");
    Input.Friends = { FVector(100.f, 0.f, 0.f), FVector(-100.f, 0.f, 0.f), FVector(std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f) };
    Input.Market = FVector::ZeroVector;
    Input.Workpoints = { FVector(std::numeric_limits<float>::infinity(), 0.f, 0.f) };
    const FVector A(0.f, 100.f, 0.f);
    const FVector B(0.f, -100.f, 0.f);
    const FHearthResidentSitingResult First = HearthResidentSiting::Evaluate(Input, A);
    const FHearthResidentSitingResult Repeat = HearthResidentSiting::Evaluate(Input, A);
    const FHearthResidentSitingResult Mirror = HearthResidentSiting::Evaluate(Input, B);
    const FHearthResidentSitingResult Bad = HearthResidentSiting::Evaluate(Input, FVector(std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f));
    TestEqual(TEXT("Repeated evaluation is deterministic"), First.Penalty, Repeat.Penalty);
    TestEqual(TEXT("Symmetric friend geometry is symmetric"), First.Penalty, Mirror.Penalty);
    TestTrue(TEXT("Invalid references do not poison finite output"), FMath::IsFinite(First.Penalty) && First.Penalty >= 0.f && First.Penalty <= 30.f);
    TestEqual(TEXT("Invalid candidate is bounded"), Bad.Penalty, 30.f);
    TestTrue(TEXT("Reason is bounded"), First.Reason.Len() <= 180 && Bad.Reason.Len() <= 180);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentSitingGoalTest,
    "ThreeHearths.Residents.SitingGoals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthResidentSitingGoalTest::RunTest(const FString& Parameters)
{
    FHearthResidentSitingInput Farmer;
    Farmer.Role = TEXT("农民");
    Farmer.Current = FVector(0.f, 0.f, 0.f);
    Farmer.Home = Farmer.Current;
    Farmer.Workpoints = { FVector(1200.f, 0.f, 0.f) };
    const FVector Farm = FVector(1200.f, 0.f, 0.f);
    const FVector Home = FVector(0.f, 0.f, 0.f);
    const FHearthResidentSitingResult FarmerAtFarm = HearthResidentSiting::Evaluate(Farmer, Farm);
    const FHearthResidentSitingResult FarmerAtHome = HearthResidentSiting::Evaluate(Farmer, Home);
    TestTrue(TEXT("Farmers prefer supplied farm workpoints"), FarmerAtFarm.Penalty < FarmerAtHome.Penalty);

    FHearthResidentSitingInput Private = Farmer;
    Private.Role = TEXT("学徒");
    Private.Workpoints.Empty();
    Private.Market = FVector(1000.f, 0.f, 0.f);
    Private.Goal = TEXT("想要安静私密的家");
    const FHearthResidentSitingResult PrivateNear = HearthResidentSiting::Evaluate(Private, FVector(100.f, 0.f, 0.f));
    const FHearthResidentSitingResult PrivatePublic = HearthResidentSiting::Evaluate(Private, FVector(900.f, 0.f, 0.f));
    TestTrue(TEXT("Privacy goal changes public activity preference"), PrivateNear.Penalty < PrivatePublic.Penalty);

    FHearthResidentSitingInput Host = Private;
    Host.Goal = TEXT("招待邻里");
    Host.Friends = { FVector(900.f, 0.f, 0.f) };
    const FHearthResidentSitingResult HostNear = HearthResidentSiting::Evaluate(Host, FVector(900.f, 0.f, 0.f));
    const FHearthResidentSitingResult HostFar = HearthResidentSiting::Evaluate(Host, FVector(100.f, 0.f, 0.f));
    TestTrue(TEXT("Social goal changes friend preference"), HostNear.Penalty < HostFar.Penalty);
    auto FutureOrder = Private;
    FutureOrder.Goal += TEXT("。将来有家人或皇城订单是我的愿望，目前尚未发生。");
    TestEqual(TEXT("Unrealized court-order narrative does not create a current market preference"),HearthResidentSiting::Evaluate(FutureOrder,FVector(100.f,0.f,0.f)).Penalty,PrivateNear.Penalty);
    return true;
}
