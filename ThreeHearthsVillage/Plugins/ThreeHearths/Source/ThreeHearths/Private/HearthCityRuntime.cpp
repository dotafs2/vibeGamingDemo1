#include "HearthVillage.h"
#include "HearthResidentStory.h"
#include "HearthCityPlan.h"
#include "HearthBotanicalCatalog.h"
#include "HearthRoyalWorksPlan.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Components/StaticMeshComponent.h"

bool AHearthVillage::ReviewResidentHome(int32 Index) { return RequestVisualReview(Index); }

FString AHearthVillage::ExportCityGroundReport() const
{
    auto Root=MakeShared<FJsonObject>();
    const auto City=HearthCityPlan::BuildForVersion(TownLayoutVersion);
    Root->SetNumberField(TEXT("town_layout_version"),TownLayoutVersion);
    Root->SetNumberField(TEXT("recommended_population"),City.RecommendedPopulation);
    Root->SetNumberField(TEXT("population_limit"),HearthVillageLimits::MaxPopulation);
    Root->SetBoolField(TEXT("town_layout_ready"),TownLayoutError.IsEmpty());
    Root->SetStringField(TEXT("town_layout_error"),TownLayoutError);
    Root->SetField(TEXT("map_min"),MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
        MakeShared<FJsonValueNumber>(City.MapMin.X),MakeShared<FJsonValueNumber>(City.MapMin.Y)}));
    Root->SetField(TEXT("map_max"),MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
        MakeShared<FJsonValueNumber>(City.MapMax.X),MakeShared<FJsonValueNumber>(City.MapMax.Y)}));
    Root->SetNumberField(TEXT("walkable_grid_cells"),LandGrid.Num());
    TArray<TSharedPtr<FJsonValue>> Probes;
    auto AddProbe=[&](const TCHAR* Id,const FVector& Position)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("id"),Id);
        Row->SetNumberField(TEXT("x"),Position.X); Row->SetNumberField(TEXT("y"),Position.Y); Row->SetNumberField(TEXT("z"),Position.Z);
        Row->SetBoolField(TEXT("is_land"),IsLand(Position)); Row->SetBoolField(TEXT("is_clear"),IsClearPoint(Position));
        Probes.Add(MakeShared<FJsonValueObject>(Row));
    };
    AddProbe(TEXT("map_min"),FVector(City.MapMin.X,City.MapMin.Y,8.f));
    AddProbe(TEXT("map_max"),FVector(City.MapMax.X,City.MapMax.Y,8.f));
    AddProbe(TEXT("map_center"),FVector((City.MapMin.X+City.MapMax.X)*.5f,(City.MapMin.Y+City.MapMax.Y)*.5f,8.f));
    for(const auto& Landmark:City.Landmarks) if(Landmark.Kind==TEXT("castle"))
    {
        AddProbe(TEXT("castle_center"),Landmark.Position); AddProbe(TEXT("castle_entrance"),Landmark.Approach);
        Root->SetNumberField(TEXT("castle_radius_cm"),Landmark.Radius); Root->SetNumberField(TEXT("castle_entrance_ground_z"),Landmark.Approach.Z);
    }
    Root->SetArrayField(TEXT("ground_probes"),Probes);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& District:City.Districts)
    {
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("id"),District.Id); Row->SetStringField(TEXT("kind"),District.Kind);
        Row->SetNumberField(TEXT("x"),District.Center.X); Row->SetNumberField(TEXT("y"),District.Center.Y); Row->SetNumberField(TEXT("radius"),District.Radius);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("districts"),Rows); FString Result;
    FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Result));return Result;
}

void AHearthVillage::EnsureResidentStory(int32 Index)
{
    if(!Residents.IsValidIndex(Index)) return;
    auto& R=Residents[Index];
    if(R.InnerStory.IsEmpty())
    {
        FHearthResidentStoryInput Input; Input.StableId=R.StableId;Input.Name=R.Name;Input.Role=R.Role;Input.Personality=R.Personality;Input.bKing=R.bKing;
        R.InnerStory=HearthResidentStory::Create(Input);
    }
    if(bUseCropoutMap && TownLayoutVersion>=2 && R.BuildingArchetype.IsEmpty())
    {
        if(R.bKing) R.BuildingArchetype=TEXT("keep");
        else if(R.Role==TEXT("商人")) R.BuildingArchetype=TEXT("shop_house");
        else if(R.Role==TEXT("木匠") || R.Role==TEXT("铁匠") || R.Role==TEXT("陶工")) R.BuildingArchetype=TEXT("courtyard_workshop");
        else if(R.Role==TEXT("农民") || R.Role==TEXT("石匠")) R.BuildingArchetype=TEXT("warehouse");
        else if(R.Role==TEXT("采集者")) R.BuildingArchetype=TEXT("inn");
        else R.BuildingArchetype=TEXT("rowhouse");
    }
    EnsureResidentDesignGoal(Index);
}

bool AHearthVillage::IsRoyalSite(int32 Index) const
{
    if(TownLayoutVersion<2 || !ProductionSites.IsValidIndex(Index)) return false;
    for(const auto& Landmark:HearthCityPlan::BuildForVersion(TownLayoutVersion).Landmarks)
        if(Landmark.Kind==TEXT("castle") && ProductionSites[Index].Position.Equals(Landmark.Position,1.f)) return true;
    return false;
}

void AHearthVillage::RefreshBotanicalLandscape()
{
    for(auto& M:BotanicalMeshes) if(IsValid(M)) M->DestroyComponent();
    BotanicalMeshes.Reset();
    if(!bUseCropoutMap || TownLayoutVersion<2) return;
    // Wild planting belongs to the initial terrain. Paid palace planting is
    // installed separately as completed royal construction modules.
    FRandomStream Random(9017); const auto Species=HearthBotanicalCatalog::Species();
    const auto City=HearthCityPlan::BuildForVersion(TownLayoutVersion);
    int32 Planted=0;
    for(int32 Attempt=0;Attempt<240 && Planted<36;++Attempt)
    {
        const FString Kind=Species[Planted%Species.Num()]; const float Radius=HearthBotanicalCatalog::Radius(Kind);
        const FVector Position(Random.FRandRange(-4400,1100),Random.FRandRange(-4000,4200),8);
        if(!IsLand(Position) || !IsClearPoint(Position) || !IsLand(Position+FVector(Radius,Radius,0)) || !IsLand(Position-FVector(Radius,Radius,0))) continue;
        bool Clear=true;
        for(const auto& L:City.Landmarks) if(FVector::Dist2D(Position,L.Position)<L.Radius+Radius+100) Clear=false;
        for(const auto& Road:City.Roads) if(FVector::Dist2D(Position,FMath::ClosestPointOnSegment(Position,Road.A,Road.B))<Radius+Road.Width*.5f+100) Clear=false;
        for(int32 I=0;I<HousingPlotCount();++I) if(FVector::Dist2D(Position,PlotPositions[I])<Radius+360 || FVector::Dist2D(Position,HomeApproach(I))<Radius+160) Clear=false;
        for(const auto& S:ProductionSites) if(FVector::Dist2D(Position,S.Position)<Radius+S.Radius+100 || FVector::Dist2D(Position,S.Approach)<Radius+160) Clear=false;
        if(!Clear) continue;
        for(const auto& Part:HearthBotanicalCatalog::Build(Kind,Attempt))
            if(auto* M=AddMesh(Part.MeshPath,Position+Part.Offset,Part.Scale,&Part.Color))
            {M->SetWorldRotation(FRotator(0,Part.Yaw,0));BotanicalMeshes.Add(M);}
        ++Planted;
    }
    UE_LOG(LogTemp,Display,TEXT("BOTANICAL_LANDSCAPE species=%d clusters=%d parts=%d"),Species.Num(),Planted,BotanicalMeshes.Num());
}

bool AHearthVillage::ResidentObservationTarget(int32 Index,FVector& Center,double& Width,FString& TargetId) const
{
    if(!Residents.IsValidIndex(Index)) return false;
    const auto& R=Residents[Index]; FBox Bounds(ForceInit);
    auto Include=[&](const UStaticMeshComponent* Mesh){if(IsValid(Mesh) && Mesh->IsVisible()) Bounds+=Mesh->Bounds.GetBox();};
    const bool bKeepTemplate=PublicProject.TemplateId==TEXT("royal_keep_garden_v1") || PublicProject.TemplateId==TEXT("royal_keep_garden_v2");
    if(R.bKing && bKeepTemplate && PublicProject.Completed>0 && ProductionSites.IsValidIndex(PublicProject.Site))
    {
        TargetId=PublicProject.Id;for(const auto& M:PublicMeshes) Include(M);
        const float Radius=TownLayoutVersion>=3?5000.f:850.f;
        const float Height=TownLayoutVersion>=3?3400.f:1100.f;
        if(!Bounds.IsValid) Bounds=FBox(ProductionSites[PublicProject.Site].Position-FVector(Radius,Radius,0),ProductionSites[PublicProject.Site].Position+FVector(Radius,Radius,Height));
    }
    else
    {
        const FHearthSite* Selected=nullptr;
        for(const auto& S:ProductionSites) if(S.Owner==Index && !S.BuildPlanId.IsEmpty()) Selected=&S;
        if(Selected)
        {
            TargetId=Selected->BuildPlanId;for(const auto& M:Selected->Meshes) Include(M.Get());
            if(!Bounds.IsValid) Bounds=FBox(Selected->Position-FVector(270,270,0),Selected->Position+FVector(270,270,450));
        }
        else if(R.Plot>=0 && R.Plot<HousingPlotCount() && R.BuildProgress>=1)
        {
            TargetId=PlotIds[R.Plot];
            if(HouseMeshes.IsValidIndex(R.Plot)) Include(HouseMeshes[R.Plot]);
            for(const auto& M:StarterArchitectureMeshes[R.Plot]) Include(M.Get());
            if(!Bounds.IsValid) Bounds=FBox(PlotPositions[R.Plot]-FVector(250,250,0),PlotPositions[R.Plot]+FVector(250,250,650));
        }
        else return false;
    }
    Center=Bounds.GetCenter();
    const FVector Size=Bounds.GetSize();
    const double MaximumWidth=TownLayoutVersion>=3?24000.0:8000.0;
    Width=FMath::Clamp(FMath::Max(FMath::Max(Size.X,Size.Y)*1.65,Size.Z*1.8)+240,1100.0,MaximumWidth);
    return true;
}
