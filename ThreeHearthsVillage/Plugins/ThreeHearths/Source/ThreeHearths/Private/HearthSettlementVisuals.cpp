#include "HearthVillage.h"
#include "HearthSettlementPlan.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

const FHearthSettlementPlan* AHearthVillage::GetSettlementPlan() const
{
    return IsOrganicVillage() ? SettlementPlan.Get() : nullptr;
}

void AHearthVillage::ToggleTownPlanning()
{
    bTownPlanningVisible = !bTownPlanningVisible;
    for (auto& Mesh : PlanningMeshes) if (IsValid(Mesh)) Mesh->SetVisibility(bTownPlanningVisible);
}

void AHearthVillage::RefreshTownPlanning()
{
    if (!IsOrganicVillage()) return;
    const FVector Castle = ProductionSites.IsValidIndex(PublicProject.Site)
        ? ProductionSites[PublicProject.Site].Position : FVector(6500,6500,GroundHeightAt(FVector(6500,6500,0)));
    const FString Key = WorldId + PublicProject.TemplateId + Castle.ToString();
    if (SettlementPlan.IsValid() && Key == SettlementPlanKey) return;
    for (auto& Mesh : PlanningMeshes) if (IsValid(Mesh)) Mesh->DestroyComponent();
    PlanningMeshes.Reset();
    SettlementPlan = MakeShared<FHearthSettlementPlan>(HearthSettlementPlan::Build(Castle,PublicProject.TemplateId));
    SettlementPlanKey = Key;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube) return;
    auto Batch = [&](const FLinearColor& Color)
    {
        auto* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
        Mesh->ComponentTags.Add(TEXT("HearthPlanningOnly"));
        if (TintMaterial)
        {
            auto* Material = UMaterialInstanceDynamic::Create(TintMaterial,this);
            Material->SetVectorParameterValue(TEXT("VillageTint"),Color);
            Mesh->SetMaterial(0,Material);
        }
        Mesh->SetVisibility(bTownPlanningVisible);
        Mesh->RegisterComponent();
        PlanningMeshes.Add(Mesh);
        return Mesh;
    };
    auto Edge = [](UInstancedStaticMeshComponent* Mesh,const FVector& A,const FVector& B,float Width)
    {
        const FVector D = B-A;
        if (D.Size() < .1f) return;
        Mesh->AddInstance(FTransform(D.Rotation(),(A+B)*.5f,FVector(D.Size()/100.f,Width/100.f,Width/100.f)));
    };
    // Dashed contours hug the actual terrain. They are guidance, not fences or lots.
    for (const auto& District : SettlementPlan->Districts)
    {
        auto* Mesh = Batch(District.Color);
        for (int32 I=0;I<District.Boundary.Num();++I)
        {
            const FVector A=District.Boundary[I], B=District.Boundary[(I+1)%District.Boundary.Num()];
            const float Length=FVector::Dist2D(A,B);
            for(float Start=0;Start<Length;Start+=190.f)
            {
                FVector P=FMath::Lerp(A,B,Start/Length);
                FVector Q=FMath::Lerp(A,B,FMath::Min(Start+120.f,Length)/Length);
                P.Z=GroundHeightAt(P)+20.f; Q.Z=GroundHeightAt(Q)+20.f;
                Edge(Mesh,P,Q,16.f);
            }
        }
    }
    auto* CastleMesh = Batch(FLinearColor(.93f,.69f,.26f));
    for (const auto& Outline : SettlementPlan->CastleLines)
    {
        const int32 Count=Outline.Points.Num();
        for(int32 I=1;I<Count;++I) Edge(CastleMesh,Outline.Points[I-1],Outline.Points[I],20.f);
        if(Outline.bClosed && Count>2) Edge(CastleMesh,Outline.Points.Last(),Outline.Points[0],20.f);
    }
}
