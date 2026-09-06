#if WITH_DEV_AUTOMATION_TESTS
#include "HearthTownLayout.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTownLayoutTest, "ThreeHearths.Layout.ContinuousStreetPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthTownLayoutTest::RunTest(const FString&)
{
    FHearthTownLayoutInput Input;
    Input.IslandMin = FVector2D(-1600.f, -1200.f); Input.IslandMax = FVector2D(1600.f, 1200.f);
    Input.Roads = {
        { FVector(-1400.f, -500.f, 8.f), FVector(1400.f, -500.f, 8.f), 180.f },
        { FVector(-900.f, -1000.f, 8.f), FVector(-900.f, 900.f, 8.f), 180.f },
        { FVector(700.f, -900.f, 8.f), FVector(1200.f, -400.f, 8.f), 180.f }
    };
    Input.Markets = { FVector(0.f, -400.f, 8.f) };
    Input.Friends = { FVector(-500.f, -350.f, 8.f) };
    Input.Workpoints = { FVector(800.f, 700.f, 8.f) };
    Input.RequestedHomes = 6; Input.Seed = 17;

    const FHearthTownLayoutPlan Plan = HearthTownLayout::Build(Input);
    TestTrue(TEXT("Planner produces a valid bounded street plan"), HearthTownLayout::IsValid(Plan, Input));
    TestTrue(TEXT("Plan has multiple homes"), Plan.Homes.Num() >= 3);
    TestTrue(TEXT("Plan exposes a shared courtyard for a block"), Plan.bHasCourtyard);
    TestTrue(TEXT("Plan records street endpoints for access"), Plan.StreetNodes.Num() >= 6);

    TSet<int32> RoundedYaw;
    for (const FHearthTownFootprint& Home : Plan.Homes) RoundedYaw.Add(FMath::RoundToInt(Home.Yaw));
    TestTrue(TEXT("Three road directions create distinct orientations"), RoundedYaw.Num() >= 3);
    for (const FHearthTownFootprint& Home : Plan.Homes)
    {
        TestTrue(TEXT("Every door remains near its frontage street"), FVector::Dist2D(Home.Center, Home.Door) >= 90.f && FVector::Dist2D(Home.Center, Home.Door) <= 300.f);
    }

    FHearthTownLayoutInput WithExisting = Input;
    WithExisting.ExistingBuildings.Add({ TEXT("old_house"), FVector(1400.f, 1000.f, 8.f), FVector(1400.f, 850.f, 8.f), FVector2D(120.f, 100.f), 0.f, true });
    WithExisting.RequestedHomes = 6;
    const FHearthTownLayoutPlan Before = HearthTownLayout::Build(WithExisting);
    TestTrue(TEXT("Existing occupancy remains valid while adding a block"), HearthTownLayout::IsValid(Before, WithExisting));
    TestTrue(TEXT("A future expansion slot is planned"), Before.Expansions.Num() > 0);
    TestEqual(TEXT("Existing footprint remains first and unchanged"), Before.Homes[0].Id, FString(TEXT("old_house")));
    TestTrue(TEXT("Expansion references an existing planned home"), Before.Expansions[0].ParentIndex >= 1);
    for (const FHearthTownFootprint& Home : Plan.Homes)
        if (!Home.bExisting)
            TestTrue(TEXT("Candidate ID remains stable when an existing building is inserted"), Before.Homes.ContainsByPredicate([&](const FHearthTownFootprint& Other) { return Other.Id == Home.Id; }));
    for (const FHearthTownExpansion& Expansion : Plan.Expansions)
        TestTrue(TEXT("Expansion ID remains stable when an existing building is inserted"), Before.Expansions.ContainsByPredicate([&](const FHearthTownExpansion& Other) { return Other.Id == Expansion.Id; }));

    const FHearthTownLayoutPlan Repeat = HearthTownLayout::Build(Input);
    TestEqual(TEXT("Same seed gives same home count"), Repeat.Homes.Num(), Plan.Homes.Num());
    for (int32 I = 0; I < Plan.Homes.Num() && I < Repeat.Homes.Num(); ++I)
    {
        TestTrue(TEXT("Same seed gives deterministic centers"), Plan.Homes[I].Center.Equals(Repeat.Homes[I].Center, .01f));
        TestTrue(TEXT("Same seed gives deterministic orientation"), FMath::IsNearlyEqual(Plan.Homes[I].Yaw, Repeat.Homes[I].Yaw));
    }
    return true;
}
#endif
