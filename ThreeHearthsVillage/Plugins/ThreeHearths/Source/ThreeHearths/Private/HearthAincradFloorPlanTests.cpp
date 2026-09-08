#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradFloorPlan.h"
#include "Misc/AutomationTest.h"

namespace
{
    using namespace HearthAincradFloorPlan;

    bool HasRoute(const FPlan& Plan, const TCHAR* A, const TCHAR* B)
    {
        return Plan.Routes.ContainsByPredicate([A, B](const FRoute& Route)
        {
            return (Route.FromRegionId == A && Route.ToRegionId == B)
                || (Route.FromRegionId == B && Route.ToRegionId == A);
        });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradFloorPlanNodesInsideTest, "ThreeHearths.AincradFloorPlan.AllNodesInside", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradFloorPlanNodesInsideTest::RunTest(const FString&)
{
    const FPlan Plan = Build();
    for (const FRegion& Region : Plan.Regions)
        TestTrue(FString::Printf(TEXT("Region %s is inside floor"), *Region.Id), IsInsideFloor(Region.CenterCm));
    for (const FRoute& Route : Plan.Routes)
        for (const FVector2D& Point : Route.WaypointsCm)
            TestTrue(TEXT("Route waypoint is inside floor"), IsInsideFloor(Point));
    TestTrue(TEXT("Town wall center is inside floor"), IsInsideFloor(Plan.TownWallCenterCm));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradFloorPlanTopologyTest, "ThreeHearths.AincradFloorPlan.CardinalTopology", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradFloorPlanTopologyTest::RunTest(const FString&)
{
    const FPlan Plan = Build();
    TestTrue(TEXT("South city has a north gate"), HasRoute(Plan, TEXT("beginnings_plaza"), TEXT("beginnings_northgate")));
    TestTrue(TEXT("West gate is connected to south city"), HasRoute(Plan, TEXT("beginnings_plaza"), TEXT("western_gate")));
    TestTrue(TEXT("West topology reaches forest"), HasRoute(Plan, TEXT("horunka"), TEXT("nepenthes_forest")));
    TestTrue(TEXT("East topology reaches lake"), HasRoute(Plan, TEXT("meadow"), TEXT("lake_region")));
    TestTrue(TEXT("North topology reaches labyrinth"), HasRoute(Plan, TEXT("tolbana"), TEXT("labyrinth")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradFloorPlanReachabilityTest, "ThreeHearths.AincradFloorPlan.NorthernLabyrinthReachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradFloorPlanReachabilityTest::RunTest(const FString&)
{
    const FPlan Plan = Build();
    TSet<FString> Seen;
    TArray<FString> Open;
    Open.Add(TEXT("beginnings_plaza"));
    while (!Open.IsEmpty())
    {
        const FString Current = Open.Pop(EAllowShrinking::No);
        if (Seen.Contains(Current)) continue;
        Seen.Add(Current);
        for (const FRoute& Route : Plan.Routes)
        {
            if (Route.FromRegionId == Current && !Seen.Contains(Route.ToRegionId)) Open.Add(Route.ToRegionId);
            if (Route.ToRegionId == Current && !Seen.Contains(Route.FromRegionId)) Open.Add(Route.FromRegionId);
        }
    }
    TestTrue(TEXT("Northern labyrinth is reachable from southern city"), Seen.Contains(TEXT("labyrinth")));
    TestTrue(TEXT("Tolbana is reachable"), Seen.Contains(TEXT("tolbana")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradFloorPlanPolicyAndTerrainTest, "ThreeHearths.AincradFloorPlan.StableIdsAndTerrainPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradFloorPlanPolicyAndTerrainTest::RunTest(const FString&)
{
    const FPlan Plan = Build();
    TSet<FString> Ids;
    for (const FRegion& Region : Plan.Regions)
    {
        TestFalse(TEXT("No duplicate region IDs"), Ids.Contains(Region.Id));
        Ids.Add(Region.Id);
    }
    TestFalse(TEXT("No superseded royal or tax policy"), Plan.SourcePolicy.Contains(TEXT("royal")) || Plan.SourcePolicy.Contains(TEXT("tax")));
    TestTrue(TEXT("Terrain is finite at representative points"), FMath::IsFinite(HeightAt(FVector2D(0.f, -477000.f))) && FMath::IsFinite(HeightAt(FVector2D(0.f, 420000.f))) && FMath::IsFinite(HeightAt(FVector2D(235000.f, -120000.f))));
    TestTrue(TEXT("Lake is below water level"), HeightAt(FVector2D(235000.f, -120000.f)) <= 0.f);
    TestTrue(TEXT("Southern city is flat"), FMath::IsNearlyZero(HeightAt(FVector2D(0.f, -477000.f))));
    return true;
}

#endif
