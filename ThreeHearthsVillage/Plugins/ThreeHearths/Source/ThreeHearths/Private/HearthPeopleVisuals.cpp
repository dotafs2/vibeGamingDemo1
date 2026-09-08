#include "HearthVillage.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

bool AHearthVillager::ConfigureServiceAppearance(const FString& RoleKey)
{
    if(RoleKey!=TEXT("gatekeeper") && RoleKey!=TEXT("royal_guard") && RoleKey!=TEXT("carter")) return false;
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const FString Path=FPaths::ProjectContentDir()/TEXT("ThreeHearths/Data/MedievalLifePeopleRigCatalog.json");
    if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* People=nullptr;
    if(!Root->TryGetArrayField(TEXT("people"),People)) return false;
    TSharedPtr<FJsonObject> Person;
    for(const auto& Value:*People)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Object) continue;
        const auto Candidate=Value->AsObject(); FString Id;
        if(Candidate->TryGetStringField(TEXT("id"),Id) && Id==RoleKey) { Person=Candidate; break; }
    }
    if(!Person.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* MeshLayers=nullptr;
    const TSharedPtr<FJsonObject>* Animations=nullptr;
    if(!Person->TryGetArrayField(TEXT("meshes"),MeshLayers) || !Person->TryGetObjectField(TEXT("animations"),Animations)) return false;
    auto LoadClip=[&](const TCHAR* Name)->UAnimSequence*
    {
        const TSharedPtr<FJsonObject>* Clip=nullptr; FString AssetPath;
        if(!(*Animations)->TryGetObjectField(Name,Clip) || !(*Clip)->TryGetStringField(TEXT("asset"),AssetPath)) return nullptr;
        return LoadObject<UAnimSequence>(nullptr,*AssetPath);
    };
    UAnimSequence* NewIdle=LoadClip(TEXT("Idle")); UAnimSequence* NewWalk=LoadClip(TEXT("Walk"));
    TArray<USkeletalMesh*> Meshes; USkeletalMesh* Main=nullptr;
    for(const auto& Value:*MeshLayers)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
        FString AssetPath,Layer; const auto Entry=Value->AsObject();
        if(!Entry->TryGetStringField(TEXT("mesh"),AssetPath) || !Entry->TryGetStringField(TEXT("layer"),Layer)) return false;
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*AssetPath); if(!Mesh) return false;
        Meshes.Add(Mesh); if(Layer==TEXT("structure")) Main=Mesh;
    }
    // Validate the whole layered character before changing its visible body.
    const int32 Expected=RoleKey==TEXT("gatekeeper")?7:RoleKey==TEXT("royal_guard")?9:4;
    if(!Main || !NewIdle || !NewWalk || Meshes.Num()!=Expected || NewIdle->GetSkeleton()!=Main->GetSkeleton() || NewWalk->GetSkeleton()!=Main->GetSkeleton()) return false;
    for(const auto* Mesh:Meshes) if(Mesh->GetSkeleton()!=Main->GetSkeleton()) return false;

    for(const auto& Part:AppearanceParts) if(IsValid(Part)) Part->DestroyComponent();
    AppearanceParts.Reset();
    for(const auto& Part:ServiceAppearanceParts) if(IsValid(Part)) Part->DestroyComponent();
    ServiceAppearanceParts.Reset();
    Body->SetSkeletalMesh(Main); Body->EmptyOverrideMaterials();
    Body->SetRelativeLocation(FVector::ZeroVector); Body->SetRelativeRotation(FRotator(0,-90,0)); Body->SetRelativeScale3D(FVector::OneVector);
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Hat->SetLeaderPoseComponent(nullptr); Hat->SetSkeletalMesh(nullptr); Hat->SetVisibility(false);
    Tool->SetVisibility(false);
    for(auto* Mesh:Meshes)
    {
        if(Mesh==Main) continue;
        auto* Part=NewObject<USkeletalMeshComponent>(this);
        Part->SetupAttachment(Body); Part->SetRelativeTransform(FTransform::Identity);
        Part->SetSkeletalMesh(Mesh); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false); Part->SetLeaderPoseComponent(Body,true);
        Part->ComponentTags.Add(TEXT("HearthServiceLayer"));
        AddInstanceComponent(Part); Part->RegisterComponent(); ServiceAppearanceParts.Add(Part);
    }
    Idle=NewIdle; Walk=NewWalk;
    // Unsupported work is forbidden for these residents; all fallback clips
    // must still share their real skeleton if an old state requests one.
    Chop=Build=Farm=Mine=Gather=NewIdle;
    LastMotion=EHearthTask::Settled; LastWorkKind=-99;
    Body->PlayAnimation(Idle,true); bServiceAppearanceReady=true;
    UE_LOG(LogTemp,Display,TEXT("MEDIEVAL_PERSON_READY resident=%d role=%s layers=%d bones=%d"),ResidentIndex,*RoleKey,Meshes.Num(),Main->GetRefSkeleton().GetNum());
    return true;
}
