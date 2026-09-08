#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradTownLayout.h"
#include "Misc/AutomationTest.h"

namespace
{

    struct TownTestFWorldAabb
    {
        FVector2D Min = FVector2D::ZeroVector;
        FVector2D Max = FVector2D::ZeroVector;
    };

    FVector2D TownTestRotate(const FVector2D& Point, float Degrees)
    {
        const float Radians = FMath::DegreesToRadians(Degrees);
        const float Cosine = FMath::Cos(Radians);
        const float Sine = FMath::Sin(Radians);
        return FVector2D(
            Cosine * Point.X - Sine * Point.Y,
            Sine * Point.X + Cosine * Point.Y);
    }

    FVector2D TownTestToLocal(const HearthAincradTownLayout::FBuilding& Building, const FVector& WorldPoint)
    {
        return TownTestRotate(
            FVector2D(WorldPoint.X - Building.CenterCm.X, WorldPoint.Y - Building.CenterCm.Y),
            -Building.YawDegrees);
    }

    TownTestFWorldAabb TownTestWorldFootprint(const HearthAincradTownLayout::FBuilding& Building)
    {
        const float HalfWidth = Building.FootprintCm.X * 0.5f;
        const float HalfDepth = Building.FootprintCm.Y * 0.5f;
        const FVector2D Corners[] = {
            TownTestRotate(FVector2D(-HalfWidth, -HalfDepth), Building.YawDegrees),
            TownTestRotate(FVector2D(-HalfWidth, HalfDepth), Building.YawDegrees),
            TownTestRotate(FVector2D(HalfWidth, -HalfDepth), Building.YawDegrees),
            TownTestRotate(FVector2D(HalfWidth, HalfDepth), Building.YawDegrees)
        };

        TownTestFWorldAabb Result;
        Result.Min = FVector2D(FLT_MAX, FLT_MAX);
        Result.Max = FVector2D(-FLT_MAX, -FLT_MAX);
        for (const FVector2D& Corner : Corners)
        {
            Result.Min.X = FMath::Min(Result.Min.X, Corner.X + Building.CenterCm.X);
            Result.Min.Y = FMath::Min(Result.Min.Y, Corner.Y + Building.CenterCm.Y);
            Result.Max.X = FMath::Max(Result.Max.X, Corner.X + Building.CenterCm.X);
            Result.Max.Y = FMath::Max(Result.Max.Y, Corner.Y + Building.CenterCm.Y);
        }
        return Result;
    }

    bool TownTestIsFinite(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    bool TownTestOverlaps(const TownTestFWorldAabb& A, const TownTestFWorldAabb& B)
    {
        return FMath::Max(A.Min.X, B.Min.X) < FMath::Min(A.Max.X, B.Max.X)
            && FMath::Max(A.Min.Y, B.Min.Y) < FMath::Min(A.Max.Y, B.Max.Y);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradTownLayoutStableBuildingsTest,
    "ThreeHearths.AincradTownLayout.StableBuildings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradTownLayoutStableBuildingsTest::RunTest(const FString&)
{
    const HearthAincradTownLayout::FPlan Plan = HearthAincradTownLayout::Build();
    const TCHAR* ExpectedIds[] = {
        TEXT("sao_inn_01"), TEXT("sao_smithy_01"), TEXT("sao_carpentry_01"),
        TEXT("sao_residential_01"), TEXT("sao_residential_02"), TEXT("sao_residential_03"),
        TEXT("sao_residential_04"), TEXT("sao_residential_05"), TEXT("sao_residential_06"),
        TEXT("sao_residential_07"), TEXT("sao_residential_08"), TEXT("sao_residential_09")
    };

    TestEqual(TEXT("S0 has twelve fixed buildings"), Plan.Buildings.Num(), 12);
    TestEqual(TEXT("S0 plan ID is stable"), Plan.Id, FString(TEXT("sao_starter_street_s0")));
    TestEqual(TEXT("S0 plan revision is stable"), Plan.Revision, 1);
    TestEqual(TEXT("Main street has stable endpoints"), Plan.MainStreet.Num(), 2);
    TestEqual(TEXT("Main street starts at the requested north end"), Plan.MainStreet[0], FVector(0.f, 470000.f, 0.f));
    TestEqual(TEXT("Main street ends at the requested south end"), Plan.MainStreet[1], FVector(0.f, 455000.f, 0.f));

    TSet<FString> Ids;
    for (int32 Index = 0; Index < Plan.Buildings.Num(); ++Index)
    {
        const HearthAincradTownLayout::FBuilding& Building = Plan.Buildings[Index];
        TestFalse(TEXT("Building IDs are unique"), Ids.Contains(Building.Id));
        Ids.Add(Building.Id);
        TestEqual(TEXT("Building ID matches the fixed S0 list"), Building.Id, FString(ExpectedIds[Index]));
        TestTrue(TEXT("Building center is finite"), TownTestIsFinite(Building.CenterCm));
        TestTrue(TEXT("Building entrance is finite"), TownTestIsFinite(Building.EntranceCm));
        TestTrue(TEXT("Building work point is finite"), TownTestIsFinite(Building.WorkCm));
        TestTrue(TEXT("Building observation point is finite"), TownTestIsFinite(Building.ObserveCm));
        TestTrue(TEXT("Building spawn point is finite"), TownTestIsFinite(Building.SpawnCm));
        TestTrue(TEXT("Building has two or three floors"), Building.Floors >= 2 && Building.Floors <= 3);
        TestTrue(TEXT("HearthAincradTownLayout::Find returns the same building"), HearthAincradTownLayout::Find(Plan, Building.Id) == &Building);
    }

    const HearthAincradTownLayout::FBuilding* Inn = HearthAincradTownLayout::Find(Plan, TEXT("sao_inn_01"));
    const HearthAincradTownLayout::FBuilding* Smithy = HearthAincradTownLayout::Find(Plan, TEXT("sao_smithy_01"));
    const HearthAincradTownLayout::FBuilding* Carpentry = HearthAincradTownLayout::Find(Plan, TEXT("sao_carpentry_01"));
    TestNotNull(TEXT("Inn exists"), Inn);
    TestNotNull(TEXT("Smithy exists"), Smithy);
    TestNotNull(TEXT("Carpentry exists"), Carpentry);
    if (Inn && Smithy && Carpentry)
    {
        TestEqual(TEXT("Inn center is fixed"), Inn->CenterCm, FVector(-1800.f, 467000.f, 0.f));
        TestEqual(TEXT("Smithy center is fixed"), Smithy->CenterCm, FVector(1800.f, 465000.f, 0.f));
        TestEqual(TEXT("Carpentry center is fixed"), Carpentry->CenterCm, FVector(-1800.f, 462000.f, 0.f));
        TestEqual(TEXT("Inn spawn override is fixed"), Inn->SpawnCm, FVector(-400.f, 468700.f, 92.f));
        TestEqual(TEXT("Smithy spawn override is fixed"), Smithy->SpawnCm, FVector(400.f, 466700.f, 92.f));
        TestEqual(TEXT("Carpentry spawn override is fixed"), Carpentry->SpawnCm, FVector(-400.f, 463700.f, 92.f));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradTownLayoutGeometryTest,
    "ThreeHearths.AincradTownLayout.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradTownLayoutGeometryTest::RunTest(const FString&)
{
    const HearthAincradTownLayout::FPlan Plan = HearthAincradTownLayout::Build();
    constexpr float MainStreetHalfWidthCm = 600.f;

    TestTrue(TEXT("Replacement bounds are valid"), Plan.ReplacementBounds.IsValid != 0);
    TestTrue(TEXT("Replacement bounds are finite"), TownTestIsFinite(Plan.ReplacementBounds.Min) && TownTestIsFinite(Plan.ReplacementBounds.Max));
    TestEqual(TEXT("Replacement bounds preserve the northern street limit"), Plan.ReplacementBounds.Max.Y, 470000.0);
    TestEqual(TEXT("Replacement bounds preserve the southern street limit"), Plan.ReplacementBounds.Min.Y, 455000.0);

    for (const HearthAincradTownLayout::FBuilding& Building : Plan.Buildings)
    {
        const TownTestFWorldAabb Aabb = TownTestWorldFootprint(Building);
        TestTrue(TEXT("Footprint is inside replacement bounds"),
            Aabb.Min.X >= Plan.ReplacementBounds.Min.X && Aabb.Max.X <= Plan.ReplacementBounds.Max.X
            && Aabb.Min.Y >= Plan.ReplacementBounds.Min.Y && Aabb.Max.Y <= Plan.ReplacementBounds.Max.Y);
        TestTrue(TEXT("Footprint clears the twelve metre central road"),
            Aabb.Max.X <= -MainStreetHalfWidthCm || Aabb.Min.X >= MainStreetHalfWidthCm);

        const FVector2D EntranceLocal = TownTestToLocal(Building, Building.EntranceCm);
        const FVector2D WorkLocal = TownTestToLocal(Building, Building.WorkCm);
        const FVector2D ObserveLocal = TownTestToLocal(Building, Building.ObserveCm);
        TestTrue(TEXT("Entrance is outside the front wall"), EntranceLocal.Y < -Building.FootprintCm.Y * 0.5f);
        TestTrue(TEXT("Work point is inside the footprint"),
            FMath::Abs(WorkLocal.X) <= Building.FootprintCm.X * 0.5f
            && WorkLocal.Y >= -Building.FootprintCm.Y * 0.5f
            && WorkLocal.Y <= Building.FootprintCm.Y * 0.5f);
        TestTrue(TEXT("Observation point is outside the front wall"), ObserveLocal.Y < -Building.FootprintCm.Y * 0.5f);
    }

    for (int32 AIndex = 0; AIndex < Plan.Buildings.Num(); ++AIndex)
    {
        for (int32 BIndex = AIndex + 1; BIndex < Plan.Buildings.Num(); ++BIndex)
            TestFalse(TEXT("Building footprints do not overlap"), TownTestOverlaps(TownTestWorldFootprint(Plan.Buildings[AIndex]), TownTestWorldFootprint(Plan.Buildings[BIndex])));
    }

    TestTrue(TEXT("Geographic north is converted to UE south for the replacement area"),
        HearthAincradTownLayout::IsReplacementArea(FVector2D(0.f, -467000.f)));
    TestTrue(TEXT("Replacement area includes its geographic edge"),
        HearthAincradTownLayout::IsReplacementArea(FVector2D(0.f, -455000.f)));
    TestFalse(TEXT("Replacement area excludes a point north of the street"),
        HearthAincradTownLayout::IsReplacementArea(FVector2D(0.f, -470001.f)));
    TestFalse(TEXT("Replacement area excludes a point east of the street block"),
        HearthAincradTownLayout::IsReplacementArea(FVector2D(3201.f, -467000.f)));
    TestFalse(TEXT("Unknown building IDs are not found"), HearthAincradTownLayout::Find(Plan, TEXT("missing")) != nullptr);
    return true;
}

#endif
