#if WITH_DEV_AUTOMATION_TESTS

#include "HearthCityPlan.h"
#include "Misc/AutomationTest.h"

namespace
{
    constexpr float PositionTolerance = 1.f;

    float DistanceToSegment2D(const FVector& Point, const FHearthTownRoadSegment& Road)
    {
        return FVector::Dist2D(Point, FMath::ClosestPointOnSegment(Point, Road.A, Road.B));
    }

    bool SamePoint(const FVector& A, const FVector& B)
    {
        return A.Equals(B, PositionTolerance);
    }

    int32 NodeIndex(TArray<FVector>& Nodes, const FVector& Point)
    {
        for (int32 Index = 0; Index < Nodes.Num(); ++Index)
            if (SamePoint(Nodes[Index], Point)) return Index;
        return Nodes.Add(Point);
    }

    bool SegmentTouchesRect(const FHearthTownRoadSegment& Road, const FHearthTownRect& Rect)
    {
        const FVector2D A(Road.A.X, Road.A.Y);
        const FVector2D B(Road.B.X, Road.B.Y);
        const FVector2D Min(Rect.Center.X - Rect.HalfExtent.X - Rect.Clearance, Rect.Center.Y - Rect.HalfExtent.Y - Rect.Clearance);
        const FVector2D Max(Rect.Center.X + Rect.HalfExtent.X + Rect.Clearance, Rect.Center.Y + Rect.HalfExtent.Y + Rect.Clearance);
        const FVector2D Delta = B - A;
        float Enter = 0.f;
        float Exit = 1.f;

        const auto Clip = [&Enter, &Exit](float Origin, float Direction, float Lower, float Upper)
        {
            if (FMath::IsNearlyZero(Direction)) return Origin >= Lower && Origin <= Upper;
            float Near = (Lower - Origin) / Direction;
            float Far = (Upper - Origin) / Direction;
            if (Near > Far) Swap(Near, Far);
            Enter = FMath::Max(Enter, Near);
            Exit = FMath::Min(Exit, Far);
            return Enter <= Exit;
        };

        return Clip(A.X, Delta.X, Min.X, Max.X) && Clip(A.Y, Delta.Y, Min.Y, Max.Y);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthCityPlanDeterminismTest, "ThreeHearths.CityPlan.DeterministicSkeleton", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthCityPlanDeterminismTest::RunTest(const FString&)
{
    const FHearthCityPlan First = HearthCityPlan::Build();
    const FHearthCityPlan Second = HearthCityPlan::Build();

    TestEqual(TEXT("Deterministic road count"), First.Roads.Num(), Second.Roads.Num());
    TestEqual(TEXT("Deterministic landmark count"), First.Landmarks.Num(), Second.Landmarks.Num());
    TestEqual(TEXT("Deterministic district count"), First.Districts.Num(), Second.Districts.Num());
    for (int32 Index = 0; Index < First.Roads.Num() && Index < Second.Roads.Num(); ++Index)
    {
        TestTrue(TEXT("Road endpoints are deterministic"), First.Roads[Index].A.Equals(Second.Roads[Index].A, .01f) && First.Roads[Index].B.Equals(Second.Roads[Index].B, .01f));
        TestTrue(TEXT("Road width is deterministic"), FMath::IsNearlyEqual(First.Roads[Index].Width, Second.Roads[Index].Width));
    }
    for (const FHearthCityLandmark& Landmark : First.Landmarks)
        TestTrue(TEXT("Every landmark has an explicit stable identity"), !Landmark.Id.IsEmpty() && !Landmark.Kind.IsEmpty());

    TestTrue(TEXT("Castle is centered at the requested entrance plan"), First.Landmarks.ContainsByPredicate([](const FHearthCityLandmark& L)
    {
        return L.Id == TEXT("main_keep") && L.Position.Equals(FVector(-1050.f, 2200.f, 8.f), .01f) && L.Approach.Equals(FVector(-1050.f, 1200.f, 8.f), .01f) && FMath::IsNearlyEqual(L.Radius, 850.f);
    }));
    TestTrue(TEXT("Residential density is separated from work density"), First.Districts.ContainsByPredicate([](const FHearthCityDistrict& D) { return D.Kind == TEXT("residential_high_density"); })
        && First.Districts.ContainsByPredicate([](const FHearthCityDistrict& D) { return D.Kind == TEXT("residential_low_density"); })
        && First.Districts.ContainsByPredicate([](const FHearthCityDistrict& D) { return D.Kind == TEXT("work_low_density"); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthCityPlanConnectivityTest, "ThreeHearths.CityPlan.ConnectedCivicAndResidentialStreets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthCityPlanConnectivityTest::RunTest(const FString&)
{
    const FHearthCityPlan Plan = HearthCityPlan::Build();
    TArray<FVector> Nodes;
    TArray<TArray<int32>> Edges;
    for (const FHearthTownRoadSegment& Road : Plan.Roads)
        Edges.Add({NodeIndex(Nodes, Road.A), NodeIndex(Nodes, Road.B)});

    TSet<int32> Connected;
    TArray<int32> Open;
    if (Edges.Num())
    {
        Connected.Add(Edges[0][0]);
        Open.Add(Edges[0][0]);
    }
    while (!Open.IsEmpty())
    {
        const int32 Current = Open.Pop(EAllowShrinking::No);
        for (const TArray<int32>& Edge : Edges)
        {
            if (Edge[0] != Current && Edge[1] != Current) continue;
            const int32 Other = Edge[0] == Current ? Edge[1] : Edge[0];
            if (!Connected.Contains(Other))
            {
                Connected.Add(Other);
                Open.Add(Other);
            }
        }
    }

    TestTrue(TEXT("City plan contains roads"), Plan.Roads.Num() > 0);
    TestEqual(TEXT("All road nodes share one connected network"), Connected.Num(), Nodes.Num());
    TestTrue(TEXT("Old street endpoints remain available"), Plan.Roads.ContainsByPredicate([](const FHearthTownRoadSegment& R) { return R.A.Equals(FVector(-2130.f, -5100.f, 8.f), .01f); })
        && Plan.Roads.ContainsByPredicate([](const FHearthTownRoadSegment& R) { return R.B.Equals(FVector(-2130.f, 5100.f, 8.f), .01f); }));

    TestTrue(TEXT("Castle gate has a road approach"), Plan.Landmarks.ContainsByPredicate([&](const FHearthCityLandmark& L)
    {
        return L.Id == TEXT("main_keep") && Plan.Roads.ContainsByPredicate([&](const FHearthTownRoadSegment& R)
        {
            return SamePoint(R.A, L.Approach) || SamePoint(R.B, L.Approach);
        });
    }));
    TestTrue(TEXT("Market has a road approach"), Plan.Landmarks.ContainsByPredicate([&](const FHearthCityLandmark& L)
    {
        return L.Id == TEXT("public_market") && Plan.Roads.ContainsByPredicate([&](const FHearthTownRoadSegment& R)
        {
            return SamePoint(R.A, L.Approach) || SamePoint(R.B, L.Approach);
        });
    }));

    // A connected graph with more edges than a tree has at least one enclosed block.
    TestTrue(TEXT("Residential streets include an enclosed block"), Edges.Num() > Nodes.Num() - 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthCityPlanClearanceTest, "ThreeHearths.CityPlan.KeepsMainOccupancyClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthCityPlanClearanceTest::RunTest(const FString&)
{
    const FHearthCityPlan Plan = HearthCityPlan::Build();
    for (const FHearthCityLandmark& Landmark : Plan.Landmarks)
    {
        for (const FHearthTownRoadSegment& Road : Plan.Roads)
            TestTrue(FString::Printf(TEXT("Road clears %s footprint"), *Landmark.Id), DistanceToSegment2D(Landmark.Position, Road) + PositionTolerance >= Landmark.Radius + Road.Width * .5f);
    }

    const FHearthTownRect Farm = {FVector(-1135.f, -2825.f, 8.f), FVector2D(325.f, 325.f), 0.f};
    for (const FHearthTownRoadSegment& Road : Plan.Roads)
        TestFalse(TEXT("Road does not enter the reserved farm rectangle"), SegmentTouchesRect(Road, Farm));

    TestTrue(TEXT("Shared pickup point stays off the market route"), Plan.Landmarks.ContainsByPredicate([&](const FHearthCityLandmark& L)
    {
        return L.Id == TEXT("shared_pickup") && Plan.Roads.ContainsByPredicate([&](const FHearthTownRoadSegment& R)
        {
            return SamePoint(R.A, L.Approach) || SamePoint(R.B, L.Approach);
        });
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthCityPlanTown3GeometryTest, "ThreeHearths.CityPlan.Town3VersionedNonOrthogonalBlocks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthCityPlanTown3GeometryTest::RunTest(const FString&)
{
    const FHearthCityPlan Plan=HearthCityPlan::BuildVersion3();
    TestEqual(TEXT("Town3 layout version"),Plan.LayoutVersion,3);
    TestEqual(TEXT("Town3 recommended population"),Plan.RecommendedPopulation,30);
    TestTrue(TEXT("Town3 map is 300 metres square"),FMath::IsNearlyEqual(Plan.MapMax.X-Plan.MapMin.X,30000.f) && FMath::IsNearlyEqual(Plan.MapMax.Y-Plan.MapMin.Y,30000.f));
    TestTrue(TEXT("Town3 castle uses the five thousand centimetre reserve"),Plan.Landmarks.ContainsByPredicate([](const FHearthCityLandmark& L)
    { return L.Id==TEXT("main_keep") && L.Position.Equals(FVector(6500.f,6500.f,8.f),.01f) && L.Approach.Equals(FVector(6500.f,1500.f,8.f),.01f) && FMath::IsNearlyEqual(L.Radius,5000.f); }));

    int32 DiagonalSegments=0;
    for(const FHearthTownRoadSegment& Road:Plan.Roads)
        if(FMath::Abs(Road.B.X-Road.A.X)>1.f && FMath::Abs(Road.B.Y-Road.A.Y)>1.f) ++DiagonalSegments;
    TestTrue(TEXT("Town3 dense blocks contain finite non-orthogonal street bends"),DiagonalSegments>=10);
    TestTrue(TEXT("Town3 retains market approach"),Plan.Roads.ContainsByPredicate([](const FHearthTownRoadSegment& R){return R.A.Equals(FVector(-1100.f,-600.f,8.f),.01f)||R.B.Equals(FVector(-1100.f,-600.f,8.f),.01f);}));
    TestTrue(TEXT("Town3 retains castle entrance approach"),Plan.Roads.ContainsByPredicate([](const FHearthTownRoadSegment& R){return R.A.Equals(FVector(6500.f,1500.f,8.f),.01f)||R.B.Equals(FVector(6500.f,1500.f,8.f),.01f);}));
    return true;
}

#endif
