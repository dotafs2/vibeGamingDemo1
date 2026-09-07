#if WITH_DEV_AUTOMATION_TESTS
#include "HearthBuildingArchetypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthBuildingArchetypesTest, "ThreeHearths.StructurePlan.BuildingArchetypes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthBuildingArchetypesTest::RunTest(const FString&)
{
    TestEqual(TEXT("King recommendation is reserved for keep"), HearthBuildingArchetypes::SelectArchetype(TEXT("general"), TEXT("shelter"), true), FString(TEXT("keep")));
    TestEqual(TEXT("Storage goal recommends warehouse"), HearthBuildingArchetypes::SelectArchetype(TEXT("general"), TEXT("storage"), false), FString(TEXT("warehouse")));
    TestEqual(TEXT("Merchant role recommends shop house"), HearthBuildingArchetypes::SelectArchetype(TEXT("merchant"), TEXT("shelter"), false), FString(TEXT("shop_house")));
    TestEqual(TEXT("Craft role recommends courtyard workshop"), HearthBuildingArchetypes::SelectArchetype(TEXT("carpenter"), TEXT("shelter"), false), FString(TEXT("courtyard_workshop")));
    TestEqual(TEXT("Inn role recommends inn"), HearthBuildingArchetypes::SelectArchetype(TEXT("innkeeper"), TEXT("shelter"), false), FString(TEXT("inn")));
    TestEqual(TEXT("Unclassified resident recommends rowhouse"), HearthBuildingArchetypes::SelectArchetype(TEXT("general"), TEXT("shelter"), false), FString(TEXT("rowhouse")));
    return true;
}
#endif
