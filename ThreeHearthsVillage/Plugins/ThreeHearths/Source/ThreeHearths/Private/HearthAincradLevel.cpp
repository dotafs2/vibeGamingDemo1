#include "HearthAincradLevel.h"
#include "HearthAincradFloorPlan.h"
#include "HearthAincradWorldStore.h"
#include "HearthAincradTownLayout.h"
#include "HearthAincradTownVisuals.h"
#include "HearthAincradResidentRuntime.h"
#include "HearthAincradCharacterReview.h"
#include "HearthAincradViewGrade.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/Canvas.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "RenderingThread.h"

namespace
{
    // Geography uses east/north/up; render east/south/up in UE. This adapter
    // preserves north-up maps and the clockwise front faces expected by UE.
    FVector ToUE(const FVector& P){return FVector(P.X,-P.Y,P.Z);}
    FVector At(const FVector2D& P,float Offset=0){return FVector(P,HearthAincradFloorPlan::HeightAt(P)+Offset);}
    FString MaterialPath(const FString& Id){return Id==TEXT("BlackIron")?TEXT("/Game/ThreeHearths/Materials/AincradLevel0/M_BlackIron"):TEXT("/Game/ThreeHearths/Materials/AincradLevel0/MI_Town_")+Id;}
    double DistSegment(const FVector2D& P,const FVector2D& A,const FVector2D& B)
    {
        const FVector2D D=B-A;const double T=D.SizeSquared()>0?FMath::Clamp(FVector2D::DotProduct(P-A,D)/D.SizeSquared(),0.0,1.0):0;
        return (P-A-D*T).Size();
    }
    void Pose(int32 Mode,FVector& Eye,FVector& Aim)
    {
        if(Mode==1){Eye=FVector(0,-760000,1220000);Aim=FVector(0,0,0);}
        else if(Mode==3){Eye=FVector(0,-484000,180);Aim=FVector(0,-477000,1250);}
        else if(Mode==4){Eye=At(FVector2D(-217000,-269000),15000);Aim=At(FVector2D(-205000,-255000),500);}
        else if(Mode==5){Eye=At(FVector2D(16000,268000),12000);Aim=At(FVector2D(0,290000),300);}
        else if(Mode==6){Eye=FVector(0,-470100,210);Aim=FVector(0,-465000,280);}
        else if(Mode==7){Eye=FVector(900,-468300,450);Aim=FVector(-1800,-467000,400);}
        else if(Mode==8){Eye=FVector(-1600,-467350,170);Aim=FVector(-1900,-466700,160);}
        else {Eye=FVector(61000,-523000,62000);Aim=FVector(0,-473500,0);}
        Eye=ToUE(Eye);Aim=ToUE(Aim);
    }
}

AHearthAincradLevel::AHearthAincradLevel()
{PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));}

UInstancedStaticMeshComponent* AHearthAincradLevel::Batch(const FString& ShapeName,const FString& Material)
{
    const FString Key=ShapeName+TEXT("|")+Material;
    if(auto* Existing=Batches.Find(Key)) return Existing->Get();
    const FString Path=ShapeName.StartsWith(TEXT("/"))?ShapeName:TEXT("/Engine/BasicShapes/")+ShapeName;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
    if(!Mesh){UE_LOG(LogTemp,Error,TEXT("LEVEL0_MISSING_MESH %s"),*Path);return nullptr;}
    auto* Component=NewObject<UInstancedStaticMeshComponent>(this);Component->SetupAttachment(RootComponent);
    Component->SetStaticMesh(Mesh);Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if(!Material.IsEmpty()) if(auto* M=LoadObject<UMaterialInterface>(nullptr,*MaterialPath(Material))) Component->SetMaterial(0,M);
    Component->RegisterComponent();Generated.Add(Component);Batches.Add(Key,Component);return Component;
}
void AHearthAincradLevel::Shape(const FString& Mesh,const FVector& Center,const FVector& Size,float Yaw,const FString& Material)
{if(auto* B=Batch(Mesh,Material)) B->AddInstance(FTransform(FRotator(0,-Yaw,0),ToUE(Center),Size/100.f));}
void AHearthAincradLevel::Box(const FVector& Center,const FVector& Size,float Yaw,const FString& Material)
{Shape(TEXT("Cube"),Center,Size,Yaw,Material);}
void AHearthAincradLevel::Disc(const FVector2D& Center,float Radius,float Z,const FString& Material)
{Shape(TEXT("Cylinder"),FVector(Center,Z),FVector(Radius*2,Radius*2,12),0,Material);}

void AHearthAincradLevel::Road(const TArray<FVector2D>& Points,float Width)
{
    TArray<FVector> V;TArray<int32> I;TArray<FVector2D> UV;
    for(int32 S=1;S<Points.Num();++S)
    {
        const FVector2D A=Points[S-1],B=Points[S],D=B-A,Side=FVector2D(-D.Y,D.X).GetSafeNormal()*Width*.5;
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(D.Size()/1000),1,600);
        for(int32 N=0;N<=Steps;++N)
        {
            const FVector2D P=FMath::Lerp(A,B,double(N)/Steps);const int32 K=V.Num();
            V.Add(ToUE(At(P-Side,12)));V.Add(ToUE(At(P+Side,12)));UV.Add(FVector2D(0,N));UV.Add(FVector2D(1,N));
            if(N>0 && !(HearthAincradTownLayout::IsReplacementArea(P) && FMath::Abs(P.X)>620)) I.Append({K-2,K,K-1,K-1,K,K+1});
        }
    }
    auto* C=NewObject<UProceduralMeshComponent>(this);C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->CreateMeshSection_LinearColor(0,V,I,TArray<FVector>(),UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*MaterialPath(TEXT("Paving"))));C->RegisterComponent();Generated.Add(C);
}

void AHearthAincradLevel::House(const FVector2D& P,float Yaw,int32 Seed)
{
    FRandomStream R(Seed);const float W=R.FRandRange(850,1250),D=R.FRandRange(1100,1700);
    const int32 Floors=R.RandRange(2,3);const float H=Floors*320.f;const float Z=HearthAincradFloorPlan::HeightAt(P);
    const FTransform Frame(FRotator(0,Yaw,0),FVector(P,Z));
    const FString Wall=(Seed%3==0)?TEXT("PlasterOchre"):(Seed%3==1)?TEXT("PlasterWarm"):TEXT("PlasterSage");
    const auto Part=[&](FVector C,FVector S,const FString& M){Box(Frame.TransformPosition(C),S,Yaw,M);};
    Part(FVector(0,0,H*.5),FVector(W,D,H),Wall);
    Part(FVector(0,0,25),FVector(W+30,D+30,50),TEXT("Stone"));
    // Proportional bays, thin eaves and a shared roof plane, independent of
    // the retired miniature-house kit. This blockout has no usable interiors.
    for(int32 S:{-1,1})
    {
        Part(FVector(S*W*.5,0,H*.5),FVector(16,D+16,H),TEXT("Timber"));
        Part(FVector(0,S*D*.5,H),FVector(W+55,45,20),TEXT("TimberDark"));
        for(int32 F=0;F<Floors;++F)
        {
            Part(FVector(0,S*(D*.5+6),F*320+8),FVector(W+12,14,14),TEXT("Timber"));
            for(int32 Win:{-1,1})
            {
                Part(FVector(Win*W*.27,S*(D*.5+10),F*320+180),FVector(120,12,155),TEXT("Slate"));
                Part(FVector(Win*W*.27,S*(D*.5+19),F*320+180),FVector(7,10,155),TEXT("TimberDark"));
            }
        }
    }
    Part(FVector(0,-D*.5-10,110),FVector(110,20,220),TEXT("TimberDark"));
    if(Seed%4==0) Part(FVector(W*.28,D*.25,H+180),FVector(90,100,360),TEXT("Stone"));
    const int32 Color=Seed%2;const int32 Base=RoofVertices[Color].Num();
    const float X=W*.5+28,Y=D*.5+32,Ridge=H+W*.27;
    const FVector Local[]={{-X,-Y,H},{X,-Y,H},{-X,Y,H},{X,Y,H},{0,-Y,Ridge},{0,Y,Ridge}};
    for(const auto& L:Local){RoofVertices[Color].Add(ToUE(Frame.TransformPosition(L)));RoofUVs[Color].Add(FVector2D(L.X/200,L.Y/200));}
    const int32 Tri[]={0,4,2,2,4,5,4,1,5,5,1,3,0,1,4,2,5,3};
    for(int32 Id:Tri) RoofIndices[Color].Add(Base+Id);
    ++HouseCount;
}

void AHearthAincradLevel::BuildPreview()
{
    for(auto C:Generated) if(IsValid(C)) C->DestroyComponent();Generated.Reset();Batches.Reset();HouseCount=0;TreeCount=0;
    for(int32 N=0;N<2;++N){RoofVertices[N].Reset();RoofIndices[N].Reset();RoofUVs[N].Reset();}
    const auto Plan=HearthAincradFloorPlan::Build();
    TArray<FVector> V;TArray<int32> I;TArray<FVector2D> UV;
    constexpr int32 Rings=140,Angles=320;
    V.Add(ToUE(At(FVector2D(0,0))));UV.Add(FVector2D(0,0));
    for(int32 Ring=1;Ring<=Rings;++Ring) for(int32 A=0;A<Angles;++A)
    {
        const double T=2*PI*A/Angles;const FVector2D P(FMath::Cos(T)*Plan.FloorRadiusCm*Ring/Rings,FMath::Sin(T)*Plan.FloorRadiusCm*Ring/Rings);
        V.Add(ToUE(At(P)));UV.Add(P/1000);
        const int32 K=1+(Ring-1)*Angles+A,Next=1+(Ring-1)*Angles+(A+1)%Angles;
        if(Ring==1) I.Append({0,K,Next});
        else {const int32 Prev=K-Angles,PrevNext=Next-Angles;I.Append({Prev,K,PrevNext,PrevNext,K,Next});}
    }
    auto* Ground=NewObject<UProceduralMeshComponent>(this);Ground->SetupAttachment(RootComponent);
    Ground->CreateMeshSection_LinearColor(0,V,I,TArray<FVector>(),UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),true);
    Ground->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*MaterialPath(TEXT("Grass"))));Ground->RegisterComponent();Generated.Add(Ground);
    // Real circular floor edge and exposed underside, rather than a square
    // ground plane with a central artificial hill.
    Shape(TEXT("Cylinder"),FVector(0,0,-8100),FVector(1000000,1000000,200),0,TEXT("Stone"));
    TArray<FVector> RimV;TArray<int32> RimI;TArray<FVector2D> RimUV;
    for(int32 A=0;A<=Angles;++A)
    {
        const double T=2*PI*A/Angles;const FVector2D P(FMath::Cos(T)*Plan.FloorRadiusCm,FMath::Sin(T)*Plan.FloorRadiusCm);
        const int32 K=RimV.Num();RimV.Add(ToUE(At(P)));RimV.Add(ToUE(FVector(P,-8000)));RimUV.Add(FVector2D(A,0));RimUV.Add(FVector2D(A,1));
        if(A>0) RimI.Append({K-2,K-1,K,K,K-1,K+1});
    }
    auto* Rim=NewObject<UProceduralMeshComponent>(this);Rim->SetupAttachment(RootComponent);
    Rim->CreateMeshSection_LinearColor(0,RimV,RimI,TArray<FVector>(),RimUV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    Rim->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*MaterialPath(TEXT("Stone"))));Rim->RegisterComponent();Generated.Add(Rim);
    for(const auto& Route:Plan.Routes) Road(Route.WaypointsCm,Route.FromRegionId.Contains(TEXT("beginnings"))?1200:850);
    Disc(FVector2D(235000,-120000),62500,-100,TEXT("Slate"));

    TArray<TArray<FVector2D>> CityRoads;
    for(float Radius:{14000.f,23000.f,32000.f,41000.f})
    {
        TArray<FVector2D> Arc;
        for(int32 A=5;A<=175;A+=2) Arc.Add(Plan.TownWallCenterCm+FVector2D(FMath::Cos(FMath::DegreesToRadians(double(A))),FMath::Sin(FMath::DegreesToRadians(double(A))))*Radius);
        Road(Arc,650);CityRoads.Add(Arc);
    }
    for(float A:{25.f,60.f,90.f,120.f,155.f})
    {
        const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(A)),FMath::Sin(FMath::DegreesToRadians(A)));
        TArray<FVector2D> Lane={Plan.TownWallCenterCm+Dir*11500,Plan.TownWallCenterCm+Dir*48000};Road(Lane,900);CityRoads.Add(Lane);
    }
    for(const auto& Route:Plan.Routes) if(Route.FromRegionId.Contains(TEXT("beginnings"))) CityRoads.Add(Route.WaypointsCm);
    // Frontages line streets. Depth and height vary within a bounded grammar.
    int32 Seed=800;
    for(float StreetRadius:{14000.f,23000.f,32000.f,41000.f}) for(int32 Side:{-1,1})
    {
        const double Radius=StreetRadius+Side*1500;
        for(double A=8;A<173;A+=FMath::RadiansToDegrees(1550.0/Radius))
        {
            const double Radians=FMath::DegreesToRadians(A);const FVector2D P=Plan.TownWallCenterCm+FVector2D(FMath::Cos(Radians),FMath::Sin(Radians))*Radius;
            if(HearthAincradTownLayout::IsReplacementArea(P))continue;
            if((P-FVector2D(0,-477000)).Size()<7600 || (P-FVector2D(0,-490000)).Size()<10800) continue;
            bool Clear=true;
            for(const auto& Lane:CityRoads) for(int32 N=1;N<Lane.Num();++N) if(DistSegment(P,Lane[N-1],Lane[N])<1280) Clear=false;
            if(Clear) House(P,float(A)+(Side>0? -90.f:90.f),Seed++);
        }
    }
    // Northern semicircular city wall, leaving three unmistakable gates.
    for(int32 A=3;A<178;A+=3)
    {
        if(FMath::Abs(A-30)<3 || FMath::Abs(A-90)<3 || FMath::Abs(A-150)<3) continue;
        const double T=FMath::DegreesToRadians(double(A));const FVector2D P=Plan.TownWallCenterCm+FVector2D(FMath::Cos(T),FMath::Sin(T))*50000;
        Box(FVector(P,600),FVector(2620,350,1200),A+90,TEXT("Stone"));
        if(A%12==3) {Shape(TEXT("Cylinder"),FVector(P,850),FVector(900,900,1700),0,TEXT("Stone"));Shape(TEXT("Cone"),FVector(P,1970),FVector(1100,1100,570),0,TEXT("Slate"));}
    }
    const FVector2D Plaza(0,-477000);Disc(Plaza,6200,15,TEXT("Paving"));
    // Clock/teleport landmark and connected Black Iron Palace massing.
    Box(FVector(Plaza+FVector2D(0,2400),1600),FVector(650,650,3200),0,TEXT("Stone"));
    Shape(TEXT("Cone"),FVector(Plaza+FVector2D(0,2400),3650),FVector(1050,1050,1000),0,TEXT("Slate"));
    for(int32 S:{-1,1}) Box(FVector(Plaza+FVector2D(S*700,1000),400),FVector(160,220,800),0,TEXT("Stone"));
    Box(FVector(Plaza+FVector2D(0,1000),900),FVector(1560,220,200),0,TEXT("Stone"));
    const FVector2D Palace(0,-490000);
    Box(FVector(Palace,900),FVector(13500,6500,1800),0,TEXT("BlackIron"));
    Shape(TEXT("Sphere"),FVector(Palace,1800),FVector(7500,7500,7000),0,TEXT("BlackIron"));
    for(int32 S:{-1,1}) {Shape(TEXT("Cylinder"),FVector(Palace+FVector2D(S*5900,0),1800),FVector(2300,2300,3600),0,TEXT("BlackIron"));Shape(TEXT("Cone"),FVector(Palace+FVector2D(S*5900,0),4400),FVector(2800,2800,1800),0,TEXT("BlackIron"));}
    Box(FVector(Palace+FVector2D(0,3300),800),FVector(2100,200,1600),0,TEXT("TimberDark"));

    // Two separate settlements, each built around a small public space.
    for(const FVector2D Center:{FVector2D(-205000,-255000),FVector2D(0,290000)})
    {
        Disc(Center,2100,HearthAincradFloorPlan::HeightAt(Center)+10,TEXT("Paving"));
        for(int32 Row=0;Row<2;++Row) for(int32 A=0;A<12;++A)
        {
            const double T=2*PI*A/12;const FVector2D P=Center+FVector2D(FMath::Cos(T),FMath::Sin(T))*(3800+Row*2800);
            House(P,float(A*30-90),Seed++);
        }
    }
    // Valley-town windmill silhouettes, northern labyrinth and ruins.
    for(int32 S:{-1,1})
    {
        FVector P=At(FVector2D(S*8500,294000),1700);Shape(TEXT("Cylinder"),P,FVector(750,750,3400),0,TEXT("Stone"));
        Box(P+FVector(0,-410,600),FVector(3200,70,110),0,TEXT("Timber"));Box(P+FVector(0,-410,600),FVector(110,70,3200),0,TEXT("Timber"));
    }
    const FVector Lab=At(FVector2D(0,420000),5000);
    Shape(TEXT("Cylinder"),Lab,FVector(30000,30000,10000),0,TEXT("Stone"));
    for(int32 A=0;A<16;++A)
    {const double T=2*PI*A/16;Shape(TEXT("Cylinder"),Lab+FVector(FMath::Cos(T)*14300,FMath::Sin(T)*14300,0),FVector(1900,1900,11600),0,TEXT("Slate"));}
    Box(Lab+FVector(0,-14900,-3000),FVector(3000,400,4000),0,TEXT("TimberDark"));
    for(int32 A=0;A<22;++A)
    {const FVector2D P(FMath::Sin(A*2.4)*23000,75000+FMath::Cos(A*1.73)*18000);Shape(TEXT("Cylinder"),At(P,1500),FVector(650,650,1800+(A%5)*600),0,TEXT("Stone"));}

    FRandomStream Forest(9273);
    for(int32 Cluster=0;Cluster<4;++Cluster)
    {
        const FVector2D C=Cluster==0?FVector2D(-190000,-170000):Cluster==1?FVector2D(-205000,-255000):Cluster==2?FVector2D(-185000,230000):FVector2D(320000,130000);
        const float Radius=Cluster==1?38000:95000;
        // A continuous forest-floor biome remains readable from the full-floor
        // view; tree instances provide the local silhouettes at street height.
        TArray<FVector> FloorV;TArray<int32> FloorI;TArray<FVector2D> FloorUV;
        FloorV.Add(ToUE(At(C,5)));FloorUV.Add(C/1000);
        for(int32 Ring=1;Ring<=12;++Ring) for(int32 Angle=0;Angle<96;++Angle)
        {
            const double A=2*PI*Angle/96;const double Edge=.87+.08*FMath::Sin(A*5)+.04*FMath::Cos(A*9);
            const FVector2D P=C+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius*Edge*Ring/12;
            FloorV.Add(ToUE(At(P,5)));FloorUV.Add(P/1000);
            const int32 K=1+(Ring-1)*96+Angle,Next=1+(Ring-1)*96+(Angle+1)%96;
            if(Ring==1)FloorI.Append({0,K,Next});else FloorI.Append({K-96,K,Next-96,Next-96,K,Next});
        }
        auto* ForestFloor=NewObject<UProceduralMeshComponent>(this);ForestFloor->SetupAttachment(RootComponent);
        ForestFloor->CreateMeshSection_LinearColor(0,FloorV,FloorI,TArray<FVector>(),FloorUV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
        auto* ForestMat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,*MaterialPath(TEXT("Grass"))),this);
        ForestMat->SetVectorParameterValue(TEXT("HearthTint"),FLinearColor(.11f,.19f,.08f));ForestFloor->SetMaterial(0,ForestMat);ForestFloor->RegisterComponent();Generated.Add(ForestFloor);
        for(int32 N=0;N<1050;++N)
        {
            const double A=Forest.FRand()*2*PI,R=FMath::Sqrt(Forest.FRand())*Radius;const FVector2D P=C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R;
            bool Clear=HearthAincradFloorPlan::IsInsideFloor(P);
            for(const auto& Region:Plan.Regions) if(Region.Kind==TEXT("settlement") && (P-Region.CenterCm).Size()<13000) Clear=false;
            for(const auto& Route:Plan.Routes) for(int32 S=1;S<Route.WaypointsCm.Num();++S) if(DistSegment(P,Route.WaypointsCm[S-1],Route.WaypointsCm[S])<1700) Clear=false;
            if(!Clear) continue;
            if(auto* B=Batch(N%2?TEXT("/Game/Environment/Meshes/Crops/SM_Tree_01"):TEXT("/Game/Environment/Meshes/Crops/SM_Tree_02"),TEXT("")))
            {const float Scale=Forest.FRandRange(1.5f,2.3f);B->AddInstance(FTransform(FRotator(0,Forest.FRand()*360,0),ToUE(At(P)),FVector(Scale)));++TreeCount;}
        }
    }
    for(int32 N=0;N<2;++N)
    {
        auto* Roof=NewObject<UProceduralMeshComponent>(this);Roof->SetupAttachment(RootComponent);
        Roof->CreateMeshSection_LinearColor(0,RoofVertices[N],RoofIndices[N],TArray<FVector>(),RoofUVs[N],TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
        Roof->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*MaterialPath(N?TEXT("Terracotta"):TEXT("Slate"))));Roof->RegisterComponent();Generated.Add(Roof);
    }
    HouseCount+=HearthAincradTownVisuals::Build(this,Generated);
    auto* Sun=NewObject<UDirectionalLightComponent>(this);Sun->SetupAttachment(RootComponent);Sun->SetMobility(EComponentMobility::Movable);Sun->SetIntensity(8.f);Sun->SetLightColor(FLinearColor(1.f,.95f,.86f));Sun->SetWorldRotation(FRotator(-48,-25,0));Sun->LightSourceAngle=2.f;Sun->RegisterComponent();Generated.Add(Sun);
    auto* Sky=NewObject<USkyLightComponent>(this);Sky->SetupAttachment(RootComponent);Sky->SetMobility(EComponentMobility::Movable);Sky->SourceType=SLS_SpecifiedCubemap;Sky->SetCubemap(LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")));Sky->SetIntensity(2.5f);Sky->RegisterComponent();Generated.Add(Sky);
    auto* Post=NewObject<UPostProcessComponent>(this);Post->SetupAttachment(RootComponent);Post->bUnbound=true;Post->Priority=120;HearthAincradViewGrade::Apply(Post->Settings);Post->RegisterComponent();Generated.Add(Post);
    UE_LOG(LogTemp,Display,TEXT("LEVEL0_LAYOUT floor_diameter_m=10000 houses=%d trees=%d regions=%d batches=%d retired_royal_actors=0"),HouseCount,TreeCount,Plan.Regions.Num(),Batches.Num());
}

void AHearthAincradLevel::BeginPlay()
{
    Super::BeginPlay();StartedAt=FPlatformTime::Seconds();LastSaved=StartedAt;
    StatePath=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradLevel0/world.json");
    FString VerificationPath;
    if(FParse::Value(FCommandLine::Get(),TEXT("AincradVerificationWorld="),VerificationPath))
    {
        VerificationPath=FPaths::ConvertRelativePathToFull(VerificationPath);
        FPaths::NormalizeFilename(VerificationPath);
        FPaths::CollapseRelativeDirectories(VerificationPath);
        const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradLevel0/VerificationWorlds"));
        if(!FParse::Param(FCommandLine::Get(),TEXT("HearthDisableApi"))
            || !FPaths::IsUnderDirectory(VerificationPath,Allowed) || !IFileManager::Get().FileExists(*VerificationPath))
        {bFailed=true;UE_LOG(LogTemp,Error,TEXT("LEVEL0_VERIFICATION_WORLD_REFUSED"));FPlatformMisc::RequestExit(false);return;}
        StatePath=VerificationPath;
        UE_LOG(LogTemp,Display,TEXT("LEVEL0_VERIFICATION_COPY no paid decisions, original world untouched"));
    }
    FString Error;
    if(!HearthAincradWorldStore::LoadOrCreate(StatePath,PersistentState,Error)){bFailed=true;UE_LOG(LogTemp,Error,TEXT("LEVEL0_STORE_REFUSED %s"),*Error);return;}
    StartElapsed=PersistentState->GetNumberField(TEXT("elapsed_seconds"));
    BuildPreview();SetView(9);FParse::Value(FCommandLine::Get(),TEXT("AincradReviewSeconds="),DurationSeconds);
    ResidentRuntime=GetWorld()->SpawnActor<AHearthAincradResidentRuntime>();
    const TWeakObjectPtr<AHearthAincradLevel> WeakThis(this);
    if(!ResidentRuntime || !ResidentRuntime->Initialize(PersistentState,[WeakThis](){return WeakThis.IsValid() && WeakThis->SaveState();}))
    {bFailed=true;UE_LOG(LogTemp,Error,TEXT("TOWN_RESIDENT_INIT_FAILED"));}
    UE_LOG(LogTemp,Display,TEXT("LEVEL0_WORLD loaded=%s elapsed=%.3f"),*WorldId(),StartElapsed);
}
FString AHearthAincradLevel::WorldId() const{return PersistentState.IsValid()?PersistentState->GetStringField(TEXT("world_id")):TEXT("unloaded");}
bool AHearthAincradLevel::SaveState()
{
    if(bFailed || !PersistentState.IsValid()) return false;
    PersistentState->SetNumberField(TEXT("elapsed_seconds"),StartElapsed+FPlatformTime::Seconds()-StartedAt);
    FString Error;if(!HearthAincradWorldStore::Save(StatePath,PersistentState.ToSharedRef(),Error)) {UE_LOG(LogTemp,Error,TEXT("LEVEL0_SAVE_FAILED %s"),*Error);return false;}return true;
}
void AHearthAincradLevel::EndPlay(const EEndPlayReason::Type Reason){if(IsValid(ResidentRuntime))ResidentRuntime->Destroy();SaveState();Super::EndPlay(Reason);}
void AHearthAincradLevel::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(bFailed) return;const double Now=FPlatformTime::Seconds();
    double StopAtUtc=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("AincradStopUtc="),StopAtUtc)
        && (!FMath::IsFinite(StopAtUtc) || static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp())>=StopAtUtc-25.0))
    {
        SaveState();
        UE_LOG(LogTemp,Display,TEXT("LEVEL0_ABSOLUTE_STOP saved before user deadline"));
        FPlatformMisc::RequestExit(false);
        return;
    }
    if(Now-LastSaved>=20){SaveState();LastSaved=Now;}
    if(Now-StartedAt>8) HearthAincradCharacterReview::TryCapture(this,StatePath);
    if(auto* PC=GetWorld()->GetFirstPlayerController())
    {
        for(int32 N=1;N<=9;++N)
        {
            const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine};
            if(PC->WasInputKeyJustPressed(Keys[N-1])) SetView(N);
        }
        if(PC->WasInputKeyJustPressed(EKeys::F)) SetView(ViewMode==9?6:9);
    }
    if(!bCaptured && Now-StartedAt>12 && FParse::Param(FCommandLine::Get(),TEXT("AincradCapture")))
    {
        bCaptured=true;
        CaptureViews();
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradLevel0/Views");
        if(IsValid(ResidentRuntime))
            FFileHelper::SaveStringToFile(ResidentRuntime->LifeReport(),*(Folder/TEXT("life-report.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        // SceneCapture does not include the player's HUD. Preserve a native
        // viewport frame as separate evidence without altering any NPC camera.
        FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(Folder/TEXT("player-hud.png")),true,false);
    }
    if(!bWalkVerified && Now-StartedAt>14 && FParse::Param(FCommandLine::Get(),TEXT("AincradVerifyWalk"))
        && FParse::Param(FCommandLine::Get(),TEXT("HearthDisableApi")))
    {
        bWalkVerified=true;
        if(auto* PC=GetWorld()->GetFirstPlayerController()) if(auto* Pawn=Cast<AHearthAincradExplorer>(PC->GetPawn()))
        {
            const FTransform Before=Pawn->GetActorTransform();
            Pawn->SetWalking(true);
            Pawn->SetActorLocationAndRotation(FVector(700,465350,90),FRotator::ZeroRotator);
            for(int32 Step=0;Step<90;++Step) Pawn->WalkDisplacement(FVector(10,0,0));
            const bool bWall=Pawn->GetActorLocation().X>900 && Pawn->GetActorLocation().X<1100;
            Pawn->SetActorLocationAndRotation(FVector(700,465000,90),FRotator::ZeroRotator);
            for(int32 Step=0;Step<45;++Step) Pawn->WalkDisplacement(FVector(10,0,0));
            const bool bDoor=Pawn->GetActorLocation().X>1145;
            const double EyeHeight=Pawn->Camera->GetComponentLocation().Z-(Pawn->GetActorLocation().Z-90);
            auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("source"),TEXT("local player walking movement including step and ground handling; not NPC decisions or VR validation"));
            Result->SetStringField(TEXT("door_end_cm"),Pawn->GetActorLocation().ToString());
            Result->SetBoolField(TEXT("wall_blocked"),bWall);Result->SetBoolField(TEXT("door_passable"),bDoor);
            Result->SetNumberField(TEXT("eye_height_cm"),EyeHeight);Result->SetBoolField(TEXT("passed"),bWall&&bDoor&&FMath::Abs(EyeHeight-160)<1);
            FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
            const FString Path=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradLevel0/Views/walking-check.json");
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
            FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            Pawn->SetActorTransform(Before);
            UE_LOG(LogTemp,Display,TEXT("WALKING_CHECK wall=%d door=%d eye=%.1f"),bWall,bDoor,EyeHeight);
        }
    }
    if(DurationSeconds>0 && Now-StartedAt>DurationSeconds){SaveState();DurationSeconds=0;FPlatformMisc::RequestExit(false);}
}
void AHearthAincradLevel::SetView(int32 Mode)
{
    ViewMode=FMath::Clamp(Mode,1,9);
    if(auto* PC=GetWorld()->GetFirstPlayerController()) if(auto* Pawn=Cast<AHearthAincradExplorer>(PC->GetPawn()))
    {
        if(ViewMode==9)
        {
            const auto Street=HearthAincradTownLayout::Build();
            const FVector StreetStart=Street.MainStreet.Num()>0?Street.MainStreet[0]:FVector(0,470000,0);
            const FVector StreetEnd=Street.MainStreet.Num()>1?Street.MainStreet[1]:StreetStart+FVector(0,-1,0);
            const FVector Forward=(StreetEnd-StreetStart).GetSafeNormal2D();
            Pawn->SetWalking(true);
            Pawn->SetActorLocationAndRotation(
                FVector(StreetStart.X,StreetStart.Y,90.f),
                FRotator(0.f,Forward.Rotation().Yaw,0.f));
            Pawn->Speed=320.f;
        }
        else
        {
            Pawn->SetWalking(false);
            FVector Eye,Aim;Pose(ViewMode,Eye,Aim);
            Pawn->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
            Pawn->Speed=ViewMode==1?140000:(ViewMode==3 || ViewMode>=6)?500:8000;
        }
        PC->SetViewTarget(Pawn);
    }
}
void AHearthAincradLevel::CaptureViews()
{
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradLevel0/Views");IFileManager::Get().MakeDirectory(*Folder,true);
    const auto Plan=HearthAincradFloorPlan::Build();auto Layout=MakeShared<FJsonObject>();Layout->SetStringField(TEXT("world_id"),WorldId());Layout->SetStringField(TEXT("source_policy"),Plan.SourcePolicy);Layout->SetStringField(TEXT("coordinate_frame"),TEXT("east/north/up cm; UE=(east,-north,up)"));Layout->SetNumberField(TEXT("floor_radius_cm"),Plan.FloorRadiusCm);
    TArray<TSharedPtr<FJsonValue>> RegionRows,RouteRows;
    for(const auto& R:Plan.Regions){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("id"),R.Id);O->SetStringField(TEXT("label"),R.ChineseLabel);O->SetNumberField(TEXT("east_cm"),R.CenterCm.X);O->SetNumberField(TEXT("north_cm"),R.CenterCm.Y);O->SetStringField(TEXT("kind"),R.Kind);RegionRows.Add(MakeShared<FJsonValueObject>(O));}
    for(const auto& R:Plan.Routes){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("from"),R.FromRegionId);O->SetStringField(TEXT("to"),R.ToRegionId);TArray<TSharedPtr<FJsonValue>> Points;for(const auto& P:R.WaypointsCm){auto Pt=MakeShared<FJsonObject>();Pt->SetNumberField(TEXT("east_cm"),P.X);Pt->SetNumberField(TEXT("north_cm"),P.Y);Points.Add(MakeShared<FJsonValueObject>(Pt));}O->SetArrayField(TEXT("points"),Points);RouteRows.Add(MakeShared<FJsonValueObject>(O));}
    Layout->SetArrayField(TEXT("regions"),RegionRows);Layout->SetArrayField(TEXT("routes"),RouteRows);FString LayoutJson;FJsonSerializer::Serialize(Layout,TJsonWriterFactory<>::Create(&LayoutJson));FFileHelper::SaveStringToFile(LayoutJson,*(Folder/TEXT("floor-plan.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    for(int32 Mode=1;Mode<=9;++Mode)
    {
        const int32 Width=Mode==6?512:1600,Height=Mode==6?512:1000;
        FVector Eye,Aim;Pose(Mode==6?2:Mode>6?Mode-1:Mode,Eye,Aim);auto* Target=NewObject<UTextureRenderTarget2D>(this);Target->RenderTargetFormat=RTF_RGBA8;Target->InitAutoFormat(Width,Height);Target->UpdateResourceImmediate(true);
        auto* Capture=NewObject<USceneCaptureComponent2D>(this);Capture->TextureTarget=Target;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->bAlwaysPersistRenderingState=true;
        Capture->CaptureSource=SCS_FinalColorLDR;Capture->FOVAngle=65;HearthAincradViewGrade::Apply(Capture->PostProcessSettings);Capture->PostProcessBlendWeight=1;Capture->RegisterComponent();Capture->SetWorldLocationAndRotation(Eye,(Aim-Eye).Rotation());
        for(int32 N=0;N<4;++N){Capture->CaptureScene();FlushRenderingCommands();}
        TArray<FColor> Pixels;const bool Good=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);Capture->DestroyComponent();if(!Good) continue;for(auto& P:Pixels)P.A=255;
        auto& Images=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));auto PNG=Images.CreateImageWrapper(EImageFormat::PNG);PNG->SetRaw(Pixels.GetData(),Pixels.Num()*sizeof(FColor),Width,Height,ERGBFormat::BGRA,8);
        const auto& Compressed=PNG->GetCompressed();TArray<uint8> Bytes;Bytes.Append(Compressed.GetData(),int32(Compressed.Num()));
        const TCHAR* Names[]={TEXT("floor"),TEXT("beginnings"),TEXT("plaza"),TEXT("horunka"),TEXT("tolbana"),TEXT("director-town"),TEXT("starter-street"),TEXT("starter-inn"),TEXT("inn-interior")};const FString Path=Folder/(FString(Names[Mode-1])+TEXT(".png"));FFileHelper::SaveArrayToFile(Bytes,*Path);
        auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("world_id"),WorldId());Meta->SetStringField(TEXT("provenance"),TEXT("native UE geographic blockout; coordinator camera, not NPC FOV"));Meta->SetStringField(TEXT("eye_cm"),Eye.ToString());Meta->SetStringField(TEXT("aim_cm"),Aim.ToString());Meta->SetNumberField(TEXT("horizontal_fov"),65);Meta->SetNumberField(TEXT("houses"),HouseCount);Meta->SetNumberField(TEXT("trees"),TreeCount);
        FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Path+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp,Display,TEXT("LEVEL0_CAPTURE mode=%d bytes=%d"),Mode,Bytes.Num());
    }
}

AHearthAincradExplorer::AHearthAincradExplorer()
{
    PrimaryActorTick.bCanEverTick=true;
    Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("WalkingCapsule"));
    RootComponent=Capsule;
    Capsule->InitCapsuleSize(42.f,90.f);
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    Capsule->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    Capsule->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Capsule);
    Camera->FieldOfView=65;
    HearthAincradViewGrade::Apply(Camera->PostProcessSettings);
    Camera->PostProcessBlendWeight=1;
}
void AHearthAincradExplorer::SetWalking(bool bEnable)
{
    if(bEnable && !bWalking) WalkPitch=0.f;
    bWalking=bEnable;
    if(Capsule) Capsule->SetCollisionEnabled(bWalking?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    if(Camera)
    {
        Camera->SetRelativeLocation(bWalking?FVector(0.f,0.f,70.f):FVector::ZeroVector);
        Camera->SetRelativeRotation(bWalking?FRotator(WalkPitch,0.f,0.f):FRotator::ZeroRotator);
    }
}
void AHearthAincradExplorer::WalkDisplacement(const FVector& Delta)
{
    if(!bWalking || !Capsule || !GetWorld()) return;
    constexpr float CapsuleHalfHeight=90.f;
    constexpr float MaxStepHeight=35.f;
    constexpr float MaxGroundDrop=80.f;
    const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AincradExplorerWalk),false,this);
    const auto FindGround=[&](const FVector& Position,float Drop,float& GroundZ)
    {
        FHitResult GroundHit;
        const FVector TraceStart=Position+FVector(0.f,0.f,40.f);
        const FVector TraceEnd=Position-FVector(0.f,0.f,CapsuleHalfHeight+Drop+40.f);
        if(GetWorld()->LineTraceSingleByChannel(GroundHit,TraceStart,TraceEnd,ECC_WorldStatic,QueryParams) && GroundHit.bBlockingHit)
        {
            GroundZ=GroundHit.ImpactPoint.Z;return true;
        }
        // The street preview intentionally has no road collision mesh;
        // use the authored floor datum there so walking remains grounded.
        if(HearthAincradTownLayout::IsReplacementArea(FVector2D(Position.X,-Position.Y)))
        {
            GroundZ=HearthAincradFloorPlan::HeightAt(FVector2D(Position.X,-Position.Y));return true;
        }
        return false;
    };
    const FVector Before=GetActorLocation();
    FHitResult MoveHit;
    Capsule->MoveComponent(Delta,GetActorQuat(),true,&MoveHit);
    if(MoveHit.bBlockingHit)
    {
        bool bStepped=false;
        Capsule->SetWorldLocation(Before,false);
        FHitResult StepUpHit;
        Capsule->MoveComponent(FVector(0.f,0.f,MaxStepHeight),GetActorQuat(),true,&StepUpHit);
        if(!StepUpHit.bBlockingHit)
        {
            FHitResult StepForwardHit;
            Capsule->MoveComponent(Delta,GetActorQuat(),true,&StepForwardHit);
            float GroundZ=0.f;
            const FVector Stepped=GetActorLocation();
            if(!StepForwardHit.bBlockingHit && FindGround(Stepped,MaxStepHeight+MaxGroundDrop,GroundZ)
                && GroundZ+CapsuleHalfHeight-Stepped.Z<=MaxStepHeight+2.f)
            {
                Capsule->MoveComponent(FVector(0.f,0.f,GroundZ+CapsuleHalfHeight-Stepped.Z),GetActorQuat(),true,nullptr);
                bStepped=true;
            }
        }
        if(!bStepped)
        {
            Capsule->SetWorldLocation(Before,false);
            const FVector Slide=FVector::VectorPlaneProject(Delta,MoveHit.Normal);
            Capsule->MoveComponent(Slide,GetActorQuat(),true,nullptr);
        }
    }
    float GroundZ=0.f;
    const FVector After=GetActorLocation();
    if(FindGround(After,MaxGroundDrop,GroundZ))
    {
        const float TargetZ=GroundZ+CapsuleHalfHeight;
        if(FMath::Abs(TargetZ-After.Z)<=MaxStepHeight+MaxGroundDrop)
            Capsule->MoveComponent(FVector(0.f,0.f,TargetZ-After.Z),GetActorQuat(),true,nullptr);
    }
}
void AHearthAincradExplorer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);auto* PC=Cast<APlayerController>(GetController());if(!PC)return;
    if(bWalking)
    {
        if(PC->IsInputKeyDown(EKeys::RightMouseButton))
        {
            float X,Y;PC->GetInputMouseDelta(X,Y);
            FRotator Body=GetActorRotation();Body.Yaw+=X*.2f;SetActorRotation(FRotator(0.f,Body.Yaw,0.f));
            WalkPitch=FMath::Clamp(WalkPitch-Y*.2f,-82.f,82.f);
            if(Camera) Camera->SetRelativeRotation(FRotator(WalkPitch,0.f,0.f));
        }
        FVector Wish=FVector::ZeroVector;
        if(PC->IsInputKeyDown(EKeys::W))Wish+=GetActorForwardVector();
        if(PC->IsInputKeyDown(EKeys::S))Wish-=GetActorForwardVector();
        if(PC->IsInputKeyDown(EKeys::D))Wish+=GetActorRightVector();
        if(PC->IsInputKeyDown(EKeys::A))Wish-=GetActorRightVector();
        Wish.Z=0.f;
        if(!Wish.IsNearlyZero())
        {
            const float MoveSpeed=Speed*(PC->IsInputKeyDown(EKeys::LeftShift)?1.75f:1.f);
            WalkDisplacement(Wish.GetSafeNormal()*MoveSpeed*DeltaSeconds);
        }
        return;
    }
    FVector Move=FVector::ZeroVector;
    if(PC->IsInputKeyDown(EKeys::W))Move+=GetActorForwardVector();if(PC->IsInputKeyDown(EKeys::S))Move-=GetActorForwardVector();
    if(PC->IsInputKeyDown(EKeys::D))Move+=GetActorRightVector();if(PC->IsInputKeyDown(EKeys::A))Move-=GetActorRightVector();
    if(PC->IsInputKeyDown(EKeys::E))Move+=FVector::UpVector;if(PC->IsInputKeyDown(EKeys::Q))Move-=FVector::UpVector;
    AddActorWorldOffset(Move.GetClampedToMaxSize(1)*Speed*DeltaSeconds*(PC->IsInputKeyDown(EKeys::LeftShift)?4:1));
    if(PC->IsInputKeyDown(EKeys::RightMouseButton)){float X,Y;PC->GetInputMouseDelta(X,Y);FRotator R=GetActorRotation();R.Yaw+=X*.2;R.Pitch=FMath::Clamp(R.Pitch-Y*.2,-89.f,89.f);SetActorRotation(R);}
}
AHearthAincradGameMode::AHearthAincradGameMode(){DefaultPawnClass=AHearthAincradExplorer::StaticClass();HUDClass=AHearthAincradHUD::StaticClass();}
void AHearthAincradHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;AHearthAincradLevel* Level=nullptr;for(TActorIterator<AHearthAincradLevel> It(GetWorld());It;++It){Level=*It;break;}
    DrawRect(FLinearColor(.03f,.06f,.075f,.82f),20,20,840,92);
    DrawText(TEXT("LEVEL0  /  AINCRAD FLOOR 1"),FLinearColor(.98f,.91f,.74f),38,32,nullptr,1.5);
    DrawText(TEXT("1 Floor  2 City  3 Plaza  4 Horunka  5 Tolbana  6 Street  7 Inn  8 Interior"),FLinearColor::White,38,66,nullptr,1);
    const FString Controls=Level&&Level->ViewMode==9
        ?TEXT("F survey | WASD walk | Right mouse look | Shift run")
        :TEXT("F walk | WASD/QE fly | Right mouse look | 1-8 views");
    DrawText(*Controls,FLinearColor(.8f,.85f,.85f),38,87,nullptr,.9f);
    for(TActorIterator<AHearthAincradResidentRuntime> It(GetWorld());It;++It)
    {
        const FString Life=It->LifeReport();
        if(!Life.IsEmpty())
        {
            TArray<FString> Lines;
            Life.ParseIntoArrayLines(Lines);
            float WidestLine = 1.f;
            for (const FString& Line : Lines)
            {
                float Width=0.f, Height=0.f;
                GetTextSize(Line,Width,Height,nullptr,1.f);
                WidestLine=FMath::Max(WidestLine,Width);
            }
            const float Scale = FMath::Min(1.1f,FMath::Max(.75f,(Canvas->SizeX-68.f)/WidestLine));
            const float LineHeight = 23.f * Scale;
            const float Top = Canvas->SizeY - 40.f - Lines.Num() * LineHeight;
            DrawRect(FLinearColor(.03f,.06f,.075f,.86f),20,Top,Canvas->SizeX-40,20.f+Lines.Num()*LineHeight);
            for (int32 Index=0; Index<Lines.Num(); ++Index)
                DrawText(Lines[Index],FLinearColor(.98f,.91f,.74f),34,Top+10.f+Index*LineHeight,nullptr,Scale);
        }
        break;
    }
    if(Level && Level->ViewMode==9) for(TActorIterator<AHearthAincradResidentVisual> It(GetWorld());It;++It)
    {
        const FVector Eye=PlayerOwner->PlayerCameraManager->GetCameraLocation();
        const FVector Label=It->GetActorLocation()+FVector(0,0,110);
        if(FVector::Dist(Eye,Label)>1800) continue;
        FHitResult Occlusion;FCollisionQueryParams Query(SCENE_QUERY_STAT(AincradNameplate),false,PlayerOwner->GetPawn());
        Query.AddIgnoredActor(*It);
        if(GetWorld()->LineTraceSingleByChannel(Occlusion,Eye,Label,ECC_Visibility,Query))continue;
        const FVector P=Canvas->Project(Label);if(P.Z<=0)continue;
        const FString Name=It->ActorHasTag(TEXT("innkeeper"))?TEXT("Aileen"):
            It->ActorHasTag(TEXT("blacksmith"))?TEXT("Takuma"):TEXT("Kashiwagi");
        DrawText(Name,FLinearColor(.98f,.91f,.74f),P.X-24,P.Y,nullptr,1.1f);
    }
    if(Level && Level->ViewMode==1) for(const auto& R:HearthAincradFloorPlan::Build().Regions)
    {
        if(R.Kind.Contains(TEXT("city")) && R.Id!=TEXT("beginnings_plaza"))continue;
        const FVector P=Canvas->Project(ToUE(At(R.CenterCm,1800)));if(P.Z<=0)continue;
        DrawRect(FLinearColor(.02f,.04f,.045f,.82f),P.X-4,P.Y-3,170,26);DrawText(R.Id,FLinearColor::White,P.X,P.Y,nullptr,1);
    }
}
