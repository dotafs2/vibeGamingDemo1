#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HearthResidentAgenda.h"

namespace
{
    FHearthResidentAgendaInput BaseInput()
    {
        FHearthResidentAgendaInput I; I.Role=TEXT("陶工"); I.Personality=TEXT("谨慎"); I.Energy=80.f; I.FoodStock=30; I.WoodStock=12; I.ClayStock=6; I.Coins=8; I.TreasuryCoins=100; I.TaxRatePercent=25; I.ResidentCount=3;
        I.AvailableActions={{0,TEXT("回家休息")},{50,TEXT("去村镇中心吃饭")},{115,TEXT("烧制陶瓦")},{3,TEXT("拜访邻居")}}; return I;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentAgendaTest,"ThreeHearths.Life.GroundedResidentAgenda",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthResidentAgendaTest::RunTest(const FString&)
{
    auto Input=BaseInput(); auto A=HearthResidentAgenda::Build(Input); auto B=HearthResidentAgenda::Build(Input);
    TestEqual(TEXT("Agenda is deterministic"),A.Summary,B.Summary);
    TestTrue(TEXT("Facts retain authoritative stock"),A.AuthoritativeFacts.Contains(TEXT("食物 30")) && A.AuthoritativeFacts.Contains(TEXT("黏土 6")));
    TestTrue(TEXT("Aspirations remain separate from facts"),A.PrivateAspirations.Contains(TEXT("陶工")) && !A.AuthoritativeFacts.Contains(TEXT("声望")));
    TestTrue(TEXT("Executable eating action is grounded"),!A.GroundedPriorityActions.Contains(50));
    Input.Hunger=80.f; Input.FoodStock=4; Input.Coins=2; auto Urgent=HearthResidentAgenda::Build(Input);
    TestEqual(TEXT("Urgent survival outranks long aspiration"),Urgent.GroundedPriorityActions[0],50);
    Input.Hunger=10.f; Input.Energy=80.f; Input.ClayStock=0; auto Long=HearthResidentAgenda::Build(Input);
    TestTrue(TEXT("Long role goal can still prefer an available craft action"),Long.GroundedPriorityActions.Contains(115));
    TestTrue(TEXT("Missing capability is singular and bounded"),Long.MissingCapability.Len()>0 && Long.MissingCapability.Len()<80);
    Input.Role=TEXT("农民"); Input.FoodStock=9; auto Farmer=HearthResidentAgenda::Build(Input);
    TestFalse(TEXT("Farmer agenda does not invent a ten-food recipe threshold"),Farmer.MissingInventory.Contains(TEXT("10")));
    Input.Role=TEXT("铁匠"); auto Smith=HearthResidentAgenda::Build(Input);
    TestTrue(TEXT("Actual blacksmith role gets a bounded aspiration"),Smith.PrivateAspirations.Contains(TEXT("铁匠")) && Smith.PrivateAspirations.Contains(TEXT("不虚构")));
    TestTrue(TEXT("Prompt output is bounded"),HearthResidentAgenda::ToPromptText(Long).Len()<=2400);
    return true;
}
#endif
