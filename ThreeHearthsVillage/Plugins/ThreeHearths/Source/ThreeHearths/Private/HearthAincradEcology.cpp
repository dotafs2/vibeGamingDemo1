#include "HearthAincradEcology.h"
#include "HearthAincradStyle.h"
#include "HearthRoyalHill.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace HearthAincradEcologyDetail
{
    using namespace HearthAincradEcology;
    const FName EcologyTag(TEXT("ThreeHearthsAincradEcology"));
    struct FRegion { FVector2D Center, Radius; int32 Trees, Low; };
    // Western working forest surrounds, but leaves access to, the real trees
    // and timber yard. Broken northern groves frame the expanded 300m terrain.
    // The south remains open meadow; the east has a thin boundary, not a grid.
    const FRegion Regions[] = {
        {{-6700,-1500},{1200,4400},32,48}, {{-4850,2900},{1500,1900},18,36},
        {{-6500,7200},{1500,3000},28,44}, {{-6250,12800},{1600,2500},24,40},
        {{-3200,18500},{3800,1500},24,40}, {{5500,19800},{3700,950},22,36},
        {{14300,19600},{3000,1000},22,36}, {{19900,12900},{800,3800},18,32},
        {{-2100,-7000},{3000,600},6,48}, {{15500,-3500},{3500,1800},6,48}
    };
    struct FLane { FVector2D A, B; double HalfWidth; };
    TArray<FLane> Lanes(const FInput& Input)
    {
        TArray<FLane> Result;
        for (const auto& Road : Input.Terrain.Roads)
        {
            // Legacy terrain Width is a half-width; royal Width is the full
            // carriageway. Reserve the shoulders/blend too, plus canopy radius.
            const double Half = Road.bRoyalHillRoad
                ? Road.Width*.5+Road.ShoulderWidth+Road.Transition
                : Road.Width+Road.Transition;
            for (int32 I=1; I<Road.Nodes.Num(); ++I)
                Result.Add({Road.Nodes[I-1].Position,Road.Nodes[I].Position,Half+80});
        }
        return Result;
    }
    double SegmentDistanceSquared(const FVector2D& P, const FLane& Lane)
    {
        const FVector2D D=Lane.B-Lane.A;
        const double T=D.SizeSquared()>0 ? FMath::Clamp(FVector2D::DotProduct(P-Lane.A,D)/D.SizeSquared(),0.0,1.0) : 0;
        return (P-Lane.A-D*T).SizeSquared();
    }
    double RectDistance(const FVector2D& P, const FVector2D& Center, const FVector2D& Half, float Yaw)
    {
        const FVector2D Local=(P-Center).GetRotated(-Yaw);
        return FVector2D(FMath::Max(0.0,FMath::Abs(Local.X)-Half.X),
            FMath::Max(0.0,FMath::Abs(Local.Y)-Half.Y)).Size();
    }
    // Signed free margin of the entire canopy/foliage footprint, in centimetres.
    double Clearance(const FInput& Input, const TArray<FLane>& Roads, const FVector2D& P, double Radius)
    {
        const auto& B=Input.Terrain.Bounds;
        double Margin=FMath::Min(FMath::Min(P.X-B.Min.X,B.Max.X-P.X),FMath::Min(P.Y-B.Min.Y,B.Max.Y-P.Y))-Radius-80;
        Margin=FMath::Min(Margin,(P-HearthRoyalHill::Center()).Size()-HearthRoyalHill::PlateauRadius-Radius-100);
        // Retain the entire royal work square as well as its circular plateau.
        Margin=FMath::Min(Margin,RectDistance(P,HearthRoyalHill::Center(),FVector2D(5025,5025),0)-Radius-80);
        for (const auto& C : Input.Clearings)
            Margin=FMath::Min(Margin,RectDistance(P,C.Center,C.HalfSize,C.Yaw)-Radius-80);
        for (const auto& Z : Input.Terrain.FlattenZones)
            Margin=FMath::Min(Margin,RectDistance(P,Z.Center,Z.HalfSize,Z.YawDegrees)-Radius-100);
        for (const auto& Road : Roads)
        {
            // Broad phase avoids a square root for distant road segments.
            const double Required=Road.HalfWidth+Radius;
            const double Search=Required+FMath::Max(0.0,Margin);
            if (P.X<FMath::Min(Road.A.X,Road.B.X)-Search || P.X>FMath::Max(Road.A.X,Road.B.X)+Search
                || P.Y<FMath::Min(Road.A.Y,Road.B.Y)-Search || P.Y>FMath::Max(Road.A.Y,Road.B.Y)+Search) continue;
            Margin=FMath::Min(Margin,FMath::Sqrt(SegmentDistanceSquared(P,Road))-Required);
        }
        return Margin;
    }
}

const TCHAR* HearthAincradEcology::AssetPath(int32 Species)
{
    // Verified native .uasset files in Content/Environment/Meshes/{Crops,Foliage}.
    // Keep their original materials. No substitute primitives or invented flora.
    static const TCHAR* Paths[SpeciesCount] = {
        TEXT("/Game/Environment/Meshes/Crops/SM_Tree_01"),
        TEXT("/Game/Environment/Meshes/Crops/SM_Tree_02"),
        TEXT("/Game/Environment/Meshes/Crops/SM_Shrub_01"),
        TEXT("/Game/Environment/Meshes/Crops/SM_Shrub_02"),
        TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_01"),
        TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_02"),
        TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_03"),
        TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_04")
    };
    return Species>=0 && Species<SpeciesCount ? Paths[Species] : nullptr;
}

HearthAincradEcology::FPlan HearthAincradEcology::BuildPlan(const FInput& Input)
{
    using namespace HearthAincradEcologyDetail;
    FPlan Result;
    if (!Input.Terrain.Bounds.IsValid()) return Result;
    const TArray<FLane> Roads=Lanes(Input);
    FRandomStream Random(Input.Terrain.Seed ^ 0x41C2A);
    // Reserve the three proposed expansion clearings without creating plots.
    FInput Working=Input;
    for (const FVector2D& C : {FVector2D(-4700,8800),FVector2D(6500,17600),FVector2D(17800,6500)})
        Working.Clearings.Add({C,FVector2D(1000,1000),0});
    for (int32 Region=0; Region<UE_ARRAY_COUNT(Regions); ++Region)
    {
        const auto& Zone=Regions[Region];
        for (int32 Layer=0; Layer<2; ++Layer)
        {
            const int32 Target=Layer==0?Zone.Trees:Zone.Low;
            int32 Accepted=0;
            for (int32 Attempt=0; Attempt<Target*28 && Accepted<Target; ++Attempt)
            {
                ++Result.Candidates;
                FPlacement P;
                P.Region=Region;
                P.Species=Layer==0 ? Random.RandRange(0,1) : (Random.FRand()<.28f?Random.RandRange(2,3):Random.RandRange(4,7));
                P.Scale=Layer==0?Random.FRandRange(.95f,1.65f):Random.FRandRange(.85f,1.65f);
                P.Yaw=Random.FRandRange(0.f,360.f);
                // A random ellipse makes connected crowns and irregular gaps.
                // Most understory sits on the woodland ecotone, not under trunks.
                const double Angle=Random.FRand()*2*UE_PI;
                const double R=Layer==1 && Region<8?Random.FRandRange(.72f,1.20f):FMath::Sqrt(double(Random.FRand()));
                P.XY=Zone.Center+FVector2D(FMath::Cos(Angle)*Zone.Radius.X,FMath::Sin(Angle)*Zone.Radius.Y)*R;
                P.Radius=FMath::Max(30.0,Input.MeshRadius[P.Species]*P.Scale);
                if (Clearance(Working,Roads,P.XY,P.Radius)<0) {++Result.ClearanceRejected;continue;}
                bool bCrowded=false;
                for (const auto& Other : Result.Placements)
                {
                    // Crowns may overlap at their edges; leave clear trunk gaps.
                    const double Separation=Layer==0 ? (P.Radius+Other.Radius)*.64 : (Other.Species<2?Other.Radius*.38+P.Radius:P.Radius+Other.Radius)*.7;
                    if ((P.XY-Other.XY).SizeSquared()<Separation*Separation) {bCrowded=true;break;}
                }
                if (bCrowded) {++Result.SpacingRejected;continue;}
                Result.Placements.Add(P); ++Accepted;
                if (Layer==0) ++Result.Trees; else ++Result.Understory;
            }
        }
    }
    return Result;
}

void HearthAincradEcology::Build(AActor& TerrainOwner, const FInput& Input,
    TFunctionRef<float(const FVector&)> GroundHeight)
{
    using namespace HearthAincradEcologyDetail;
    if (!TerrainOwner.GetRootComponent()) return;
    // Repeated decoration only replaces this layer on its explicitly owned actor.
    TArray<UInstancedStaticMeshComponent*> Previous; TerrainOwner.GetComponents(Previous);
    for (auto* C : Previous) if (C->ComponentHasTag(EcologyTag)) C->DestroyComponent();
    UStaticMesh* Meshes[SpeciesCount]={};
    UInstancedStaticMeshComponent* Batches[SpeciesCount]={};
    FInput Actual=Input;
    int32 Missing=0;
    for (int32 I=0; I<SpeciesCount; ++I)
    {
        Meshes[I]=LoadObject<UStaticMesh>(nullptr,AssetPath(I));
        if (!Meshes[I]) {++Missing;UE_LOG(LogTemp,Warning,TEXT("AINCRAD_ECOLOGY missing_asset=%s"),AssetPath(I));continue;}
        const auto Bounds=Meshes[I]->GetBounds();
        Actual.MeshRadius[I]=FVector2D(FMath::Abs(Bounds.Origin.X)+Bounds.BoxExtent.X,
            FMath::Abs(Bounds.Origin.Y)+Bounds.BoxExtent.Y).Size();
    }
    const FPlan Plan=BuildPlan(Actual);
    int32 Trees=0,Low=0,GroundRejected=0;
    int32 RegionsRendered[UE_ARRAY_COUNT(Regions)]={};
    double MinClearance=DBL_MAX,MinGround=DBL_MAX,MaxGround=-DBL_MAX;
    const TArray<FLane> Roads=Lanes(Actual);
    for (const auto& P : Plan.Placements)
    {
        UStaticMesh* Mesh=Meshes[P.Species];
        if (!Mesh) continue;
        const FVector XY(P.XY,0);
        const float Z=GroundHeight(XY);
        const double DX=(GroundHeight(XY+FVector(100,0,0))-GroundHeight(XY-FVector(100,0,0)))/200.0;
        const double DY=(GroundHeight(XY+FVector(0,100,0))-GroundHeight(XY-FVector(0,100,0)))/200.0;
        if (!FMath::IsFinite(Z) || !FMath::IsFinite(DX) || !FMath::IsFinite(DY) || DX*DX+DY*DY>.36)
        {++GroundRejected;continue;}
        auto*& Batch=Batches[P.Species];
        if (!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(&TerrainOwner,NAME_None,RF_Transient);
            Batch->ComponentTags.Add(EcologyTag);
            Batch->SetMobility(EComponentMobility::Movable);
            Batch->SetupAttachment(TerrainOwner.GetRootComponent());
            Batch->SetStaticMesh(Mesh);
            Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Batch->SetCanEverAffectNavigation(false);
            Batch->SetGenerateOverlapEvents(false);
            Batch->RegisterComponent();
            HearthAincradStyle::ApplyToMesh(Batch);
        }
        // XY and sampled Z are already world coordinates. Compensate only the
        // native mesh's base pivot; never pass through AddMesh/reprojection again.
        const auto Bounds=Mesh->GetBounds();
        const double Base=(Bounds.Origin.Z-Bounds.BoxExtent.Z)*P.Scale;
        Batch->AddInstance(FTransform(FRotator(0,P.Yaw,0),FVector(P.XY,Z-Base),FVector(P.Scale)),true);
        if (P.Species<2) ++Trees; else ++Low;
        ++RegionsRendered[P.Region];
        MinClearance=FMath::Min(MinClearance,Clearance(Actual,Roads,P.XY,P.Radius));
        MinGround=FMath::Min(MinGround,double(Z)); MaxGround=FMath::Max(MaxGround,double(Z));
    }
    int32 BatchesUsed=0;
    for (auto* Batch : Batches) if (Batch) {Batch->UpdateBounds();++BatchesUsed;}
    const bool bAny=Trees+Low>0;
    UE_LOG(LogTemp,Display,TEXT("AINCRAD_ECOLOGY seed=%d candidates=%d planned_trees=%d trees=%d understory=%d batches=%d clearance_rejected=%d spacing_rejected=%d ground_rejected=%d missing_assets=%d min_clearance_cm=%.1f ground_z=%.1f..%.1f resource_nodes_added=0 blockers_added=0"),
        Input.Terrain.Seed,Plan.Candidates,Plan.Trees,Trees,Low,BatchesUsed,Plan.ClearanceRejected,Plan.SpacingRejected,GroundRejected,Missing,
        bAny?MinClearance:0,bAny?MinGround:0,bAny?MaxGround:0);
    for (int32 I=0; I<UE_ARRAY_COUNT(Regions); ++I)
        UE_LOG(LogTemp,Display,TEXT("AINCRAD_ECOLOGY_REGION id=%d center=%s instances=%d"),I,*Regions[I].Center.ToString(),RegionsRendered[I]);
    if (Trees<120) UE_LOG(LogTemp,Warning,TEXT("AINCRAD_ECOLOGY tree_target_shortfall actual=%d target=120..220; safety clearings retained"),Trees);
}
