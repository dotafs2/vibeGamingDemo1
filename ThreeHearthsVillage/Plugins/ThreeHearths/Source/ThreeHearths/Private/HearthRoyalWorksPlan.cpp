#include "HearthRoyalWorksPlan.h"

namespace HearthRoyalWorksPlan
{
    namespace
    {
        const TCHAR* const Cube = TEXT("/Engine/BasicShapes/Cube");
        const TCHAR* const Cone = TEXT("/Engine/BasicShapes/Cone");

        const FLinearColor Stone(0.34f, 0.36f, 0.38f, 1.f);
        const FLinearColor Trim(0.49f, 0.47f, 0.42f, 1.f);
        const FLinearColor Timber(0.38f, 0.20f, 0.09f, 1.f);
        const FLinearColor Dark(0.10f, 0.08f, 0.06f, 1.f);
        const FLinearColor Roof(0.12f, 0.15f, 0.19f, 1.f);
        const FLinearColor Leaf(0.18f, 0.42f, 0.16f, 1.f);
        const FLinearColor Blossom(0.72f, 0.35f, 0.42f, 1.f);

        void AddMesh(FHearthRoyalWorksPlan& Plan, const TCHAR* Id, const TCHAR* MeshPath,
            const FVector& CenterCm, const FVector& SizeCm, int32 Stage,
            const FIntVector& Materials, const FLinearColor& Color)
        {
            FHearthRoyalModule Module;
            Module.Id = Id;
            Module.MeshPath = MeshPath;
            Module.Offset = CenterCm;
            // Engine Cube/Cone have centered 100cm bounds at unit scale.
            // All dimensions below are full lengths in cm, never half extents.
            Module.Scale = SizeCm / 100.f;
            Module.Stage = Stage;
            Module.Materials = Materials;
            Module.Color = Color;
            Plan.Modules.Add(MoveTemp(Module));
        }

        void AddPlant(FHearthRoyalWorksPlan& Plan, const TCHAR* Id, const TCHAR* PlantId,
            const FVector& RootCm, float CatalogScale, const FIntVector& Materials,
            const FLinearColor& Color)
        {
            FHearthRoyalModule Module;
            Module.Id = Id;
            Module.PlantId = PlantId;
            Module.Offset = RootCm;
            Module.Scale = FVector(CatalogScale);
            Module.Stage = 13;
            Module.Materials = Materials;
            Module.Color = Color;
            Plan.Modules.Add(MoveTemp(Module));
        }
    }

    FHearthRoyalWorksPlan Build()
    {
        FHearthRoyalWorksPlan Plan;
        Plan.TemplateId = TEXT("royal_keep_garden_v1");
        Plan.Radius = 850.f;
        Plan.Modules.Reserve(44);

        // Materials are abstract stone/plank/beam shares: total (73, 46, 40).
        // Stages are dependency barriers, including for simultaneous workers.
        // Keep 1-5 -> gate/curtain 6-9 -> wings 10-12 -> garden 13.
        // Local -Y faces the gate; |X| <= 90cm stays clear up to the door at Y=114cm.

        // Stage 1: Keep-only footings cost six stone shares; the front step rises 10cm, then 20cm.
        AddMesh(Plan, TEXT("keep_foundation"), Cube, FVector(0.f, 300.f, 10.f),
            FVector(460.f, 380.f, 20.f), 1, FIntVector(5, 0, 0), Stone);
        AddMesh(Plan, TEXT("keep_threshold"), Cube, FVector(0.f, 65.f, 5.f),
            FVector(160.f, 90.f, 10.f), 1, FIntVector(1, 0, 0), Trim);

        // Stage 2: A solid 4.4m x 3.6m lower storey makes the keep visible early.
        AddMesh(Plan, TEXT("keep_lower_body"), Cube, FVector(0.f, 300.f, 200.f),
            FVector(440.f, 360.f, 360.f), 2, FIntVector(8, 3, 3), Stone);

        // Stage 3: The enclosed upper storey bears across its complete footprint.
        AddMesh(Plan, TEXT("keep_upper_body"), Cube, FVector(0.f, 300.f, 540.f),
            FVector(400.f, 320.f, 320.f), 3, FIntVector(7, 3, 3), Stone);

        // Stage 4: Turret and battlements sit on the completed upper storey. Door/windows are facade inlays.
        AddMesh(Plan, TEXT("keep_tower"), Cube, FVector(0.f, 300.f, 790.f),
            FVector(180.f, 180.f, 180.f), 4, FIntVector(4, 1, 2), Stone);
        AddMesh(Plan, TEXT("keep_merlon_west_front"), Cube, FVector(-170.f, 170.f, 730.f),
            FVector(60.f, 60.f, 60.f), 4, FIntVector(1, 0, 0), Trim);
        AddMesh(Plan, TEXT("keep_merlon_west_back"), Cube, FVector(-170.f, 430.f, 730.f),
            FVector(60.f, 60.f, 60.f), 4, FIntVector(1, 0, 0), Trim);
        AddMesh(Plan, TEXT("keep_merlon_east_front"), Cube, FVector(170.f, 170.f, 730.f),
            FVector(60.f, 60.f, 60.f), 4, FIntVector(1, 0, 0), Trim);
        AddMesh(Plan, TEXT("keep_merlon_east_back"), Cube, FVector(170.f, 430.f, 730.f),
            FVector(60.f, 60.f, 60.f), 4, FIntVector(1, 0, 0), Trim);
        AddMesh(Plan, TEXT("keep_door"), Cube, FVector(0.f, 118.f, 130.f),
            FVector(140.f, 8.f, 220.f), 4, FIntVector(0, 2, 1), Timber);
        AddMesh(Plan, TEXT("keep_window_west"), Cube, FVector(-110.f, 138.f, 520.f),
            FVector(40.f, 8.f, 100.f), 4, FIntVector(0, 1, 0), Dark);
        AddMesh(Plan, TEXT("keep_window_east"), Cube, FVector(110.f, 138.f, 520.f),
            FVector(40.f, 8.f, 100.f), 4, FIntVector(0, 1, 0), Dark);

        // Stage 5: The central spire reaches 1040cm; its full base fits on the turret.
        AddMesh(Plan, TEXT("keep_roof"), Cone, FVector(0.f, 300.f, 960.f),
            FVector(180.f, 180.f, 160.f), 5, FIntVector(0, 4, 3), Roof);

        // Stage 6: Gate and continuous curtain footings follow the complete keep. Outermost corner: (540, 640).
        AddMesh(Plan, TEXT("gate_foundation_west"), Cube, FVector(-180.f, -480.f, 10.f),
            FVector(160.f, 180.f, 20.f), 6, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("gate_foundation_east"), Cube, FVector(180.f, -480.f, 10.f),
            FVector(160.f, 180.f, 20.f), 6, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("curtain_foundation_west"), Cube, FVector(-520.f, 70.f, 10.f),
            FVector(40.f, 1140.f, 20.f), 6, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("curtain_foundation_east"), Cube, FVector(520.f, 70.f, 10.f),
            FVector(40.f, 1140.f, 20.f), 6, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("curtain_foundation_north"), Cube, FVector(0.f, 620.f, 10.f),
            FVector(1080.f, 40.f, 20.f), 6, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("curtain_foundation_south_west"), Cube, FVector(-390.f, -480.f, 10.f),
            FVector(300.f, 40.f, 20.f), 6, FIntVector(1, 0, 0), Stone);
        AddMesh(Plan, TEXT("curtain_foundation_south_east"), Cube, FVector(390.f, -480.f, 10.f),
            FVector(300.f, 40.f, 20.f), 6, FIntVector(1, 0, 0), Stone);

        // Stage 7: All wall footprints fit their foundations. The gate has 220cm between its towers.
        AddMesh(Plan, TEXT("gatehouse_west"), Cube, FVector(-180.f, -480.f, 180.f),
            FVector(140.f, 160.f, 320.f), 7, FIntVector(3, 1, 1), Stone);
        AddMesh(Plan, TEXT("gatehouse_east"), Cube, FVector(180.f, -480.f, 180.f),
            FVector(140.f, 160.f, 320.f), 7, FIntVector(3, 1, 1), Stone);
        AddMesh(Plan, TEXT("curtain_wall_west"), Cube, FVector(-520.f, 70.f, 130.f),
            FVector(30.f, 1130.f, 220.f), 7, FIntVector(4, 1, 1), Stone);
        AddMesh(Plan, TEXT("curtain_wall_east"), Cube, FVector(520.f, 70.f, 130.f),
            FVector(30.f, 1130.f, 220.f), 7, FIntVector(4, 1, 1), Stone);
        AddMesh(Plan, TEXT("curtain_wall_north"), Cube, FVector(0.f, 620.f, 130.f),
            FVector(1070.f, 30.f, 220.f), 7, FIntVector(4, 1, 1), Stone);
        AddMesh(Plan, TEXT("curtain_wall_south_west"), Cube, FVector(-392.5f, -480.f, 130.f),
            FVector(285.f, 30.f, 220.f), 7, FIntVector(2, 0, 1), Stone);
        AddMesh(Plan, TEXT("curtain_wall_south_east"), Cube, FVector(392.5f, -480.f, 130.f),
            FVector(285.f, 30.f, 220.f), 7, FIntVector(2, 0, 1), Stone);

        // Stage 8: A 500cm lintel has 140cm of bearing on EACH tower and 340cm headroom.
        AddMesh(Plan, TEXT("gate_lintel"), Cube, FVector(0.f, -480.f, 360.f),
            FVector(500.f, 160.f, 40.f), 8, FIntVector(2, 2, 3), Trim);

        // Stage 9: Both gate roofs bear on the completed lintel.
        AddMesh(Plan, TEXT("gatehouse_roof_west"), Cone, FVector(-180.f, -480.f, 440.f),
            FVector(140.f, 140.f, 120.f), 9, FIntVector(0, 2, 2), Roof);
        AddMesh(Plan, TEXT("gatehouse_roof_east"), Cone, FVector(180.f, -480.f, 440.f),
            FVector(140.f, 140.f, 120.f), 9, FIntVector(0, 2, 2), Roof);

        // Stage 10: Attached side-wing foundations; their tops match the keep foundation.
        AddMesh(Plan, TEXT("side_wing_west_foundation"), Cube, FVector(-350.f, 330.f, 10.f),
            FVector(280.f, 280.f, 20.f), 10, FIntVector(2, 0, 0), Stone);
        AddMesh(Plan, TEXT("side_wing_east_foundation"), Cube, FVector(350.f, 330.f, 10.f),
            FVector(280.f, 280.f, 20.f), 10, FIntVector(2, 0, 0), Stone);

        // Stage 11: Solid timber wings touch the lower keep at local X = +/-220cm.
        AddMesh(Plan, TEXT("side_wing_west"), Cube, FVector(-350.f, 330.f, 160.f),
            FVector(260.f, 260.f, 280.f), 11, FIntVector(1, 5, 3), Timber);
        AddMesh(Plan, TEXT("side_wing_east"), Cube, FVector(350.f, 330.f, 160.f),
            FVector(260.f, 260.f, 280.f), 11, FIntVector(1, 5, 3), Timber);

        // Stage 12: Wing roofs sit on the bodies; front beams penetrate the facade by 4cm.
        AddMesh(Plan, TEXT("side_wing_west_roof"), Cone, FVector(-350.f, 330.f, 350.f),
            FVector(260.f, 260.f, 100.f), 12, FIntVector(0, 3, 2), Roof);
        AddMesh(Plan, TEXT("side_wing_east_roof"), Cone, FVector(350.f, 330.f, 350.f),
            FVector(260.f, 260.f, 100.f), 12, FIntVector(0, 3, 2), Roof);
        AddMesh(Plan, TEXT("side_wing_west_beam"), Cube, FVector(-350.f, 198.f, 280.f),
            FVector(260.f, 12.f, 24.f), 12, FIntVector(0, 0, 1), Dark);
        AddMesh(Plan, TEXT("side_wing_east_beam"), Cube, FVector(350.f, 198.f, 280.f),
            FVector(260.f, 12.f, 24.f), 12, FIntVector(0, 0, 1), Dark);

        // Stage 13: Grounded catalog plants. Costs represent edging, planting stakes and bed boards, not new currencies.
        AddPlant(Plan, TEXT("court_oak"), TEXT("oak"), FVector(-315.f, -100.f, 0.f),
            0.65f, FIntVector(0, 1, 1), Leaf);
        AddPlant(Plan, TEXT("court_birch"), TEXT("birch"), FVector(315.f, -100.f, 0.f),
            0.75f, FIntVector(0, 1, 1), Leaf);
        AddPlant(Plan, TEXT("court_flowering_shrub_west"), TEXT("flowering_shrub"), FVector(-360.f, -320.f, 0.f),
            0.6f, FIntVector(1, 0, 0), Blossom);
        AddPlant(Plan, TEXT("court_flowering_shrub_east"), TEXT("flowering_shrub"), FVector(360.f, -320.f, 0.f),
            0.6f, FIntVector(1, 0, 0), Blossom);
        AddPlant(Plan, TEXT("court_wildflowers_west"), TEXT("wildflowers"), FVector(-320.f, 100.f, 0.f),
            0.6f, FIntVector(0, 1, 0), Blossom);
        AddPlant(Plan, TEXT("court_wildflowers_east"), TEXT("wildflowers"), FVector(320.f, 100.f, 0.f),
            0.6f, FIntVector(0, 1, 0), Blossom);

        return Plan;
    }
}

// Keep the v2 builder in a named namespace with unique helper names. UE unity
// builds concatenate private translation units, so generic anonymous-namespace
// helper names are deliberately avoided here.
namespace HearthRoyalWorksPlan::CastleV2Detail
{
    namespace
    {
        const TCHAR* const V2Cube = TEXT("/Engine/BasicShapes/Cube");
        const TCHAR* const V2Cone = TEXT("/Engine/BasicShapes/Cone");
        const FLinearColor V2Stone(0.31f, 0.33f, 0.35f, 1.f);
        const FLinearColor V2Trim(0.55f, 0.49f, 0.38f, 1.f);
        const FLinearColor V2Timber(0.32f, 0.16f, 0.07f, 1.f);
        const FLinearColor V2Roof(0.10f, 0.13f, 0.18f, 1.f);
        const FLinearColor V2Floor(0.24f, 0.22f, 0.18f, 1.f);
        const FLinearColor V2Garden(0.18f, 0.40f, 0.14f, 1.f);
        const FLinearColor V2Flower(0.68f, 0.29f, 0.38f, 1.f);
        const TCHAR* const V2WallStone = TEXT("/Game/ThreeHearths/Generated/VillageKit/wall_stone_2m/wall_stone_2m.wall_stone_2m");
        const TCHAR* const V2WallWindow = TEXT("/Game/ThreeHearths/Generated/VillageKit/wall_window_timber_2m/wall_window_timber_2m.wall_window_timber_2m");
        const TCHAR* const V2WallDoor = TEXT("/Game/ThreeHearths/Generated/VillageKit/wall_door_stone_2m/wall_door_stone_2m.wall_door_stone_2m");
        const TCHAR* const V2FloorTile = TEXT("/Game/ThreeHearths/Generated/VillageKit/floor_timber_2m/floor_timber_2m.floor_timber_2m");
        const TCHAR* const V2FloorOpening = TEXT("/Game/ThreeHearths/Generated/VillageKit/floor_opening_2m/floor_opening_2m.floor_opening_2m");
        const TCHAR* const V2Stairs = TEXT("/Game/ThreeHearths/Generated/VillageKit/stairs_switchback_2x4m/stairs_switchback_2x4m.stairs_switchback_2x4m");
        const TCHAR* const V2Post = TEXT("/Game/ThreeHearths/Generated/VillageKit/post_timber_2_4m/post_timber_2_4m.post_timber_2_4m");
        const TCHAR* const V2Beam = TEXT("/Game/ThreeHearths/Generated/VillageKit/beam_timber_2m/beam_timber_2m.beam_timber_2m");
        const TCHAR* const V2RoofSlope = TEXT("/Game/ThreeHearths/Generated/VillageKit/roof_slope_terracotta_2m/roof_slope_terracotta_2m.roof_slope_terracotta_2m");
        const TCHAR* const V2RoofRidge = TEXT("/Game/ThreeHearths/Generated/VillageKit/roof_ridge_terracotta_2m/roof_ridge_terracotta_2m.roof_ridge_terracotta_2m");
        const TCHAR* const V2Gable = TEXT("/Game/ThreeHearths/Generated/VillageKit/gable_timber_4m/gable_timber_4m.gable_timber_4m");

        void AddCastleV2Mesh(FHearthRoyalWorksPlan& Plan, const FString& Id, const TCHAR* MeshPath,
            const FVector& CenterCm, const FVector& SizeCm, int32 Stage, const FIntVector& Materials,
            const FLinearColor& Color)
        {
            FHearthRoyalModule Module;
            Module.Id = Id;
            Module.MeshPath = MeshPath;
            Module.Offset = CenterCm;
            Module.Scale = SizeCm / 100.f;
            Module.BoundsSizeCm = SizeCm;
            Module.Stage = Stage;
            Module.Materials = Materials;
            Module.Color = Color;
            Plan.Modules.Add(MoveTemp(Module));
        }

        // The assembly uses Blender's author frame: GLB (x,y,z) -> (x,-z,y)
        // in centimetres. UE imports the same vertices as (x,+z,y), so the
        // native mesh's LOCAL Y must be reflected before assembly yaw. This
        // applies uniformly to VillageKit geometry, never to world/site Y.
        const FVector V2NativeToAssemblyAxes(1.f, -1.f, 1.f);

        // These are ASSEMBLY-frame bounds, not loaded UStaticMesh bounds.
        // For example stairs have assembly centre Y=-28.25 but UE Y=+28.25.
        // MeshTransform uses the actual imported bounds to correct the pivot.
        FBox CastleV2AssemblyBounds(const TCHAR* MeshPath)
        {
            struct FSource { const TCHAR* Path; FBox Bounds; };
            static const FSource Sources[] = {
                {V2WallStone, FBox(FVector(-90.4000044, -7.9999998, 16.5999994), FVector(90.4000103, 7.9999998, 223.3999968))},
                {V2WallWindow, FBox(FVector(-90.6000018, -27.0000011, 15.9999967), FVector(90.5999959, 9.2000000, 224.0000010))},
                {V2WallDoor, FBox(FVector(-90.4000044, -12.7999991, 12.4999970), FVector(90.4000103, 9.2000000, 223.3999968))},
                {V2FloorTile, FBox(FVector(-100.0000000, -100.0000000, 0.0000000), FVector(100.0000000, 100.0000000, 15.9999996))},
                {V2FloorOpening, FBox(FVector(-100.0000000, -100.0000000, 0.0000000), FVector(99.9999940, 100.0000000, 15.9999996))},
                {V2Post, FBox(FVector(-9.0000004, -9.0000004, 0.0000000), FVector(9.0000004, 9.0000004, 240.0000095))},
                {V2Beam, FBox(FVector(-91.0000026, -9.0000004, 0.0000000), FVector(91.0000026, 9.0000004, 20.0000003))},
                {V2RoofSlope, FBox(FVector(-4.3400452, -100.0000000, -13.0000085), FVector(225.0000000, 100.0000000, 128.4100413))},
                {V2RoofRidge, FBox(FVector(-15.0000006, -100.0000000, 122.0000029), FVector(15.0000006, 100.0000000, 140.9000039))},
                {V2Gable, FBox(FVector(-202.6208878, -17.5000072, -4.8503995), FVector(202.6208878, 7.9999998, 124.8504043))},
                {V2Stairs, FBox(FVector(-93.4999943, -191.9999957, 11.5000010), FVector(93.4999943, 135.5000138, 333.8973522))},
            };
            for (const FSource& Source : Sources)
                if (FCString::Strcmp(Source.Path, MeshPath) == 0) return Source.Bounds;
            checkNoEntry();
            return FBox(FVector(-50), FVector(50));
        }

        void AddCastleV2Native(FHearthRoyalWorksPlan& Plan, const FString& Id, const TCHAR* MeshPath,
            const FVector& CenterCm, const FVector& WorldSizeCm, int32 Stage,
            const FIntVector& Materials, const FLinearColor& Color, float Yaw = 0.f)
        {
            const FBox Source = CastleV2AssemblyBounds(MeshPath);
            const FRotator Rotation(0, Yaw, 0);
            FHearthRoyalModule Module;
            Module.Id = Id; Module.MeshPath = MeshPath; Module.Offset = CenterCm;
            Module.Scale = Rotation.UnrotateVector(WorldSizeCm).GetAbs() / Source.GetSize() * V2NativeToAssemblyAxes;
            const FVector HalfSize = Source.GetExtent() * Module.Scale.GetAbs();
            Module.BoundsSizeCm = FBox(-HalfSize, HalfSize).TransformBy(FTransform(Rotation)).GetSize();
            Module.bCenterMeshAtOffset = true;
            Module.Yaw = Yaw; Module.Stage = Stage; Module.Materials = Materials; Module.Color = Color;
            Plan.Modules.Add(MoveTemp(Module));
        }

        // Place a native module at unit size by its authored assembly datum.
        // The local basis reflection is already included in AddCastleV2Native;
        // OriginCm and the stairs' planned platform/well coordinates stay put.
        // Render-time centring also tolerates the importer rebasing its pivot.
        void AddCastleV2AtOrigin(FHearthRoyalWorksPlan& Plan, const FString& Id, const TCHAR* MeshPath,
            const FVector& OriginCm, int32 Stage, const FIntVector& Materials,
            const FLinearColor& Color, float Yaw = 0.f)
        {
            const FBox Source = CastleV2AssemblyBounds(MeshPath);
            const FRotator Rotation(0, Yaw, 0);
            const FVector Half = Source.GetExtent();
            const FVector Size = FBox(-Half, Half).TransformBy(FTransform(Rotation)).GetSize();
            AddCastleV2Native(Plan, Id, MeshPath, OriginCm + Rotation.RotateVector(Source.GetCenter()),
                Size, Stage, Materials, Color, Yaw);
        }

        // Each 4m bay has two slopes sharing ONE ridge projection, repeated
        // along Y in 2m strips. Neither cap nor gable is stretched across a hall.
        void AddCastleV2RoofBays(FHearthRoyalWorksPlan& Plan, const FString& Prefix,
            float X, float Y, float EaveZ, int32 Bays, int32 Strips, int32 Stage)
        {
            for (int32 Bay = 0; Bay < Bays; ++Bay)
            {
                const float RidgeX = X + 400.f * (Bay - .5f * (Bays - 1));
                for (int32 Strip = 0; Strip < Strips; ++Strip)
                {
                    const FVector Origin(RidgeX, Y + 200.f * (Strip - .5f * (Strips - 1)), EaveZ);
                    const FString Id = Prefix + FString::Printf(TEXT("bay_%02d_strip_%02d_"), Bay, Strip);
                    AddCastleV2AtOrigin(Plan, Id + TEXT("slope_east"), V2RoofSlope, Origin, Stage, FIntVector(0,3,2), V2Roof);
                    AddCastleV2AtOrigin(Plan, Id + TEXT("slope_west"), V2RoofSlope, Origin, Stage, FIntVector(0,3,2), V2Roof, 180.f);
                    AddCastleV2AtOrigin(Plan, Id + TEXT("ridge"), V2RoofRidge, Origin, Stage, FIntVector(0,2,1), V2Roof);
                }
                for (int32 End = 0; End < 2; ++End)
                    AddCastleV2AtOrigin(Plan, Prefix + FString::Printf(TEXT("bay_%02d_gable_%d"), Bay, End),
                        V2Gable, FVector(RidgeX, Y + (End == 0 ? -1.f : 1.f) * Strips * 100.f, EaveZ),
                        Stage, FIntVector(0,2,1), V2Trim);
            }
        }

        void AddCastleV2Plant(FHearthRoyalWorksPlan& Plan, const FString& Id, const TCHAR* PlantId,
            const FVector& RootCm, float Scale, const FIntVector& Materials, const FLinearColor& Color)
        {
            FHearthRoyalModule Module;
            Module.Id = Id;
            Module.PlantId = PlantId;
            Module.Offset = RootCm;
            Module.Scale = FVector(Scale);
            Module.BoundsSizeCm = FVector::ZeroVector;
            Module.Stage = 13;
            Module.Materials = Materials;
            Module.Color = Color;
            Plan.Modules.Add(MoveTemp(Module));
        }

        void AddCastleV2WallSegment(FHearthRoyalWorksPlan& Plan, const FString& Id, const FVector& Center,
            bool bAlongX, int32 Stage, bool bWindow = false, bool bDoor = false)
        {
            const TCHAR* Asset = bDoor ? V2WallDoor : (bWindow ? V2WallWindow : V2WallStone);
            const FVector BoundsCm = bAlongX ? FVector(200.f, 18.f, 560.f) : FVector(18.f, 200.f, 560.f);

            const FIntVector Materials = bDoor ? FIntVector(0, 2, 1) : (bWindow ? FIntVector(0, 1, 0) : FIntVector(4, 0, 1));
            AddCastleV2Native(Plan, Id, Asset, Center, BoundsCm, Stage, Materials, bDoor ? V2Timber : V2Stone, bAlongX ? 0.f : 90.f);
        }

        void AddCastleV2TowerLevel(FHearthRoyalWorksPlan& Plan, int32 Level, int32 Stage)
        {
            const float Z = 360.f + 600.f * Level;
            const FString Prefix = FString::Printf(TEXT("tower_level_%d_"), Level);
            for (int32 Segment = 0; Segment < 9; ++Segment)
            {
                const float Along = -800.f + 200.f * Segment;
                const bool bWindow = Level > 0 && Segment == 4;
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("north_%02d"), Segment), FVector(Along, 1550.f, Z), true, Stage, bWindow);
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("west_%02d"), Segment), FVector(-820.f, 650.f + Along, Z), false, Stage, bWindow);
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("east_%02d"), Segment), FVector(820.f, 650.f + Along, Z), false, Stage, bWindow);
                if (Level > 0 || Segment != 4)
                    AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("south_%02d"), Segment), FVector(Along, -250.f, Z), true, Stage);
            }
        }

        void AddCastleV2TowerFrame(FHearthRoyalWorksPlan& Plan, int32 Level, int32 Stage)
        {
            const float FloorZ = 80.f + 600.f * (Level - 1);
            const float BeamZ = 600.f + 600.f * (Level - 1);
            const FString Prefix = FString::Printf(TEXT("tower_frame_%d_"), Level);
            AddCastleV2Native(Plan, Prefix + TEXT("north_beam"), V2Beam, FVector(0.f, 1430.f, BeamZ), FVector(1600.f, 80.f, 80.f), Stage, FIntVector(0, 0, 2), V2Timber);
            AddCastleV2Native(Plan, Prefix + TEXT("south_beam"), V2Beam, FVector(0.f, -130.f, BeamZ), FVector(1600.f, 80.f, 80.f), Stage, FIntVector(0, 0, 2), V2Timber);
            AddCastleV2Native(Plan, Prefix + TEXT("west_beam"), V2Beam, FVector(-730.f, 650.f, BeamZ), FVector(1600.f, 80.f, 80.f), Stage, FIntVector(0, 0, 2), V2Timber, 90.f);
            AddCastleV2Native(Plan, Prefix + TEXT("east_beam"), V2Beam, FVector(730.f, 650.f, BeamZ), FVector(1600.f, 80.f, 80.f), Stage, FIntVector(0, 0, 2), V2Timber, 90.f);
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                const float X = Corner % 2 == 0 ? -760.f : 760.f;
                const float Y = Corner < 2 ? -190.f : 1490.f;
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("post_%d"), Corner), V2Post,
                    FVector(X, Y, FloorZ + 240.f), FVector(120.f, 120.f, 480.f), Stage, FIntVector(0, 0, 2), V2Timber);
            }
        }

        void AddCastleV2SmallTower(FHearthRoyalWorksPlan& Plan, const FString& Prefix, float X, int32 Stage)
        {
            const float Y = -2650.f;
            for (int32 Segment = 0; Segment < 3; ++Segment)
            {
                const float AlongX = X - 200.f + 200.f * Segment;
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("north_wall_%02d"), Segment), V2WallStone,
                    FVector(AlongX, Y + 300.f, 330.f), FVector(200.f, 18.f, 500.f), Stage, FIntVector(2, 0, 1), V2Stone);
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("south_wall_%02d"), Segment), V2WallStone,
                    FVector(AlongX, Y - 300.f, 330.f), FVector(200.f, 18.f, 500.f), Stage, FIntVector(2, 0, 1), V2Stone);
            }
            for (int32 Segment = 0; Segment < 4; ++Segment)
            {
                const float AlongY = Y - 300.f + 200.f * Segment;
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("west_wall_%02d"), Segment), V2WallStone,
                    FVector(X - 200.f, AlongY, 330.f), FVector(18.f, 200.f, 500.f), Stage, FIntVector(2, 0, 1), V2Stone, 90.f);
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("east_wall_%02d"), Segment), V2WallStone,
                    FVector(X + 200.f, AlongY, 330.f), FVector(18.f, 200.f, 500.f), Stage, FIntVector(2, 0, 1), V2Stone, 90.f);
            }
        }

        void AddCastleV2Wing(FHearthRoyalWorksPlan& Plan, const TCHAR* Side, float X)
        {
            const FString Prefix = FString::Printf(TEXT("%s_wing_"), Side);
            for (int32 Segment = 0; Segment < 9; ++Segment)
            {
                const float AlongX = X - 800.f + 200.f * Segment;
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("north_wall_%02d"), Segment), FVector(AlongX, 1590.f, 360.f), true, 3);
                // The southern facade is built with a door component in the
                // wall run itself; no closed wall is hidden behind a decal.
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("south_wall_%02d"), Segment), FVector(AlongX, -190.f, 360.f), true, 3, false, Segment == 4);
            }
            for (int32 Segment = 0; Segment < 9; ++Segment)
            {
                const float AlongY = -100.f + 200.f * Segment;
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("outer_wall_%02d"), Segment),
                    FVector(X + (X < 0.f ? -780.f : 780.f), AlongY, 360.f), false, 3);
                AddCastleV2WallSegment(Plan, Prefix + FString::Printf(TEXT("inner_wall_%02d"), Segment),
                    FVector(X + (X < 0.f ? 780.f : -780.f), AlongY, 360.f), false, 3);
            }
            AddCastleV2Native(Plan, Prefix + TEXT("north_beam"), V2Beam, FVector(X, 1590.f, 680.f), FVector(1600.f, 80.f, 80.f), 4, FIntVector(0, 0, 2), V2Timber);
            AddCastleV2Native(Plan, Prefix + TEXT("south_beam"), V2Beam, FVector(X, -190.f, 680.f), FVector(1600.f, 80.f, 80.f), 4, FIntVector(0, 0, 2), V2Timber);
            AddCastleV2RoofBays(Plan, Prefix, X, 700.f, 720.f, 5, 9, 6);
        }

        void AddCastleV2StairConnection(FHearthRoyalWorksPlan& Plan, int32 Level)
        {
            const float FloorTop = 80.f + 600.f * Level;
            const FString Prefix = FString::Printf(TEXT("tower_connection_%d_"), Level);
            // Native tread datum: entry 16cm, exit 256cm, rise 240cm.
            // Two unscaled stairs + six 20cm connector risers = a true 600cm rise.
            // The platform frame bears on solid floor / the two opening rims,
            // not on the empty well. Its underside retains 220cm headroom.
            for (int32 Column = 0; Column < 3; ++Column)
            {
                const float X = Column == 0 ? -390.f : (Column == 1 ? -190.f : 190.f);
                for (int32 Side = 0; Side < 2; ++Side)
                    AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("post_%d_%d"), Column, Side),
                        V2Post, FVector(X, Side == 0 ? 462.f : 536.f, FloorTop + 110.f),
                        FVector(18,18,220), 9, FIntVector(0,0,1), V2Timber);
            }
            for (int32 Span = 0; Span < 2; ++Span) for (int32 Side = 0; Side < 2; ++Side)
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("beam_%d_%d"), Span, Side),
                    V2Beam, FVector(Span == 0 ? -290.f : 0.f, Side == 0 ? 462.f : 536.f, FloorTop + 230.f),
                    FVector(Span == 0 ? 220.f : 400.f,18,20), 10, FIntVector(0,0,2), V2Timber);
            for (int32 Step = 0; Step < 6; ++Step)
            {
                const float Height = 20.f * (Step + 1);
                AddCastleV2Mesh(Plan, Prefix + FString::Printf(TEXT("step_%d"), Step), V2Cube,
                    FVector(-337.5f + Step * 45.f, 499.f, FloorTop + 240.f + Height * .5f),
                    FVector(45,98,Height), 11, FIntVector(0,1,1), V2Timber);
            }
            AddCastleV2Mesh(Plan, Prefix + TEXT("landing"), V2Cube,
                FVector(-45,499,FloorTop+300), FVector(90,98,120), 11, FIntVector(0,2,1), V2Timber);
            // The native exit stops 8cm before the well edge. A supported
            // 20cm apron overlaps both the exit landing and the upper floor.
            AddCastleV2Native(Plan, Prefix + TEXT("exit_apron"), V2FloorTile,
                FVector(48,450,FloorTop+580), FVector(84,20,40), 11, FIntVector(0,1,0), V2Floor);
            AddCastleV2AtOrigin(Plan, Prefix + TEXT("stair_lower"), V2Stairs,
                FVector(-450,650,FloorTop-16), 12, FIntVector(0,2,1), V2Timber);
            AddCastleV2AtOrigin(Plan, Prefix + TEXT("stair_upper"), V2Stairs,
                FVector(0,650,FloorTop+344), 12, FIntVector(0,2,1), V2Timber);
        }

        void AddCastleV2FloorTiles(FHearthRoyalWorksPlan& Plan, const FString& Prefix, const FVector& Center,
            int32 WidthTiles, int32 DepthTiles, float Z, int32 Stage, bool bOpening)
        {
            const float XStart = -200.f * (WidthTiles - 1);
            const float YStart = -200.f * (DepthTiles - 1);
            for (int32 X = 0; X < WidthTiles; ++X) for (int32 Y = 0; Y < DepthTiles; ++Y)
            {
                if (bOpening && X == WidthTiles / 2 && Y == DepthTiles / 2) continue;
                AddCastleV2Native(Plan, Prefix + FString::Printf(TEXT("floor_tile_%02d_%02d"), X, Y), V2FloorTile,
                    Center + FVector(XStart + 400.f * X, YStart + 400.f * Y, Z), FVector(400.f, 400.f, 40.f), Stage, FIntVector(0, 5, 0), V2Floor);
            }
            if (bOpening)
                AddCastleV2Native(Plan, Prefix + TEXT("floor_opening"), V2FloorOpening, Center + FVector(0.f, 0.f, Z), FVector(400.f, 400.f, 40.f), Stage, FIntVector(0, 1, 1), V2Trim);
        }
    }

    FHearthRoyalWorksPlan BuildCastleV2()
    {
        FHearthRoyalWorksPlan Plan;
        Plan.TemplateId = TEXT("royal_keep_garden_v2");
        Plan.Radius = 5000.f;
        Plan.Rooms = HearthCastleRooms::BuildV2();
        Plan.Modules.Reserve(1300);

        // The initial release is deliberately a long-lived staged project:
        // every visible element is a small floor, wall, post, beam, opening,
        // stair, roof or planted module. No storey is a solid building cube.
        // Stage 1: foundations for tower, wings, gate and the four curtain runs.
        AddCastleV2Mesh(Plan, TEXT("v2_tower_foundation"), V2Cube, FVector(0.f, 650.f, 20.f), FVector(2000.f, 2000.f, 40.f), 1, FIntVector(8, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_west_wing_foundation"), V2Cube, FVector(-2200.f, 700.f, 20.f), FVector(1800.f, 1900.f, 40.f), 1, FIntVector(7, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_east_wing_foundation"), V2Cube, FVector(2200.f, 700.f, 20.f), FVector(1800.f, 1900.f, 40.f), 1, FIntVector(7, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_gate_foundation_west"), V2Cube, FVector(-550.f, -2650.f, 20.f), FVector(560.f, 760.f, 40.f), 1, FIntVector(3, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_gate_foundation_east"), V2Cube, FVector(550.f, -2650.f, 20.f), FVector(560.f, 760.f, 40.f), 1, FIntVector(3, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_curtain_foundation_west"), V2Cube, FVector(-3250.f, 0.f, 20.f), FVector(200.f, 4800.f, 40.f), 1, FIntVector(8, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_curtain_foundation_east"), V2Cube, FVector(3250.f, 0.f, 20.f), FVector(200.f, 4800.f, 40.f), 1, FIntVector(8, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_curtain_foundation_north"), V2Cube, FVector(0.f, 2700.f, 20.f), FVector(6600.f, 200.f, 40.f), 1, FIntVector(8, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_curtain_foundation_south_west"), V2Cube, FVector(-2000.f, -2300.f, 20.f), FVector(2600.f, 200.f, 40.f), 1, FIntVector(5, 0, 0), V2Stone);
        AddCastleV2Mesh(Plan, TEXT("v2_curtain_foundation_south_east"), V2Cube, FVector(2000.f, -2300.f, 20.f), FVector(2600.f, 200.f, 40.f), 1, FIntVector(5, 0, 0), V2Stone);

        // Stage 2: walkable floors, thresholds and the first stair landings.
        AddCastleV2FloorTiles(Plan, TEXT("tower_floor_0_"), FVector(0.f, 650.f, 0.f), 5, 5, 60.f, 2, false);
        AddCastleV2FloorTiles(Plan, TEXT("west_wing_"), FVector(-2200.f, 700.f, 0.f), 5, 5, 60.f, 2, false);
        AddCastleV2FloorTiles(Plan, TEXT("east_wing_"), FVector(2200.f, 700.f, 0.f), 5, 5, 60.f, 2, false);
        AddCastleV2FloorTiles(Plan, TEXT("gate_"), FVector(0.f, -2650.f, 0.f), 2, 3, 60.f, 2, false);
        AddCastleV2Mesh(Plan, TEXT("gate_step_outer"), V2Cube, FVector(0.f, -3130.f, 20.f), FVector(700.f, 220.f, 40.f), 2, FIntVector(1, 1, 0), V2Trim);
        AddCastleV2Mesh(Plan, TEXT("gate_step_inner"), V2Cube, FVector(0.f, -2980.f, 60.f), FVector(600.f, 180.f, 40.f), 2, FIntVector(1, 1, 0), V2Trim);

        // Stages 3/5/7/9/11: five open tower levels, each made from four
        // walls. Lower south walls are split around a real 260cm door opening.
        for (int32 Level = 0; Level < 5; ++Level)
        {
            AddCastleV2TowerLevel(Plan, Level, 3 + Level * 2);
            if (Level > 0)
            {
                AddCastleV2FloorTiles(Plan, FString::Printf(TEXT("tower_floor_%d_"), Level), FVector(0.f, 650.f, 0.f),
                    5, 5, 60.f + 600.f * Level, 2 + Level * 2, true);
            }
        }

        // Reassign the upper floors to the stage before their walls. This
        // keeps their support barrier explicit while preserving module order.
        // Stage 3: open side wings and gatehouse shells, all wall segments.
        AddCastleV2Wing(Plan, TEXT("west"), -2200.f);
        AddCastleV2Wing(Plan, TEXT("east"), 2200.f);
        AddCastleV2SmallTower(Plan, TEXT("gate_tower_west_"), -550.f, 3);
        AddCastleV2SmallTower(Plan, TEXT("gate_tower_east_"), 550.f, 3);

        // Stage 4: gate lintel, curtain walls and tower frames around the
        // current floors. The southern gate corridor remains fully open.
        for (int32 Segment = 0; Segment < 6; ++Segment)
            AddCastleV2Native(Plan, FString::Printf(TEXT("gate_lintel_%02d"), Segment), V2Beam,
                FVector(-500.f + 200.f * Segment, -2650.f, 700.f), FVector(200.f, 520.f, 100.f), 4, FIntVector(0, 1, 2), V2Trim);
        for (int32 Segment = 0; Segment < 25; ++Segment)
        {
            const float Y = -2400.f + 200.f * Segment;
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_wall_west_%02d"), Segment), V2WallStone,
                FVector(-3250.f, Y, 330.f), FVector(18.f, 200.f, 620.f), 4, FIntVector(2, 0, 1), V2Stone, 90.f);
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_wall_east_%02d"), Segment), V2WallStone,
                FVector(3250.f, Y, 330.f), FVector(18.f, 200.f, 620.f), 4, FIntVector(2, 0, 1), V2Stone, 90.f);
        }
        for (int32 Segment = 0; Segment < 33; ++Segment)
        {
            const float X = -3200.f + 200.f * Segment;
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_wall_north_%02d"), Segment), V2WallStone,
                FVector(X, 2700.f, 330.f), FVector(200.f, 18.f, 620.f), 4, FIntVector(2, 0, 1), V2Stone);
        }
        for (int32 Segment = 0; Segment < 13; ++Segment)
        {
            const float XWest = -3200.f + 200.f * Segment;
            const float XEast = 800.f + 200.f * Segment;
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_wall_south_west_%02d"), Segment), V2WallStone,
                FVector(XWest, -2300.f, 330.f), FVector(200.f, 18.f, 620.f), 4, FIntVector(2, 0, 1), V2Stone);
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_wall_south_east_%02d"), Segment), V2WallStone,
                FVector(XEast, -2300.f, 330.f), FVector(200.f, 18.f, 620.f), 4, FIntVector(2, 0, 1), V2Stone);
        }
        for (int32 Level = 1; Level < 5; ++Level) AddCastleV2TowerFrame(Plan, Level, 2 + Level * 2);

        // Stage 8: explicit courtyard circulation bands, kept low and
        // grounded so no giant slab closes the inner yard.
        AddCastleV2Mesh(Plan, TEXT("courtyard_walk_north"), V2Cube, FVector(0.f, -500.f, 15.f), FVector(2500.f, 120.f, 30.f), 8, FIntVector(0, 2, 0), V2Floor);
        AddCastleV2Mesh(Plan, TEXT("courtyard_walk_south"), V2Cube, FVector(0.f, -1900.f, 15.f), FVector(2500.f, 120.f, 30.f), 8, FIntVector(0, 2, 0), V2Floor);
        AddCastleV2Mesh(Plan, TEXT("courtyard_walk_west"), V2Cube, FVector(-1190.f, -1200.f, 15.f), FVector(120.f, 1400.f, 30.f), 8, FIntVector(0, 2, 0), V2Floor);
        AddCastleV2Mesh(Plan, TEXT("courtyard_walk_east"), V2Cube, FVector(1190.f, -1200.f, 15.f), FVector(120.f, 1400.f, 30.f), 8, FIntVector(0, 2, 0), V2Floor);

        // Stage 10/12: parapet beams, openings and stair flights. These are
        // visible incremental additions rather than an instant finished shell.
        for (int32 Segment = 0; Segment < 33; ++Segment)
        {
            const float X = -3200.f + 200.f * Segment;
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_parapet_north_%02d"), Segment), V2Beam,
                FVector(X, 2700.f, 670.f), FVector(200.f, 80.f, 80.f), 10, FIntVector(0, 0, 1), V2Trim);
        }
        for (int32 Segment = 0; Segment < 25; ++Segment)
        {
            const float Y = -2400.f + 200.f * Segment;
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_parapet_west_%02d"), Segment), V2Beam,
                FVector(-3250.f, Y, 670.f), FVector(80.f, 200.f, 80.f), 10, FIntVector(0, 0, 1), V2Trim, 90.f);
            AddCastleV2Native(Plan, FString::Printf(TEXT("curtain_parapet_east_%02d"), Segment), V2Beam,
                FVector(3250.f, Y, 670.f), FVector(80.f, 200.f, 80.f), 10, FIntVector(0, 0, 1), V2Trim, 90.f);
        }
        for (int32 Level = 0; Level < 4; ++Level) AddCastleV2StairConnection(Plan, Level);
        AddCastleV2Native(Plan, TEXT("tower_main_door"), V2WallDoor, FVector(0.f, -250.f, 360.f), FVector(200.f, 18.f, 560.f), 12, FIntVector(0, 2, 1), V2Timber);

        // Native gables, paired slopes and short caps, sharing the same datum.
        AddCastleV2RoofBays(Plan, TEXT("tower_roof_"), 0.f, 650.f, 3040.f, 5, 9, 13);
        AddCastleV2RoofBays(Plan, TEXT("gate_roof_west_"), -550.f, -2650.f, 580.f, 2, 3, 13);
        AddCastleV2RoofBays(Plan, TEXT("gate_roof_east_"), 550.f, -2650.f, 580.f, 2, 3, 13);
        AddCastleV2Plant(Plan, TEXT("v2_courtyard_oak"), TEXT("oak"), FVector(-900.f, -1300.f, 0.f), .65f, FIntVector(0, 1, 1), V2Garden);
        AddCastleV2Plant(Plan, TEXT("v2_courtyard_birch"), TEXT("birch"), FVector(900.f, -1300.f, 0.f), .70f, FIntVector(0, 1, 1), V2Garden);
        AddCastleV2Plant(Plan, TEXT("v2_service_cypress"), TEXT("cypress"), FVector(-900.f, 2200.f, 0.f), .65f, FIntVector(0, 1, 1), V2Garden);
        AddCastleV2Plant(Plan, TEXT("v2_service_flowers_west"), TEXT("flowering_shrub"), FVector(-1350.f, 2150.f, 0.f), .55f, FIntVector(1, 0, 0), V2Flower);
        AddCastleV2Plant(Plan, TEXT("v2_service_flowers_east"), TEXT("flowering_shrub"), FVector(1350.f, 2150.f, 0.f), .55f, FIntVector(1, 0, 0), V2Flower);
        AddCastleV2Plant(Plan, TEXT("v2_courtyard_wildflowers"), TEXT("wildflowers"), FVector(0.f, -1200.f, 0.f), .55f, FIntVector(0, 1, 0), V2Flower);
        // PublicWork's canonical index is stage ordered. Stable ID ordering
        // inside a stage makes replay and WorldState validation deterministic.
        Plan.Modules.Sort([](const FHearthRoyalModule& A, const FHearthRoyalModule& B)
        {
            return A.Stage == B.Stage ? A.Id.Compare(B.Id) < 0 : A.Stage < B.Stage;
        });
        return Plan;
    }
}

namespace HearthRoyalWorksPlan
{
    FTransform MeshTransform(const FHearthRoyalModule& Module, const FBox& MeshLocalBounds,
        const FVector& WorldOffset)
    {
        const FQuat Rotation = FRotator(0, Module.Yaw, 0).Quaternion();
        const FVector PivotCorrection = Module.bCenterMeshAtOffset
            ? Rotation.RotateVector(MeshLocalBounds.GetCenter() * Module.Scale) : FVector::ZeroVector;
        return FTransform(Rotation, WorldOffset - PivotCorrection, Module.Scale);
    }

    FHearthRoyalWorksPlan BuildForTemplate(const FString& TemplateId)
    {
        if (TemplateId == TEXT("royal_keep_garden_v1")) return Build();
        if (TemplateId == TEXT("royal_keep_garden_v2")) return CastleV2Detail::BuildCastleV2();
        return FHearthRoyalWorksPlan();
    }
}
