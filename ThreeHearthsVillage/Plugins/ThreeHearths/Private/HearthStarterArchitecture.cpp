#include "HearthVillage.h"
#include "Components/StaticMeshComponent.h"

bool AHearthVillage::RefreshStarterArchitecture(int32 Plot,int32 Stage)
{
    if(Plot<0 || Plot>=HousingPlotCount()) return false;
    for(auto& M:StarterArchitectureMeshes[Plot]) if(M.IsValid()) M->DestroyComponent();
    StarterArchitectureMeshes[Plot].Reset();
    if(TownLayoutVersion<2 || !bUseCropoutMap || Stage<3 || !Residents.IsValidIndex(PlotOwners[Plot])) return false;
    const auto& R=Residents[PlotOwners[Plot]];
    // Initial houses retain the existing whole-house wood-delivery recipe.
    // These are exterior kit variants; later extensions use paid catalog plans.
    const FString Type=R.BuildingArchetype;
    const bool Shop=Type==TEXT("shop_house"),Workshop=Type==TEXT("courtyard_workshop"),Warehouse=Type==TEXT("warehouse"),Inn=Type==TEXT("inn"),Keep=Type==TEXT("keep");
    FRandomStream Random(FCrc::StrCrc32(*R.StableId));
    const float Depth=Warehouse?380:Workshop?280:Shop?340:Inn?340:Keep?330:290;
    const float Width=Warehouse?290:Workshop?300:Shop?300:Inn?330:Keep?320:220;
    const int32 Floors=Inn?3:Workshop||Warehouse?1:2;
    const float FloorHeight=Warehouse?280:Workshop?225:190;
    const float Height=Floors*FloorHeight+Random.FRandRange(-14,16);
    const FLinearColor Timber(.15,.075,.035),Stone(.32,.36,.39),Glass(.12,.22,.23),Roof(.43,.15,.09);
    const FLinearColor Wall=Keep?Stone:Warehouse?FLinearColor(.34,.22,.12):FLinearColor(Random.FRandRange(.56,.73),Random.FRandRange(.44,.57),Random.FRandRange(.30,.43));
    const FRotator Facing(0,PlotYaws[Plot],0);const FVector Origin=PlotPositions[Plot];
    auto Part=[&](const FString& Path,FVector P,FVector Scale,const FLinearColor* Tint,FRotator Rotation=FRotator::ZeroRotator)
    {
        if(auto* M=AddMesh(Path,Origin+Facing.RotateVector(P),Scale,Tint))
        {M->SetWorldRotation(Facing.Quaternion()*Rotation.Quaternion());StarterArchitectureMeshes[Plot].Add(M);}
    };
    auto Box=[&](FVector P,FVector Size,const FLinearColor& Tint){Part(TEXT("/Engine/BasicShapes/Cube"),P,Size/100,&Tint);};
    Box(FVector(0,0,9),FVector(Depth+16,Width+16,18),Stone);
    Box(FVector(0,0,18+Height*.5f),FVector(Depth,Width,Height),Wall);
    for(int32 X:{-1,1}) for(int32 Y:{-1,1}) Box(FVector(X*(Depth*.5f+2),Y*(Width*.5f+2),18+Height*.5f),FVector(9,9,Height),Timber);
    for(int32 Floor=1;Floor<=Floors;++Floor)
        Box(FVector(0,0,18+Floor*Height/Floors-7),FVector(Depth+8,Width+8,12),Timber);
    Box(FVector(-Depth*.5f-3,0,85),FVector(6,Shop?110.f:Warehouse?130.f:55.f,135),Timber);
    for(int32 Floor=0;Floor<Floors;++Floor) for(int32 Side:{-1,1})
    {
        const float Z=18+Floor*Height/Floors+Height/Floors*.63f;
        Box(FVector(-Depth*.5f-4,Side*Width*.29f,Z),FVector(7,45,58),Timber);
        Box(FVector(-Depth*.5f-8,Side*Width*.29f,Z),FVector(3,31,43),Glass);
        Box(FVector(0,Side*(Width*.5f+3),Z),FVector(60,5,61),Timber);
        Box(FVector(0,Side*(Width*.5f+6),Z),FVector(44,3,45),Glass);
    }
    const float RoofRise=Inn?130:Workshop?70:Keep?115:100;
    const FString RoofKind=R.RoofMaterial==TEXT("slateblue")?TEXT("slateblue"):R.RoofMaterial==TEXT("terracotta")?TEXT("terracotta"):TEXT("timber");
    const FString RoofId=TEXT("roof_slope_")+RoofKind+TEXT("_2m");
    const FString RoofPath=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"),*RoofId,*RoofId,*RoofId);
    for(int32 Side:{-1,1}) Part(RoofPath,FVector(0,0,Height+18),FVector((Depth+35)/450,(Width+40)/200,RoofRise/128.4f),nullptr,FRotator(0,Side<0?180:0,0));
    // Thin gable infill remains under the two roof slopes.
    for(int32 Side:{-1,1}) for(int32 Step=0;Step<4;++Step)
        Box(FVector(0,Side*(Width*.5f-3),Height+18+(Step+.5f)*RoofRise/4),FVector(Depth*(1.f-(Step+1)/4.f)+8,6,RoofRise/4),Wall);
    if(Shop || Inn)
    {
        const FLinearColor Cloth=Shop?FLinearColor(.26,.37,.21):FLinearColor(.40,.18,.19);
        Part(TEXT("/Engine/BasicShapes/Cube"),FVector(-Depth*.5f-28,0,162),FVector(.65,Width/100,.06),&Cloth,FRotator(-12,0,0));
        for(int32 Side:{-1,1}) Box(FVector(-Depth*.5f-51,Side*(Width*.5f-8),81),FVector(5,5,162),Timber);
        Box(FVector(-Depth*.5f-12,Width*.5f+22,Height*.67f),FVector(9,45,55),Cloth);
    }
    if(Workshop)
    {
        Box(FVector(Depth*.5f-35,Width*.5f-35,Height*.62f),FVector(45,48,Height+100),Stone);
        Box(FVector(-Depth*.5f-26,-Width*.25f,46),FVector(52,90,12),Timber);
        for(int32 Side:{-1,1}) Box(FVector(-Depth*.5f-26,-Width*.25f+Side*34,23),FVector(10,10,46),Timber);
        Part(TEXT("/Engine/BasicShapes/Cube"),FVector(50,-Width*.5f-29,170),FVector(1.7,.68,.08),&Roof,FRotator(0,0,12));
    }
    if(Keep)
    {
        for(int32 Side:{-1,1})
        {
            const FVector Base(Depth*.36f,Side*Width*.35f,0); const float TowerHeight=Height+135;
            Part(TEXT("/Engine/BasicShapes/Cylinder"),Base+FVector(0,0,TowerHeight*.5f),FVector(.73,.73,TowerHeight/100),&Stone);
            Part(TEXT("/Engine/BasicShapes/Cone"),Base+FVector(0,0,TowerHeight+45),FVector(.94,.94,.9),&Roof);
        }
    }
    HouseMeshes[Plot]->SetVisibility(false);
    return true;
}
