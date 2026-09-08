#include "HearthBuildingAppearance.h"

namespace HearthBuildingAppearance_Impl
{
    constexpr float HearthAppearanceGridCm = 200.f;
    constexpr float HearthAppearanceSpanWidthCm = 400.f;
    constexpr float HearthAppearanceRoofHalfSpanCm = 218.f; // Author spec: roof_slope nominal X=2.18m.
    constexpr float HearthAppearanceGableHalfSpanCm = 200.f; // Author spec: gable nominal width=4m.
    constexpr float HearthAppearanceCanopyHalfSpanCm = 110.f; // Author spec: canopy nominal X=2.2m.
    constexpr float HearthAppearanceCanopyProjectionCm = 125.f; // Author spec: canopy nominal Y=1.25m.
    constexpr float HearthAppearanceCanopyAlongOverhangCm = 10.f; // 2.2m canopy repeated on the 2m grid.

    struct FSection
    {
        float CenterX = 0.f;
        float StartY = 0.f;
        float Depth = 400.f;
    };

    struct FLayout
    {
        TArray<FSection> Sections;
        int32 Floors = 1;
        bool bFrontPorch = false;
        bool bSideCanopy = false;
        bool bBench = false;
        bool bHasCourtyard = false;
        FString Variant;
    };

    FString NativePath(const FString& Id)
    {
        return FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"), *Id, *Id, *Id);
    }

    void AddPart(FHearthBuildingAppearance& Out, const TCHAR* Id, const TCHAR* Role,
        const FVector& Offset, float Yaw = 0.f)
    {
        FHearthBuildingAppearancePart Part;
        Part.AssetId = Id;
        Part.AssetPath = NativePath(Part.AssetId);
        Part.Role = Role;
        Part.Offset = Offset;
        Part.Yaw = Yaw;
        Part.Scale = FVector::OneVector;
        Out.Parts.Add(MoveTemp(Part));
    }

    FString WallId(const FString& Material)
    {
        if (Material == TEXT("stone")) return TEXT("wall_stone_2m");
        if (Material == TEXT("timber")) return TEXT("wall_timber_2m");
        return TEXT("wall_plaster_2m");
    }

    FString WindowId(const FString& Material)
    {
        if (Material == TEXT("stone")) return TEXT("wall_window_stone_2m");
        if (Material == TEXT("timber")) return TEXT("wall_window_timber_2m");
        return TEXT("wall_window_plaster_2m");
    }

    FString DoorId(const FString& Material)
    {
        if (Material == TEXT("stone")) return TEXT("wall_door_stone_2m");
        if (Material == TEXT("timber")) return TEXT("wall_door_timber_2m");
        return TEXT("wall_door_plaster_2m");
    }

    FString GableId(const FString& Material)
    {
        if (Material == TEXT("stone")) return TEXT("gable_stone_4m");
        if (Material == TEXT("timber")) return TEXT("gable_timber_4m");
        return TEXT("gable_plaster_4m");
    }

    FString RoofId(const FString& Material)
    {
        if (Material == TEXT("slateblue")) return TEXT("roof_slope_slateblue_2m");
        if (Material == TEXT("timber")) return TEXT("roof_slope_timber_2m");
        return TEXT("roof_slope_terracotta_2m");
    }

    FString RidgeId(const FString& Material)
    {
        if (Material == TEXT("slateblue")) return TEXT("roof_ridge_slateblue_2m");
        if (Material == TEXT("timber")) return TEXT("roof_ridge_timber_2m");
        return TEXT("roof_ridge_terracotta_2m");
    }

    FString CanopyId(const FString& Material)
    {
        return Material == TEXT("slateblue") ? TEXT("canopy_slateblue_2m") : TEXT("canopy_terracotta_2m");
    }

    int32 BayCount(float LengthCm)
    {
        return FMath::Max(1, FMath::RoundToInt(LengthCm / HearthAppearanceGridCm));
    }

    void AddFoundationAndFloors(FHearthBuildingAppearance& Out, const TArray<FSection>& Sections, int32 Floors)
    {
        for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
        {
            const FSection& Section = Sections[SectionIndex];
            const int32 YBayCount = BayCount(Section.Depth);
            for (int32 Floor = 0; Floor < Floors; ++Floor)
            {
                const float Z = Floor * Out.FloorHeightCm;
                for (int32 XBay = 0; XBay < 2; ++XBay)
                {
                    const float X = Section.CenterX + (XBay == 0 ? -100.f : 100.f);
                    for (int32 YBay = 0; YBay < YBayCount; ++YBay)
                    {
                        const float Y = Section.StartY + (YBay + .5f) * HearthAppearanceGridCm;
                        if (Floor == 0) AddPart(Out, TEXT("foundation_stone_2m"), TEXT("foundation"), FVector(X, Y, 0.f));
                        const bool bOpening = SectionIndex == 0 && Floor > 0 && XBay == 0 && YBay < 2;
                        AddPart(Out, bOpening ? TEXT("floor_opening_2m") : TEXT("floor_timber_2m"),
                            bOpening ? TEXT("floor_opening") : TEXT("floor"), FVector(X, Y, Z));
                    }
                }
            }
        }
    }

    void AddFrame(FHearthBuildingAppearance& Out, const TArray<FSection>& Sections, int32 Floors)
    {
        const float BeamHeights[] = {230.f, 250.f, 270.f};
        for (const FSection& Section : Sections)
        {
            const int32 YBayCount = BayCount(Section.Depth);
            const float LeftX = Section.CenterX - HearthAppearanceSpanWidthCm * .5f;
            const float RightX = Section.CenterX + HearthAppearanceSpanWidthCm * .5f;
            const float EndY = Section.StartY + Section.Depth;
            for (int32 Floor = 0; Floor < Floors; ++Floor)
            {
                const float BaseZ = Floor * Out.FloorHeightCm;
                for (int32 YVertex = 0; YVertex <= YBayCount; ++YVertex)
                {
                    const float Y = Section.StartY + YVertex * HearthAppearanceGridCm;
                    AddPart(Out, TEXT("post_timber_2_4m"), TEXT("post"), FVector(LeftX, Y, BaseZ));
                    AddPart(Out, TEXT("post_timber_2_4m"), TEXT("post"), FVector(RightX, Y, BaseZ));
                }
                for (const float BeamZ : BeamHeights)
                {
                    const float Z = BaseZ + BeamZ;
                    for (int32 YBay = 0; YBay < YBayCount; ++YBay)
                    {
                        const float Y = Section.StartY + (YBay + .5f) * HearthAppearanceGridCm;
                        AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_side"), FVector(LeftX, Y, Z), 90.f);
                        AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_side"), FVector(RightX, Y, Z), -90.f);
                    }
                    AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_front"), FVector(Section.CenterX - 100.f, Section.StartY, Z));
                    AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_front"), FVector(Section.CenterX + 100.f, Section.StartY, Z));
                    AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_back"), FVector(Section.CenterX - 100.f, EndY, Z), 180.f);
                    AddPart(Out, TEXT("beam_timber_2m"), TEXT("beam_back"), FVector(Section.CenterX + 100.f, EndY, Z), 180.f);
                }
            }
        }
    }

    void AddWalls(FHearthBuildingAppearance& Out, const FString& WallMaterial, const TArray<FSection>& Sections, int32 Floors)
    {
        const FString Wall = WallId(WallMaterial);
        const FString Window = WindowId(WallMaterial);
        const FString Door = DoorId(WallMaterial);
        for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
        {
            const FSection& Section = Sections[SectionIndex];
            const int32 YBayCount = BayCount(Section.Depth);
            const float LeftX = Section.CenterX - HearthAppearanceSpanWidthCm * .5f;
            const float RightX = Section.CenterX + HearthAppearanceSpanWidthCm * .5f;
            const float EndY = Section.StartY + Section.Depth;
            for (int32 Floor = 0; Floor < Floors; ++Floor)
            {
                const float Z = Floor * Out.FloorHeightCm;
                for (int32 XBay = 0; XBay < 2; ++XBay)
                {
                    const float X = Section.CenterX + (XBay == 0 ? -100.f : 100.f);
                    const bool bFrontDoor = SectionIndex == 0 && Floor == 0 && XBay == 0;
                    AddPart(Out, bFrontDoor ? *Door : *Window, bFrontDoor ? TEXT("front_door") : TEXT("front_window"), FVector(X, Section.StartY, Z));
                    AddPart(Out, *Window, TEXT("rear_window"), FVector(X, EndY, Z), 180.f);
                }
                for (int32 YBay = 0; YBay < YBayCount; ++YBay)
                {
                    const float Y = Section.StartY + (YBay + .5f) * HearthAppearanceGridCm;
                    AddPart(Out, *Window, TEXT("side_window"), FVector(LeftX, Y, Z), -90.f);
                    AddPart(Out, YBay % 2 == 0 ? *Window : *Wall, YBay % 2 == 0 ? TEXT("side_window") : TEXT("side_wall"), FVector(RightX, Y, Z), 90.f);
                }
            }
        }
    }

    void AddRoof(FHearthBuildingAppearance& Out, const FString& WallMaterial, const FString& RoofMaterial,
        const TArray<FSection>& Sections, int32 Floors)
    {
        const FString Gable = GableId(WallMaterial);
        const FString Roof = RoofId(RoofMaterial);
        const FString Ridge = RidgeId(RoofMaterial);
        const float RoofZ = Floors * Out.FloorHeightCm;
        for (const FSection& Section : Sections)
        {
            const float EndY = Section.StartY + Section.Depth;
            AddPart(Out, *Gable, TEXT("front_gable"), FVector(Section.CenterX, Section.StartY, RoofZ));
            AddPart(Out, *Gable, TEXT("rear_gable"), FVector(Section.CenterX, EndY, RoofZ), 180.f);
            for (int32 YBay = 0; YBay < BayCount(Section.Depth); ++YBay)
            {
                const float Y = Section.StartY + (YBay + .5f) * HearthAppearanceGridCm;
                AddPart(Out, *Roof, TEXT("roof_slope_front"), FVector(Section.CenterX, Y, RoofZ), 180.f);
                AddPart(Out, *Roof, TEXT("roof_slope_back"), FVector(Section.CenterX, Y, RoofZ));
                AddPart(Out, *Ridge, TEXT("roof_ridge"), FVector(Section.CenterX, Y, RoofZ));
            }
        }
    }

    void AddFrontPorch(FHearthBuildingAppearance& Out, const FString& RoofMaterial,
        const TArray<FSection>& Sections, bool bBench)
    {
        const FString Canopy = CanopyId(RoofMaterial);
        float FrontY = FLT_MAX;
        for (const FSection& Section : Sections) FrontY = FMath::Min(FrontY, Section.StartY);
        for (const FSection& Section : Sections)
        {
            if (!FMath::IsNearlyEqual(Section.StartY, FrontY)) continue;
            for (int32 XBay = 0; XBay < 2; ++XBay)
            {
                const float X = Section.CenterX + (XBay == 0 ? -100.f : 100.f);
                AddPart(Out, *Canopy, TEXT("front_canopy"), FVector(X, FrontY - HearthAppearanceCanopyProjectionCm * .5f, 180.f));
            }
            for (int32 XPost = 0; XPost <= 2; ++XPost)
            {
                const float X = Section.CenterX - 200.f + XPost * HearthAppearanceGridCm;
                AddPart(Out, TEXT("porch_post_timber"), TEXT("porch_post"), FVector(X, FrontY - HearthAppearanceCanopyProjectionCm, 0.f));
            }
        }
        const FSection& EntranceSection = Sections[0];
        AddPart(Out, TEXT("porch_steps_stone_2m"), TEXT("porch_steps"), FVector(EntranceSection.CenterX - 100.f, EntranceSection.StartY - 45.f, 0.f));
        if (bBench) AddPart(Out, TEXT("bench_timber"), TEXT("outdoor_seating"), FVector(EntranceSection.CenterX + 100.f, EntranceSection.StartY - 82.f, 0.f));
    }

    void AddSideCanopy(FHearthBuildingAppearance& Out, const FString& RoofMaterial, const TArray<FSection>& Sections)
    {
        const FString Canopy = CanopyId(RoofMaterial);
        const FSection& Section = Sections.Last();
        const float SideX = Section.CenterX + HearthAppearanceSpanWidthCm * .5f + HearthAppearanceCanopyProjectionCm * .5f;
        for (int32 YBay = 0; YBay < BayCount(Section.Depth); ++YBay)
        {
            const float Y = Section.StartY + (YBay + .5f) * HearthAppearanceGridCm;
            AddPart(Out, *Canopy, TEXT("side_canopy"), FVector(SideX, Y, 180.f), 90.f);
            AddPart(Out, TEXT("porch_post_timber"), TEXT("porch_post"), FVector(SideX + HearthAppearanceCanopyProjectionCm * .5f, Y, 0.f), 90.f);
        }
        AddPart(Out, TEXT("workbench_carpenter"), TEXT("outdoor_workbench"), FVector(SideX, Section.StartY + Section.Depth * .5f, 0.f), 90.f);
    }

    FLayout MakeLayout(const FString& Archetype, bool bTown3, uint32 Seed)
    {
        FLayout Layout;
        const int32 Variant = static_cast<int32>((Seed ^ (Seed >> 16)) % 3u);
        auto AddSection = [](TArray<FSection>& Sections, float CenterX, float StartY, float Depth)
        {
            FSection Section; Section.CenterX = CenterX; Section.StartY = StartY; Section.Depth = Depth;
            Sections.Add(MoveTemp(Section));
        };
        auto Main = [&Layout, &AddSection](float Depth) { AddSection(Layout.Sections, 0.f, -Depth * .5f, Depth); };
        auto DoubleSpan = [&Layout, &AddSection](float Depth)
        {
            AddSection(Layout.Sections, -200.f, -Depth * .5f, Depth);
            AddSection(Layout.Sections, 200.f, -Depth * .5f, Depth);
        };
        auto CourtyardWing = [&Layout, &AddSection](float Depth)
        {
            AddSection(Layout.Sections, 0.f, -Depth * .5f, Depth);
            AddSection(Layout.Sections, 400.f, 0.f, Depth * .5f);
            Layout.bHasCourtyard = true;
        };

        if (!bTown3)
        {
            if (Archetype == TEXT("shop_house")) { Main(400.f); Layout.Floors = 2; Layout.Variant = TEXT("town2_legacy_scale_shop"); }
            else if (Archetype == TEXT("courtyard_workshop")) { Main(400.f); Layout.Variant = TEXT("town2_legacy_scale_workshop"); }
            else if (Archetype == TEXT("warehouse")) { Main(400.f); Layout.Variant = TEXT("town2_legacy_scale_warehouse"); }
            else if (Archetype == TEXT("inn")) { Main(400.f); Layout.Floors = 3; Layout.Variant = TEXT("town2_legacy_scale_inn"); }
            else if (Archetype == TEXT("keep")) { Main(400.f); Layout.Floors = 2; Layout.Variant = TEXT("town2_legacy_scale_keep"); }
            else { Main(400.f); Layout.Floors = 2; Layout.Variant = TEXT("town2_legacy_scale_rowhouse"); }
            return Layout;
        }

        if (Archetype == TEXT("courtyard_workshop"))
        {
            if (Variant == 0) { DoubleSpan(800.f); Layout.Variant = TEXT("town3_double_span_workshop"); }
            else if (Variant == 1) { CourtyardWing(800.f); Layout.Variant = TEXT("town3_l_courtyard_workshop"); }
            else { Main(600.f); Layout.Variant = TEXT("town3_narrow_workshop"); }
            Layout.bSideCanopy = true;
        }
        else if (Archetype == TEXT("warehouse"))
        {
            if (Variant == 0) { DoubleSpan(800.f); Layout.Variant = TEXT("town3_double_span_warehouse"); }
            else if (Variant == 1) { DoubleSpan(1000.f); Layout.Variant = TEXT("town3_long_double_span_warehouse"); }
            else { Main(1000.f); Layout.Variant = TEXT("town3_long_warehouse"); }
        }
        else if (Archetype == TEXT("inn"))
        {
            if (Variant == 0) { DoubleSpan(800.f); Layout.Floors = 3; Layout.Variant = TEXT("town3_double_span_inn"); }
            else if (Variant == 1) { CourtyardWing(800.f); Layout.Floors = 2; Layout.Variant = TEXT("town3_l_courtyard_inn"); }
            else { Main(800.f); Layout.Floors = 3; Layout.Variant = TEXT("town3_long_inn"); }
            Layout.bFrontPorch = true; Layout.bBench = true;
        }
        else if (Archetype == TEXT("shop_house"))
        {
            Main(Variant == 2 ? 1000.f : 800.f); Layout.Floors = Variant == 0 ? 1 : 2;
            Layout.bFrontPorch = true; Layout.Variant = Variant == 2 ? TEXT("town3_deep_shop_house") : TEXT("town3_two_storey_shop_house");
        }
        else if (Archetype == TEXT("keep"))
        {
            Main(Variant == 0 ? 600.f : 800.f); Layout.Floors = 2;
            Layout.Variant = Variant == 0 ? TEXT("town3_compact_keep") : TEXT("town3_two_storey_keep");
        }
        else
        {
            Main(Variant == 0 ? 600.f : 800.f); Layout.Floors = 2;
            Layout.Variant = Variant == 0 ? TEXT("town3_narrow_rowhouse") : TEXT("town3_deep_rowhouse");
        }
        return Layout;
    }

    void ComputeFootprint(const FLayout& Layout, FVector2D& OutCore, FVector2D& OutOccupied)
    {
        float CoreMinX = FLT_MAX, CoreMaxX = -FLT_MAX, CoreMinY = FLT_MAX, CoreMaxY = -FLT_MAX;
        float MinX = FLT_MAX, MaxX = -FLT_MAX, MinY = FLT_MAX, MaxY = -FLT_MAX;
        for (const FSection& Section : Layout.Sections)
        {
            CoreMinX = FMath::Min(CoreMinX, Section.CenterX - HearthAppearanceSpanWidthCm * .5f);
            CoreMaxX = FMath::Max(CoreMaxX, Section.CenterX + HearthAppearanceSpanWidthCm * .5f);
            CoreMinY = FMath::Min(CoreMinY, Section.StartY);
            CoreMaxY = FMath::Max(CoreMaxY, Section.StartY + Section.Depth);
            const float RoofOrGableHalfSpan = FMath::Max(HearthAppearanceRoofHalfSpanCm, HearthAppearanceGableHalfSpanCm);
            MinX = FMath::Min(MinX, Section.CenterX - RoofOrGableHalfSpan);
            MaxX = FMath::Max(MaxX, Section.CenterX + RoofOrGableHalfSpan);
            MinY = FMath::Min(MinY, Section.StartY);
            MaxY = FMath::Max(MaxY, Section.StartY + Section.Depth);
        }
        if (Layout.bFrontPorch)
        {
            const float FrontY = Layout.Sections[0].StartY;
            MinY = FMath::Min(MinY, FrontY - HearthAppearanceCanopyProjectionCm);
            for (const FSection& Section : Layout.Sections)
                if (FMath::IsNearlyEqual(Section.StartY, FrontY))
                {
                    MinX = FMath::Min(MinX, Section.CenterX - HearthAppearanceCanopyHalfSpanCm * 2.f);
                    MaxX = FMath::Max(MaxX, Section.CenterX + HearthAppearanceCanopyHalfSpanCm * 2.f);
                }
        }
        if (Layout.bSideCanopy)
        {
            const FSection& Section = Layout.Sections.Last();
            MaxX = FMath::Max(MaxX, Section.CenterX + HearthAppearanceSpanWidthCm * .5f + HearthAppearanceCanopyProjectionCm);
            MinY = FMath::Min(MinY, Section.StartY - HearthAppearanceCanopyAlongOverhangCm);
            MaxY = FMath::Max(MaxY, Section.StartY + Section.Depth + HearthAppearanceCanopyAlongOverhangCm);
        }
        OutCore = FVector2D(CoreMaxX - CoreMinX, CoreMaxY - CoreMinY);
        OutOccupied = FVector2D(MaxX - MinX, MaxY - MinY);
    }

    // Version 4 uses the same two metre kit pieces as the legacy builder, but
    // composes several independent volumes.  A volume's local +X is its four
    // metre gable and local +Y runs from the entrance toward the rear.  Keeping
    // all placement in this local frame makes a 90 degree wing a real rotated
    // building rather than a stretched footprint.
    struct FComposedVolume
    {
        FVector2D Center = FVector2D::ZeroVector;
        float Depth = 400.f;
        int32 Floors = 1;
        float Yaw = 0.f;
    };

    struct FComposedLayout
    {
        TArray<FComposedVolume> Volumes;
        FString ShapeId;
        bool bHasCourtyard = false;
        bool bFrontPorch = false;
    };

    FVector2D RotateLocal2D(const FVector2D& Local, float Yaw)
    {
        const float Radians = FMath::DegreesToRadians(Yaw);
        const float C = FMath::Cos(Radians);
        const float S = FMath::Sin(Radians);
        return FVector2D(Local.X * C - Local.Y * S, Local.X * S + Local.Y * C);
    }

    FVector ComposedOffset(const FComposedVolume& Volume, const FVector& Local)
    {
        const FVector2D Rotated = RotateLocal2D(FVector2D(Local.X, Local.Y), Volume.Yaw);
        return FVector(Volume.Center.X + Rotated.X, Volume.Center.Y + Rotated.Y, Local.Z);
    }

    void AddComposedPart(FHearthBuildingAppearance& Out, const FString& AssetId, const TCHAR* Role,
        const FComposedVolume& Volume, const FVector& LocalOffset, float LocalYaw = 0.f)
    {
        FHearthBuildingAppearancePart Part;
        Part.AssetId = AssetId;
        Part.AssetPath = NativePath(AssetId);
        Part.Role = Role;
        Part.Offset = ComposedOffset(Volume, LocalOffset);
        Part.Yaw = Volume.Yaw + LocalYaw;
        Part.Scale = FVector::OneVector;
        Out.Parts.Add(MoveTemp(Part));
    }

    FComposedLayout MakeComposedLayout(int32 LayoutId, uint32 Seed)
    {
        FComposedLayout Layout;
        const int32 SeedVariant = static_cast<int32>((Seed ^ (Seed >> 16)) % 3u);
        const int32 SecondaryVariant = static_cast<int32>(((Seed >> 8) ^ Seed) % 3u);
        const float Drift = static_cast<float>(SeedVariant - 1) * 20.f;
        const float SideDrift = static_cast<float>(SecondaryVariant - 1) * 15.f;
        auto Add = [&Layout](float X, float Y, float Depth, int32 Floors, float Yaw)
        {
            FComposedVolume Volume;
            Volume.Center = FVector2D(X, Y);
            Volume.Depth = Depth;
            Volume.Floors = Floors;
            Volume.Yaw = Yaw;
            Layout.Volumes.Add(Volume);
        };

        switch (LayoutId)
        {
        case 0: // compact cluster: staggered central house and two low annexes.
            Layout.ShapeId = TEXT("compact_cluster");
            Add(0.f + Drift, 0.f, 600.f, 2, 0.f);
            Add(-390.f + SideDrift, -70.f, 400.f, 1, 90.f);
            Add(380.f - SideDrift, 180.f, 400.f, 1, 0.f);
            Layout.bFrontPorch = true;
            break;
        case 1: // L court: a tall spine and a lower rotated wing leave a real court.
            Layout.ShapeId = TEXT("L_court");
            Add(0.f + Drift, 0.f, 800.f, 2, 0.f);
            Add(390.f + SideDrift, 210.f, 600.f, 1, 90.f);
            Layout.bHasCourtyard = true;
            Layout.bFrontPorch = true;
            break;
        case 2: // stepped wings: three heights and two opposing rotated wings.
            Layout.ShapeId = TEXT("stepped_wings");
            Add(0.f + Drift, 0.f, 600.f, 3, 0.f);
            Add(-390.f + SideDrift, -130.f, 400.f, 2, 90.f);
            Add(390.f - SideDrift, 150.f, 600.f, 1, 90.f);
            Layout.bHasCourtyard = true;
            break;
        case 3: // U court: back hall plus two rotated wings.
            Layout.ShapeId = TEXT("U_court");
            Add(0.f + Drift, 320.f, 400.f, 2, 0.f);
            Add(-300.f + SideDrift, 0.f, 600.f, 2, 90.f);
            Add(300.f - SideDrift, 0.f, 600.f, 1, 90.f);
            Layout.bHasCourtyard = true;
            Layout.bFrontPorch = true;
            break;
        case 4: // offset workshop: house, rotated shop/work bay, and an outdoor threshold.
            Layout.ShapeId = TEXT("offset_workshop");
            Add(-90.f + Drift, 0.f, 800.f, 2, 0.f);
            Add(350.f + SideDrift, 210.f, 400.f, 1, 90.f);
            Layout.bHasCourtyard = true;
            Layout.bFrontPorch = true;
            break;
        case 5: // tower annex: a high central stair core with two visibly low pieces.
            Layout.ShapeId = TEXT("tower_annex");
            Add(0.f + Drift, 0.f, 400.f, 3, 0.f);
            Add(350.f + SideDrift, 230.f, 400.f, 1, 90.f);
            Add(-350.f - SideDrift, -200.f, 400.f, 1, 0.f);
            Layout.bHasCourtyard = true;
            break;
        default:
            Layout.ShapeId.Reset();
            break;
        }
        return Layout;
    }

    void AddComposedFoundationAndFloors(FHearthBuildingAppearance& Out, const FComposedVolume& Volume)
    {
        const int32 YBayCount = BayCount(Volume.Depth);
        const float FrontY = -Volume.Depth * .5f;
        for (int32 Floor = 0; Floor < Volume.Floors; ++Floor)
        {
            const float Z = Floor * Out.FloorHeightCm;
            for (int32 XBay = 0; XBay < 2; ++XBay)
            {
                const float X = XBay == 0 ? -100.f : 100.f;
                for (int32 YBay = 0; YBay < YBayCount; ++YBay)
                {
                    const float Y = FrontY + (YBay + .5f) * HearthAppearanceGridCm;
                    if (Floor == 0)
                    {
                        AddComposedPart(Out, TEXT("foundation_stone_2m"), TEXT("foundation"), Volume, FVector(X, Y, 0.f));
                    }
                    AddComposedPart(Out, TEXT("floor_timber_2m"), TEXT("floor"), Volume, FVector(X, Y, Z));
                }
            }
        }
    }

    void AddComposedFrame(FHearthBuildingAppearance& Out, const FComposedVolume& Volume)
    {
        const int32 YBayCount = BayCount(Volume.Depth);
        const float FrontY = -Volume.Depth * .5f;
        const float BackY = Volume.Depth * .5f;
        for (int32 Floor = 0; Floor < Volume.Floors; ++Floor)
        {
            const float BaseZ = Floor * Out.FloorHeightCm;
            for (int32 YVertex = 0; YVertex <= YBayCount; ++YVertex)
            {
                const float Y = FrontY + YVertex * HearthAppearanceGridCm;
                AddComposedPart(Out, TEXT("post_timber_2_4m"), TEXT("post"), Volume, FVector(-200.f, Y, BaseZ));
                AddComposedPart(Out, TEXT("post_timber_2_4m"), TEXT("post"), Volume, FVector(200.f, Y, BaseZ));
            }
            const float BeamHeights[] = {230.f, 250.f, 270.f};
            for (const float BeamHeight : BeamHeights)
            {
                const float Z = BaseZ + BeamHeight;
                for (int32 YBay = 0; YBay < YBayCount; ++YBay)
                {
                    const float Y = FrontY + (YBay + .5f) * HearthAppearanceGridCm;
                    AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_side"), Volume, FVector(-200.f, Y, Z), 90.f);
                    AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_side"), Volume, FVector(200.f, Y, Z), -90.f);
                }
                AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_front"), Volume, FVector(-100.f, FrontY, Z));
                AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_front"), Volume, FVector(100.f, FrontY, Z));
                AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_back"), Volume, FVector(-100.f, BackY, Z), 180.f);
                AddComposedPart(Out, TEXT("beam_timber_2m"), TEXT("beam_back"), Volume, FVector(100.f, BackY, Z), 180.f);
            }
        }
    }

    void AddComposedWalls(FHearthBuildingAppearance& Out, const FString& WallMaterial,
        const FComposedVolume& Volume, bool bEntranceVolume)
    {
        const FString Wall = WallId(WallMaterial);
        const FString Window = WindowId(WallMaterial);
        const FString Door = DoorId(WallMaterial);
        const int32 YBayCount = BayCount(Volume.Depth);
        const float FrontY = -Volume.Depth * .5f;
        const float BackY = Volume.Depth * .5f;
        for (int32 Floor = 0; Floor < Volume.Floors; ++Floor)
        {
            const float Z = Floor * Out.FloorHeightCm;
            for (int32 XBay = 0; XBay < 2; ++XBay)
            {
                const float X = XBay == 0 ? -100.f : 100.f;
                const bool bFrontDoor = bEntranceVolume && Floor == 0 && XBay == 0;
                AddComposedPart(Out, bFrontDoor ? Door : Window,
                    bFrontDoor ? TEXT("front_door") : TEXT("front_window"), Volume, FVector(X, FrontY, Z));
                AddComposedPart(Out, Window, TEXT("rear_window"), Volume, FVector(X, BackY, Z), 180.f);
            }
            for (int32 YBay = 0; YBay < YBayCount; ++YBay)
            {
                const float Y = FrontY + (YBay + .5f) * HearthAppearanceGridCm;
                AddComposedPart(Out, Window, TEXT("side_window"), Volume, FVector(-200.f, Y, Z), -90.f);
                AddComposedPart(Out, YBay % 2 == 0 ? Window : Wall,
                    YBay % 2 == 0 ? TEXT("side_window") : TEXT("side_wall"), Volume, FVector(200.f, Y, Z), 90.f);
            }
        }
    }

    void AddComposedRoof(FHearthBuildingAppearance& Out, const FString& WallMaterial, const FString& RoofMaterial,
        const FComposedVolume& Volume)
    {
        const FString Gable = GableId(WallMaterial);
        const FString Roof = RoofId(RoofMaterial);
        const FString Ridge = RidgeId(RoofMaterial);
        const int32 YBayCount = BayCount(Volume.Depth);
        const float FrontY = -Volume.Depth * .5f;
        const float RoofZ = Volume.Floors * Out.FloorHeightCm;
        AddComposedPart(Out, Gable, TEXT("front_gable"), Volume, FVector(0.f, FrontY, RoofZ));
        AddComposedPart(Out, Gable, TEXT("rear_gable"), Volume, FVector(0.f, -FrontY, RoofZ), 180.f);
        for (int32 YBay = 0; YBay < YBayCount; ++YBay)
        {
            const float Y = FrontY + (YBay + .5f) * HearthAppearanceGridCm;
            AddComposedPart(Out, Roof, TEXT("roof_slope_front"), Volume, FVector(0.f, Y, RoofZ), 180.f);
            AddComposedPart(Out, Roof, TEXT("roof_slope_back"), Volume, FVector(0.f, Y, RoofZ));
            AddComposedPart(Out, Ridge, TEXT("roof_ridge"), Volume, FVector(0.f, Y, RoofZ));
        }
    }

    void AddComposedPorch(FHearthBuildingAppearance& Out, const FString& RoofMaterial, const FComposedVolume& Volume)
    {
        const FString Canopy = CanopyId(RoofMaterial);
        const float FrontY = -Volume.Depth * .5f;
        for (int32 XBay = 0; XBay < 2; ++XBay)
        {
            const float X = XBay == 0 ? -100.f : 100.f;
            AddComposedPart(Out, Canopy, TEXT("front_canopy"), Volume, FVector(X, FrontY - 62.5f, 180.f));
        }
        AddComposedPart(Out, TEXT("porch_post_timber"), TEXT("porch_post"), Volume, FVector(-200.f, FrontY - 125.f, 0.f));
        AddComposedPart(Out, TEXT("porch_post_timber"), TEXT("porch_post"), Volume, FVector(200.f, FrontY - 125.f, 0.f));
        AddComposedPart(Out, TEXT("porch_steps_stone_2m"), TEXT("porch_steps"), Volume, FVector(-100.f, FrontY - 45.f, 0.f));
    }

    void ComputeComposedBounds(const FComposedLayout& Layout, FVector2D& OutCoreMin, FVector2D& OutCoreMax,
        FVector2D& OutMin, FVector2D& OutMax)
    {
        OutCoreMin = FVector2D(FLT_MAX, FLT_MAX);
        OutCoreMax = FVector2D(-FLT_MAX, -FLT_MAX);
        OutMin = FVector2D(FLT_MAX, FLT_MAX);
        OutMax = FVector2D(-FLT_MAX, -FLT_MAX);
        for (const FComposedVolume& Volume : Layout.Volumes)
        {
            const float CoreHalfWidth = 200.f;
            const float CoreHalfDepth = Volume.Depth * .5f;
            const float BoundHalfWidth = FMath::Max(HearthAppearanceRoofHalfSpanCm, HearthAppearanceGableHalfSpanCm);
            const float BoundHalfDepth = Volume.Depth * .5f;
            const FVector2D CoreCorners[] = {
                FVector2D(-CoreHalfWidth, -CoreHalfDepth), FVector2D(CoreHalfWidth, -CoreHalfDepth),
                FVector2D(-CoreHalfWidth, CoreHalfDepth), FVector2D(CoreHalfWidth, CoreHalfDepth) };
            const FVector2D BoundCorners[] = {
                FVector2D(-BoundHalfWidth, -BoundHalfDepth), FVector2D(BoundHalfWidth, -BoundHalfDepth),
                FVector2D(-BoundHalfWidth, BoundHalfDepth), FVector2D(BoundHalfWidth, BoundHalfDepth) };
            for (const FVector2D& Corner : CoreCorners)
            {
                const FVector2D P = Volume.Center + RotateLocal2D(Corner, Volume.Yaw);
                OutCoreMin.X = FMath::Min(OutCoreMin.X, P.X); OutCoreMin.Y = FMath::Min(OutCoreMin.Y, P.Y);
                OutCoreMax.X = FMath::Max(OutCoreMax.X, P.X); OutCoreMax.Y = FMath::Max(OutCoreMax.Y, P.Y);
            }
            for (const FVector2D& Corner : BoundCorners)
            {
                const FVector2D P = Volume.Center + RotateLocal2D(Corner, Volume.Yaw);
                OutMin.X = FMath::Min(OutMin.X, P.X); OutMin.Y = FMath::Min(OutMin.Y, P.Y);
                OutMax.X = FMath::Max(OutMax.X, P.X); OutMax.Y = FMath::Max(OutMax.Y, P.Y);
            }
        }
        if (Layout.bFrontPorch && Layout.Volumes.Num() > 0)
        {
            const FComposedVolume& Entry = Layout.Volumes[0];
            const FVector2D PorchCorners[] = {
                FVector2D(-200.f, -Entry.Depth * .5f - 125.f),
                FVector2D(200.f, -Entry.Depth * .5f - 125.f) };
            for (const FVector2D& Corner : PorchCorners)
            {
                const FVector2D P = Entry.Center + RotateLocal2D(Corner, Entry.Yaw);
                OutMin.X = FMath::Min(OutMin.X, P.X); OutMin.Y = FMath::Min(OutMin.Y, P.Y);
                OutMax.X = FMath::Max(OutMax.X, P.X); OutMax.Y = FMath::Max(OutMax.Y, P.Y);
            }
        }
    }
}

namespace HearthBuildingAppearance
{
    bool Build(const FString& Archetype, const FString& WallMaterial, const FString& RoofMaterial,
        uint32 Seed, bool bTown3, FHearthBuildingAppearance& Out)
    {
        Out = FHearthBuildingAppearance();
        Out.Archetype = Archetype;
        Out.bTown3 = bTown3;
        Out.FloorHeightCm = 280.f;
        const HearthBuildingAppearance_Impl::FLayout Layout = HearthBuildingAppearance_Impl::MakeLayout(Archetype, bTown3, Seed);
        Out.Floors = Layout.Floors;
        Out.RoofRidgeHeightCm = Layout.Floors * Out.FloorHeightCm + 120.f;
        Out.StairCount = FMath::Max(0, Layout.Floors - 1);
        Out.RoofSpanCount = Layout.Sections.Num();
        Out.bHasCourtyard = Layout.bHasCourtyard;
        Out.LayoutVariant = Layout.Variant;
        HearthBuildingAppearance_Impl::ComputeFootprint(Layout, Out.CoreFootprintCm, Out.OccupiedFootprintCm);
        const HearthBuildingAppearance_Impl::FSection& EntranceSection = Layout.Sections[0];
        Out.EntranceOffsetCm = FVector2D(EntranceSection.CenterX - 100.f, EntranceSection.StartY);
        Out.EntranceYaw = 0.f;

        HearthBuildingAppearance_Impl::AddFoundationAndFloors(Out, Layout.Sections, Layout.Floors);
        HearthBuildingAppearance_Impl::AddFrame(Out, Layout.Sections, Layout.Floors);
        HearthBuildingAppearance_Impl::AddWalls(Out, WallMaterial, Layout.Sections, Layout.Floors);
        HearthBuildingAppearance_Impl::AddRoof(Out, WallMaterial, RoofMaterial, Layout.Sections, Layout.Floors);
        if (Layout.bFrontPorch) HearthBuildingAppearance_Impl::AddFrontPorch(Out, RoofMaterial, Layout.Sections, Layout.bBench);
        if (Layout.bSideCanopy) HearthBuildingAppearance_Impl::AddSideCanopy(Out, RoofMaterial, Layout.Sections);
        for (int32 Floor = 0; Floor < Out.StairCount; ++Floor)
        {
            HearthBuildingAppearance_Impl::AddPart(Out, TEXT("stairs_switchback_2x4m"), TEXT("stairs"),
                FVector(EntranceSection.CenterX - 100.f, EntranceSection.StartY + 200.f, Floor * Out.FloorHeightCm));
        }
        return Validate(Out, nullptr);
    }

    bool BuildComposed(const FString& Archetype, const FString& WallMaterial, const FString& RoofMaterial,
        uint32 Seed, int32 LayoutId, FHearthBuildingAppearance& Out)
    {
        Out = FHearthBuildingAppearance();
        const HearthBuildingAppearance_Impl::FComposedLayout Layout =
            HearthBuildingAppearance_Impl::MakeComposedLayout(LayoutId, Seed);
        if (Layout.ShapeId.IsEmpty() || Layout.Volumes.IsEmpty()) return false;

        Out.Archetype = Archetype;
        Out.bTown3 = true;
        Out.bComposed = true;
        Out.FloorHeightCm = 280.f;
        Out.ShapeId = Layout.ShapeId;
        Out.LayoutVariant = FString::Printf(TEXT("v4_%s_seed_%u"), *Layout.ShapeId, Seed);
        Out.bHasCourtyard = Layout.bHasCourtyard;
        Out.MassCount = Layout.Volumes.Num();
        Out.Floors = 1;
        Out.StairCount = 0;
        Out.RoofSpanCount = 0;

        FVector2D CoreMin, CoreMax, OccupiedMin, OccupiedMax;
        HearthBuildingAppearance_Impl::ComputeComposedBounds(Layout, CoreMin, CoreMax, OccupiedMin, OccupiedMax);
        Out.CoreFootprintCm = CoreMax - CoreMin;
        Out.OccupiedFootprintCm = OccupiedMax - OccupiedMin;
        Out.ShapeBoundsMinCm = OccupiedMin;
        Out.ShapeBoundsMaxCm = OccupiedMax;

        const HearthBuildingAppearance_Impl::FComposedVolume& Entrance = Layout.Volumes[0];
        const FVector EntranceWorld = HearthBuildingAppearance_Impl::ComposedOffset(
            Entrance, FVector(-100.f, -Entrance.Depth * .5f, 0.f));
        Out.EntranceOffsetCm = FVector2D(EntranceWorld.X, EntranceWorld.Y);
        Out.EntranceYaw = Entrance.Yaw;

        for (int32 VolumeIndex = 0; VolumeIndex < Layout.Volumes.Num(); ++VolumeIndex)
        {
            const HearthBuildingAppearance_Impl::FComposedVolume& Volume = Layout.Volumes[VolumeIndex];
            Out.Floors = FMath::Max(Out.Floors, Volume.Floors);
            Out.HeightTiers.Add(Volume.Floors);
            Out.RoofSpanCount += HearthBuildingAppearance_Impl::BayCount(Volume.Depth);
            Out.StairCount += FMath::Max(0, Volume.Floors - 1);

            HearthBuildingAppearance_Impl::AddComposedFoundationAndFloors(Out, Volume);
            HearthBuildingAppearance_Impl::AddComposedFrame(Out, Volume);
            HearthBuildingAppearance_Impl::AddComposedWalls(Out, WallMaterial, Volume, VolumeIndex == 0);
            HearthBuildingAppearance_Impl::AddComposedRoof(Out, WallMaterial, RoofMaterial, Volume);
            if (VolumeIndex == 0 && Layout.bFrontPorch)
            {
                HearthBuildingAppearance_Impl::AddComposedPorch(Out, RoofMaterial, Volume);
            }
            for (int32 Floor = 0; Floor < Volume.Floors - 1; ++Floor)
            {
                HearthBuildingAppearance_Impl::AddComposedPart(Out, TEXT("stairs_switchback_2x4m"), TEXT("stairs"),
                    Volume, FVector(-100.f, 0.f, Floor * Out.FloorHeightCm));
            }
        }
        Out.RoofRidgeHeightCm = Out.Floors * Out.FloorHeightCm + 120.f;
        return Validate(Out, nullptr);
    }

    bool Validate(const FHearthBuildingAppearance& Appearance, FString* OutError)
    {
        auto Fail = [&](const FString& Error)
        {
            if (OutError) *OutError = Error;
            return false;
        };
        if (Appearance.Archetype.IsEmpty()) return Fail(TEXT("missing_archetype"));
        if (Appearance.LayoutVariant.IsEmpty()) return Fail(TEXT("missing_layout_variant"));
        if (!FMath::IsNearlyEqual(Appearance.FloorHeightCm, 280.f, .01f)) return Fail(TEXT("invalid_floor_height"));
        if (Appearance.Floors < 1 || Appearance.CoreFootprintCm.X < 400.f || Appearance.CoreFootprintCm.Y < 200.f) return Fail(TEXT("invalid_core_footprint"));
        if (Appearance.Parts.IsEmpty() || Appearance.RoofSpanCount < 1) return Fail(TEXT("empty_appearance"));
        int32 ExpectedStairs = FMath::Max(0, Appearance.Floors - 1);
        if (Appearance.bComposed)
        {
            if (Appearance.MassCount < 1 || Appearance.HeightTiers.Num() != Appearance.MassCount || Appearance.ShapeId.IsEmpty())
                return Fail(TEXT("invalid_composed_metadata"));
            ExpectedStairs = 0;
            for (const int32 Tier : Appearance.HeightTiers) ExpectedStairs += FMath::Max(0, Tier - 1);
            if (Appearance.OccupiedFootprintCm.X > 1400.f || Appearance.OccupiedFootprintCm.Y > 1400.f)
                return Fail(TEXT("composed_footprint_exceeds_contract"));
            const FVector2D ShapeSize = Appearance.ShapeBoundsMaxCm - Appearance.ShapeBoundsMinCm;
            if (!ShapeSize.Equals(Appearance.OccupiedFootprintCm, .01f)) return Fail(TEXT("composed_bounds_mismatch"));
        }
        else if (Appearance.bTown3 && (Appearance.OccupiedFootprintCm.X > 950.f || Appearance.OccupiedFootprintCm.Y > 1125.f))
            return Fail(TEXT("town3_footprint_exceeds_contract"));
        else if (!Appearance.bTown3 && (Appearance.OccupiedFootprintCm.X > 500.f || Appearance.OccupiedFootprintCm.Y > 500.f))
            return Fail(TEXT("town2_footprint_exceeds_contract"));
        if (Appearance.StairCount != ExpectedStairs) return Fail(TEXT("stair_transition_count_mismatch"));
        for (const FHearthBuildingAppearancePart& Part : Appearance.Parts)
        {
            if (Part.AssetId.IsEmpty() || !Part.AssetPath.StartsWith(TEXT("/Game/ThreeHearths/Generated/VillageKit/"))) return Fail(TEXT("non_native_asset"));
            if (!Part.Scale.Equals(FVector::OneVector, .001f)) return Fail(TEXT("starter_part_scaled"));
            if (Part.AssetId == TEXT("Cube")) return Fail(TEXT("solid_cube_part"));
            if (!FMath::IsFinite(Part.Offset.X) || !FMath::IsFinite(Part.Offset.Y) || !FMath::IsFinite(Part.Offset.Z) || !FMath::IsFinite(Part.Yaw)) return Fail(TEXT("non_finite_part"));
        }
        const bool bHasDoor = Appearance.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role == TEXT("front_door"); });
        const bool bHasGable = Appearance.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role == TEXT("front_gable"); });
        const bool bHasRoof = Appearance.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role == TEXT("roof_slope_front"); });
        const bool bHasRidge = Appearance.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role == TEXT("roof_ridge"); });
        const bool bHasWindow = Appearance.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role.Contains(TEXT("window")); });
        int32 ActualStairs = 0;
        for (const FHearthBuildingAppearancePart& Part : Appearance.Parts) if (Part.Role == TEXT("stairs")) ++ActualStairs;
        return bHasDoor && bHasGable && bHasRoof && bHasRidge && bHasWindow && ActualStairs == Appearance.StairCount
            ? true : Fail(TEXT("missing_visible_opening_roof_or_stair_parts"));
    }
}
