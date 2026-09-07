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
        if (Appearance.StairCount != FMath::Max(0, Appearance.Floors - 1)) return Fail(TEXT("stair_transition_count_mismatch"));
        if (Appearance.bTown3 && (Appearance.OccupiedFootprintCm.X > 950.f || Appearance.OccupiedFootprintCm.Y > 1125.f)) return Fail(TEXT("town3_footprint_exceeds_contract"));
        if (!Appearance.bTown3 && (Appearance.OccupiedFootprintCm.X > 500.f || Appearance.OccupiedFootprintCm.Y > 500.f)) return Fail(TEXT("town2_footprint_exceeds_contract"));
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
