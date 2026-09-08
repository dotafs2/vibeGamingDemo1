#include "HearthVillage.h"
#include "HearthBuildingAppearance.h"
#include "HearthStructureCatalog.h"
#include "Components/StaticMeshComponent.h"

bool AHearthVillage::RefreshStarterArchitecture(int32 Plot,int32 Stage)
{
    if(Plot<0 || Plot>=HousingPlotCount()) return false;
    if(IsOrganicVillage() && Residents.IsValidIndex(PlotOwners[Plot]) && !Residents[PlotOwners[Plot]].bKing)
    { RefreshOrganicHome(PlotOwners[Plot]); return true; }
    for(auto& Mesh:StarterArchitectureMeshes[Plot]) if(Mesh.IsValid()) Mesh->DestroyComponent();
    StarterArchitectureMeshes[Plot].Reset();
    if(TownLayoutVersion<2 || Stage<3 || !Residents.IsValidIndex(PlotOwners[Plot])) return false;
    const int32 ResidentIndex=PlotOwners[Plot];EnsureResidentStory(ResidentIndex);
    const auto& R=Residents[ResidentIndex];const FString Type=R.BuildingArchetype;
    const uint32 Seed=GetTypeHash(R.StableId);

    // Town2 keeps the original starter presentation and its old plot scale. It is
    // intentionally isolated from the Town3 modular appearance contract below.
    if(TownLayoutVersion<3)
    {
        float Depth=290,Width=220,FloorHeight=190;int32 Floors=2;
        if(Type==TEXT("warehouse")){Depth=380;Width=290;FloorHeight=280;Floors=1;}
        else if(Type==TEXT("courtyard_workshop")){Depth=280;Width=300;FloorHeight=225;Floors=1;}
        else if(Type==TEXT("shop_house")){Depth=340;Width=300;}
        else if(Type==TEXT("inn")){Depth=340;Width=330;Floors=3;}
        else if(Type==TEXT("keep")){Depth=330;Width=320;}
        FloorHeight+=static_cast<int32>(Seed%3)*14-14;
        const float Height=FloorHeight*Floors;const float Base=14;
        const FLinearColor Timber(.25f,.12f,.06f),Stone(.40f,.39f,.35f),Dark(.08f,.065f,.05f),Glass(.12f,.23f,.26f);
        const FLinearColor Plaster=(Type==TEXT("keep") || R.WallMaterial==TEXT("stone"))?Stone:FLinearColor(.65f+(Seed%3)*.05f,.52f+(Seed%4)*.04f,.35f+(Seed%2)*.06f);
        const FRotator Facing(0,PlotYaws[Plot],0);
        auto Mesh=[&](const FString& Path,const FVector& Offset,const FVector& Scale,const FLinearColor* Color,float Yaw=0.f)
        {
            if(auto* M=AddMesh(Path,PlotPositions[Plot]+Facing.RotateVector(Offset),Scale,Color))
            {M->SetWorldRotation(FRotator(0,PlotYaws[Plot]+Yaw,0));StarterArchitectureMeshes[Plot].Add(M);}
        };
        auto Cube=[&](const FVector& Offset,const FVector& Size,const FLinearColor& Color)
        {Mesh(TEXT("/Engine/BasicShapes/Cube"),Offset,Size/100.f,&Color);};
        Cube(FVector(0,0,Base/2),FVector(Depth+18,Width+18,Base),Stone);
        Cube(FVector(0,0,Base+Height/2),FVector(Depth,Width,Height),Plaster);
        for(int32 X:{-1,1}) for(int32 Y:{-1,1})
            Cube(FVector(X*(Depth/2-5),Y*(Width/2-5),Base+Height/2),FVector(14,14,Height),Timber);
        for(int32 F=0;F<=Floors;++F)
        {
            const float Z=Base+F*FloorHeight;
            for(int32 Side:{-1,1})
            {
                Cube(FVector(Side*Depth/2,0,Z),FVector(12,Width+10,12),Timber);
                Cube(FVector(0,Side*Width/2,Z),FVector(Depth+10,12,12),Timber);
            }
        }
        const float DoorWidth=Type==TEXT("warehouse")?115.f:65.f;
        Cube(FVector(-Depth/2-3,0,Base+70),FVector(10,DoorWidth+14,148),Timber);
        Cube(FVector(-Depth/2-9,0,Base+67),FVector(5,DoorWidth,134),Dark);
        for(int32 F=0;F<Floors;++F)
        {
            const float Z=Base+F*FloorHeight+FloorHeight*.59f;
            for(int32 Side:{-1,1})
            {
                Cube(FVector(0,Side*(Width/2+3),Z),FVector(62,8,77),Timber);
                Cube(FVector(0,Side*(Width/2+8),Z),FVector(48,4,63),Glass);
                if(F>0) Cube(FVector(-Depth/2-5,Side*Width*.27f,Z),FVector(8,42,65),Glass);
            }
        }
        const float Rise=100+(Seed%3)*20;
        const TCHAR* RoofIds[]={TEXT("roof_slope_timber_2m"),TEXT("roof_slope_terracotta_2m"),TEXT("roof_slope_slateblue_2m")};
        const int32 RoofIndex=R.RoofMaterial==TEXT("terracotta")?1:R.RoofMaterial==TEXT("slateblue")?2:0;
        if(const auto* Roof=HearthStructureCatalog::Find(RoofIds[RoofIndex]))
        {
            for(int32 Side=0;Side<2;++Side)
                Mesh(Roof->AssetPath,FVector(0,0,Base+Height),FVector((Depth+35)/450.f,(Width+40)/200.f,Rise/128.41004f),nullptr,Side*180.f);
            for(int32 Side:{-1,1}) for(int32 Band=0;Band<4;++Band)
                Cube(FVector(0,Side*(Width/2-5),Base+Height+(Band+.5f)*Rise/5),FVector(Depth*(1-(Band+1)/5.f),12,Rise/5),Timber);
        }
        if(Type==TEXT("shop_house") || Type==TEXT("inn"))
        {
            const float X=-Depth/2-42,AwningHeight=Base+155;
            const FLinearColor Canvas=Type==TEXT("inn")?FLinearColor(.42f,.20f,.20f):FLinearColor(.48f,.44f,.19f);
            Cube(FVector(X,0,AwningHeight),FVector(90,Width-10,12),Canvas);
            for(int32 Side:{-1,1}) Cube(FVector(X-36,Side*(Width/2-18),AwningHeight/2),FVector(9,9,AwningHeight),Timber);
            Cube(FVector(-Depth/2-16,Width*.32f,Base+FloorHeight+40),FVector(10,45,50),Timber);
        }
        if(Type==TEXT("courtyard_workshop"))
        {
            const float CanopyZ=175,CanopyY=Width/2+30;
            Cube(FVector(20,CanopyY,CanopyZ),FVector(Depth-35,75,12),Timber);
            for(int32 Side:{-1,1}) Cube(FVector(Side*(Depth/2-25),CanopyY+29,CanopyZ/2),FVector(10,10,CanopyZ),Timber);
            Cube(FVector(20,CanopyY,70),FVector(110,45,14),Timber);
            for(int32 Side:{-1,1}) Cube(FVector(20+Side*44,CanopyY,33),FVector(12,35,66),Timber);
            Cube(FVector(Depth*.28f,Width*.23f,(Height+Rise+90)/2),FVector(50,55,Height+Rise+90),Stone);
        }
        if(Type==TEXT("keep"))
        {
            for(int32 Side:{-1,1})
            {
                Mesh(TEXT("/Engine/BasicShapes/Cylinder"),FVector(Depth*.28f,Side*Width*.36f,(Height+65)/2),FVector(.85f,.85f,(Height+65)/100),&Stone);
                Mesh(TEXT("/Engine/BasicShapes/Cone"),FVector(Depth*.28f,Side*Width*.36f,Height+120),FVector(1,1,1.1),&Dark);
            }
        }
        if(HouseMeshes.IsValidIndex(Plot) && IsValid(HouseMeshes[Plot])) HouseMeshes[Plot]->SetVisibility(false,true);
        return !StarterArchitectureMeshes[Plot].IsEmpty();
    }

    FHearthBuildingAppearance Appearance;
    if(!HearthBuildingAppearance::Build(Type,R.WallMaterial,R.RoofMaterial,Seed,TownLayoutVersion>=3,Appearance)) return false;
    const FRotator Facing(0,PlotYaws[Plot],0);
    auto Mesh=[&](const FHearthBuildingAppearancePart& Part)
    {
        if(auto* M=AddMesh(Part.AssetPath,PlotPositions[Plot]+Facing.RotateVector(Part.Offset),Part.Scale,nullptr))
        {M->SetWorldRotation(FRotator(0,PlotYaws[Plot]+Part.Yaw,0));StarterArchitectureMeshes[Plot].Add(M);}
    };
    for(const FHearthBuildingAppearancePart& Part:Appearance.Parts) Mesh(Part);
    if(HouseMeshes.IsValidIndex(Plot) && IsValid(HouseMeshes[Plot])) HouseMeshes[Plot]->SetVisibility(false,true);
    return !StarterArchitectureMeshes[Plot].IsEmpty();
}
