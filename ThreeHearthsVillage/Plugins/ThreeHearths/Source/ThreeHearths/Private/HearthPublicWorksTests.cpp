#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPublicWallTest, "ThreeHearths.Economy.PublicConstructionRecipe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPublicWallTest::RunTest(const FString&)
{
    FHearthPublicProject Project;
    Project.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    HearthPublicWorks::Populate(Project);
    TestEqual(TEXT("Canonical wall has fifteen stable parts"), Project.Parts.Num(), 15);
    int32 Totals[3] = {0, 0, 0};
    for (int32 I = 0; I < Project.Parts.Num(); ++I)
    {
        const auto& Part = Project.Parts[I];
        TestTrue(TEXT("Part has stable project identity"), Part.Id.StartsWith(Project.Id + TEXT(":")));
        TestTrue(TEXT("Part has a native public wall asset"), Part.Asset.StartsWith(TEXT("public_wall_")));
        for (int32 M = 0; M < 3; ++M) Totals[M] += Part.Required[M];
    }
    TestEqual(TEXT("Stone recipe is thirty six"), Totals[0], 36);
    TestEqual(TEXT("Plank recipe is twenty one"), Totals[1], 21);
    TestEqual(TEXT("Beam recipe is twelve"), Totals[2], 12);
    TestEqual(TEXT("Foundations are stage one"), Project.Parts[0].Stage, 1);
    TestEqual(TEXT("Stone courses are stage two"), Project.Parts[3].Stage, 2);
    TestEqual(TEXT("Walkways are stage three"), Project.Parts[6].Stage, 3);
    TestEqual(TEXT("Parapets are stage four"), Project.Parts.Last().Stage, 4);
    const FString FirstId = Project.Parts[0].Id;
    HearthPublicWorks::Populate(Project);
    TestEqual(TEXT("Populate keeps canonical stable part order"), Project.Parts[0].Id, FirstId);
    return true;
}
#endif
