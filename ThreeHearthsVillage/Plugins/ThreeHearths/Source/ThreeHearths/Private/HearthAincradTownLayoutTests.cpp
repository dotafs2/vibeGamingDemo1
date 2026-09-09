#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradTownLayout.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

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

    float TownTestPointToAabbDistance(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max)
    {
        const float DeltaX = FMath::Max(FMath::Max(Min.X - Point.X, 0.f), Point.X - Max.X);
        const float DeltaY = FMath::Max(FMath::Max(Min.Y - Point.Y, 0.f), Point.Y - Max.Y);
        return FVector2D(DeltaX, DeltaY).Size();
    }

    float TownTestPointToSegmentDistance(const FVector2D& Point, const FVector2D& Start, const FVector2D& End)
    {
        const FVector2D Segment = End - Start;
        const float Alpha = Segment.SizeSquared() > KINDA_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / Segment.SizeSquared(), 0.f, 1.f) : 0.f;
        return FVector2D::Distance(Point, Start + Segment * Alpha);
    }

    bool TownTestSegmentIntersectsAabb(const FVector2D& Start, const FVector2D& End,
        const FVector2D& Min, const FVector2D& Max)
    {
        const FVector2D Delta = End - Start;
        float Enter = 0.f, Exit = 1.f;
        const auto ClipAxis = [&Enter, &Exit](float Origin, float Direction, float AxisMin, float AxisMax)
        {
            if (FMath::Abs(Direction) <= KINDA_SMALL_NUMBER) return Origin >= AxisMin && Origin <= AxisMax;
            float First = (AxisMin - Origin) / Direction;
            float Last = (AxisMax - Origin) / Direction;
            if (First > Last) Swap(First, Last);
            Enter = FMath::Max(Enter, First);
            Exit = FMath::Min(Exit, Last);
            return Enter <= Exit;
        };
        return ClipAxis(Start.X, Delta.X, Min.X, Max.X)
            && ClipAxis(Start.Y, Delta.Y, Min.Y, Max.Y);
    }

    float TownTestSegmentToAabbDistance(const FVector2D& Start, const FVector2D& End,
        const FVector2D& Min, const FVector2D& Max)
    {
        if (TownTestSegmentIntersectsAabb(Start, End, Min, Max)) return 0.f;
        float Result = FMath::Min(TownTestPointToAabbDistance(Start, Min, Max),
            TownTestPointToAabbDistance(End, Min, Max));
        const FVector2D Corners[] = {
            FVector2D(Min.X, Min.Y), FVector2D(Min.X, Max.Y),
            FVector2D(Max.X, Min.Y), FVector2D(Max.X, Max.Y)
        };
        for (const FVector2D& Corner : Corners)
            Result = FMath::Min(Result, TownTestPointToSegmentDistance(Corner, Start, End));
        return Result;
    }

    bool TownTestReadWorkingTable(const FString& AssetName, FVector2D& OutLocalCm)
    {
        FString Text;
        const FString Path = FPaths::ProjectDir() / TEXT("Art/AincradLevel0/Recipes") / (AssetName + TEXT(".json"));
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Text, *Path)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
        const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
        if (!Root->TryGetArrayField(TEXT("parts"), Parts) || !Parts) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Parts)
        {
            const TSharedPtr<FJsonObject> Part = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (!Part.IsValid() || Part->GetStringField(TEXT("id")) != TEXT("working_table")) continue;
            const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
            if (!Part->TryGetArrayField(TEXT("position_m"), Position) || !Position || Position->Num() != 3) return false;
            double X = 0.0, Y = 0.0;
            if (!(*Position)[0]->TryGetNumber(X) || !(*Position)[1]->TryGetNumber(Y)) return false;
            // Recipe coordinates use metres with geographic north positive; the
            // runtime layout uses centimetres with UE south positive.
            OutLocalCm = FVector2D(static_cast<float>(X * 100.0), static_cast<float>(-Y * 100.0));
            return true;
        }
        return false;
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
        TestTrue(TEXT("Building legacy work point is finite"), TownTestIsFinite(Building.LegacyWorkCm));
        TestTrue(TEXT("Building work point is finite"), TownTestIsFinite(Building.WorkCm));
        if (Building.bHasWorkbench) TestTrue(TEXT("Workbench point is finite"), TownTestIsFinite(Building.WorkbenchCm));
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
        TestTrue(TEXT("Inn entrance is stable"), Inn->EntranceCm.Equals(FVector(-910.f, 467000.f, 92.f), 0.1f));
        TestTrue(TEXT("Smithy entrance is stable"), Smithy->EntranceCm.Equals(FVector(1010.f, 465000.f, 92.f), 0.1f));
        TestTrue(TEXT("Carpentry entrance is stable"), Carpentry->EntranceCm.Equals(FVector(-1010.f, 462000.f, 92.f), 0.1f));
        TestEqual(TEXT("Inn spawn override is fixed"), Inn->SpawnCm, FVector(-400.f, 468700.f, 92.f));
        TestEqual(TEXT("Smithy spawn override is fixed"), Smithy->SpawnCm, FVector(400.f, 466700.f, 92.f));
        TestEqual(TEXT("Carpentry spawn override is fixed"), Carpentry->SpawnCm, FVector(-400.f, 463700.f, 92.f));
        TestTrue(TEXT("Inn has a fixed workbench"), Inn->bHasWorkbench);
        TestTrue(TEXT("Smithy has a fixed workbench"), Smithy->bHasWorkbench);
        TestTrue(TEXT("Carpentry has a fixed workbench"), Carpentry->bHasWorkbench);
        TestTrue(TEXT("Inn legacy work point is stable"), Inn->LegacyWorkCm.Equals(FVector(-1350.f, 467000.f, 92.f), 0.1f));
        TestTrue(TEXT("Smithy legacy work point is stable"), Smithy->LegacyWorkCm.Equals(FVector(1450.f, 465000.f, 92.f), 0.1f));
        TestTrue(TEXT("Carpentry legacy work point is stable"), Carpentry->LegacyWorkCm.Equals(FVector(-1450.f, 462000.f, 92.f), 0.1f));
        TestTrue(TEXT("Inn work point is beside its table"), Inn->WorkCm.Equals(FVector(-1477.85f, 467204.56f, 92.f), 0.2f));
        TestTrue(TEXT("Smithy work point is beside its table"), Smithy->WorkCm.Equals(FVector(1520.68f, 464858.64f, 92.f), 0.2f));
        TestTrue(TEXT("Carpentry work point is beside its table"), Carpentry->WorkCm.Equals(FVector(-1520.68f, 462141.36f, 92.f), 0.2f));

        const struct { const TCHAR* Asset; const HearthAincradTownLayout::FBuilding* Building; } WorkbenchRecipes[] = {
            { TEXT("SM_Inn"), Inn }, { TEXT("SM_Smithy"), Smithy }, { TEXT("SM_Carpentry"), Carpentry }
        };
        for (const auto& Recipe : WorkbenchRecipes)
        {
            FVector2D RecipeLocalCm;
            TestTrue(*FString::Printf(TEXT("%s recipe contains working_table"), Recipe.Asset),
                TownTestReadWorkingTable(Recipe.Asset, RecipeLocalCm));
            const FVector2D LayoutLocal = TownTestToLocal(*Recipe.Building, Recipe.Building->WorkbenchCm);
            TestTrue(*FString::Printf(TEXT("%s workbench local X matches recipe"), Recipe.Asset), FMath::IsNearlyEqual(LayoutLocal.X, RecipeLocalCm.X, 0.1f));
            TestTrue(*FString::Printf(TEXT("%s workbench local Y matches recipe"), Recipe.Asset), FMath::IsNearlyEqual(LayoutLocal.Y, RecipeLocalCm.Y, 0.1f));
            TestEqual(*FString::Printf(TEXT("%s workbench tabletop height is 88 cm"), Recipe.Asset), Recipe.Building->WorkbenchCm.Z, 88.0);
            TestTrue(*FString::Printf(TEXT("%s work point keeps a 155 cm table standoff"), Recipe.Asset),
                FMath::IsNearlyEqual(FVector::Dist2D(Recipe.Building->WorkCm, Recipe.Building->WorkbenchCm), 155.f, 0.1f));
        }

        constexpr float CapsuleRadiusCm = 42.f;
        const struct
        {
            const TCHAR* Asset;
            const HearthAincradTownLayout::FBuilding* Building;
            FVector2D BoundsMinCm;
            FVector2D BoundsMaxCm;
        } WorkbenchBounds[] = {
            { TEXT("SM_Inn/work_table"), Inn, FVector2D(-90.f, -42.5f), FVector2D(90.f, 42.5f) },
            { TEXT("SM_Smithy/smithy_workbench"), Smithy, FVector2D(-90.f, -41.0125f), FVector2D(90.f, 40.9f) },
            { TEXT("SM_Carpentry/carpentry_workbench"), Carpentry, FVector2D(-90.f, -40.9f), FVector2D(90.f, 64.6f) }
        };
        for (const auto& Fixture : WorkbenchBounds)
        {
            const FVector2D WorkLocal = TownTestToLocal(*Fixture.Building, Fixture.Building->WorkCm);
            const FVector2D BenchLocal = TownTestToLocal(*Fixture.Building, Fixture.Building->WorkbenchCm);
            const FVector2D EntranceLocal = TownTestToLocal(*Fixture.Building, Fixture.Building->EntranceCm);
            const FVector2D RelativeWork = WorkLocal - BenchLocal;
            const FVector2D RelativeEntrance = EntranceLocal - BenchLocal;
            TestTrue(*FString::Printf(TEXT("%s work point clears the authored table bounds"), Fixture.Asset),
                TownTestPointToAabbDistance(RelativeWork, Fixture.BoundsMinCm, Fixture.BoundsMaxCm) > CapsuleRadiusCm);
            TestTrue(*FString::Printf(TEXT("%s entrance route clears the authored table bounds"), Fixture.Asset),
                TownTestSegmentToAabbDistance(RelativeEntrance, RelativeWork,
                    Fixture.BoundsMinCm, Fixture.BoundsMaxCm) > CapsuleRadiusCm);
            TestTrue(*FString::Printf(TEXT("%s work capsule stays inside side walls"), Fixture.Asset),
                FMath::Abs(WorkLocal.X) + CapsuleRadiusCm < Fixture.Building->FootprintCm.X * 0.5f);
            TestTrue(*FString::Printf(TEXT("%s work capsule stays inside front and back walls"), Fixture.Asset),
                FMath::Abs(WorkLocal.Y) + CapsuleRadiusCm < Fixture.Building->FootprintCm.Y * 0.5f);
            const float FrontWallY = -Fixture.Building->FootprintCm.Y * 0.5f;
            const float DoorAlpha = (FrontWallY - EntranceLocal.Y) / (WorkLocal.Y - EntranceLocal.Y);
            const float DoorCrossingX = FMath::Lerp(EntranceLocal.X, WorkLocal.X, DoorAlpha);
            TestTrue(*FString::Printf(TEXT("%s capsule route clears the 160 cm doorway"), Fixture.Asset),
                FMath::Abs(DoorCrossingX) + CapsuleRadiusCm < 80.f);
        }
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
