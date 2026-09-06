#include "HearthVillage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogThreeHearths, Log, All);

namespace Hearth
{
    const FString Shapes = TEXT("/Engine/BasicShapes/");
    const FString Houses = TEXT("/Game/Environment/Meshes/Building/");
    const FLinearColor Grass(0.25f,0.40f,0.14f);
    const FLinearColor Soil(0.26f,0.16f,0.085f);
    const FLinearColor Path(0.56f,0.43f,0.24f);
    const FLinearColor Wood(0.32f,0.16f,0.06f);

    const FLinearColor Foliage(0.11f,0.26f,0.12f);
    const FLinearColor Water(0.15f,0.34f,0.37f);
    const FName Generated(TEXT("ThreeHearthsGenerated"));
}

void AHearthVillage::AssignHouseStyle(int32 Index,FHearthResident& R) const
{
    const int32 Style=R.bKing?2:Index%3;
    SetHouseStyle(Style,R);
}

bool AHearthVillage::SetHouseStyle(int32 Style,FHearthResident& R) const
{
    if(Style==0) { R.HouseBlueprint=TEXT("cottage_terracotta"); R.WallMaterial=TEXT("plaster"); R.RoofMaterial=TEXT("terracotta"); }
    else if(Style==1) { R.HouseBlueprint=TEXT("longhouse_slateblue"); R.WallMaterial=TEXT("timber"); R.RoofMaterial=TEXT("slateblue"); }
    else if(Style==2) { R.HouseBlueprint=TEXT("townhouse_terracotta"); R.WallMaterial=TEXT("stone"); R.RoofMaterial=TEXT("terracotta"); }
    else return false;
    return true;
}

AHearthVillager::AHearthVillager()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Villager"));
    Body->SetupAttachment(RootComponent);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetRelativeRotation(FRotator(0,-90,0));
    Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Meshes/SKM_Villager"));
    Body->SetSkeletalMesh(Mesh.Object);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Characters/Animations/A_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("/Game/Characters/Animations/A_Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> ChopAsset(TEXT("/Game/Characters/Animations/A_Woodchopping"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> BuildAsset(TEXT("/Game/Characters/Animations/A_Building"));
    Idle=IdleAsset.Object; Walk=WalkAsset.Object; Chop=ChopAsset.Object; Build=BuildAsset.Object;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> FarmAsset(TEXT("/Game/Characters/Animations/A_Farming"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MineAsset(TEXT("/Game/Characters/Animations/A_Mining"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> GatherAsset(TEXT("/Game/Characters/Animations/A_Gathering"));
    Farm=FarmAsset.Object; Mine=MineAsset.Object; Gather=GatherAsset.Object;
    Hat = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ProfessionHat"));
    Hat->SetupAttachment(Body);
    Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hat->SetLeaderPoseComponent(Body);
    auto* Pick = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ClickTarget"));
    Pick->SetupAttachment(RootComponent);
    Pick->SetRelativeLocation(FVector(0,0,75));
    Pick->SetCapsuleSize(60,95);
    Pick->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Pick->SetCollisionResponseToAllChannels(ECR_Ignore);
    Pick->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    SelectionDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Selection"));
    SelectionDisc->SetupAttachment(RootComponent);
    SelectionDisc->SetStaticMesh(Cylinder.Object);
    SelectionDisc->SetRelativeLocation(FVector(0,0,3));
    SelectionDisc->SetRelativeScale3D(FVector(1.1f,1.1f,0.035f));
    SelectionDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionDisc->SetCastShadow(false);
    Bundle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedWood"));
    Bundle->SetupAttachment(RootComponent);
    Bundle->SetStaticMesh(Cube.Object);
    Bundle->SetRelativeLocation(FVector(37,0,65));
    Bundle->SetRelativeScale3D(FVector(0.32f,0.60f,0.25f));
    Bundle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bundle->SetVisibility(false);
    Tool = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorkTool"));
    Tool->SetupAttachment(Body,TEXT("hand_r"));
    Tool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tool->SetCanEverAffectNavigation(false);
    Tool->SetVisibility(false);
}

void AHearthVillager::ConfigureAppearance(int32 Profile)
{
    for(const auto& Part:AppearanceParts) if(IsValid(Part)) Part->DestroyComponent();
    AppearanceParts.Reset();
    const auto* Mesh=Body->GetSkeletalMeshAsset(); if(!Mesh) return;
    // The old profession instances belong to M_Hat, not the skin/body slot.
    if(Mesh->GetMaterials().Num()>0) Body->SetMaterial(0,Mesh->GetMaterials()[0].MaterialInterface);
    const auto& Ref=Mesh->GetRefSkeleton();
    auto Add=[&](const TCHAR* Id,const TCHAR* Bone,const TCHAR* Kit=TEXT("ResidentKit"),FVector Offset=FVector(0,0,0),float Scale=1.f)
    {
        const int32 BoneIndex=Ref.FindBoneIndex(FName(Bone)); if(BoneIndex==INDEX_NONE) return;
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/%s/%s/%s"),Kit,Id,Id);
        auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path); if(!Asset) return;
        FTransform BoneRef=Ref.GetRefBonePose()[BoneIndex];
        for(int32 Parent=Ref.GetParentIndex(BoneIndex);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent)) BoneRef=BoneRef*Ref.GetRefBonePose()[Parent];
        // Exported origins coincide with the reference bone; their axes remain mesh-parallel.
        const FTransform Desired(FQuat::Identity,BoneRef.GetLocation()+Offset,FVector(Scale));
        auto* Part=NewObject<UStaticMeshComponent>(this,FName(Id));
        Part->SetStaticMesh(Asset); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false); Part->SetupAttachment(Body,FName(Bone));
        Part->SetRelativeTransform(Desired.GetRelativeTransform(BoneRef));
        AddInstanceComponent(Part); Part->RegisterComponent(); AppearanceParts.Add(Part);
    };
    switch(Profile)
    {
    case 0: Add(TEXT("hair_cropped_dark"),TEXT("head")); Add(TEXT("pouch_belt_double"),TEXT("pelvis")); break;
    case 1: Add(TEXT("hair_braid_auburn"),TEXT("head")); Add(TEXT("hat_straw_wide"),TEXT("head")); Add(TEXT("apron_linen_short"),TEXT("spine_02")); break;
    case 2: Add(TEXT("hair_waves_chestnut"),TEXT("head")); Add(TEXT("scarf_red"),TEXT("neck")); Add(TEXT("pouch_belt_double"),TEXT("pelvis")); break;
    case 3: Add(TEXT("hair_swept_silver"),TEXT("head")); Add(TEXT("beard_neat_silver"),TEXT("head")); Add(TEXT("cape_royal_blue"),TEXT("spine_02")); Add(TEXT("regalia_king_crown"),TEXT("head"),TEXT("SocietyKit"),FVector(0,0,23.5f),2.f); break;
    case 4: Add(TEXT("hair_cropped_dark"),TEXT("head")); Add(TEXT("cap_merchant_plum"),TEXT("head")); Add(TEXT("bag_crossbody_leather"),TEXT("spine_02")); break;
    case 5: Add(TEXT("headwrap_sage"),TEXT("head")); Add(TEXT("apron_linen_short"),TEXT("spine_02")); break;
    case 6: Add(TEXT("hair_bun_dark"),TEXT("head")); Add(TEXT("scarf_red"),TEXT("neck")); break;
    case 7: Add(TEXT("hair_swept_silver"),TEXT("head")); Add(TEXT("shawl_ochre"),TEXT("spine_02")); Add(TEXT("bag_crossbody_leather"),TEXT("spine_02")); break;
    case 8: Add(TEXT("hair_waves_chestnut"),TEXT("head")); Add(TEXT("backpack_bedroll"),TEXT("spine_02")); Add(TEXT("scarf_red"),TEXT("neck")); break;
    default: Add(TEXT("hair_bun_dark"),TEXT("head")); Add(TEXT("apron_linen_short"),TEXT("spine_02")); Add(TEXT("pouch_belt_double"),TEXT("pelvis")); break;
    }
    Hat->SetVisibility(AppearanceParts.IsEmpty());
}

void AHearthVillager::SetMotion(EHearthTask Task, float Rate, int32 WorkKind)
{
    UpdateTool(Task,WorkKind);
    if (Task != LastMotion || WorkKind != LastWorkKind || !Body->IsPlaying())
    {
        UAnimSequence* Animation = Idle;
        if (Task==EHearthTask::ToWood || Task==EHearthTask::ToHome || Task==EHearthTask::LifeTravel || Task==EHearthTask::ProductionTravel || Task==EHearthTask::ProductionDeliver || Task==EHearthTask::TradeTravel) Animation=Walk;
        else if (Task==EHearthTask::Chopping) Animation=Chop;
        else if (Task==EHearthTask::Building) Animation=Build;
        if(Task==EHearthTask::ProductionWork)
        {
            Animation=WorkKind==10?Chop:WorkKind==11?Mine:(WorkKind==9 || WorkKind==12)?Gather:(WorkKind==0 || WorkKind==8)?Farm:Build;
        }
        LastWorkKind=WorkKind;
        Body->PlayAnimation(Animation,true);
        LastMotion=Task;
    }
    Body->GlobalAnimRateScale=Rate;
}

void AHearthVillager::UpdateTool(EHearthTask Task,int32 WorkKind)
{
    const FString Id=(Task==EHearthTask::ProductionWork)?EquippedToolId:FString(); FVector Grip;
    if(Id==TEXT("tool_axe")) Grip=FVector(-7.925f,0,12.5f);
    else if(Id==TEXT("tool_pickaxe")) Grip=FVector(1.347f,0,12.5f);
    else if(Id==TEXT("tool_hammer")) Grip=FVector(1.8f,.1f,11.5f);
    else if(Id==TEXT("tool_shovel")) Grip=FVector(0,.3f,13.5f);
    else if(Id==TEXT("tool_trowel")) Grip=FVector(-.868f,.2f,9.8f);
    else if(Id==TEXT("tool_hoe")) Grip=FVector(0,0,13.5f);
    else if(Id==TEXT("tool_saw")) Grip=FVector(-32.35f,.25f,10.2f);
    else if(Id==TEXT("tool_mallet")) Grip=FVector(0,0,11.5f);
    if(Id.IsEmpty())
    {
        EquippedToolId.Empty(); Tool->SetVisibility(false); return;
    }
    if(!Tool->GetStaticMesh() || Tool->GetStaticMesh()->GetName()!=Id)
    {
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/ToolKit/%s/%s.%s"),*Id,*Id,*Id);
        UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
        if(!Mesh) { EquippedToolId.Empty(); Tool->SetVisibility(false); return; }
        Tool->SetStaticMesh(Mesh); EquippedToolId=Id;
    }
    else EquippedToolId=Id;
    Tool->SetRelativeRotation(FRotator::ZeroRotator);
    Tool->SetRelativeLocation(-Grip);
    Tool->SetRelativeScale3D(FVector::OneVector);
    Tool->SetVisibility(true);
}

AHearthVillage::AHearthVillage()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PrimaryActorTick.bCanEverTick=true;
    PlotPositions[0]=FVector(420,-620,8);
    PlotPositions[1]=FVector(420,0,8);
    PlotPositions[2]=FVector(420,620,8);
    WoodPositions[0]=FVector(-860,-590,8);
    WoodPositions[1]=FVector(-860,0,8);
    WoodPositions[2]=FVector(-860,590,8);
}

UStaticMeshComponent* AHearthVillage::AddMesh(const FString& Path, const FVector& Position, const FVector& Scale, const FLinearColor* Color)
{
    UStaticMesh* Asset=LoadObject<UStaticMesh>(nullptr,*Path);
    if (!Asset) { UE_LOG(LogThreeHearths,Error,TEXT("Missing scene mesh: %s"),*Path); return nullptr; }
    auto* Mesh=NewObject<UStaticMeshComponent>(this);
    Mesh->ComponentTags.Add(Hearth::Generated);
    Mesh->SetStaticMesh(Asset);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetRelativeLocation(Position);
    Mesh->SetRelativeScale3D(Scale);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (Color && TintMaterial)
    {
        auto* Material=UMaterialInstanceDynamic::Create(TintMaterial,this);
        Material->SetVectorParameterValue(TEXT("VillageTint"),*Color);
        Mesh->SetMaterial(0,Material);
    }
    Mesh->RegisterComponent();
    return Mesh;
}

void AHearthVillage::BuildEnvironment()
{
    TArray<UStaticMeshComponent*> Previous;
    GetComponents(Previous);
    for (auto* Component:Previous) if (Component->ComponentHasTag(Hearth::Generated)) Component->DestroyComponent();
    HouseMeshes.Empty(); StockMeshes.Empty();
    TintMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ThreeHearths/Materials/M_VillageTint"));
    if(bUseCropoutMap) { BuildIslandVillage(); return; }
    AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(0,0,-23),FVector(24,21,0.45f),&Hearth::Grass);
    AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(0,0,-105),FVector(24.15f,21.15f,1.2f),&Hearth::Soil);
    AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(0,0,-210),FVector(180,180,0.10f),&Hearth::Water);
    AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(-85,0,0.5f),FVector(2.7f,18.8f,0.04f),&Hearth::Path);
    const FLinearColor Plinth(0.39f,0.32f,0.19f);
    for (int32 I=0;I<3;++I)
    {
        const FVector P=PlotPositions[I];
        AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(-370,P.Y,1),FVector(7.3f,0.9f,0.04f),&Hearth::Path);
        AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(0,0,-3),FVector(4.3f,4.3f,0.06f),&Plinth);
        const FLinearColor Marker=ResidentColor(I);
        for (int32 Side=-1;Side<=1;Side+=2)
        {
            AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(0,Side*218,2),FVector(4.4f,0.055f,0.04f),&Marker);
            AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(Side*218,0,2),FVector(0.055f,4.4f,0.04f),&Marker);
        }
        auto* House=AddMesh(Hearth::Houses+TEXT("SM_House_01"),P,FVector(1));
        HouseMeshes.Add(House);
        House->SetVisibility(false);
        auto* Stock=AddMesh(Hearth::Shapes+TEXT("Cube"),WoodPositions[I]+FVector(-15,0,20),FVector(1.1f,1.2f,0.4f),&Hearth::Wood);
        StockMeshes.Add(Stock);
        for (int32 T=0;T<3;++T)
        {
            FVector Tree=WoodPositions[I]+FVector(-160+(T%2)*150,125+T*85,0);
            AddMesh(Hearth::Shapes+TEXT("Cylinder"),Tree+FVector(0,0,65),FVector(0.22f,0.22f,1.3f),&Hearth::Wood);
            AddMesh(Hearth::Shapes+TEXT("Cone"),Tree+FVector(0,0,190),FVector(1.95f,1.95f,2.35f),&Hearth::Foliage);
            AddMesh(Hearth::Shapes+TEXT("Cone"),Tree+FVector(0,0,295),FVector(1.30f,1.30f,1.70f),&Hearth::Foliage);
        }
    }
    // Give the woodland plot a visible landmark of its own.
    for(int32 T=0;T<2;++T)
    {
        const FVector Tree(880+T*170,-770+T*80,8);
        AddMesh(Hearth::Shapes+TEXT("Cylinder"),Tree+FVector(0,0,65),FVector(0.22f,0.22f,1.3f),&Hearth::Wood);
        AddMesh(Hearth::Shapes+TEXT("Cone"),Tree+FVector(0,0,190),FVector(1.95f,1.95f,2.35f),&Hearth::Foliage);
        AddMesh(Hearth::Shapes+TEXT("Cone"),Tree+FVector(0,0,295),FVector(1.30f,1.30f,1.70f),&Hearth::Foliage);
    }
    // A small shared garden is a landmark for the sociable resident's choice.
    for (int32 Row=0;Row<2;++Row) for(int32 Col=0;Col<4;++Col)
        AddMesh(TEXT("/Game/Environment/Meshes/Crops/SM_Crop_Corn_03"),FVector(865+Row*90,-130+Col*85,5),FVector(0.65f));
    for(int32 I=0;I<14;++I)
    {
        FVector P(-1090+(I%7)*340,(I/7==0?-950:950),2);
        AddMesh(TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_01"),P,FVector(1.1f));
    }
}

void AHearthVillage::BuildIslandVillage()
{
    // This verified land corridor belongs to the copied /Game/Village island.
    // Keep props clear of the walking lanes and retain the terrain's own materials.
    const FLinearColor Plinth(0.43f,0.36f,0.22f);
    AddMesh(Hearth::Shapes+TEXT("Cube"),FVector(-2130,0,3.6f),FVector(3.4f,52.f,0.012f),&Hearth::Path);
    const FVector AddedPlots[]={FVector(-2800,-1900,8),FVector(-2800,0,8),FVector(-2800,1900,8),
        FVector(400,-1900,8),FVector(-2800,-950,8),FVector(-2800,950,8),FVector(-1000,950,8)};
    for(int32 I=0;I<HousingPlotCount();++I)
    {
        PlotPositions[I]=I<3?FVector(-1000,-1900+I*1900,8):AddedPlots[I-3];
        const FVector P=PlotPositions[I];
        AddMesh(Hearth::Shapes+TEXT("Cube"),FVector((-2130+P.X)*.5f,P.Y,3.8f),FVector(FMath::Abs(P.X+2130)/100.f,1.5f,.014f),&Hearth::Path);
        AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(0,0,-3),FVector(5.4f,5.4f,0.025f),&Plinth);
        const FLinearColor Marker=ResidentColor(I);
        for(int32 Side=-1;Side<=1;Side+=2)
        {
            AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(0,Side*270,-1),FVector(5.4f,0.065f,0.025f),&Marker);
            AddMesh(Hearth::Shapes+TEXT("Cube"),P+FVector(Side*270,0,-1),FVector(0.065f,5.4f,0.025f),&Marker);
        }
        auto* House=AddMesh(Hearth::Houses+TEXT("SM_House_01"),P,FVector(1));
        HouseMeshes.Add(House); House->SetVisibility(false);
    }
    for(int32 I=0;I<3;++I)
    {
        WoodPositions[I]=FVector(-3600,-1900+I*1900,8);
        auto* Stock=AddMesh(Hearth::Shapes+TEXT("Cube"),WoodPositions[I]+FVector(-15,0,20),FVector(1.1f,1.2f,0.4f),&Hearth::Wood);
        StockMeshes.Add(Stock);
    }
    auto Decorate=[this](const FString& Asset,FVector Position,float Scale)
    {
        FHitResult Hit;
        FCollisionQueryParams Query; Query.bTraceComplex=true; Query.AddIgnoredActor(this);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Position+FVector(0,0,1000),Position-FVector(0,0,1000),ECC_Visibility,Query)
           && Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("ThreeHearthsBaseTerrain")) && Hit.ImpactPoint.Z>0)
            AddMesh(Asset,FVector(Position.X,Position.Y,Hit.ImpactPoint.Z),FVector(Scale));
    };
    const FString Crops=TEXT("/Game/Environment/Meshes/Crops/");
    // Productive farms, trees, shrubs and stone nodes are created by InitializeProduction.
    Decorate(Hearth::Houses+TEXT("SM_TownHall"),FVector(-1100,-1050,0),1.0f);
    // Repopulate the base island without the original spawner's GameMode dependency.
    FRandomStream Scatter(583);
    for(int32 X=-6500;X<=6500;X+=1000) for(int32 Y=-6500;Y<=6500;Y+=1000)
    {
        if(X>-5200 && X<2500 && Y>-3800 && Y<4000) continue;
        const FVector P(X+Scatter.FRandRange(-240,240),Y+Scatter.FRandRange(-240,240),0);
        const int32 Kind=Scatter.RandRange(0,4);
        const FString Asset=Kind<3?Crops+(Kind%2?TEXT("SM_Tree_01"):TEXT("SM_Tree_02")):
            Crops+(Kind==3?TEXT("SM_Stone_02"):TEXT("SM_Shrub_01"));
        Decorate(Asset,P,Scatter.FRandRange(0.8f,1.25f));
    }
}

void AHearthVillage::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!GetWorld()->IsGameWorld()) BuildEnvironment();
}

void AHearthVillage::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(),TEXT("HearthCaptureDelay="),AcceptanceCaptureDelay);
    BuildEnvironment();
    LoadHistory();
    ResetVillageState();
    InitializeWorldPersistence();
    float RequestedSpeed=0;
    if(FParse::Value(FCommandLine::Get(),TEXT("HearthSimulationSpeed="),RequestedSpeed) && FMath::IsFinite(RequestedSpeed))
        SetSimulationSpeed(RequestedSpeed);
}

FLinearColor AHearthVillage::ResidentColor(int32 I) const
{
    const FLinearColor Colors[]={FLinearColor(.36f,.72f,.64f),FLinearColor(.95f,.62f,.24f),FLinearColor(.66f,.51f,.85f),FLinearColor(.9f,.75f,.2f),
        FLinearColor(.3f,.62f,.85f),FLinearColor(.82f,.42f,.33f),FLinearColor(.45f,.5f,.6f),FLinearColor(.76f,.48f,.66f),FLinearColor(.45f,.67f,.28f),FLinearColor(.85f,.65f,.5f)};
    return Colors[FMath::Clamp(I,0,9)];
}

void AHearthVillage::ResetVillageState()
{
    CloseHistoryRun(TEXT("重新开始了新一轮，旧任务已结束。"));
    StopDecisionRequests();
    CurrentRun=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    WorldId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    WorldRevision=0; WorldSaveTimer=0;
    LastLifeResident=-1;
    LoadApiConfig();
    for (auto& R:Residents) if (IsValid(R.Actor)) R.Actor->Destroy();
    Residents.Empty();
    Conversations.Reset(); Commitments.Reset(); Transactions.Reset(); WagePayables.Reset(); TradeOffers.Reset(); TreasuryCoins=500; bSocialOpen=false; ++SocialRevision;
    Elapsed=0; SnapshotTimer=0; SimulationRemainder=0; NextTradeAt=8.f; bReportedComplete=false; bSimulationPaused=false;
    for(int32 I=0;I<3;++I)
    {
        WoodStock[I]=bUseCropoutMap?33:12;
        if(StockMeshes.IsValidIndex(I)) { StockMeshes[I]->SetVisibility(true); StockMeshes[I]->SetRelativeScale3D(FVector(1.1f,1.2f,.4f)); }
    }
    for (int32 I=0;I<HousingPlotCount();++I)
    {
        PlotOwners[I]=-1;
        PlotIds[I]=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        if (HouseMeshes.IsValidIndex(I)) { HouseMeshes[I]->SetVisibility(false); HouseMeshes[I]->SetWorldScale3D(FVector(1)); }
        FHearthResident R;
        R.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        InitializeResidentIdentity(I,R);
        AssignHouseStyle(I,R);
        R.Reason=TEXT("先看看村庄里哪些地块适合自己。");
        R.LatestEvent=TEXT("带着自己的想法来到村庄。");
        R.Timer=1.5f+I*1.4f;
        R.Actor=GetWorld()->SpawnActor<AHearthVillager>(AHearthVillager::StaticClass(),FVector(bUseCropoutMap?-2100.f:-60.f,bUseCropoutMap?-850+I*180:-180+I*180,8),FRotator(0,180,0));
        R.Actor->ResidentIndex=I;
        const TCHAR* Hats[]={TEXT("SKM_Woodcutter"),TEXT("SKM_Farmer"),TEXT("SKM_Miner")};
        if (auto* Hat=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Characters/Meshes/Hats/"))+Hats[I%3])))
        {
            R.Actor->Hat->SetSkeletalMesh(Hat);
            R.Actor->Hat->SetLeaderPoseComponent(R.Actor->Body,true);
        }
        R.Actor->ConfigureAppearance(I);
        if (TintMaterial)
        {
            auto* M=UMaterialInstanceDynamic::Create(TintMaterial,this);
            M->SetVectorParameterValue(TEXT("VillageTint"),ResidentColor(I));
            R.Actor->SelectionDisc->SetMaterial(0,M);
            auto* B=UMaterialInstanceDynamic::Create(TintMaterial,this);
            B->SetVectorParameterValue(TEXT("VillageTint"),Hearth::Wood);
            R.Actor->Bundle->SetMaterial(0,B);
        }
        Residents.Add(R);
    }
    PendingDecisions.SetNum(Residents.Num());
    InitializeProduction();
    SelectResident(0);
    bHistoryOpen=false;
    VillageEvent=FString::Printf(TEXT("%d位居民抵达了。每个人都带着自己的想法，准备在这里安家。"),Residents.Num());
    UE_LOG(LogThreeHearths,Display,TEXT("SESSION_STARTED residents=%d wood=%d backend=%s"),Residents.Num(),AvailableWood(),bApiReady?*ApiBackend:TEXT("local"));
    WriteSnapshot();
}

void AHearthVillage::SelectResident(int32 Index)
{
    if(!Residents.IsValidIndex(Index)) return;
    SelectedResident=Index;
    if(!bSocialOpen) bHistoryOpen=true;
    for(int32 I=0;I<Residents.Num();++I) if(IsValid(Residents[I].Actor))
        Residents[I].Actor->SelectionDisc->SetRelativeScale3D(FVector(I==Index?1.25f:0.68f,I==Index?1.25f:0.68f,0.035f));
}

void AHearthVillage::TogglePause() { bSimulationPaused=!bSimulationPaused; WriteSnapshot(); }
void AHearthVillage::SetSimulationSpeed(float Speed)
{
    if(!FMath::IsFinite(Speed)) return;
    SimulationSpeed=FMath::Clamp(Speed,1.f,1000.f);
    WriteSnapshot();
}
void AHearthVillage::CycleSpeed()
{
    const float Presets[]={1.f,3.f,10.f,30.f,100.f,300.f,1000.f};
    for(float Speed:Presets) if(Speed>SimulationSpeed) { SetSimulationSpeed(Speed); return; }
    SetSimulationSpeed(1.f);
}

void AHearthVillage::Decide(int32 Index)
{
    if(bApiReady) RequestDecision(Index);
    else DecideLocally(Index,bApiConfigured?ApiStatus:FString());
}

void AHearthVillage::DecideLocally(int32 Index, const FString& Failure)
{
    auto& R=Residents[Index];
    // All candidates are validated locally. Personality supplies preference, not world mutations.
    int32 Best=-1; float BestScore=-FLT_MAX;
    for(int32 P=0;P<HousingPlotCount();++P)
    {
        if(PlotOwners[P]!=-1) continue;
        float Score=0;
        if(Index==0) Score=P==0?100.f:10.f; // Quiet woodland edge.
        if(Index==1) Score=100.f-FMath::Abs(PlotPositions[P].Y)*0.1f; // Shared garden / center.
        if(Index==2) Score=100.f-PlotCosts[P]*5.f; // Low material budget.
        if(Index>=3) Score=100.f-FMath::Abs(P-Index)*12.f-PlotCosts[P]*2.f;
        if(Score>BestScore) { BestScore=Score; Best=P; }
    }
    if(Best<0) { R.Timer=2.f; R.Reason=TEXT("暂时没有空地，等一会儿再看看。"); return; }
    const TCHAR* Reasons[]={TEXT("树林边安静，离木材也近。多花一点木料，我也想住这里。"),TEXT("我想住在花园和邻居旁边。以后大家见面、互相帮忙都方便。"),TEXT("先建一间够住的小屋。只要六份木材，余下的留给村庄。")};
    FString Reason=Best==Index && Index<3?Reasons[Index]:FString::Printf(TEXT("我想在%s安家，准备好 %d 份木材，再慢慢添置我喜欢的东西。"),*PlotLabel(Best),PlotCosts[Best]);
    if(ReservePlot(Index,Best,Reason,false))
    {
        R.DecisionSource=Failure.IsEmpty()?TEXT("local"):TEXT("local_fallback");
        R.DecisionNote=Failure;
        auto& Record=DecisionHistory[R.HistoryIndex]; Record.Source=R.DecisionSource;
        if(!Failure.IsEmpty() && !Record.Context.Contains(Failure)) Record.Context+=TEXT("\n采用备用规则：")+Failure;
        if(!Failure.IsEmpty()) Record.Result=TEXT("本地备用选择：")+Failure+TEXT("；正在执行。");
        ++HistoryRevision; SaveHistory();
    }
}

bool AHearthVillage::ReservePlot(int32 Index,int32 Plot,const FString& Reason,bool bFromApi)
{
    if(!Residents.IsValidIndex(Index) || Plot<0 || Plot>=HousingPlotCount() || PlotOwners[Plot]!=-1) return false;
    if(bUseCropoutMap)
    {
        const FVector P=PlotPositions[Plot];
        if(!IsLand(P) || !IsLand(P+FVector(230,230,0)) || !IsLand(P+FVector(-230,230,0))
            || !IsLand(P+FVector(230,-230,0)) || !IsLand(P-FVector(230,230,0)) || !IsClearPoint(P-FVector(245,0,0))) return false;
    }
    auto& R=Residents[Index];
    if(R.Plot!=-1 || R.Task!=EHearthTask::Choosing) return false;
    if(!DecisionHistory.IsValidIndex(R.HistoryIndex) || DecisionHistory[R.HistoryIndex].Status!=TEXT("thinking")) StartHistory(Index,false,bFromApi?TEXT("api"):TEXT("local"));
    PlotOwners[Plot]=Index; R.Plot=Plot; R.Reason=Reason;
    R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    R.DecisionSource=bFromApi?TEXT("api"):TEXT("local");
    AcceptHistory(Index,TEXT("在")+PlotNameFor(Index)+TEXT("建家"),Reason,R.DecisionSource);
    R.LatestEvent=FString::Printf(TEXT("选中了%s，需要 %d 份木材。"),*PlotNameFor(Index),CostFor(Index));
    VillageEvent=R.Name+TEXT("：")+R.LatestEvent;
    SetHouseStage(Plot,0);
    UE_LOG(LogThreeHearths,Display,TEXT("PLOT_RESERVED resident=%d plot=%d cost=%d source=%s"),Index,Plot,PlotCosts[Plot],*R.DecisionSource);
    SeekWood(Index);
    return true;
}

void AHearthVillage::SetRoute(int32 Index,const FVector& Target)
{
    auto& R=Residents[Index]; R.Route.Empty(); R.MoveRetry=0; R.bMovementBlocked=false;
    const FVector Start=R.Actor->GetActorLocation();
    if(bUseCropoutMap && !ProductionSites.IsEmpty())
    {
        if(FindActivityRoute(Index,Target,R.Route)) return;
        R.Route={Target}; R.MoveRetry=.5f; R.bMovementBlocked=true; return;
    }
    const float Lane=(bUseCropoutMap?-2200.f:-170.f)+Index*70.f;
    R.Route.Add(FVector(Lane,Start.Y,Start.Z));
    R.Route.Add(FVector(Lane,Target.Y,Target.Z));
    R.Route.Add(Target);
}

void AHearthVillage::SeekWood(int32 Index)
{
    auto& R=Residents[Index];
    int32 Best=-1; double Distance=DBL_MAX;
    for(int32 S=0;S<3;++S) if(WoodStock[S]>0)
    {
        const double D=FVector::DistSquared(R.Actor->GetActorLocation(),WoodPositions[S]);
        if(D<Distance) { Best=S; Distance=D; }
    }
    if(Best<0) { R.Timer=1.f; R.Task=EHearthTask::Chopping; R.Source=-1; R.LatestEvent=TEXT("木材暂时不足，等待补充。"); return; }
    R.Source=Best; R.Task=EHearthTask::ToWood;
    SetRoute(Index,WoodPositions[Best]+FVector(80,(Index%3)*120-120,0));
}

void AHearthVillage::SetHouseStage(int32 Plot,int32 Stage)
{
    if(!HouseMeshes.IsValidIndex(Plot)) return;
    FString Path=Hearth::Houses+FString::Printf(TEXT("SM_House_%02d"),FMath::Clamp(Stage+1,1,4));
    if(Stage>=3 && Plot>=0 && Plot<UE_ARRAY_COUNT(PlotOwners) && Residents.IsValidIndex(PlotOwners[Plot]))
    {
        const auto& R=Residents[PlotOwners[Plot]];
        if(!R.HouseBlueprint.IsEmpty()) Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/example__%s/%s.%s"),*R.HouseBlueprint,*R.HouseBlueprint,*R.HouseBlueprint);
    }
    if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path))
    {
        HouseMeshes[Plot]->SetStaticMesh(Mesh);
        HouseMeshes[Plot]->SetVisibility(true);
        const float Size=Stage>=3?0.9f:(Plot==2?0.80f:(Plot==0?1.05f:0.95f));
        HouseMeshes[Plot]->SetWorldScale3D(FVector(Size));
    }
}

void AHearthVillage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Small, fixed simulation steps preserve transfers and task transitions at high speeds.
    // Clamp long stalls to bound work per frame; HTTP timeouts continue using wall time.
    const float RealDt=FMath::IsFinite(DeltaSeconds)?FMath::Clamp(DeltaSeconds,0.f,0.25f):0.f;
    if(!bAcceptanceCaptureDone && AcceptanceCaptureDelay>=0.f)
    {
        AcceptanceCaptureDelay-=RealDt;
        if(AcceptanceCaptureDelay<=0.f)
        {
            bAcceptanceCaptureDone=true;
            const FString Capture=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/continuous-development/tool-action.png");
            FScreenshotRequest::RequestScreenshot(Capture,false,false);
        }
    }
    if(!bSimulationPaused && RealDt>0.f)
    {
        ConsumeDecision();
        constexpr double Step=0.05;
        SimulationRemainder+=static_cast<double>(RealDt)*SimulationSpeed;
        const int32 Steps=FMath::Min(5000,FMath::FloorToInt(SimulationRemainder/Step));
        SimulationRemainder-=Steps*Step;
        for(int32 I=0;I<Steps;++I)
        {
            AdvanceSimulation(static_cast<float>(Step));
        }
        AdvanceSocial(static_cast<float>(Steps*Step));
        UpdateLifeDecisions();
        // Disk snapshots follow real time, not the accelerated village clock.
        SnapshotTimer+=RealDt;
        if(SnapshotTimer>=0.5f) { SnapshotTimer=0; WriteSnapshot(); }
    }
    RefreshProductionVisuals();
    if(bWorldPersistenceEnabled && !bWorldWriteBlocked)
    {
        WorldSaveTimer+=RealDt;
        if(WorldSaveTimer>=30.f) { WorldSaveTimer=0; SaveWorld(); }
    }
    for(auto& R:Residents) if(IsValid(R.Actor))
    {
        const float MotionRate=!R.ConversationId.IsEmpty() && R.Task==EHearthTask::LifeActivity?1.f:SimulationSpeed;
        const EHearthTask Motion=R.Task==EHearthTask::LifeChoosing && !R.Route.IsEmpty()?EHearthTask::LifeTravel:R.Task;
        R.Actor->EquippedToolId=R.HeldToolId;
        R.Actor->SetMotion(R.bMovementBlocked?EHearthTask::LifeChoosing:Motion,bSimulationPaused?0.f:MotionRate,R.ProductionOp);
        const bool bTradeCargo=(R.Task==EHearthTask::TradeTravel || R.Task==EHearthTask::TradeWaiting)
            && TradeOffers.ContainsByPredicate([&](const FHearthTradeOffer& T) { return T.Id==R.ActiveTaskId && T.Seller==R.Actor->ResidentIndex && T.ReservedQuantity>0; });
        R.Actor->Bundle->SetVisibility(R.CarriedWood>0 || R.CargoAmount>0 || bTradeCargo);
        const TCHAR* CargoMesh=bTradeCargo?TEXT("/Game/ThreeHearths/Generated/VillageKit/carry_planks/carry_planks"):(R.CarriedWood>0 || R.CargoType==1)?TEXT("/Game/ThreeHearths/Generated/VillageKit/carry_logs/carry_logs"):
            R.CargoType==3?TEXT("/Game/ThreeHearths/Generated/VillageKit/carry_planks/carry_planks"):
            R.CargoType==4?TEXT("/Game/ThreeHearths/Generated/SocietyKit/goods_beams_bundle/goods_beams_bundle"):TEXT("/Engine/BasicShapes/Cube");
        if(auto* Asset=LoadObject<UStaticMesh>(nullptr,CargoMesh); Asset && R.Actor->Bundle->GetStaticMesh()!=Asset)
        {
            R.Actor->Bundle->SetStaticMesh(Asset);
            R.Actor->Bundle->SetRelativeScale3D(R.CargoType>=3 || bTradeCargo?FVector(.7f):FVector(.32f,.6f,.25f));
            if(R.CarriedWood>0 || R.CargoType>=1 || bTradeCargo) R.Actor->Bundle->SetMaterial(0,nullptr);
        }
        if(auto* Material=Cast<UMaterialInstanceDynamic>(R.Actor->Bundle->GetMaterial(0)))
            Material->SetVectorParameterValue(TEXT("VillageTint"),R.CargoType==0?FLinearColor(.55f,.65f,.12f):R.CargoType==2?FLinearColor(.45f,.46f,.43f):Hearth::Wood);
    }
}

void AHearthVillage::AdvanceSimulation(float Dt)
{
    Elapsed+=Dt;
    AdvanceNeeds(Dt);
    AdvanceEconomy(Dt);
    AdvanceProductionWorld(Dt);
    for(int32 I=0;I<Residents.Num();++I)
    {
        auto& R=Residents[I];
        R.bMovementBlocked=false;
        R.Timer-=Dt;
        if(R.Task!=EHearthTask::Choosing && R.Task!=EHearthTask::LifeChoosing && R.Task!=EHearthTask::Settled)
        {
            R.SocialNeed=FMath::Min(100.f,R.SocialNeed+Dt*(I==1?0.10f:0.025f));
            if(R.Task!=EHearthTask::LifeActivity) R.Energy=FMath::Max(0.f,R.Energy-Dt*0.025f);
        }
        switch(R.Task)
        {
        case EHearthTask::Choosing:
            if(R.Timer<=0) Decide(I);
            break;
        case EHearthTask::ToWood:
            if(MoveResident(I,Dt)) { R.Task=EHearthTask::Chopping; R.Timer=1.4f; }
            break;
        case EHearthTask::Chopping:
            if(R.Timer<=0)
            {
                if(R.Source<0 || WoodStock[R.Source]<=0) { SeekWood(I); break; }
                --WoodStock[R.Source]; ++R.CarriedWood; R.Timer=1.4f;
                StockMeshes[R.Source]->SetRelativeScale3D(FVector(1.1f,1.2f,FMath::Max(0.04f,WoodStock[R.Source]/12.f*0.4f)));
                StockMeshes[R.Source]->SetVisibility(WoodStock[R.Source]>0);
                const int32 Needed=CostFor(I)-R.DeliveredWood;
                if(R.CarriedWood>=FMath::Min(3,Needed))
                {
                    R.Task=EHearthTask::ToHome;
                    R.LatestEvent=FString::Printf(TEXT("带着 %d 份木材回到工地。"),R.CarriedWood);
                    SetRoute(I,PlotPositions[R.Plot]+FVector(-245,0,0));
                }
            }
            break;
        case EHearthTask::ToHome:
            if(MoveResident(I,Dt)) { R.Task=EHearthTask::Delivering; R.Timer=0.7f; }
            break;
        case EHearthTask::Delivering:
            if(R.Timer<=0)
            {
                const int32 Accepted=FMath::Min(R.CarriedWood,CostFor(I)-R.DeliveredWood);
                R.DeliveredWood+=Accepted; R.CarriedWood-=Accepted; ++R.Trips;
                R.LatestEvent=FString::Printf(TEXT("第 %d 趟送达：木材 %d / %d。"),R.Trips,R.DeliveredWood,CostFor(I));
                if(R.DeliveredWood>=CostFor(I))
                {
                    R.Task=EHearthTask::Building; R.Timer=0;
                    R.Actor->SetActorRotation(FRotator(0,0,0));
                    VillageEvent=R.Name+TEXT("备齐了木料，开始搭建自己的家。");
                }
                else SeekWood(I);
            }
            break;
        case EHearthTask::Building:
            {
                const int32 Before=FMath::FloorToInt(R.BuildProgress*3.f);
                R.BuildProgress=FMath::Min(1.f,R.BuildProgress+Dt/(8.f+CostFor(I)));
                const int32 After=FMath::Min(3,FMath::FloorToInt(R.BuildProgress*3.f));
                if(After!=Before) SetHouseStage(R.Plot,After);
                if(R.BuildProgress>=1.f)
                {
                    R.Task=EHearthTask::Settled;
                    R.LatestEvent=TEXT("小屋建好了。这就是我选的家。");
                    CompleteHistory(I,FString::Printf(TEXT("在%s建好了家，交付 %d 份木材。"),*PlotNameFor(I),R.DeliveredWood));
                    VillageEvent=R.Name+TEXT("的小屋完工了。");
                    UE_LOG(LogThreeHearths,Display,TEXT("HOUSE_COMPLETED resident=%d plot=%d wood=%d t=%.1f"),I,R.Plot,R.DeliveredWood,Elapsed);
                }
            }
            break;
        case EHearthTask::Settled:
            R.Task=EHearthTask::LifeChoosing; R.LatestEvent=TEXT("家已经建好了，准备选择接下来的生活。");
            break;
        case EHearthTask::LifeChoosing:
            if(!R.Route.IsEmpty()) MoveResident(I,Dt);
            break;
        case EHearthTask::ProductionTravel:
        case EHearthTask::ProductionWork:
        case EHearthTask::ProductionDeliver:
        case EHearthTask::ProductionDeposit:
            AdvanceProduction(I,Dt);
            break;
        case EHearthTask::LifeTravel:
        case EHearthTask::LifeActivity:
            AdvanceLife(I,Dt);
            break;
        default: break;
        }
    }
    if(!bReportedComplete && CompletedHomes()==Residents.Num())
    {
        bReportedComplete=true;
        VillageEvent=TEXT("所有人的小屋都建好了。大家开始安排接下来的生活。");
        UE_LOG(LogThreeHearths,Display,TEXT("DEMO_COMPLETE homes=%d wood_remaining=%d t=%.1f"),CompletedHomes(),AvailableWood(),Elapsed);
        WriteSnapshot();
    }
}

int32 AHearthVillage::CompletedHomes() const { int32 N=0; for(const auto& R:Residents) N+=(R.BuildProgress>=1.f); return N; }
int32 AHearthVillage::AvailableWood() const { return WoodStock[0]+WoodStock[1]+WoodStock[2]; }
int32 AHearthVillage::CostFor(int32 I) const { return Residents.IsValidIndex(I) && Residents[I].Plot>=0?PlotCosts[Residents[I].Plot]:0; }
FString AHearthVillage::PlotNameFor(int32 I) const
{
    if(!Residents.IsValidIndex(I)||Residents[I].Plot<0) return TEXT("正在选址");
    return PlotLabel(Residents[I].Plot);
}
FString AHearthVillage::StatusFor(int32 I) const
{
    if(!Residents.IsValidIndex(I)) return TEXT("");
    if(Residents[I].DecisionSource==TEXT("waiting")) return TEXT("正在思考");
    if(Residents[I].Task==EHearthTask::ProductionTravel) return TEXT("前往工作地点");
    if(Residents[I].Task==EHearthTask::ProductionWork) return TEXT("正在生产 / 施工");
    if(Residents[I].Task==EHearthTask::ProductionDeliver) return TEXT("运送产出");
    if(Residents[I].Task==EHearthTask::ProductionDeposit) return TEXT("交付入库");
    if(Residents[I].Task==EHearthTask::TradeTravel) return TEXT("携带自有木板前往交货");
    if(Residents[I].Task==EHearthTask::TradeWaiting) return TEXT("等待木板交易结算");
    if(Residents[I].Task==EHearthTask::LifeChoosing)
    {
        const int32 Wait=FMath::CeilToInt(FMath::Max(0.0,Residents[I].NextLifeDecision-Elapsed));
        return Wait>0?FString::Printf(TEXT("稍作休息 · %d 秒"),Wait):TEXT("等待下一步调度");
    }
    if(Residents[I].Task==EHearthTask::LifeTravel) return TEXT("前往活动地点");
    if(Residents[I].Task==EHearthTask::LifeActivity) return LifeActionName(I,Residents[I].LifeAction);
    const TCHAR* Status[]={TEXT("观察地块"),TEXT("前往木材堆"),TEXT("整理木材"),TEXT("搬运木材"),TEXT("放下木材"),TEXT("搭建小屋"),TEXT("新家落成")};
    return Status[static_cast<int32>(Residents[I].Task)];
}
FString AHearthVillage::GetSnapshot() const
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("world_id"),WorldId);
    Root->SetNumberField(TEXT("world_revision"),WorldRevision);
    Root->SetStringField(TEXT("world_save_status"),WorldSaveStatus);
    Root->SetStringField(TEXT("backend"),bApiReady?ApiBackend:TEXT("local_personality_policy"));
    Root->SetStringField(TEXT("api_status"),ApiStatus);
    Root->SetStringField(TEXT("model"),ApiModel);
    Root->SetNumberField(TEXT("api_requests"),ApiRequests);
    Root->SetNumberField(TEXT("api_successes"),ApiSuccesses);
    Root->SetNumberField(TEXT("api_tokens"),ApiTokens);
    Root->SetBoolField(TEXT("api_usage_available"),bHasApiUsage);
    TArray<TSharedPtr<FJsonValue>> Pending;
    int32 FirstPending=-1; bool AnyReturned=false;
    for(int32 I=0;I<PendingDecisions.Num();++I) if(PendingDecisions[I].bActive)
    {
        const auto& P=PendingDecisions[I]; auto Item=MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("resident"),I); Item->SetBoolField(TEXT("returned"),P.bReturned);
        Item->SetBoolField(TEXT("life"),P.bLife);
        Pending.Add(MakeShared<FJsonValueObject>(Item));
        if(FirstPending<0) FirstPending=I;
        AnyReturned|=P.bReturned;
    }
    Root->SetArrayField(TEXT("pending_requests"),Pending);
    Root->SetNumberField(TEXT("pending_request_count"),Pending.Num());
    Root->SetNumberField(TEXT("max_concurrent_requests"),DecisionConcurrencyLimit());
    Root->SetNumberField(TEXT("max_requests_per_resident"),1);
    Root->SetNumberField(TEXT("resident_decision_interval_seconds"),LifeDecisionInterval);
    // Keep the old single-request diagnostics usable for older local inspection scripts.
    Root->SetNumberField(TEXT("pending_resident"),FirstPending);
    Root->SetBoolField(TEXT("decision_returned"),AnyReturned);
    Root->SetNumberField(TEXT("simulation_speed"),SimulationSpeed);
    Root->SetStringField(TEXT("map_mode"),bUseCropoutMap?TEXT("cropout_base_island"):TEXT("small_platform"));
    Root->SetNumberField(TEXT("elapsed"),Elapsed);
    Root->SetNumberField(TEXT("completed_homes"),CompletedHomes());
    Root->SetNumberField(TEXT("available_wood"),AvailableWood());
    Root->SetBoolField(TEXT("paused"),bSimulationPaused);
    Root->SetStringField(TEXT("run"),CurrentRun);
    Root->SetBoolField(TEXT("autonomous_life"),bAutonomousLifeEnabled);
    Root->SetNumberField(TEXT("api_request_limit"),ApiMaxRequests);
    Root->SetBoolField(TEXT("api_budget_gateway"),bApiBudgeted);
    Root->SetStringField(TEXT("api_budget_ledger"),ApiBudgetLedger);
    Root->SetNumberField(TEXT("api_budget_settled_cny"),ApiBudgetSpent);
    Root->SetNumberField(TEXT("api_budget_reserved_cny"),ApiBudgetReserved);
    Root->SetNumberField(TEXT("api_budget_remaining_cny"),ApiBudgetRemaining);
    Root->SetStringField(TEXT("api_budget_values_scope"),TEXT("last_received_gateway_snapshot; authoritative totals are in persistent ledger"));
    Root->SetNumberField(TEXT("history_records"),DecisionHistory.Num());
    Root->SetStringField(TEXT("history_status"),HistorySaveStatus);
    TArray<TSharedPtr<FJsonValue>> People;
    int32 AccountedWood=AvailableWood()+Spent[1]-Produced[1];
    TSharedPtr<FJsonObject> Production;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GetProductionState()),Production);
    if(Production.IsValid()) Root->SetObjectField(TEXT("production"),Production);
    TSet<int32> Reserved;
    bool bUnique=true;
    for(int32 I=0;I<Residents.Num();++I)
    {
        const auto& R=Residents[I]; auto J=MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("id"),I); J->SetStringField(TEXT("name"),R.Name);
        J->SetStringField(TEXT("stable_id"),R.StableId); J->SetStringField(TEXT("task_id"),R.ActiveTaskId);
        J->SetStringField(TEXT("role"),R.Role); J->SetBoolField(TEXT("king"),R.bKing); J->SetNumberField(TEXT("age"),R.Age);
        J->SetNumberField(TEXT("hunger"),R.Hunger); J->SetNumberField(TEXT("mood"),R.Mood);
        J->SetStringField(TEXT("conversation_id"),R.ConversationId); J->SetStringField(TEXT("speech"),R.Speech);
        J->SetNumberField(TEXT("speech_remaining"),R.SpeechRemaining);
        J->SetStringField(TEXT("reason"),R.Reason); J->SetStringField(TEXT("status"),StatusFor(I));
        J->SetStringField(TEXT("decision_source"),R.DecisionSource); J->SetStringField(TEXT("decision_note"),R.DecisionNote);
        J->SetStringField(TEXT("house_blueprint"),R.HouseBlueprint); J->SetStringField(TEXT("wall_material"),R.WallMaterial); J->SetStringField(TEXT("roof_material"),R.RoofMaterial);
        J->SetStringField(TEXT("equipped_tool"),IsValid(R.Actor)?R.Actor->EquippedToolId:FString());
        J->SetStringField(TEXT("held_tool"),R.HeldToolId); J->SetStringField(TEXT("held_tool_operation"),R.HeldToolOperationId);
        J->SetNumberField(TEXT("task"),static_cast<int32>(R.Task)); J->SetNumberField(TEXT("plot"),R.Plot);
        J->SetNumberField(TEXT("carried"),R.CarriedWood); J->SetNumberField(TEXT("delivered"),R.DeliveredWood);
        J->SetNumberField(TEXT("cost"),CostFor(I)); J->SetNumberField(TEXT("build_progress"),R.BuildProgress);
        J->SetNumberField(TEXT("energy"),R.Energy); J->SetNumberField(TEXT("social_need"),R.SocialNeed);
        J->SetNumberField(TEXT("coins"),R.Coins);
        J->SetNumberField(TEXT("personal_planks"),R.PersonalPlanks);
        J->SetNumberField(TEXT("life_action"),R.LifeAction); J->SetNumberField(TEXT("history_count"),HistoryCount(I));
        if(IsValid(R.Actor)) J->SetStringField(TEXT("position"),R.Actor->GetActorLocation().ToString());
        People.Add(MakeShared<FJsonValueObject>(J));
        AccountedWood+=R.CarriedWood+R.DeliveredWood+(R.CargoType==1?R.CargoAmount:0);
        J->SetNumberField(TEXT("production_site"),R.ProductionSite); J->SetNumberField(TEXT("production_op"),R.ProductionOp);
        J->SetNumberField(TEXT("cargo_type"),R.CargoType); J->SetNumberField(TEXT("cargo_amount"),R.CargoAmount);
        TArray<TSharedPtr<FJsonValue>> Route;
        for(const auto& P:R.Route) Route.Add(MakeShared<FJsonValueString>(P.ToString()));
        J->SetArrayField(TEXT("route"),Route);
        J->SetBoolField(TEXT("waiting_for_space"),R.bMovementBlocked);
        if(R.Plot>=0) { if(Reserved.Contains(R.Plot)) bUnique=false; Reserved.Add(R.Plot); }
    }
    Root->SetArrayField(TEXT("residents"),People);
    TArray<TSharedPtr<FJsonValue>> Tools;
    const TCHAR* ToolIds[]={TEXT("tool_hammer"),TEXT("tool_mallet"),TEXT("tool_axe"),TEXT("tool_saw"),TEXT("tool_pickaxe"),TEXT("tool_shovel"),TEXT("tool_hoe"),TEXT("tool_trowel")};
    for(const TCHAR* ToolId:ToolIds)
    {
        int32 Holder=-1; FString Operation;
        for(int32 I=0;I<Residents.Num();++I) if(Residents[I].HeldToolId==ToolId) { Holder=I; Operation=Residents[I].HeldToolOperationId; break; }
        auto Tool=MakeShared<FJsonObject>(); Tool->SetStringField(TEXT("id"),ToolId); Tool->SetNumberField(TEXT("holder"),Holder);
        Tool->SetStringField(TEXT("operation_id"),Operation); Tool->SetStringField(TEXT("location"),Holder<0?TEXT("shared_tool_store"):TEXT("resident"));
        Tools.Add(MakeShared<FJsonValueObject>(Tool));
    }
    Root->SetArrayField(TEXT("tool_inventory"),Tools);
    Root->SetNumberField(TEXT("treasury_coins"),TreasuryCoins);
    TArray<TSharedPtr<FJsonValue>> Economy;
    for(const auto& T:Transactions)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),T.Id); J->SetStringField(TEXT("kind"),T.Kind);
        J->SetStringField(TEXT("task_id"),T.TaskId); J->SetNumberField(TEXT("from"),T.From); J->SetNumberField(TEXT("to"),T.To);
        J->SetNumberField(TEXT("amount"),T.Amount); J->SetStringField(TEXT("item"),T.Item); J->SetNumberField(TEXT("quantity"),T.Quantity);
        J->SetNumberField(TEXT("at"),T.At); Economy.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("transactions"),Economy);
    TArray<TSharedPtr<FJsonValue>> Payables;
    for(const auto& P:WagePayables)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),P.Id); J->SetStringField(TEXT("task_id"),P.TaskId);
        J->SetStringField(TEXT("status"),P.Status); J->SetNumberField(TEXT("worker"),P.Worker); J->SetNumberField(TEXT("amount"),P.Amount);
        Payables.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("wage_payables"),Payables);
    TArray<TSharedPtr<FJsonValue>> Trades;
    for(const auto& T:TradeOffers)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),T.Id); J->SetStringField(TEXT("conversation_id"),T.ConversationId); J->SetStringField(TEXT("status"),T.Status); J->SetStringField(TEXT("result"),T.Result);
        J->SetNumberField(TEXT("seller"),T.Seller); J->SetNumberField(TEXT("buyer"),T.Buyer); J->SetNumberField(TEXT("quantity"),T.Quantity);
        J->SetNumberField(TEXT("price"),T.Price); J->SetNumberField(TEXT("reserved_quantity"),T.ReservedQuantity); J->SetNumberField(TEXT("remaining"),T.Remaining);
        Trades.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("trade_offers"),Trades);
    Root->SetNumberField(TEXT("accounted_wood"),AccountedWood);
    Root->SetBoolField(TEXT("unique_plot_owners"),bUnique);
    FString Out; auto Writer=TJsonWriterFactory<>::Create(&Out); FJsonSerializer::Serialize(Root,Writer); return Out;
}
void AHearthVillage::WriteSnapshot() const
{
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("ThreeHearths");
    IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(GetSnapshot(),*(Dir/TEXT("demo-state.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void AHearthVillage::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bWorldPersistenceEnabled) SaveWorld();
    else CloseHistoryRun(TEXT("本次播放已结束，任务在此中断。"));
    StopDecisionRequests();
    WorldLease.Reset();
    WriteSnapshot();
    Super::EndPlay(Reason);
}
