#if WITH_DEV_AUTOMATION_TESTS

#include "HearthCityPlan.h"
#include "HearthTownLayout.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPathsTown3CoverageTest, "ThreeHearths.Paths.Town3BoundedGroundCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPathsTown3CoverageTest::RunTest(const FString&)
{
    const FHearthCityPlan City=HearthCityPlan::BuildVersion3();
    const TArray<FHearthTownRoadSegment> Roads=HearthTownLayout::VillageRoads(true,3);
    TestEqual(TEXT("Town3 planner exposes the versioned road graph"),Roads.Num(),City.Roads.Num());
    int32 Diagonal=0;
    for(const FHearthTownRoadSegment& Road:Roads)
    {
        TestTrue(TEXT("Town3 road endpoint stays inside the 300 metre ground"),Road.A.X>=City.MapMin.X && Road.A.X<=City.MapMax.X && Road.A.Y>=City.MapMin.Y && Road.A.Y<=City.MapMax.Y
            && Road.B.X>=City.MapMin.X && Road.B.X<=City.MapMax.X && Road.B.Y>=City.MapMin.Y && Road.B.Y<=City.MapMax.Y);
        if(!FMath::IsNearlyZero(Road.B.X-Road.A.X) && !FMath::IsNearlyZero(Road.B.Y-Road.A.Y)) ++Diagonal;
    }
    TestTrue(TEXT("Town3 path graph contains bent walkable street segments"),Diagonal>=10);
    TestTrue(TEXT("Town3 path graph keeps the castle entrance node"),Roads.ContainsByPredicate([](const FHearthTownRoadSegment& Road)
    { return Road.A.Equals(FVector(6500.f,1500.f,8.f),.01f) || Road.B.Equals(FVector(6500.f,1500.f,8.f),.01f); }));
    return true;
}

#endif
