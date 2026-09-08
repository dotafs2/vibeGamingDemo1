#if WITH_DEV_AUTOMATION_TESTS
#include "HearthResidentDesignChoice.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentDesignChoiceStabilityTest,
    "ThreeHearths.Design.ResidentChoice.Stability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthResidentDesignChoiceStabilityTest::RunTest(const FString&)
{
    FHearthResidentDesignInput Input;
    Input.StableId = TEXT("resident-amelia"); Input.Name = TEXT("Amelia"); Input.Role = TEXT("carpenter");
    Input.Occupation = TEXT("carpenter"); Input.PersistentGoals = TEXT("quiet workshop with a welcoming courtyard");
    Input.HouseholdSize = 2; Input.CloseRelationships = 3; Input.NeighborCount = 2;
    Input.Coins = 60; Input.Planks = 50; Input.Beams = 20; Input.Stone = 30; Input.Tiles = 20;
    Input.SiteSlope = .08f; Input.WorldSeed = 77; Input.Revision = 1;
    const FHearthResidentDesignChoice First = HearthResidentDesignChoice::ChooseLocal(Input);
    const FHearthResidentDesignChoice Again = HearthResidentDesignChoice::ChooseLocal(Input);
    TestEqual(TEXT("Stable identity produces a stable layout"), First.LayoutId, Again.LayoutId);
    TestEqual(TEXT("Stable identity produces a stable seed"), First.Seed, Again.Seed);
    TestEqual(TEXT("Six alternatives are always exposed"), First.Alternatives.Num(), 6);
    TestEqual(TEXT("Compact intent uses family master"), First.Alternatives.FindByPredicate([](const FHearthResidentDesignAlternative& Alt) { return Alt.LayoutId == HearthResidentDesignChoice::CompactCluster; })->Family, FString(TEXT("family_cluster")));
    TestEqual(TEXT("L court intent uses carpenter master"), First.Alternatives.FindByPredicate([](const FHearthResidentDesignAlternative& Alt) { return Alt.LayoutId == HearthResidentDesignChoice::LCourt; })->Family, FString(TEXT("carpenter_court")));
    TestEqual(TEXT("Stepped intent uses merchant master"), First.Alternatives.FindByPredicate([](const FHearthResidentDesignAlternative& Alt) { return Alt.LayoutId == HearthResidentDesignChoice::SteppedWings; })->Family, FString(TEXT("merchant_steps")));
    TestTrue(TEXT("Local choice is feasible"), First.bLocked);
    TestEqual(TEXT("Local fallback source is explicit"), First.Source, FString(TEXT("local_rules")));
    TestTrue(TEXT("Selected reason includes resident needs"), First.Reason.Contains(TEXT("作坊")) || First.Reason.Contains(TEXT("社交")) || First.Reason.Contains(TEXT("安静")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentDesignChoiceNeedsTest,
    "ThreeHearths.Design.ResidentChoice.NeedsAndConstraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthResidentDesignChoiceNeedsTest::RunTest(const FString&)
{
    FHearthResidentDesignInput Family;
    Family.StableId = TEXT("family"); Family.Name = TEXT("Family"); Family.Role = TEXT("farmer");
    Family.PersistentGoals = TEXT("family courtyard for children and neighbors"); Family.HouseholdSize = 4;
    Family.Dependents = 2; Family.CloseRelationships = 3; Family.NeighborCount = 3;
    Family.Coins = 80; Family.Planks = 60; Family.Beams = 30; Family.Stone = 40; Family.Tiles = 30;
    Family.SiteSlope = .04f; Family.WorldSeed = 11;
    FHearthResidentDesignInput Merchant = Family;
    Merchant.StableId = TEXT("merchant"); Merchant.Name = TEXT("Merchant"); Merchant.Role = TEXT("merchant");
    Merchant.Occupation = TEXT("merchant"); Merchant.PersistentGoals = TEXT("storage for goods and workshop");
    Merchant.HouseholdSize = 1; Merchant.Dependents = 0; Merchant.CloseRelationships = 0; Merchant.NeighborCount = 0;
    const FHearthResidentDesignChoice FamilyChoice = HearthResidentDesignChoice::ChooseLocal(Family);
    const FHearthResidentDesignChoice MerchantChoice = HearthResidentDesignChoice::ChooseLocal(Merchant);
    TestTrue(TEXT("Household and social needs affect the family score"), FamilyChoice.LayoutId == HearthResidentDesignChoice::UCourt
        || FamilyChoice.LayoutId == HearthResidentDesignChoice::LCourt);
    TestTrue(TEXT("Occupation and storage goals affect merchant score"), MerchantChoice.LayoutId == HearthResidentDesignChoice::OffsetWorkshop
        || MerchantChoice.LayoutId == HearthResidentDesignChoice::TowerAnnex
        || MerchantChoice.LayoutId == HearthResidentDesignChoice::SteppedWings);

    FHearthResidentDesignInput HouseholdSemantics = Family;
    HouseholdSemantics.StableId = TEXT("household-semantics");
    HouseholdSemantics.PersistentGoals = TEXT("quiet private home");
    HouseholdSemantics.HouseholdSize = 2;
    HouseholdSemantics.Dependents = 2;
    HouseholdSemantics.CloseRelationships = 0;
    HouseholdSemantics.NeighborCount = 0;
    const FHearthResidentDesignChoice TotalTwo = HearthResidentDesignChoice::ChooseLocal(HouseholdSemantics);
    HouseholdSemantics.StableId = TEXT("household-semantics");
    HouseholdSemantics.HouseholdSize = 4;
    HouseholdSemantics.Dependents = 0;
    const FHearthResidentDesignChoice TotalFour = HearthResidentDesignChoice::ChooseLocal(HouseholdSemantics);
    TestTrue(TEXT("Dependents do not get counted twice"), TotalTwo.LayoutId != TotalFour.LayoutId);

    FHearthResidentDesignInput Poor = Merchant;
    Poor.StableId = TEXT("poor"); Poor.Coins = 0; Poor.Planks = 4; Poor.Beams = 0; Poor.Stone = 0; Poor.Tiles = 0;
    const FHearthResidentDesignChoice PoorChoice = HearthResidentDesignChoice::ChooseLocal(Poor);
    TestFalse(TEXT("Unavailable rich alternatives are marked infeasible"), PoorChoice.Alternatives.ContainsByPredicate(
        [](const FHearthResidentDesignAlternative& Alt) { return Alt.LayoutId == HearthResidentDesignChoice::TowerAnnex && Alt.bFeasible; }));
    TestEqual(TEXT("Constraint reason reports a resource deficit"), PoorChoice.Alternatives[0].Reason.Contains(TEXT("coins")), true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentDesignChoiceKimiValidationTest,
    "ThreeHearths.Design.ResidentChoice.StructuredValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthResidentDesignChoiceKimiValidationTest::RunTest(const FString&)
{
    FHearthResidentDesignInput Input;
    Input.StableId = TEXT("kim"); Input.Name = TEXT("Kim"); Input.Role = TEXT("carpenter"); Input.Occupation = TEXT("carpenter");
    Input.PersistentGoals = TEXT("workshop"); Input.Coins = 60; Input.Planks = 60; Input.Beams = 30; Input.Stone = 30; Input.Tiles = 30;
    Input.SiteSlope = .08f; Input.WorldSeed = 42; Input.Revision = 2;
    const uint32 Seed = HearthResidentDesignChoice::StableSeed(Input);
    const FString Valid = FString::Printf(TEXT("{\"layout_id\":4,\"seed\":%u,\"reason\":\"我的作坊目标与库存允许偏置工作间。\"}"), Seed);
    FHearthResidentDesignChoice Parsed; FString Error;
    TestTrue(TEXT("Structured feasible choice is accepted"), HearthResidentDesignChoice::ParseKimiChoice(Valid, Input, Parsed, Error));
    TestEqual(TEXT("Accepted source is Kimi"), Parsed.Source, FString(TEXT("kimi")));
    const FString WrongSeed = TEXT("{\"layout_id\":4,\"seed\":7,\"reason\":\"理由\"}");
    TestFalse(TEXT("A non-replay seed is rejected"), HearthResidentDesignChoice::ParseKimiChoice(WrongSeed, Input, Parsed, Error));
    const FString RichChoice = FString::Printf(TEXT("{\"layout_id\":5,\"seed\":%u,\"reason\":\"想要高塔。\"}"), Seed);
    FHearthResidentDesignInput Poor = Input; Poor.Coins = 0; Poor.Planks = 0; Poor.Beams = 0; Poor.Stone = 0; Poor.Tiles = 0;
    TestFalse(TEXT("A model cannot bypass material constraints"), HearthResidentDesignChoice::ParseKimiChoice(RichChoice, Poor, Parsed, Error));
    Input.Personality = TEXT("reserved but caring");
    Input.HouseholdDescription = TEXT("lives with an elder");
    Input.RelationshipSummary = TEXT("trusts two neighbors");
    const FString Prompt = HearthResidentDesignChoice::BuildKimiPrompt(Input, HearthResidentDesignChoice::RankAlternatives(Input));
    TestTrue(TEXT("Kimi prompt carries personality"), Prompt.Contains(TEXT("reserved but caring")));
    TestTrue(TEXT("Kimi prompt carries household description"), Prompt.Contains(TEXT("lives with an elder")));
    TestTrue(TEXT("Kimi prompt carries relationship summary"), Prompt.Contains(TEXT("trusts two neighbors")));
    TestTrue(TEXT("Kimi prompt describes expansion intents over three masters"), Prompt.Contains(TEXT("三个已制作的视觉母版")));
    return true;
}
#endif
