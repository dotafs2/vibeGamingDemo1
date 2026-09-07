#if WITH_DEV_AUTOMATION_TESTS
#include "HearthRoyalWorksPlan.h"
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Misc/PackageName.h"

namespace HearthRoyalWorksPlanTests
{
    constexpr double RoyalTolerance = 0.01;
    constexpr double RoyalGardenClearance = 10.0;

    void Require(TArray<FString>& Issues, bool bCondition, const FString& Code)
    {
        if (!bCondition) Issues.Add(Code);
    }

    const FHearthRoyalModule* Find(const FHearthRoyalWorksPlan& Plan, const TCHAR* Id)
    {
        return Plan.Modules.FindByPredicate([Id](const FHearthRoyalModule& M) { return M.Id == Id; });
    }

    FHearthRoyalModule* FindMutable(FHearthRoyalWorksPlan& Plan, const TCHAR* Id)
    {
        return Plan.Modules.FindByPredicate([Id](const FHearthRoyalModule& M) { return M.Id == Id; });
    }

    // The manifest intentionally uses unrotated, centered, 100cm Engine shapes.
    // Manifest validation rejects nonzero yaw so this AABB cannot hide rotated gaps.
    FBox Bounds(const FHearthRoyalModule& M)
    {
        const FVector HalfExtent = (M.BoundsSizeCm.IsNearlyZero() ? M.Scale * 100.0 : M.BoundsSizeCm) * .5;
        return FBox(M.Offset - HalfExtent, M.Offset + HalfExtent);
    }

    double CatalogRadius(const FString& Id)
    {
        if (Id == TEXT("oak")) return 190.0;
        if (Id == TEXT("birch")) return 135.0;
        if (Id == TEXT("orchard")) return 165.0;
        if (Id == TEXT("cypress")) return 100.0;
        if (Id == TEXT("flowering_shrub")) return 135.0;
        if (Id == TEXT("wildflowers")) return 90.0;
        return 0.0;
    }

    double CrownRadius(const FHearthRoyalModule& M)
    {
        return CatalogRadius(M.PlantId) * FMath::Max(M.Scale.X, M.Scale.Y);
    }

    bool IsTree(const FString& Id)
    {
        return Id == TEXT("oak") || Id == TEXT("birch")
            || Id == TEXT("orchard") || Id == TEXT("cypress");
    }

    double DistanceSquaredXY(const FVector& Point, const FBox& Box)
    {
        const double DX = Point.X - FMath::Clamp(Point.X, Box.Min.X, Box.Max.X);
        const double DY = Point.Y - FMath::Clamp(Point.Y, Box.Min.Y, Box.Max.Y);
        return DX * DX + DY * DY;
    }

    bool ContainsXY(const FBox& Parent, const FBox& Child)
    {
        return Child.Min.X >= Parent.Min.X - RoyalTolerance && Child.Max.X <= Parent.Max.X + RoyalTolerance
            && Child.Min.Y >= Parent.Min.Y - RoyalTolerance && Child.Max.Y <= Parent.Max.Y + RoyalTolerance;
    }

    bool FullBearing(const FHearthRoyalModule& Child, const FHearthRoyalModule& Parent)
    {
        const FBox C = Bounds(Child), P = Bounds(Parent);
        return Parent.Stage < Child.Stage && ContainsXY(P, C)
            && FMath::Abs(C.Min.Z - P.Max.Z) <= RoyalTolerance;
    }

    bool FacadeBearing(const FHearthRoyalModule& Child, const FHearthRoyalModule& Parent)
    {
        const FBox C = Bounds(Child), P = Bounds(Parent);
        return Parent.Stage < Child.Stage
            && C.Min.X >= P.Min.X - RoyalTolerance && C.Max.X <= P.Max.X + RoyalTolerance
            && C.Min.Z >= P.Min.Z - RoyalTolerance && C.Max.Z <= P.Max.Z + RoyalTolerance
            && C.Min.Y < P.Min.Y && C.Max.Y >= P.Min.Y + 1.0 && C.Max.Y <= P.Max.Y;
    }

    bool OverlapsVolume(const FBox& A, const FBox& B)
    {
        return A.Min.X < B.Max.X - RoyalTolerance && B.Min.X < A.Max.X - RoyalTolerance
            && A.Min.Y < B.Max.Y - RoyalTolerance && B.Min.Y < A.Max.Y - RoyalTolerance
            && A.Min.Z < B.Max.Z - RoyalTolerance && B.Min.Z < A.Max.Z - RoyalTolerance;
    }

    bool WallContact(const FBox& A, const FBox& B)
    {
        return A.Min.X <= B.Max.X + RoyalTolerance && B.Min.X <= A.Max.X + RoyalTolerance
            && A.Min.Y <= B.Max.Y + RoyalTolerance && B.Min.Y <= A.Max.Y + RoyalTolerance
            && A.Min.Z < B.Max.Z && B.Min.Z < A.Max.Z;
    }

    TArray<FString> ManifestIssues(const FHearthRoyalWorksPlan& Plan)
    {
        TArray<FString> Issues;
        Require(Issues, Plan.TemplateId == TEXT("royal_keep_garden_v1"), TEXT("template"));
        Require(Issues, Plan.Radius == 850.f, TEXT("radius"));
        Require(Issues, Plan.Modules.Num() > 0 && Plan.Modules.Num() <= 60, TEXT("module_budget"));
        TSet<FString> Ids;
        TSet<int32> Stages;
        FIntVector Total = FIntVector::ZeroValue;
        int32 PreviousStage = 0;
        int32 FirstStageStone = 0;
        for (const auto& M : Plan.Modules)
        {
            Require(Issues, !M.Id.IsEmpty() && !Ids.Contains(M.Id), TEXT("id:") + M.Id);
            Ids.Add(M.Id);
            Require(Issues, M.Stage >= 1 && M.Stage <= 13 && M.Stage >= PreviousStage, TEXT("stage:") + M.Id);
            PreviousStage = M.Stage;
            Stages.Add(M.Stage);
            Require(Issues, M.Materials.X >= 0 && M.Materials.Y >= 0 && M.Materials.Z >= 0
                && (M.Materials.X > 0 || M.Materials.Y > 0 || M.Materials.Z > 0), TEXT("materials:") + M.Id);
            Total += M.Materials;
            if (M.Stage == 1) FirstStageStone += M.Materials.X;
            Require(Issues, FMath::IsFinite(M.Offset.X) && FMath::IsFinite(M.Offset.Y) && FMath::IsFinite(M.Offset.Z)
                && FMath::IsFinite(M.Scale.X) && FMath::IsFinite(M.Scale.Y) && FMath::IsFinite(M.Scale.Z)
                && M.Scale.X > 0 && M.Scale.Y > 0 && M.Scale.Z > 0 && M.Yaw == 0.f, TEXT("transform:") + M.Id);
            Require(Issues, FMath::IsFinite(M.Color.R) && FMath::IsFinite(M.Color.G)
                && FMath::IsFinite(M.Color.B) && M.Color.A == 1.f, TEXT("color:") + M.Id);
            if (M.PlantId.IsEmpty())
            {
                Require(Issues, M.MeshPath == TEXT("/Engine/BasicShapes/Cube")
                    || M.MeshPath == TEXT("/Engine/BasicShapes/Cone"), TEXT("mesh:") + M.Id);
            }
            else
            {
                Require(Issues, CatalogRadius(M.PlantId) > 0 && M.MeshPath.IsEmpty()
                    && M.Stage == 13, TEXT("plant:") + M.Id);
            }
        }
        Require(Issues, Stages.Num() == 13, TEXT("stage_coverage"));
        Require(Issues, Total.X <= 85, TEXT("stone_budget"));
        Require(Issues, Total.Y <= 65, TEXT("plank_budget"));
        Require(Issues, Total.Z <= 55, TEXT("beam_budget"));
        Require(Issues, FirstStageStone <= 10, TEXT("first_stage_budget"));
        return Issues;
    }

    struct FSupportRule
    {
        const TCHAR* Child;
        const TCHAR* Parent;
        bool bFacade = false;
    };

    TArray<FString> GeometryIssues(const FHearthRoyalWorksPlan& Plan)
    {
        TArray<FString> Issues;
        // Complete XY bearing is required for floors, foundations and cone bases.
        // Facade inlays instead need actual penetration and full X/Z attachment.
        const FSupportRule Supports[] = {
            { TEXT("keep_lower_body"), TEXT("keep_foundation") },
            { TEXT("keep_upper_body"), TEXT("keep_lower_body") },
            { TEXT("keep_tower"), TEXT("keep_upper_body") },
            { TEXT("keep_merlon_west_front"), TEXT("keep_upper_body") },
            { TEXT("keep_merlon_west_back"), TEXT("keep_upper_body") },
            { TEXT("keep_merlon_east_front"), TEXT("keep_upper_body") },
            { TEXT("keep_merlon_east_back"), TEXT("keep_upper_body") },
            { TEXT("keep_door"), TEXT("keep_lower_body"), true },
            { TEXT("keep_window_west"), TEXT("keep_upper_body"), true },
            { TEXT("keep_window_east"), TEXT("keep_upper_body"), true },
            { TEXT("keep_roof"), TEXT("keep_tower") },
            { TEXT("gatehouse_west"), TEXT("gate_foundation_west") },
            { TEXT("gatehouse_east"), TEXT("gate_foundation_east") },
            { TEXT("curtain_wall_west"), TEXT("curtain_foundation_west") },
            { TEXT("curtain_wall_east"), TEXT("curtain_foundation_east") },
            { TEXT("curtain_wall_north"), TEXT("curtain_foundation_north") },
            { TEXT("curtain_wall_south_west"), TEXT("curtain_foundation_south_west") },
            { TEXT("curtain_wall_south_east"), TEXT("curtain_foundation_south_east") },
            { TEXT("gatehouse_roof_west"), TEXT("gate_lintel") },
            { TEXT("gatehouse_roof_east"), TEXT("gate_lintel") },
            { TEXT("side_wing_west"), TEXT("side_wing_west_foundation") },
            { TEXT("side_wing_east"), TEXT("side_wing_east_foundation") },
            { TEXT("side_wing_west_roof"), TEXT("side_wing_west") },
            { TEXT("side_wing_east_roof"), TEXT("side_wing_east") },
            { TEXT("side_wing_west_beam"), TEXT("side_wing_west"), true },
            { TEXT("side_wing_east_beam"), TEXT("side_wing_east"), true }
        };
        TSet<FString> CheckedSupports;
        for (const auto& Rule : Supports)
        {
            const auto* Child = Find(Plan, Rule.Child);
            const auto* Parent = Find(Plan, Rule.Parent);
            CheckedSupports.Add(FString(Rule.Child));
            Require(Issues, Child && Parent && (Rule.bFacade
                ? FacadeBearing(*Child, *Parent) : FullBearing(*Child, *Parent)),
                FString(TEXT("bearing:")) + Rule.Child);
        }

        const auto* Lintel = Find(Plan, TEXT("gate_lintel"));
        const auto* West = Find(Plan, TEXT("gatehouse_west"));
        const auto* East = Find(Plan, TEXT("gatehouse_east"));
        CheckedSupports.Add(TEXT("gate_lintel"));
        Require(Issues, Lintel && West && East, TEXT("gate_parts"));
        if (Lintel && West && East)
        {
            const FBox L = Bounds(*Lintel), W = Bounds(*West), E = Bounds(*East);
            const double WestBearing = FMath::Min(L.Max.X, W.Max.X) - FMath::Max(L.Min.X, W.Min.X);
            const double EastBearing = FMath::Min(L.Max.X, E.Max.X) - FMath::Max(L.Min.X, E.Min.X);
            Require(Issues, WestBearing >= 40.0 && EastBearing >= 40.0
                && L.Min.Y >= W.Min.Y && L.Max.Y <= W.Max.Y
                && L.Min.Y >= E.Min.Y && L.Max.Y <= E.Max.Y
                && FMath::Abs(L.Min.Z - W.Max.Z) <= RoyalTolerance
                && FMath::Abs(L.Min.Z - E.Max.Z) <= RoyalTolerance
                && West->Stage < Lintel->Stage && East->Stage < Lintel->Stage, TEXT("lintel_bearing"));
            Require(Issues, E.Min.X - W.Max.X >= 200.0 && L.Min.Z >= 240.0
                && Lintel->Offset.Y < 0, TEXT("gate_clearance"));
        }

        double KeepHeight = 0.0;
        for (const auto& M : Plan.Modules)
        {
            if (!M.PlantId.IsEmpty()) continue;
            const FBox B = Bounds(M);
            Require(Issues, B.Min.Z >= -RoyalTolerance, TEXT("below_ground:") + M.Id);
            const bool bGrounded = M.Id.Contains(TEXT("foundation")) || M.Id == TEXT("keep_threshold");
            if (bGrounded)
                Require(Issues, FMath::Abs(B.Min.Z) <= RoyalTolerance, TEXT("ground_bearing:") + M.Id);
            else
                Require(Issues, CheckedSupports.Contains(M.Id), TEXT("untested_support:") + M.Id);
            for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y)
            {
                const double PX = X ? B.Max.X : B.Min.X, PY = Y ? B.Max.Y : B.Min.Y;
                Require(Issues, PX * PX + PY * PY <= FMath::Square(double(Plan.Radius)),
                    TEXT("mesh_radius:") + M.Id);
            }
            if (M.Id.StartsWith(TEXT("keep_"))) KeepHeight = FMath::Max(KeepHeight, B.Max.Z);
        }
        Require(Issues, KeepHeight >= 850.0 && KeepHeight <= 1100.0, TEXT("keep_height"));
        const auto* Lower = Find(Plan, TEXT("keep_lower_body"));
        const auto* Upper = Find(Plan, TEXT("keep_upper_body"));
        Require(Issues, Lower && Upper, TEXT("solid_keep"));
        if (Lower && Upper)
        {
            Require(Issues, Lower->MeshPath == TEXT("/Engine/BasicShapes/Cube")
                && Lower->Scale.X >= 4.0 && Lower->Scale.Y >= 3.0 && Lower->Scale.Z >= 3.0
                && Upper->MeshPath == TEXT("/Engine/BasicShapes/Cube")
                && Upper->Scale.X >= 3.5 && Upper->Scale.Y >= 3.0 && Upper->Scale.Z >= 3.0
                && Lower->Stage == 2 && Upper->Stage == 3, TEXT("solid_keep"));
        }

        const TCHAR* WallLinks[][2] = {
            { TEXT("curtain_wall_west"), TEXT("curtain_wall_north") },
            { TEXT("curtain_wall_north"), TEXT("curtain_wall_east") },
            { TEXT("curtain_wall_west"), TEXT("curtain_wall_south_west") },
            { TEXT("curtain_wall_south_west"), TEXT("gatehouse_west") },
            { TEXT("curtain_wall_east"), TEXT("curtain_wall_south_east") },
            { TEXT("curtain_wall_south_east"), TEXT("gatehouse_east") },
            { TEXT("side_wing_west"), TEXT("keep_lower_body") },
            { TEXT("side_wing_east"), TEXT("keep_lower_body") }
        };
        for (const auto& Link : WallLinks)
        {
            const auto* A = Find(Plan, Link[0]);
            const auto* B = Find(Plan, Link[1]);
            Require(Issues, A && B && WallContact(Bounds(*A), Bounds(*B)),
                FString(TEXT("wall_contact:")) + Link[0] + TEXT(":") + Link[1]);
        }

        const auto* Door = Find(Plan, TEXT("keep_door"));
        const auto* Foundation = Find(Plan, TEXT("keep_foundation"));
        const auto* Step = Find(Plan, TEXT("keep_threshold"));
        Require(Issues, Door && Foundation && Step, TEXT("entry_parts"));
        if (Door && Foundation && Step)
        {
            const FBox D = Bounds(*Door), F = Bounds(*Foundation), S = Bounds(*Step);
            Require(Issues, Door->Offset.X == 0 && Door->Offset.Y > 0
                && FMath::Abs(S.Max.Y - F.Min.Y) <= RoyalTolerance
                && S.Max.Z > 0 && S.Max.Z <= 20.0 && F.Max.Z >= S.Max.Z && F.Max.Z - S.Max.Z <= 20.0
                && FMath::Abs(D.Min.Z - F.Max.Z) <= RoyalTolerance && ContainsXY(F, D), TEXT("entry_step"));
            // A continuous 180cm-wide, 220cm-high walking prism, allowing at most
            // the 20cm foundation step, reaches the closed decorative front door.
            const FBox Corridor(FVector(-90, -850, 20.1), FVector(90, D.Min.Y, 240.1));
            for (const auto& M : Plan.Modules)
            {
                const bool bClear = M.PlantId.IsEmpty() ? !OverlapsVolume(Bounds(M), Corridor)
                    : DistanceSquaredXY(M.Offset, Corridor) >= FMath::Square(CrownRadius(M) + RoyalGardenClearance);
                Require(Issues, bClear, TEXT("corridor:") + M.Id);
            }
        }

        int32 Trees = 0, Flowers = 0;
        for (int32 I = 0; I < Plan.Modules.Num(); ++I)
        {
            const auto& Plant = Plan.Modules[I];
            if (Plant.PlantId.IsEmpty()) continue;
            const double Radius = CrownRadius(Plant);
            Require(Issues, Plant.Offset.Z == 0 && Plant.Scale.X == Plant.Scale.Y
                && Plant.Scale.Y == Plant.Scale.Z, TEXT("plant_ground_scale:") + Plant.Id);
            if (IsTree(Plant.PlantId))
            {
                ++Trees;
                Require(Issues, Plant.Scale.Z >= 0.6 && Plant.Scale.Z <= 0.8
                    && 600.0 * Plant.Scale.Z < KeepHeight, TEXT("tree_scale:") + Plant.Id);
            }
            if (Plant.PlantId == TEXT("wildflowers") || Plant.PlantId == TEXT("flowering_shrub")) ++Flowers;
            Require(Issues, FVector2D(Plant.Offset.X, Plant.Offset.Y).Size() + Radius <= Plan.Radius,
                TEXT("plant_radius:") + Plant.Id);
            for (int32 J = 0; J < Plan.Modules.Num(); ++J)
            {
                const auto& Other = Plan.Modules[J];
                if (Other.PlantId.IsEmpty())
                {
                    Require(Issues, DistanceSquaredXY(Plant.Offset, Bounds(Other))
                        >= FMath::Square(Radius + RoyalGardenClearance), TEXT("plant_structure:") + Plant.Id);
                }
                else if (J > I)
                {
                    const FVector2D Delta(Plant.Offset.X - Other.Offset.X, Plant.Offset.Y - Other.Offset.Y);
                    Require(Issues, Delta.SizeSquared() >= FMath::Square(Radius + CrownRadius(Other) + RoyalGardenClearance),
                        TEXT("plant_crowns:") + Plant.Id);
                }
            }
        }
        Require(Issues, Trees > 0 && Flowers > 0, TEXT("garden_variety"));
        return Issues;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalWorksPlanTest,
    "ThreeHearths.RoyalWorksPlan.ManifestInvariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthRoyalWorksPlanTest::RunTest(const FString&)
{
    using namespace HearthRoyalWorksPlanTests;
    const auto Plan = HearthRoyalWorksPlan::Build();
    for (const auto& Issue : ManifestIssues(Plan)) AddError(Issue);
    for (const auto& Issue : GeometryIssues(Plan)) AddError(Issue);
    const auto Again = HearthRoyalWorksPlan::Build();
    TestEqual(TEXT("Repeated builds keep the module count"), Plan.Modules.Num(), Again.Modules.Num());
    for (int32 I = 0; I < FMath::Min(Plan.Modules.Num(), Again.Modules.Num()); ++I)
    {
        const auto& A = Plan.Modules[I];
        const auto& B = Again.Modules[I];
        TestTrue(TEXT("Pure builds preserve IDs, geometry, stage and recipes"),
            A.Id == B.Id && A.MeshPath == B.MeshPath && A.PlantId == B.PlantId
            && A.Offset == B.Offset && A.Scale == B.Scale && A.Yaw == B.Yaw
            && A.Stage == B.Stage && A.Materials == B.Materials && A.Color == B.Color);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalWorksPlanRegressionTest,
    "ThreeHearths.RoyalWorksPlan.RejectsBrokenGeometryAndBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthRoyalWorksPlanRegressionTest::RunTest(const FString&)
{
    using namespace HearthRoyalWorksPlanTests;
    const auto Plan = HearthRoyalWorksPlan::Build();
    // Each copy introduces one real failure; unchanged Z cannot mask missing XY bearing.
    struct FGeometryMutation
    {
        const TCHAR* Id;
        FVector NewScale;
        FVector Shift;
        const TCHAR* ExpectedIssue;
    };
    const FGeometryMutation Mutations[] = {
        { TEXT("keep_foundation"), FVector(2.25, 1.80, .20), FVector::ZeroVector, TEXT("bearing:keep_lower_body") },
        { TEXT("curtain_foundation_west"), FVector(.40, 3.10, .20), FVector::ZeroVector, TEXT("bearing:curtain_wall_west") },
        { TEXT("curtain_wall_north"), FVector(5.20, .30, 2.20), FVector::ZeroVector, TEXT("wall_contact:curtain_wall_west:curtain_wall_north") },
        { TEXT("gate_lintel"), FVector(1.65, 1.60, .40), FVector::ZeroVector, TEXT("lintel_bearing") },
        { TEXT("gatehouse_west"), FVector(1.4, 1.6, 3.2), FVector(180, 0, 0), TEXT("corridor:gatehouse_west") },
        { TEXT("side_wing_west_beam"), FVector(2.60, .12, .24), FVector(0, -18, 0), TEXT("bearing:side_wing_west_beam") },
        { TEXT("keep_upper_body"), FVector(4.0, 3.2, 3.2), FVector(300, 0, 0), TEXT("bearing:keep_upper_body") },
        { TEXT("keep_roof"), FVector(1.8, 1.8, 1.6), FVector(0, 0, 20), TEXT("bearing:keep_roof") },
        { TEXT("gate_foundation_west"), FVector(1.6, 1.8, .20), FVector(800, 0, 0), TEXT("mesh_radius:gate_foundation_west") },
        { TEXT("court_oak"), FVector(.65), FVector(0, 0, 45), TEXT("plant_ground_scale:court_oak") },
        { TEXT("court_birch"), FVector(1.75), FVector::ZeroVector, TEXT("tree_scale:court_birch") },
        { TEXT("court_oak"), FVector(.65), FVector(-150, 0, 0), TEXT("plant_structure:court_oak") },
        { TEXT("court_wildflowers_west"), FVector(.6), FVector(320, -100, 0), TEXT("corridor:court_wildflowers_west") }
    };
    for (const auto& Mutation : Mutations)
    {
        auto Broken = Plan;
        auto* M = FindMutable(Broken, Mutation.Id);
        if (!TestNotNull(Mutation.Id, M)) continue;
        M->Scale = Mutation.NewScale;
        M->Offset += Mutation.Shift;
        TestTrue(Mutation.ExpectedIssue, GeometryIssues(Broken).Contains(FString(Mutation.ExpectedIssue)));
    }
    auto SameStage = Plan;
    if (auto* M = FindMutable(SameStage, TEXT("keep_roof")))
    {
        M->Stage = 4;
        TestTrue(TEXT("Parallel roof work requires an earlier support stage"),
            GeometryIssues(SameStage).Contains(FString(TEXT("bearing:keep_roof"))));
    }
    auto TooShort = Plan;
    for (auto& M : TooShort.Modules)
    {
        if (!M.Id.StartsWith(TEXT("keep_"))) continue;
        M.Offset.Z *= .5;
        M.Scale.Z *= .5;
    }
    TestTrue(TEXT("A house-height keep is rejected"),
        GeometryIssues(TooShort).Contains(FString(TEXT("keep_height"))));
    const TCHAR* BudgetIssues[] = { TEXT("stone_budget"), TEXT("plank_budget"), TEXT("beam_budget") };
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        auto Broken = Plan;
        if (auto* M = FindMutable(Broken, TEXT("keep_lower_body")))
        {
            M->Materials[Axis] += 100;
            TestTrue(BudgetIssues[Axis], ManifestIssues(Broken).Contains(FString(BudgetIssues[Axis])));
        }
    }
    auto Free = Plan;
    if (auto* M = FindMutable(Free, TEXT("court_oak")))
    {
        M->Materials = FIntVector::ZeroValue;
        TestTrue(TEXT("Plant preparation cannot be free"),
            ManifestIssues(Free).Contains(FString(TEXT("materials:court_oak"))));
    }
    auto Negative = Plan;
    if (auto* M = FindMutable(Negative, TEXT("keep_lower_body")))
    {
        M->Materials.X = -1;
        TestTrue(TEXT("Negative material quantities are rejected"),
            ManifestIssues(Negative).Contains(FString(TEXT("materials:keep_lower_body"))));
    }
    return true;
}

namespace HearthCastleNativeAudit
{
    // CPU LOD triangles are the imported render geometry. Do not use the
    // handwritten manifest box or collision proxy to certify stairs/roof holes.
    // The import-basis reflection has negative determinant: keep this ray test
    // two-sided (signed D), with no normal/backface rejection. Vertex heights
    // and barycentric inclusion are independent of the resulting winding.
    bool SurfaceAt(const FHearthRoyalModule& Module, UStaticMesh* Mesh,
        const FVector2D& XY, double MinZ, double MaxZ, double* OutZ = nullptr)
    {
        if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0) return false;
        const FTransform Transform = HearthRoyalWorksPlan::MeshTransform(Module, Mesh->GetBounds().GetBox(), Module.Offset);
        const FBox Bounds = Mesh->GetBounds().GetBox().TransformBy(Transform);
        if (XY.X < Bounds.Min.X || XY.X > Bounds.Max.X || XY.Y < Bounds.Min.Y || XY.Y > Bounds.Max.Y
            || MaxZ < Bounds.Min.Z || MinZ > Bounds.Max.Z) return false;
        const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
        const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
        const auto& Positions = LOD.VertexBuffers.PositionVertexBuffer;
        bool bHit = false;
        double Highest = MinZ;
        for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
        {
            const FVector A = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I])));
            const FVector B = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I+1])));
            const FVector C = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I+2])));
            const double D = (B.Y-C.Y)*(A.X-C.X) + (C.X-B.X)*(A.Y-C.Y);
            if (FMath::Abs(D) < 1.e-8) continue;
            const double U = ((B.Y-C.Y)*(XY.X-C.X) + (C.X-B.X)*(XY.Y-C.Y))/D;
            const double V = ((C.Y-A.Y)*(XY.X-C.X) + (A.X-C.X)*(XY.Y-C.Y))/D;
            if (U < -1.e-6 || V < -1.e-6 || U+V > 1.000001) continue;
            const double Z = U*A.Z + V*B.Z + (1-U-V)*C.Z;
            if (Z >= MinZ && Z <= MaxZ) { bHit = true; Highest = FMath::Max(Highest,Z); }
        }
        if (bHit && OutZ) *OutZ = Highest;
        return bHit;
    }

    // Failure-only instrumentation: preserve the same two-sided triangle
    // predicate, but report every distinct height on the complete vertical ray.
    // Reflection probes diagnose import-axis changes; they never satisfy tests.
    FString StairSampleDiagnostic(const FHearthRoyalModule& Module, UStaticMesh* Mesh,
        const FVector2D& XY, double ExpectedZ)
    {
        if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
            return TEXT("CastleStairDiagnostic: missing CPU LOD0");
        const FBox LocalBounds = Mesh->GetBounds().GetBox();
        const FTransform Transform = HearthRoyalWorksPlan::MeshTransform(Module, LocalBounds, Module.Offset);
        const auto& LOD = Mesh->GetRenderData()->LODResources[0];
        const auto& Positions = LOD.VertexBuffers.PositionVertexBuffer;
        const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
        FBox VertexBounds(ForceInit);
        for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
            VertexBounds += FVector(Positions.VertexPosition(I));
        struct FHit { double Z; double NormalZ; int32 Triangle; };
        TArray<FHit> Hits;
        for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
        {
            const FVector A = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I])));
            const FVector B = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I+1])));
            const FVector C = Transform.TransformPosition(FVector(Positions.VertexPosition(Indices[I+2])));
            const double D = (B.Y-C.Y)*(A.X-C.X) + (C.X-B.X)*(A.Y-C.Y);
            if (FMath::Abs(D) < 1.e-8) continue;
            const double U = ((B.Y-C.Y)*(XY.X-C.X) + (C.X-B.X)*(XY.Y-C.Y))/D;
            const double V = ((C.Y-A.Y)*(XY.X-C.X) + (A.X-C.X)*(XY.Y-C.Y))/D;
            if (U < -1.e-6 || V < -1.e-6 || U+V > 1.000001) continue;
            const double Z = U*A.Z + V*B.Z + (1-U-V)*C.Z;
            const double NormalZ = FVector::CrossProduct(B-A,C-A).GetSafeNormal().Z;
            if (!Hits.ContainsByPredicate([&](const FHit& H){ return FMath::Abs(H.Z-Z) < .01
                && FMath::Abs(H.NormalZ-NormalZ) < .01; }))
                Hits.Add({Z,NormalZ,I/3});
        }
        Hits.Sort([](const FHit& A, const FHit& B){ return A.Z < B.Z; });
        FString Heights;
        for (int32 I = 0; I < FMath::Min(Hits.Num(),24); ++I)
            Heights += FString::Printf(TEXT(" Z=%.4f/Nz=%.3f/tri=%d"),Hits[I].Z,Hits[I].NormalZ,Hits[I].Triangle);
        if (Heights.IsEmpty()) Heights = TEXT(" no triangle intersects this XY");
        const FVector Centre = LocalBounds.TransformBy(Transform).GetCenter();
        const bool bMirrorX = SurfaceAt(Module,Mesh,FVector2D(2*Centre.X-XY.X,XY.Y),ExpectedZ-.08,ExpectedZ+.08);
        const bool bMirrorY = SurfaceAt(Module,Mesh,FVector2D(XY.X,2*Centre.Y-XY.Y),ExpectedZ-.08,ExpectedZ+.08);
        const bool bMirrorXY = SurfaceAt(Module,Mesh,FVector2D(2*Centre.X-XY.X,2*Centre.Y-XY.Y),ExpectedZ-.08,ExpectedZ+.08);
        return FString::Printf(TEXT("CastleStairDiagnostic XY=(%.3f,%.3f) expectedZ=%.4f hits[%d]=[%s] mirrorAtExpectedZ(X,Y,XY)=(%d,%d,%d) localProbe={%s} meshBounds={%s -> %s} LOD0Bounds={%s -> %s} vertices=%u indices=%d offset={%s} transform={%s}"),
            XY.X,XY.Y,ExpectedZ,Hits.Num(),*Heights,int32(bMirrorX),int32(bMirrorY),int32(bMirrorXY),
            *Transform.InverseTransformPosition(FVector(XY.X,XY.Y,ExpectedZ)).ToString(),
            *LocalBounds.Min.ToString(),*LocalBounds.Max.ToString(),*VertexBounds.Min.ToString(),*VertexBounds.Max.ToString(),
            Positions.GetNumVertices(),Indices.Num(),*Module.Offset.ToString(),*Transform.ToString());
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthRoyalWorksPlanTown3Test,
    "ThreeHearths.RoyalWorksPlan.Town3CastleIsModularAndStaged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthRoyalWorksPlanTown3Test::RunTest(const FString&)
{
    using namespace HearthRoyalWorksPlanTests;
    using namespace HearthCastleNativeAudit;
    const auto Legacy = HearthRoyalWorksPlan::Build();
    const auto LegacyById = HearthRoyalWorksPlan::BuildForTemplate(TEXT("royal_keep_garden_v1"));
    const auto V2 = HearthRoyalWorksPlan::BuildForTemplate(TEXT("royal_keep_garden_v2"));
    TestEqual(TEXT("Legacy Build remains v1"), Legacy.TemplateId, FString(TEXT("royal_keep_garden_v1")));
    TestEqual(TEXT("Legacy count"), LegacyById.Modules.Num(), 44);
    for (int32 I = 0; I < Legacy.Modules.Num() && I < LegacyById.Modules.Num(); ++I)
    {
        const auto& A = Legacy.Modules[I]; const auto& B = LegacyById.Modules[I];
        TestTrue(TEXT("Legacy recipe and transforms remain exact"), A.Id == B.Id && A.Offset == B.Offset
            && A.Scale == B.Scale && A.Materials == B.Materials && A.Stage == B.Stage
            && !A.bCenterMeshAtOffset && !B.bCenterMeshAtOffset);
        const auto Transform = HearthRoyalWorksPlan::MeshTransform(A,
            FBox(FVector(-40,-50,10),FVector(60,50,110)), A.Offset);
        TestTrue(TEXT("v1 ignores asymmetric source pivots"), Transform.GetTranslation().Equals(A.Offset));
    }
    TestEqual(TEXT("Town3 template"), V2.TemplateId, FString(TEXT("royal_keep_garden_v2")));
    TestEqual(TEXT("Town3 radius"), V2.Radius, 5000.f);
    TestEqual(TEXT("Eight room metadata entries (not navigation acceptance)"), V2.Rooms.Num(), 8);

    // Audited by source loop/category, not by substituting a failing total.
    // Original floor bill: (25 + 4*24 + 2*25 + 2*3)*5 = 885 planks.
    // The old 711 total omitted 360 planks; original actual total was 1071.
    // Four new exit aprons add four planks. Every stair connection has
    // 2 native stairs, 6 risers, 1 landing, 6 posts, 4 beams and 1 apron.
    // Roofs: 3*(5*9) + 2*(2*3) = 147 paired strips and 38 end gables.
    struct FBill { const TCHAR* Asset; int32 Count; FIntVector Total; };
    const FBill Bills[] = {
        {TEXT("/Engine/BasicShapes/Cube"), 44, FIntVector(64,42,28)},
        {TEXT("floor_timber_2m"), 181, FIntVector(0,889,0)},
        {TEXT("wall_stone_2m"), 374, FIntVector(1222,0,374)},
        {TEXT("wall_window_timber_2m"), 12, FIntVector(0,12,0)},
        {TEXT("floor_opening_2m"), 4, FIntVector(0,4,4)},
        {TEXT("wall_door_stone_2m"), 3, FIntVector(0,6,3)},
        {TEXT("beam_timber_2m"), 125, FIntVector(0,6,167)},
        {TEXT("roof_slope_terracotta_2m"), 294, FIntVector(0,882,588)},
        {TEXT("roof_ridge_terracotta_2m"), 147, FIntVector(0,294,147)},
        {TEXT("gable_timber_4m"), 38, FIntVector(0,76,38)},
        {TEXT("post_timber_2_4m"), 40, FIntVector(0,0,56)},
        {TEXT("stairs_switchback_2x4m"), 8, FIntVector(0,16,8)},
        {TEXT("oak"), 1, FIntVector(0,1,1)},
        {TEXT("birch"), 1, FIntVector(0,1,1)},
        {TEXT("cypress"), 1, FIntVector(0,1,1)},
        {TEXT("flowering_shrub"), 2, FIntVector(2,0,0)},
        {TEXT("wildflowers"), 1, FIntVector(0,1,0)},
    };
    TMap<FString,int32> Counts;
    TMap<FString,FIntVector> CategoryTotals;
    TMap<FString,UStaticMesh*> Meshes;
    TMap<FString,FBox> Rendered;
    TSet<FString> Ids, Placements;
    TSet<int32> Stages;
    int32 PreviousStage = 0;
    auto* Component = NewObject<UStaticMeshComponent>();
    for (const auto& Module : V2.Modules)
    {
        TestTrue(Module.Id + TEXT(": unique ID"), !Ids.Contains(Module.Id));
        Ids.Add(Module.Id);
        TestTrue(Module.Id + TEXT(": ordered stage"), Module.Stage >= PreviousStage && Module.Stage >= 1 && Module.Stage <= 13);
        PreviousStage = Module.Stage; Stages.Add(Module.Stage);
        TestTrue(Module.Id + TEXT(": positive real bill"), Module.Materials.X >= 0 && Module.Materials.Y >= 0 && Module.Materials.Z >= 0
            && Module.Materials.X + Module.Materials.Y + Module.Materials.Z > 0);
        const FString Asset = Module.PlantId.IsEmpty()
            ? (Module.MeshPath.StartsWith(TEXT("/Engine/")) ? Module.MeshPath : FPackageName::ObjectPathToObjectName(Module.MeshPath))
            : Module.PlantId;
        Counts.FindOrAdd(Asset)++;
        CategoryTotals.FindOrAdd(Asset, FIntVector::ZeroValue) += Module.Materials;
        const FString Placement = Asset + Module.Offset.ToString() + Module.Scale.ToString() + FString::SanitizeFloat(Module.Yaw);
        TestTrue(Module.Id + TEXT(": no duplicate paid placement"), !Placements.Contains(Placement));
        Placements.Add(Placement);
        if (!Module.PlantId.IsEmpty())
        {
            TestTrue(TEXT("Catalog plants remain grounded"), Module.Offset.Z == 0 && Module.Stage == 13);
            continue;
        }
        UStaticMesh*& Mesh = Meshes.FindOrAdd(Module.MeshPath);
        if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, *Module.MeshPath);
        if (!TestNotNull(Module.Id + TEXT(": real mesh"), Mesh)) continue;
        const FTransform Transform = HearthRoyalWorksPlan::MeshTransform(Module, Mesh->GetBounds().GetBox(), Module.Offset);
        Component->SetStaticMesh(Mesh);
        const FBox Box = Component->CalcBounds(Transform).GetBox();
        Rendered.Add(Module.Id, Box);
        TestTrue(Module.Id + TEXT(": actual component centre"), Box.GetCenter().Equals(Module.Offset, .05));
        TestTrue(Module.Id + TEXT(": actual bounds agree with scaled source"), Box.GetSize().Equals(Module.BoundsSizeCm, .05));
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const double X = Corner & 1 ? Box.Min.X : Box.Max.X;
            const double Y = Corner & 2 ? Box.Min.Y : Box.Max.Y;
            TestTrue(Module.Id + TEXT(": inside site radius"), X*X + Y*Y <= FMath::Square(double(V2.Radius)));
        }
        TestFalse(Module.Id + TEXT(": no solid storey cube"), Module.MeshPath.StartsWith(TEXT("/Engine/"))
            && Box.GetSize().X > 2000 && Box.GetSize().Y > 2000 && Box.GetSize().Z > 2000);
        if (Module.Id.Contains(TEXT("curtain_wall")) || Module.Id.Contains(TEXT("tower_level")))
            TestTrue(Module.Id + TEXT(": small facade segment"), FMath::Max(Box.GetSize().X,Box.GetSize().Y) <= 400.01);
        if (Module.bCenterMeshAtOffset)
            TestTrue(Module.Id + TEXT(": UE local Y converts to the fixed assembly frame"),
                Module.Scale.X > 0 && Module.Scale.Y < 0 && Module.Scale.Z > 0);
        if (Module.MeshPath.Contains(TEXT("roof_")) || Module.MeshPath.Contains(TEXT("gable_")) || Module.MeshPath.Contains(TEXT("stairs_")))
            TestTrue(Module.Id + TEXT(": unit native roof/stair size with explicit basis reflection"), Module.Scale.GetAbs().Equals(FVector::OneVector, .001));
    }
    int32 AuditedCount = 0;
    FIntVector AuditedTotal = FIntVector::ZeroValue;
    for (const FBill& Bill : Bills)
    {
        TestEqual(FString(Bill.Asset) + TEXT(": audited category count"), Counts.FindRef(Bill.Asset), Bill.Count);
        TestTrue(FString(Bill.Asset) + TEXT(": audited category bill"), CategoryTotals.FindRef(Bill.Asset) == Bill.Total);
        AuditedCount += Bill.Count; AuditedTotal += Bill.Total;
    }
    TestEqual(TEXT("No unaccounted asset category"), Counts.Num(), int32(UE_ARRAY_COUNT(Bills)));
    TestEqual(TEXT("Count includes exactly the audited assemblies"), V2.Modules.Num(), AuditedCount);
    TestEqual(TEXT("Every dependency stage exists"), Stages.Num(), 13);
    AddInfo(FString::Printf(TEXT("Audited Town3 BOM: %d parts, stone=%d plank=%d beam=%d."),
        AuditedCount,AuditedTotal.X,AuditedTotal.Y,AuditedTotal.Z));
    if (Rendered.Num() + 6 != V2.Modules.Num()) return false;

    const auto BoxFor = [&](const FString& Id) -> const FBox* { return Rendered.Find(Id); };
    for (int32 Level = 0; Level < 5; ++Level)
    {
        const FString WallId = FString::Printf(TEXT("tower_level_%d_north_04"),Level);
        const FString FloorId = FString::Printf(TEXT("tower_floor_%d_floor_tile_00_00"),Level);
        const FBox* Wall = BoxFor(WallId); const FBox* Floor = BoxFor(FloorId);
        if (TestNotNull(TEXT("Tower bearing wall"), Wall) && TestNotNull(TEXT("Tower floor"), Floor))
            TestTrue(TEXT("Loaded wall starts at the real floor top"), FMath::Abs(Wall->Min.Z-Floor->Max.Z) < .05);
        if (Level < 4)
        {
            const FBox* Next = BoxFor(FString::Printf(TEXT("tower_floor_%d_floor_opening"),Level+1));
            if (TestNotNull(TEXT("Upper floor opening"), Next))
                TestTrue(TEXT("Loaded upper floor rests at lower wall top"), Wall && FMath::Abs(Next->Min.Z-Wall->Max.Z) < .05);
        }
    }
    const auto CheckSurface = [&](const FString& Id, double X, double Y, double Z)
    {
        const auto* Module = V2.Modules.FindByPredicate([&](const auto& M){return M.Id == Id;});
        if (!TestNotNull(Id + TEXT(": surface module"), Module)) return;
        double Actual = 0;
        UStaticMesh* Mesh = Meshes.FindRef(Module->MeshPath);
        const bool bAtRequiredHeight = SurfaceAt(*Module, Mesh, FVector2D(X,Y), Z-.08, Z+.08, &Actual);
        FString Label = Id + FString::Printf(TEXT(": real triangle at required tread height XY=(%.3f,%.3f) expectedZ=%.4f"),X,Y,Z);
        // One representative native instance is enough to inspect all 13
        // tread/exit samples without duplicating LOD dumps on eight stairs.
        if (!bAtRequiredHeight && Id == TEXT("tower_connection_0_stair_lower"))
            Label += TEXT(" | ") + StairSampleDiagnostic(*Module,Mesh,FVector2D(X,Y),Z);
        TestTrue(Label,bAtRequiredHeight);
    };
    for (int32 Level = 0; Level < 4; ++Level)
    {
        const double Base = 80 + 600*Level;
        const FString Prefix = FString::Printf(TEXT("tower_connection_%d_"),Level);
        for (int32 Flight = 0; Flight < 2; ++Flight)
        {
            const FString Id = Prefix + (Flight == 0 ? TEXT("stair_lower") : TEXT("stair_upper"));
            const double X = Flight == 0 ? -450 : 0;
            const double Z = Base + (Flight == 0 ? 0 : 360);
            for (int32 Step = 0; Step < 6; ++Step)
            {
                CheckSurface(Id,X-48,560+24*Step,Z+20*(Step+1));
                CheckSurface(Id,X+48,680-24*Step,Z+120+20*(Step+1));
            }
            CheckSurface(Id,X+48,503,Z+240);
        }
        for (int32 Step = 0; Step < 6; ++Step)
            CheckSurface(Prefix+FString::Printf(TEXT("step_%d"),Step),-337.5+45*Step,499,Base+260+20*Step);
        CheckSurface(Prefix+TEXT("landing"),-48,530,Base+360);
        CheckSurface(Prefix+TEXT("exit_apron"),53,451,Base+600);
        const auto* Opening = V2.Modules.FindByPredicate([&](const auto& M){
            return M.Id == FString::Printf(TEXT("tower_floor_%d_floor_opening"),Level+1);});
        if (TestNotNull(TEXT("Stair well exists"),Opening))
            TestFalse(TEXT("Real upper floor triangles leave the stair well open"),
                SurfaceAt(*Opening,Meshes.FindRef(Opening->MeshPath),FVector2D(0,650),Base+560,Base+605));

        // Check actual geometric headroom on both stair runs. This does not
        // assert collision cooking, navigation, or resident traversal.
        for (int32 Flight = 0; Flight < 2; ++Flight)
        {
            const FVector2D XY(Flight == 0 ? -498 : -48,560);
            const double Z = Base + (Flight == 0 ? 20 : 380);
            for (const auto& M : V2.Modules)
                if (M.PlantId.IsEmpty())
                {
                    double ActualBlockingZ = 0;
                    const bool bBlocked = SurfaceAt(M,Meshes.FindRef(M.MeshPath),XY,Z+3,Z+195,&ActualBlockingZ);
                    TestFalse(Prefix+TEXT("headroom blocked by ")+M.Id+FString::Printf(
                        TEXT(" XY=(%.3f,%.3f) walkingZ=%.4f checked=(%.4f,%.4f) actualBlockingZ=%.4f"),
                        XY.X,XY.Y,Z,Z+3,Z+195,ActualBlockingZ),bBlocked);
                }
        }
    }
    struct FRoofArea { const TCHAR* Prefix; double X; double Eave; };
    const FRoofArea Roofs[] = {{TEXT("tower_roof_"),0,3040},{TEXT("west_wing_"),-2200,720},{TEXT("east_wing_"),2200,720}};
    for (const auto& Roof : Roofs)
    {
        const double Y = Roof.X == 0 ? 650 : 700;
        for (int32 IX=0; IX<=16; ++IX) for (int32 IY=0; IY<=10; ++IY)
        {
            const FVector2D XY(Roof.X-880+110*IX,Y-890+178*IY);
            bool bCovered = false;
            for (const auto& M : V2.Modules)
                if (M.Id.StartsWith(Roof.Prefix) && M.MeshPath.Contains(TEXT("roof_slope_"))
                    && SurfaceAt(M,Meshes.FindRef(M.MeshPath),XY,Roof.Eave-15,Roof.Eave+145))
                { bCovered = true; break; }
            TestTrue(FString(Roof.Prefix)+FString::Printf(TEXT("real roof coverage at %.0f,%.0f"),XY.X,XY.Y),bCovered);
        }
    }
    const FBox* Cap = BoxFor(TEXT("tower_roof_bay_02_strip_04_ridge"));
    if (TestNotNull(TEXT("Native short ridge cap"),Cap))
        TestTrue(TEXT("Real tower roof reaches 30-35m"),Cap->Max.Z>=3000 && Cap->Max.Z<=3500);
    const auto* Door = Find(V2,TEXT("west_wing_south_wall_04"));
    if (TestNotNull(TEXT("Wing door replaces solid wall"),Door))
    {
        TestTrue(TEXT("Door is a native opening"),Door->MeshPath.Contains(TEXT("wall_door_stone_2m")));
        const auto* Floor = BoxFor(TEXT("west_wing_floor_tile_00_00"));
        const auto* Wall = BoxFor(Door->Id);
        TestTrue(TEXT("Wing wall is grounded on its real floor"),Floor && Wall && FMath::Abs(Floor->Max.Z-Wall->Min.Z)<.05);
    }
    const FBox* GateWest=BoxFor(TEXT("v2_gate_foundation_west"));
    const FBox* GateEast=BoxFor(TEXT("v2_gate_foundation_east"));
    TestTrue(TEXT("South entry retains a real four-metre gap"),GateWest && GateEast && GateEast->Min.X-GateWest->Max.X>=400);

    const auto Repeat = HearthRoyalWorksPlan::BuildForTemplate(TEXT("royal_keep_garden_v2"));
    TestEqual(TEXT("Canonical rebuild count"),Repeat.Modules.Num(),V2.Modules.Num());
    for (int32 I=0; I<V2.Modules.Num() && I<Repeat.Modules.Num(); ++I)
    {
        const auto& A=V2.Modules[I]; const auto& B=Repeat.Modules[I];
        TestTrue(TEXT("Canonical IDs, dimensions, pivots, stages and bills repeat"),A.Id==B.Id && A.MeshPath==B.MeshPath
            && A.Offset==B.Offset && A.Scale==B.Scale && A.BoundsSizeCm==B.BoundsSizeCm && A.Yaw==B.Yaw
            && A.bCenterMeshAtOffset==B.bCenterMeshAtOffset && A.Materials==B.Materials && A.Stage==B.Stage);
    }
    return true;
}
#endif
