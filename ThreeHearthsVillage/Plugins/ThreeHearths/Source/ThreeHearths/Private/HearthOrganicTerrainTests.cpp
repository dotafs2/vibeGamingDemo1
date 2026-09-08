#include "HearthOrganicTerrain.h"
#include "Misc/AutomationTest.h"

namespace
{
    HearthOrganicTerrain::FSettings TestSettings()
    {
        HearthOrganicTerrain::FSettings Settings;
        Settings.Seed = 12457;
        Settings.GridQuadsX = 12; Settings.GridQuadsY = 9;
        HearthOrganicTerrain::FFlattenZone Zone;
        Zone.Center = FVector2D(1200.f, -700.f); Zone.HalfSize = FVector2D(300.f, 180.f);
        Zone.YawDegrees = 31.f; Zone.Elevation = 260.f; Zone.Transition = 400.f;
        Settings.FlattenZones.Add(Zone);
        HearthOrganicTerrain::FRoadCenterline Road;
        Road.Width = 150.f; Road.Transition = 350.f;
        Road.Nodes = {{FVector2D(-3000.f, -1000.f), 170.f}, {FVector2D(0.f, 0.f), 220.f}, {FVector2D(3000.f, 1500.f), 290.f}};
        Settings.Roads.Add(Road);
        return Settings;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicTerrainDeterminismTest, "ThreeHearths.OrganicTerrain.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicTerrainDeterminismTest::RunTest(const FString&)
{
    const auto Settings = TestSettings();
    const FVector2D Points[] = {FVector2D(-8400.f, -8400.f), FVector2D(0.f, 0.f), FVector2D(6500.f, 6500.f), FVector2D(21000.f, 21000.f)};
    for (const FVector2D& P : Points)
    {
        const float A = HearthOrganicTerrain::HeightAt(P, Settings);
        const float B = HearthOrganicTerrain::HeightAt(P, Settings);
        TestEqual(TEXT("same seed and inputs replay exactly"), A, B);
        TestTrue(TEXT("height remains finite"), FMath::IsFinite(A));
    }
    HearthOrganicTerrain::FGrid A, B;
    TestTrue(TEXT("grid generation succeeds"), HearthOrganicTerrain::GenerateGrid(Settings, A));
    TestTrue(TEXT("same settings generate the same topology"), HearthOrganicTerrain::GenerateGrid(Settings, B));
    TestEqual(TEXT("vertex count replays"), A.Vertices.Num(), B.Vertices.Num());
    TestEqual(TEXT("index count replays"), A.Indices.Num(), B.Indices.Num());
    for (int32 I = 0; I < A.Vertices.Num(); ++I) TestTrue(TEXT("vertex replay"), A.Vertices[I].Equals(B.Vertices[I], 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicTerrainContinuityTest, "ThreeHearths.OrganicTerrain.ContinuityAndBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicTerrainContinuityTest::RunTest(const FString&)
{
    const auto Settings = TestSettings();
    const auto Bounds = Settings.Bounds;
    const float Epsilon = .1f;
    const float X0 = HearthOrganicTerrain::HeightAt(FVector2D(3200.f - Epsilon, 4100.f), Settings);
    const float X1 = HearthOrganicTerrain::HeightAt(FVector2D(3200.f + Epsilon, 4100.f), Settings);
    const float Y0 = HearthOrganicTerrain::HeightAt(FVector2D(3200.f, 4100.f - Epsilon), Settings);
    const float Y1 = HearthOrganicTerrain::HeightAt(FVector2D(3200.f, 4100.f + Epsilon), Settings);
    TestTrue(TEXT("height is continuous across an x seam"), FMath::Abs(X1 - X0) < 1.f);
    TestTrue(TEXT("height is continuous across a y seam"), FMath::Abs(Y1 - Y0) < 1.f);
    const auto Outside = HearthOrganicTerrain::NormalOrSlopeAt(FVector2D(Bounds.Min.X - 100.f, Bounds.Max.Y + 100.f), Settings);
    TestFalse(TEXT("out of bounds is reported"), Outside.bInsideBounds);
    TestTrue(TEXT("clamped boundary sample is finite"), FMath::IsFinite(Outside.Height));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicTerrainFeaturesTest, "ThreeHearths.OrganicTerrain.FlattenRoadAndSlope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicTerrainFeaturesTest::RunTest(const FString&)
{
    const auto Settings = TestSettings();
    const FVector2D LocalOffset(50.f, -40.f);
    const float Flattened = HearthOrganicTerrain::HeightAt(FVector2D(1200.f, -700.f) + LocalOffset, Settings);
    TestTrue(TEXT("rotated flatten zone reaches its requested elevation"), FMath::Abs(Flattened - 260.f) < 3.f);
    const auto RoadSample = HearthOrganicTerrain::NormalOrSlopeAt(FVector2D(0.f, 0.f), Settings);
    TestTrue(TEXT("graded road remains finite"), FMath::IsFinite(RoadSample.Height) && FMath::IsFinite(RoadSample.SlopeDegrees));
    TestTrue(TEXT("natural nonroad slopes are gentle"), HearthOrganicTerrain::NormalOrSlopeAt(FVector2D(7000.f, -5000.f), Settings).SlopeDegrees < 15.f);
    const auto FlatSample = HearthOrganicTerrain::NormalOrSlopeAt(FVector2D(1200.f, -700.f), Settings);
    TestTrue(TEXT("flat zone has a usable normal"), FlatSample.Normal.Z > .98f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicTerrainGridTest, "ThreeHearths.OrganicTerrain.GridTopology", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicTerrainGridTest::RunTest(const FString&)
{
    const auto Settings = TestSettings();
    HearthOrganicTerrain::FGrid Grid;
    TestTrue(TEXT("grid produces output"), HearthOrganicTerrain::GenerateGrid(Settings, Grid));
    TestEqual(TEXT("grid columns"), Grid.VertexColumns, Settings.GridQuadsX + 1);
    TestEqual(TEXT("grid rows"), Grid.VertexRows, Settings.GridQuadsY + 1);
    TestEqual(TEXT("two triangles per quad"), Grid.Indices.Num(), Settings.GridQuadsX * Settings.GridQuadsY * 6);
    TestEqual(TEXT("attributes match vertices"), Grid.Normals.Num(), Grid.Vertices.Num());
    TestEqual(TEXT("colours match vertices"), Grid.VertexColors.Num(), Grid.Vertices.Num());
    for (const int32 Index : Grid.Indices) TestTrue(TEXT("index is in range"), Grid.Vertices.IsValidIndex(Index));
    auto Invalid = Settings;
    Invalid.Bounds.Max.X = Invalid.Bounds.Min.X;
    HearthOrganicTerrain::FGrid Rejected;
    TestFalse(TEXT("invalid finite boundary is rejected"), HearthOrganicTerrain::GenerateGrid(Invalid, Rejected));
    return true;
}
