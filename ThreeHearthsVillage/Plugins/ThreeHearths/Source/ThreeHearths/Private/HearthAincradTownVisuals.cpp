#include "HearthAincradTownVisuals.h"
#include "HearthAincradTownLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace HearthAincradTownVisuals
{
    int32 Build(AActor* Owner,TArray<TObjectPtr<UActorComponent>>& Generated)
    {
        if(!Owner)return 0;
        const auto Plan=HearthAincradTownLayout::Build();
        const FString Kit=FPaths::ProjectDir()/TEXT("Art/AincradLevel0");
        const FString Native=TEXT("/Game/ThreeHearths/Generated/AincradTownKit/");
        TMap<FString,UInstancedStaticMeshComponent*> Batches;
        TMap<FString,TSharedPtr<FJsonObject>> Recipes;
        const auto Instance=[&](const FString& Name,const FTransform& Transform,bool Collision)
        {
            const FString Key=Name+(Collision?TEXT("_solid"):TEXT("_decor"));
            UInstancedStaticMeshComponent* Batch=nullptr;
            if(auto Found=Batches.Find(Key))Batch=*Found;
            else
            {
                auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Native+Name+TEXT("/")+Name));
                if(!Mesh){UE_LOG(LogTemp,Warning,TEXT("TOWN_ART_MISSING %s"),*Name);return;}
                Batch=NewObject<UInstancedStaticMeshComponent>(Owner);Batch->SetupAttachment(Owner->GetRootComponent());
                Batch->SetStaticMesh(Mesh);Batch->SetMobility(EComponentMobility::Static);
                if(Name==TEXT("lantern_hanging"))Batch->SetCastShadow(false);
                Batch->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
                Batch->SetCollisionResponseToAllChannels(ECR_Block);Batch->SetCanEverAffectNavigation(false);
                Batch->RegisterComponent();Generated.Add(Batch);Batches.Add(Key,Batch);
            }
            Batch->AddInstance(Transform,true);
        };
        const auto SolidBox=[&](FVector Center,FVector Size,float Yaw)
        {
            auto* Box=NewObject<UBoxComponent>(Owner);Box->SetupAttachment(Owner->GetRootComponent());
            Box->SetBoxExtent(Size*.5);Box->SetCollisionProfileName(TEXT("BlockAll"));Box->SetCanEverAffectNavigation(false);
            Box->RegisterComponent();Box->SetWorldLocationAndRotation(Center,FRotator(0,Yaw,0));Generated.Add(Box);
        };
        for(const auto& B:Plan.Buildings)
        {
            const FString Base=B.Role==TEXT("innkeeper")?TEXT("SM_Inn"):B.Role==TEXT("blacksmith")?TEXT("SM_Smithy"):B.Role==TEXT("carpenter")?TEXT("SM_Carpentry"):B.Floors==3?TEXT("SM_TownHouse3"):TEXT("SM_TownHouse");
            const FVector2D Source=B.Role==TEXT("innkeeper")?FVector2D(1200,1600):B.Role==TEXT("residential")?FVector2D(800,1000):FVector2D(1000,1400);
            const FVector Scale(B.FootprintCm.X/Source.X,B.FootprintCm.Y/Source.Y,1);
            const FTransform Frame(FRotator(0,B.YawDegrees,0),B.CenterCm,Scale);
            TSharedPtr<FJsonObject> Recipe;
            if(auto* Found=Recipes.Find(Base))Recipe=*Found;
            else
            {
                FString Text;
                if(FFileHelper::LoadFileToString(Text,*(Kit/TEXT("Recipes")/(Base+TEXT(".json")))))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Recipe);
                Recipes.Add(Base,Recipe);
            }
            if(!Recipe.IsValid()){UE_LOG(LogTemp,Error,TEXT("TOWN_RECIPE_MISSING %s"),*Base);continue;}
            // Each wall/window/floor remains an independent module instance.
            // Only authored trim, roof and furniture are batched as details.
            for(const auto& Value:Recipe->GetArrayField(TEXT("parts")))
            {
                const auto Part=Value->AsObject();const auto P=Part->GetArrayField(TEXT("position_m"));
                if(P.Num()!=3)continue;
                const FVector Local(P[0]->AsNumber()*100,-P[1]->AsNumber()*100,P[2]->AsNumber()*100);
                const FString Module=Part->GetStringField(TEXT("module"));
                if(Module.Contains(TEXT("/")) || Module.Contains(TEXT("..")))continue;
                const FTransform LocalFrame(FRotator(0,-Part->GetNumberField(TEXT("yaw_degrees")),0),Local);
                // Wall collision uses simple portal-aware boxes below, giving
                // deterministic capsule sweeps even before cooked triangle data.
                Instance(Module,LocalFrame*Frame,false);
            }
            Instance(Base+TEXT("_Details"),Frame,false);
            const float W=B.FootprintCm.X,D=B.FootprintCm.Y,H=B.Floors*320.f;
            const FTransform Rigid(FRotator(0,B.YawDegrees,0),B.CenterCm);
            const auto WallBox=[&](FVector P,FVector Size){SolidBox(Rigid.TransformPosition(P),Size,B.YawDegrees);};
            WallBox(FVector(-W/2,0,H/2),FVector(22,D,H));WallBox(FVector(W/2,0,H/2),FVector(22,D,H));
            WallBox(FVector(0,D/2,H/2),FVector(W,22,H));
            const float DoorHalf=80.f*(B.FootprintCm.X/Source.X);
            for(int32 Side:{-1,1})WallBox(FVector(Side*(W*.25f+DoorHalf*.5f),-D/2,H/2),FVector(W/2-DoorHalf,22,H));
            WallBox(FVector(0,-D/2,(H+240)*.5f),FVector(DoorHalf*2,22,H-240));
            // Broad flat floor ends at z=0; actor feet retain a small clearance.
            WallBox(FVector(0,0,-15),FVector(W,D,30));
            if(B.Role!=TEXT("residential"))for(int32 Side:{-1,1})
            {
                // Light positions match the authored hanging lantern modules.
                auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Owner->GetRootComponent());
                Lamp->SetMobility(EComponentMobility::Movable);Lamp->SetIntensityUnits(ELightUnits::Candelas);
                Lamp->SetIntensity(35.f);Lamp->SetLightColor(FLinearColor(1.f,.91f,.78f));Lamp->SetAttenuationRadius(1400);
                Lamp->SetSourceRadius(2);Lamp->SetCastShadows(true);Lamp->RegisterComponent();
                Lamp->SetWorldLocation(Rigid.TransformPosition(FVector(Side*W*.25,0,262)));Generated.Add(Lamp);
            }
            UE_LOG(LogTemp,Display,TEXT("TOWN_BUILDING %s role=%s center=%s door=%s modules=%d"),*B.Id,*B.Role,*B.CenterCm.ToString(),*B.EntranceCm.ToString(),Recipe->GetArrayField(TEXT("parts")).Num());
        }
        // Frontage vegetation belongs to maintained planting strips, with gaps
        // at every doorway and at the observation/arrival lanes.
        for(int32 Side:{-1,1})for(int32 N=0;N<7;++N)
        {
            const FVector P(Side*850.f,469850.f-N*2200.f,0);
            bool Clear=true;
            for(const auto& B:Plan.Buildings)if(FMath::Abs(B.CenterCm.Y-P.Y)<850)Clear=false;
            if(!Clear)continue;
            Instance(N%3?TEXT("SM_StreetTree"):TEXT("SM_CourtyardTree"),FTransform(FRotator(0,N*57.f,0),P,FVector(.85f)),false);
            for(int32 S:{-1,1})Instance(TEXT("SM_Shrub"),FTransform(FRotator(0,90,0),P+FVector(0,S*260,0),FVector(.7f)),false);
        }
        for(const auto& B:Plan.Buildings)
        {
            const FTransform Frame(FRotator(0,B.YawDegrees,0),B.CenterCm);
            for(int32 S:{-1,1})Instance(TEXT("SM_FlowerBed"),FTransform(FRotator(0,B.YawDegrees,0),Frame.TransformPosition(FVector(S*B.FootprintCm.X*.33,-B.FootprintCm.Y*.5-60,0))),false);
        }
        UE_LOG(LogTemp,Display,TEXT("TOWN_MODULAR_PREVIEW buildings=%d mesh_batches=%d"),Plan.Buildings.Num(),Batches.Num());
        return Plan.Buildings.Num();
    }
}
