#include "HearthVillage.h"
#include "HearthOrganicTerrain.h"
#include "HearthTownLayout.h"
#include "HearthRoyalHill.h"
#include "HearthAincradStyle.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"

namespace
{
    using namespace HearthOrganicTerrain;

    const FName OrganicGroundTag(TEXT("ThreeHearthsOrganicTerrain"));
    const FName BaseTerrainTag(TEXT("ThreeHearthsBaseTerrain"));
    const FName OrganicLightingTag(TEXT("ThreeHearthsOrganicLighting"));
    const FName GeneratedTag(TEXT("ThreeHearthsGenerated"));
    constexpr float LegacyGround = 3.f;
    constexpr float LegacyAnchor = 8.f;

    float GridHeightAt(const FSettings& Settings, const FGrid& Grid, const FVector& Point)
    {
        if (Grid.VertexColumns < 2 || Grid.VertexRows < 2 || Grid.Vertices.Num() != Grid.VertexColumns * Grid.VertexRows
            || !Settings.Bounds.IsValid()) return LegacyGround;
        const float U = FMath::Clamp((Point.X - Settings.Bounds.Min.X) / (Settings.Bounds.Max.X - Settings.Bounds.Min.X), 0.f, 1.f);
        const float V = FMath::Clamp((Point.Y - Settings.Bounds.Min.Y) / (Settings.Bounds.Max.Y - Settings.Bounds.Min.Y), 0.f, 1.f);
        const float GX = U * static_cast<float>(Grid.VertexColumns - 1);
        const float GY = V * static_cast<float>(Grid.VertexRows - 1);
        const int32 X = FMath::Min(FMath::FloorToInt(GX), Grid.VertexColumns - 2);
        const int32 Y = FMath::Min(FMath::FloorToInt(GY), Grid.VertexRows - 2);
        const float TX = FMath::Clamp(GX - X, 0.f, 1.f);
        const float TY = FMath::Clamp(GY - Y, 0.f, 1.f);
        const int32 A = Y * Grid.VertexColumns + X;
        const int32 B = A + 1;
        const int32 C = A + Grid.VertexColumns;
        const int32 D = C + 1;
        const float ZA = Grid.Vertices[A].Z, ZB = Grid.Vertices[B].Z, ZC = Grid.Vertices[C].Z, ZD = Grid.Vertices[D].Z;
        return TX + TY <= 1.f ? ZA * (1.f - TX - TY) + ZB * TX + ZC * TY
            : ZB * (1.f - TY) + ZC * (1.f - TX) + ZD * (TX + TY - 1.f);
    }

    void AddRoadRibbon(const FSettings& Settings, const FGrid& Grid, const FRoadCenterline& Road, TArray<FVector>& Vertices,
        TArray<int32>& Indices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FLinearColor>& Colors)
    {
        if (Road.Nodes.Num() < 2) return;
        for (int32 Segment = 1; Segment < Road.Nodes.Num(); ++Segment)
        {
            const FVector2D A = Road.Nodes[Segment - 1].Position, B = Road.Nodes[Segment].Position;
            const float Length = FVector2D::Distance(A, B);
            const int32 Samples = FMath::Clamp(FMath::CeilToInt(Length / 65.f), 1, 512);
            const int32 CrossSamples=FMath::Max(1,FMath::CeilToInt(Road.Width/65.f));
            const int32 Columns=CrossSamples+1;
            for (int32 Step = 0; Step <= Samples; ++Step)
            {
                const float T = static_cast<float>(Step) / Samples;
                const FVector2D P = FMath::Lerp(A, B, T);
                const FVector2D Tangent = (B - A).GetSafeNormal();
                const FVector2D Side(-Tangent.Y, Tangent.X);
                const float HalfWidth = FMath::Max(20.f, Road.Width * .5f);
                const int32 Base = Vertices.Num();
                for(int32 Across=0;Across<=CrossSamples;++Across)
                {
                    const float U=float(Across)/CrossSamples;
                    const FVector2D XY=P+Side*FMath::Lerp(HalfWidth,-HalfWidth,U);
                    Vertices.Add(FVector(XY.X,XY.Y,GridHeightAt(Settings,Grid,FVector(XY,0.f))+5.f));
                    Normals.Add(FVector::UpVector); UVs.Add(FVector2D(U,T));
                    Colors.Add(FLinearColor(.45f,.30f,.14f,1.f));
                }
                if (Step > 0)
                {
                    for(int32 Across=0;Across<CrossSamples;++Across)
                    {
                        const int32 A0=Base-Columns+Across,B0=A0+1,C0=Base+Across,D0=C0+1;
                        Indices.Append({A0,C0,B0,B0,C0,D0});
                    }
                }
            }
        }
    }
}

void AHearthVillage::BuildOrganicGround()
{
    if (!IsOrganicVillage() || !GetWorld()) return;

    if (OrganicGroundActor.IsValid()) OrganicGroundActor->Destroy();
    OrganicGroundActor.Reset();
    OrganicTerrainSettings = MakeShared<HearthOrganicTerrain::FSettings>();
    OrganicTerrainGrid = MakeShared<HearthOrganicTerrain::FGrid>();
    OrganicTerrainSettings->Seed = OrganicWorldSeed;
    OrganicTerrainSettings->GridQuadsX = 200;
    OrganicTerrainSettings->GridQuadsY = 200;

    // Every selected house receives a full footprint pad. Elevations are taken
    // from the untouched field first, so adding one pad cannot move another.
    FSettings Natural = *OrganicTerrainSettings;
    OrganicTerrainSettings->bRoyalHill = true;
    // Private home pads retain precedence, preserving saved plot heights.
    const FTransform WatchFrame=GetServiceGateFrame();
    FFlattenZone WatchZone;
    WatchZone.Center=FVector2D(WatchFrame.GetLocation().X,WatchFrame.GetLocation().Y);
    WatchZone.HalfSize=FVector2D(600.f,240.f); WatchZone.YawDegrees=WatchFrame.Rotator().Yaw;
    WatchZone.Elevation=HeightAt(WatchZone.Center,Natural); WatchZone.Transition=250.f;
    OrganicTerrainSettings->FlattenZones.Add(WatchZone);
    for (int32 Plot = 0; Plot < HousingPlotCount(); ++Plot)
    {
        FFlattenZone Zone;
        Zone.Center = FVector2D(PlotPositions[Plot].X, PlotPositions[Plot].Y);
        Zone.HalfSize = FVector2D(610.f, 700.f);
        Zone.YawDegrees = PlotYaws[Plot];
        Zone.Elevation = HeightAt(Zone.Center, Natural);
        Zone.Transition = 400.f;
        OrganicTerrainSettings->FlattenZones.Add(Zone);
    }

    const auto AddFlatZone = [&Natural](FSettings& Target, const FVector2D& Center, const FVector2D& HalfSize, float Yaw, float Transition)
    {
        FFlattenZone Zone;
        Zone.Center = Center; Zone.HalfSize = HalfSize; Zone.YawDegrees = Yaw;
        Zone.Elevation = HeightAt(Center, Natural);
        Zone.Transition = Transition; Target.FlattenZones.Add(Zone);
    };
    // Keep civic, production, gate and royal working areas level as roads are
    // graded around them.
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-1100.f, -1050.f), FVector2D(380.f, 320.f), 0.f, 360.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-1650.f, -1050.f), FVector2D(180.f, 180.f), 0.f, 300.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-2250.f, -1050.f), FVector2D(280.f, 240.f), 0.f, 320.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-2800.f, -1050.f), FVector2D(300.f, 260.f), 0.f, 320.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-1550.f, -2200.f), FVector2D(270.f, 250.f), 0.f, 250.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-1135.f, -2825.f), FVector2D(700.f, 520.f), 0.f, 450.f);
    // Retain the old cargo waiting ground and quarry at the foot of the new
    // hill. Existing goods/vehicles keep XY positions and a usable connection.
    AddFlatZone(*OrganicTerrainSettings, FVector2D(1200.f, 1250.f), FVector2D(650.f, 1500.f), 0.f, 700.f);
    AddFlatZone(*OrganicTerrainSettings, FVector2D(-200.f, 3100.f), FVector2D(1150.f, 450.f), 0.f, 650.f);
    // The complete keep sits on the radial royal plateau. A tiny old square
    // pad here would cut a pit into that plateau and bury the retained walls.

    // Use the authored roads as grade profiles. Their endpoint elevations are
    // read after pads are installed, making approaches join each footprint.
    for (const FHearthTownRoadSegment& Segment : HearthTownLayout::VillageRoads(true, 4))
    {
        // The uphill profile is one continuous polyline, applied last below.
        // Do not derive its elevation from the ungraded hillside.
        if (Segment.Width >= HearthRoyalHill::RoadWidth) continue;
        FRoadCenterline Road; Road.Width = Segment.Width; Road.Transition = 360.f;
        for (const FVector& Point : {Segment.A, Segment.B})
        {
            const FVector2D XY(Point.X, Point.Y);
            Road.Nodes.Add({XY, HeightAt(XY, Natural)});
        }
        OrganicTerrainSettings->Roads.Add(MoveTemp(Road));
    }

    // Persisted entries get a short graded spur to their nearest authored
    // road, so each home has a continuous physical approach.
    for (int32 Plot = 0; Plot < HousingPlotCount(); ++Plot)
    {
        const FVector Entry3 = PlotEntrances[Plot].IsNearlyZero() ? PlotPositions[Plot] : PlotEntrances[Plot];
        const FVector2D Entry(Entry3.X, Entry3.Y);
        FVector2D Nearest = Entry; float BestDistance = FLT_MAX;
        for (const FRoadCenterline& Existing : OrganicTerrainSettings->Roads)
        {
            if (Existing.Nodes.Num() < 2) continue;
            const FVector2D A = Existing.Nodes[0].Position, B = Existing.Nodes.Last().Position, D = B - A;
            const float LengthSquared = D.SizeSquared();
            const float Alpha = LengthSquared > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Entry - A, D) / LengthSquared, 0.f, 1.f) : 0.f;
            const FVector2D Candidate = A + D * Alpha;
            const float Distance = FVector2D::Distance(Entry, Candidate);
            if (Distance < BestDistance) { BestDistance = Distance; Nearest = Candidate; }
        }
        if (BestDistance < FLT_MAX && BestDistance > 20.f)
        {
            FRoadCenterline Spur; Spur.Width = 140.f; Spur.Transition = 300.f;
            Spur.Nodes.Add({Nearest, HeightAt(Nearest, *OrganicTerrainSettings)});
            Spur.Nodes.Add({Entry, HeightAt(Entry, *OrganicTerrainSettings)});
            OrganicTerrainSettings->Roads.Add(MoveTemp(Spur));
        }
    }

    HearthRoyalHill::AddToTerrain(*OrganicTerrainSettings);
    if (!GenerateGrid(*OrganicTerrainSettings, *OrganicTerrainGrid))
    {
        OrganicTerrainSettings.Reset(); OrganicTerrainGrid.Reset();
        return;
    }

    FActorSpawnParameters Spawn;
    Spawn.Owner = this;
    Spawn.OverrideLevel = GetLevel();
    Spawn.bAllowDuringConstructionScript = true;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* GroundActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
    if (!GroundActor)
    {
        OrganicTerrainSettings.Reset(); OrganicTerrainGrid.Reset();
        return;
    }
    GroundActor->Tags.AddUnique(OrganicGroundTag);
    GroundActor->Tags.AddUnique(BaseTerrainTag);
    auto* Root = NewObject<USceneComponent>(GroundActor, TEXT("OrganicGroundRoot"));
    GroundActor->SetRootComponent(Root); Root->RegisterComponent();
    auto* Ground = NewObject<UProceduralMeshComponent>(GroundActor, TEXT("OrganicGroundMesh"));
    Ground->ComponentTags.Add(OrganicGroundTag); Ground->ComponentTags.Add(BaseTerrainTag);
    Ground->SetupAttachment(Root); Ground->RegisterComponent();
    Ground->CreateMeshSection_LinearColor(0, OrganicTerrainGrid->Vertices, OrganicTerrainGrid->Indices,
        OrganicTerrainGrid->Normals, OrganicTerrainGrid->UV0, OrganicTerrainGrid->VertexColors,
        TArray<FProcMeshTangent>(), true);
    Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Ground->SetCollisionProfileName(TEXT("BlockAll"));
    Ground->bUseComplexAsSimpleCollision = true;
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ThreeHearths/Materials/AincradStyle/MI_Grass")))
        Ground->SetMaterial(0, Material);

    TArray<FVector> RoadVertices, RoadNormals; TArray<int32> RoadIndices; TArray<FVector2D> RoadUVs; TArray<FLinearColor> RoadColors;
    for (const FRoadCenterline& Road : OrganicTerrainSettings->Roads)
        AddRoadRibbon(*OrganicTerrainSettings, *OrganicTerrainGrid, Road, RoadVertices, RoadIndices, RoadNormals, RoadUVs, RoadColors);
    if (!RoadVertices.IsEmpty())
    {
        auto* RoadMesh = NewObject<UProceduralMeshComponent>(GroundActor, TEXT("OrganicRoadMesh"));
        RoadMesh->ComponentTags.Add(OrganicGroundTag); RoadMesh->SetupAttachment(Root); RoadMesh->RegisterComponent();
        RoadMesh->CreateMeshSection_LinearColor(0, RoadVertices, RoadIndices, RoadNormals, RoadUVs, RoadColors,
            TArray<FProcMeshTangent>(), false);
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ThreeHearths/Materials/AincradStyle/MI_Paving")))
            RoadMesh->SetMaterial(0, Material);
    }
    OrganicGroundActor = GroundActor;

    // The overview previously had direct light on the terrain but no usable
    // ambient contribution, leaving every unlit facade near RGB(0,0,0). Keep
    // these lights on the actual v4 actor so game view and scene capture share
    // the same readable lighting.
    UDirectionalLightComponent* ExistingSun = nullptr;
    USkyLightComponent* ExistingSky = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It && (!ExistingSun || !ExistingSky); ++It)
    {
        TArray<UDirectionalLightComponent*> Suns; It->GetComponents(Suns);
        if (!ExistingSun) for (UDirectionalLightComponent* Candidate : Suns)
            if (IsValid(Candidate) && !Candidate->ComponentHasTag(OrganicLightingTag)) { ExistingSun = Candidate; break; }
        TArray<USkyLightComponent*> Skies; It->GetComponents(Skies);
        if (!ExistingSky) for (USkyLightComponent* Candidate : Skies)
            if (IsValid(Candidate) && !Candidate->ComponentHasTag(OrganicLightingTag)) { ExistingSky = Candidate; break; }
    }
    if (!ExistingSun)
    {
        auto* Sun = NewObject<UDirectionalLightComponent>(GroundActor, TEXT("OrganicSun"));
        Sun->ComponentTags.Add(OrganicLightingTag);
        Sun->SetMobility(EComponentMobility::Movable);
        Sun->Intensity = 3.5f;
        Sun->LightColor = FColor(255, 244, 218);
        Sun->DynamicShadowDistanceMovableLight = 50000.f;
        Sun->SetupAttachment(Root); Sun->SetRelativeRotation(FRotator(-48.f, -35.f, 0.f)); Sun->RegisterComponent();
    }
    if (ExistingSky)
    {
        // Reuse the map's sky instead of stacking a second ambient source.
        ExistingSky->SetIntensity(FMath::Max(ExistingSky->Intensity, 1.35f));
        ExistingSky->LightColor = FColor(190, 210, 235);
        ExistingSky->bLowerHemisphereIsBlack = false;
        ExistingSky->LowerHemisphereColor = FLinearColor(.16f, .20f, .24f, 1.f);
        if (ExistingSky->SourceType == SLS_SpecifiedCubemap && !ExistingSky->Cubemap)
            ExistingSky->Cubemap = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/EngineMaterials/DefaultCubemap"));
        if (ExistingSky->SourceType == SLS_CapturedScene) ExistingSky->RecaptureSky();
    }
    else
    {
        auto* Sky = NewObject<USkyLightComponent>(GroundActor, TEXT("OrganicSky"));
        Sky->ComponentTags.Add(OrganicLightingTag);
        Sky->SetMobility(EComponentMobility::Movable);
        Sky->SourceType = SLS_SpecifiedCubemap;
        Sky->Cubemap = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/EngineMaterials/DefaultCubemap"));
        Sky->SetIntensity(1.35f);
        Sky->LightColor = FColor(190, 210, 235);
        Sky->bLowerHemisphereIsBlack = false;
        Sky->LowerHemisphereColor = FLinearColor(.16f, .20f, .24f, 1.f);
        Sky->SetupAttachment(Root); Sky->RegisterComponent();
    }

    HearthAincradStyle::ConfigureWorld(*this,*GroundActor);

    // The legacy scaled cube remains in the actor for v3 save compatibility,
    // but it must not shadow or collide with the organic surface.
    if (GeneratedTown3Terrain.IsValid())
    {
        GeneratedTown3Terrain->SetActorHiddenInGame(true);
        GeneratedTown3Terrain->SetActorEnableCollision(false);
        GeneratedTown3Terrain->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // Reproject known anchors and existing generated props by preserving their
    // authored vertical offsets. Starter buildings created later can call the
    // public GroundHeightAt forwarding API without needing this pass again.
    for (int32 Plot = 0; Plot < HousingPlotCount(); ++Plot)
    {
        PlotPositions[Plot].Z = GroundHeightAt(PlotPositions[Plot]);
        PlotEntrances[Plot].Z = GroundHeightAt(PlotEntrances[Plot]);
    }
    for (int32 I = 0; I < 3; ++I) WoodPositions[I].Z = GroundHeightAt(WoodPositions[I]);
    TArray<USceneComponent*> Components; GetComponents(Components);
    for (USceneComponent* Component : Components)
    {
        if (!Component || Component->ComponentHasTag(GeneratedTag) == false || Component->IsA<UProceduralMeshComponent>()) continue;
        const FVector Location = Component->GetRelativeLocation();
        const float GroundDelta = GroundHeightAt(Location) - LegacyAnchor;
        Component->SetRelativeLocation(Location + FVector(0.f, 0.f, GroundDelta));
        const FVector Scale = Component->GetRelativeScale3D();
        if (Component->IsA<UStaticMeshComponent>() && Scale.X > 5.f && Scale.Y < 4.f && Scale.Z < .08f)
            Component->SetVisibility(false); // replace old flat road strips with the graded ribbons
    }
}

float AHearthVillage::OrganicGroundHeightAt(const FVector& Point) const
{
    if (OrganicTerrainSettings.IsValid() && OrganicTerrainGrid.IsValid())
        return GridHeightAt(*OrganicTerrainSettings, *OrganicTerrainGrid, Point);
    return LegacyGround;
}

float AHearthVillage::GroundHeightAt(const FVector& Point) const
{
    return IsOrganicVillage() ? OrganicGroundHeightAt(Point) : LegacyGround;
}
