#include "HearthHorseCart.h"
#include "HearthVillage.h"

#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool ReadObject(const TCHAR* Name,TSharedPtr<FJsonObject>& Out)
    {
        FString Text;
        return FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("ThreeHearths/Data")/Name))
            && Text.Len()<4*1024*1024 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out) && Out.IsValid();
    }
    bool ReadStaticMeshes(const TCHAR* Name,TMap<FString,TArray<UStaticMesh*>>& Out)
    {
        TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Assets=nullptr;
        if(!ReadObject(Name,Root) || !Root->TryGetArrayField(TEXT("assets"),Assets)) return false;
        for(const auto& Value:*Assets)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
            FString Id,Path;
            if(!Value->AsObject()->TryGetStringField(TEXT("id"),Id) || !Value->AsObject()->TryGetStringField(TEXT("mesh"),Path)) return false;
            const FString Module=Id.Left(Id.Find(TEXT("/")));
            if(Module!=TEXT("cart_body") && Module!=TEXT("cart_wheel") && Module!=TEXT("cart_traces")
                && Module!=TEXT("cargo_planks") && Module!=TEXT("cargo_beams") && Module!=TEXT("freight_depot_rack")) continue;
            UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);if(!Mesh) return false;
            Out.FindOrAdd(Module).Add(Mesh);
        }
        return true;
    }
}

AHearthHorseCart::AHearthHorseCart()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("CartFrame"));
    Horse=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HorseBody"));
    Horse->SetupAttachment(RootComponent);
    Horse->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Horse->SetCanEverAffectNavigation(false);
    Horse->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Tags.Add(TEXT("HearthLiveFreight"));
    SetActorHiddenInGame(true);
}

bool AHearthHorseCart::Configure(AHearthVillage* Village)
{
    if(!IsValid(Village)) return false;
    VillageOwner=Village;SetOwner(Village);AddTickPrerequisiteActor(Village);
    // Load the complete catalog before changing an already working actor.
    TSharedPtr<FJsonObject> Catalog,Coat;
    const TArray<TSharedPtr<FJsonValue>>* Horses=nullptr;
    if(!ReadObject(TEXT("MedievalLifeHorseRigCatalog.json"),Catalog) || !Catalog->TryGetArrayField(TEXT("horses"),Horses)) return false;
    for(const auto& Value:*Horses)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Object) continue;
        FString Id;if(Value->AsObject()->TryGetStringField(TEXT("id"),Id) && Id==TEXT("horse_bay")) { Coat=Value->AsObject();break; }
    }
    const TArray<TSharedPtr<FJsonValue>>* Meshes=nullptr;
    const TSharedPtr<FJsonObject>* Clips=nullptr;
    if(!Coat.IsValid() || !Coat->TryGetArrayField(TEXT("meshes"),Meshes) || !Coat->TryGetObjectField(TEXT("animations"),Clips)) return false;
    auto Clip=[&](const TCHAR* Name)->UAnimSequence*
    {
        const TSharedPtr<FJsonObject>* Entry=nullptr;FString Path;
        return (*Clips)->TryGetObjectField(Name,Entry) && (*Entry)->TryGetStringField(TEXT("asset"),Path)?LoadObject<UAnimSequence>(nullptr,*Path):nullptr;
    };
    UAnimSequence* NewIdle=Clip(TEXT("Idle"));UAnimSequence* NewWalk=Clip(TEXT("Walk"));
    USkeletalMesh* Main=nullptr;TArray<USkeletalMesh*> NewMeshes;
    for(const auto& Value:*Meshes)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
        FString Path,Layer;
        if(!Value->AsObject()->TryGetStringField(TEXT("mesh"),Path) || !Value->AsObject()->TryGetStringField(TEXT("layer"),Layer)) return false;
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*Path);if(!Mesh) return false;
        NewMeshes.Add(Mesh);if(Layer==TEXT("structure")) Main=Mesh;
    }
    if(!Main || NewMeshes.Num()!=3 || !NewIdle || !NewWalk || NewIdle->GetSkeleton()!=Main->GetSkeleton() || NewWalk->GetSkeleton()!=Main->GetSkeleton()) return false;
    for(auto* Mesh:NewMeshes) if(Mesh->GetSkeleton()!=Main->GetSkeleton()) return false;
    TMap<FString,TArray<UStaticMesh*>> ModuleMeshes;
    if(!ReadStaticMeshes(TEXT("MedievalLifeCatalog.json"),ModuleMeshes)
        || !ReadStaticMeshes(TEXT("MedievalFreightKitCatalog.json"),ModuleMeshes)) return false;
    for(const TCHAR* Id:{TEXT("cart_body"),TEXT("cart_wheel"),TEXT("cart_traces"),TEXT("cargo_planks"),TEXT("cargo_beams")})
        if(!ModuleMeshes.Contains(Id) || ModuleMeshes[Id].Num()<2) return false;

    for(auto Part:StaticParts) if(IsValid(Part)) Part->DestroyComponent();
    for(auto Part:HorseLayers) if(IsValid(Part)) Part->DestroyComponent();
    StaticParts.Reset();WheelLayers.Reset();PlankLayers.Reset();BeamLayers.Reset();HorseLayers.Reset();WheelBases.Reset();
    Horse->SetSkeletalMesh(Main);Horse->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Idle=NewIdle;Walk=NewWalk;Horse->SetAnimation(Idle);bWasWalking=false;
    for(auto* Mesh:NewMeshes)
    {
        if(Mesh==Main) continue;
        auto* Part=NewObject<USkeletalMeshComponent>(this);Part->SetupAttachment(Horse);
        Part->SetSkeletalMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);
        Part->SetLeaderPoseComponent(Horse,true);AddInstanceComponent(Part);Part->RegisterComponent();HorseLayers.Add(Part);
    }
    auto AddModule=[&](const TCHAR* Id,const FVector& Position,int32 CargoIndex,TArray<TObjectPtr<UStaticMeshComponent>>* Out)
    {
        for(auto* Mesh:ModuleMeshes[Id])
        {
            auto* Part=NewObject<UStaticMeshComponent>(this);Part->SetupAttachment(RootComponent);
            Part->SetStaticMesh(Mesh);Part->SetMobility(EComponentMobility::Movable);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCanEverAffectNavigation(false);
            Part->SetRelativeTransform(FTransform(FRotator(0,-90,0),Position));
            Part->ComponentTags.Add(FName(Id));
            if(CargoIndex>=0) Part->ComponentTags.Add(FName(*FString::Printf(TEXT("CargoUnit=%d"),CargoIndex)));
            AddInstanceComponent(Part);Part->RegisterComponent();StaticParts.Add(Part);if(Out) Out->Add(Part);
        }
    };
    AddModule(TEXT("cart_body"),FVector::ZeroVector,-1,nullptr);
    AddModule(TEXT("cart_traces"),FVector::ZeroVector,-1,nullptr);
    for(float Side:{-1.f,1.f}) AddModule(TEXT("cart_wheel"),FVector(0,104.f*Side,63.8f),-1,&WheelLayers);
    for(auto Part:WheelLayers) WheelBases.Add(Part->GetRelativeTransform());
    for(int32 I=0;I<6;++I)
    {
        // One instance equals one inventory unit; cargo never stretches or
        // spawns as a decorative, permanently full load.
        const float Lateral=(I%3-1)*24.f;
        AddModule(TEXT("cargo_planks"),FVector(-8.f,Lateral,88.f+(I/3)*7.6f),I,&PlankLayers);
        AddModule(TEXT("cargo_beams"),FVector(-8.f,Lateral,88.f+(I/3)*19.6f),I,&BeamLayers);
    }
    bReady=true;
    UE_LOG(LogTemp,Display,TEXT("MEDIEVAL_HORSE_CART_READY horse_layers=%d static_layers=%d wheel_layers=%d"),HorseLayers.Num()+1,StaticParts.Num(),WheelLayers.Num());
    ApplyState(Village->GetFreightVisualState());return true;
}

void AHearthHorseCart::ApplyState(const FHearthFreightVisualState& State)
{
    SetActorHiddenInGame(!bReady || !State.bEnabled);
    auto* Village=VillageOwner.Get();if(!bReady || !State.bEnabled || !Village) return;
    if(State.CartPosition.ContainsNaN() || !FMath::IsFinite(State.CartYaw) || !FMath::IsFinite(State.WheelDistanceCm)) { SetActorHiddenInGame(true);return; }
    const FRotator Flat(0,State.CartYaw,0);const FVector Forward=Flat.Vector(),Right=Flat.RotateVector(FVector::RightVector);
    FVector Position=State.CartPosition;Position.Z=Village->GroundHeightAt(Position)+.8f;
    const float Ahead=Village->GroundHeightAt(Position+Forward*100.f),Behind=Village->GroundHeightAt(Position-Forward*100.f);
    const float Left=Village->GroundHeightAt(Position-Right*104.f),RightHeight=Village->GroundHeightAt(Position+Right*104.f);
    const FVector SurfaceForward=FVector(Forward.X,Forward.Y,(Ahead-Behind)/200.f).GetSafeNormal();
    const FVector SurfaceRight=FVector(Right.X,Right.Y,(RightHeight-Left)/208.f).GetSafeNormal();
    const FQuat Frame=FRotationMatrix::MakeFromXY(SurfaceForward,SurfaceRight).ToQuat();
    SetActorLocationAndRotation(Position,Frame);
    for(int32 I=0;I<WheelLayers.Num();++I)
    {
        FTransform Pose=WheelBases[I];
        const double Angle=FMath::Fmod(State.WheelDistanceCm/63.8,2.0*PI);
        Pose.SetRotation(WheelBases[I].GetRotation()*FQuat(FVector::ForwardVector,-Angle));
        WheelLayers[I]->SetRelativeTransform(Pose);
    }
    FVector HorsePosition=Position+Forward*290.f;
    HorsePosition.Z=Village->GroundHeightAt(HorsePosition);
    Horse->SetWorldLocationAndRotation(HorsePosition,FRotator(0,State.CartYaw-90.f,0));
    if(bWasWalking!=State.bMoving) { Horse->SetAnimation(State.bMoving?Walk:Idle);bWasWalking=State.bMoving; }
    // Walk phase follows actual travelled distance, so speed-up, pause,
    // blocked routes and rendering stalls cannot advance the legs alone.
    const double ClipTime=State.bMoving?FMath::Fmod(State.WheelDistanceCm/60.0,static_cast<double>(Walk->GetPlayLength())):FMath::Fmod(State.ElapsedSeconds,static_cast<double>(Idle->GetPlayLength()));
    Horse->SetPosition(static_cast<float>(FMath::Max(0.0,ClipTime)),false);
    Horse->TickAnimation(0.f,false);Horse->RefreshBoneTransforms();
    auto ShowCargo=[&](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts,int32 Kind)
    {
        for(auto Part:Parts)
        {
            int32 Index=-1;
            for(const FName Tag:Part->ComponentTags) { const FString Value=Tag.ToString();if(Value.StartsWith(TEXT("CargoUnit="))) Index=FCString::Atoi(*Value.Mid(10)); }
            Part->SetVisibility(State.CargoType==Kind && Index>=0 && Index<FMath::Clamp(State.CargoQuantity,0,6));
        }
    };
    ShowCargo(PlankLayers,3);ShowCargo(BeamLayers,4);
    if(!bCargoAuditCaptured && State.CargoQuantity>0 && FParse::Param(FCommandLine::Get(),TEXT("HearthAuditFreightCargo")))
    {
        bCargoAuditCaptured=true;
        for(const auto Part:BeamLayers)
            UE_LOG(LogTemp,Display,TEXT("FREIGHT_CARGO_AUDIT quantity=%d kind=%d visible=%d hidden=%d mesh=%s relative=%s world=%s bounds=%s tags=%s"),
                State.CargoQuantity,State.CargoType,Part->IsVisible(),Part->bHiddenInGame,*Part->GetStaticMesh()->GetPathName(),
                *Part->GetRelativeLocation().ToString(),*Part->GetComponentLocation().ToString(),*Part->Bounds.Origin.ToString(),
                Part->ComponentTags.Num()>1?*Part->ComponentTags[1].ToString():TEXT("none"));
    }
    if(!bReviewCaptured && State.bMoving && State.CargoQuantity>0 && State.WheelDistanceCm>500
        && FParse::Param(FCommandLine::Get(),TEXT("HearthCaptureFreight")))
    {
        bReviewCaptured=true;
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/MedievalReview/FreightCapture");
        IFileManager::Get().MakeDirectory(*Directory,true);
        FScreenshotRequest::RequestScreenshot(Directory/TEXT("freight-cart.png"),false,false);
        const FString Record=FString::Printf(TEXT("{\"kind\":\"actual_game_viewport_request\",\"world_id\":\"%s\",\"elapsed\":%.3f,\"cargo_type\":%d,\"cargo_quantity\":%d,\"distance_cm\":%.3f,\"cart_position\":[%.3f,%.3f,%.3f],\"yaw\":%.3f}"),
            *Village->WorldId,State.ElapsedSeconds,State.CargoType,State.CargoQuantity,State.WheelDistanceCm,Position.X,Position.Y,Position.Z,State.CartYaw);
        FFileHelper::SaveStringToFile(Record,*(Directory/TEXT("capture-state.json")));
        UE_LOG(LogTemp,Display,TEXT("FREIGHT_VIEWPORT_CAPTURE world=%s quantity=%d distance=%.1f"),*Village->WorldId,State.CargoQuantity,State.WheelDistanceCm);
    }
}

void AHearthHorseCart::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(auto* Village=VillageOwner.Get()) ApplyState(Village->GetFreightVisualState());
    else Destroy();
}
