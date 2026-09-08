#include "HearthAincradStyle.h"
#include "HearthVillage.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "RenderingThread.h"

namespace
{
    void Exposure(FPostProcessSettings& P)
    {
        P.bOverride_AutoExposureMethod=true;P.AutoExposureMethod=AEM_Manual;
        P.bOverride_AutoExposureApplyPhysicalCameraExposure=true;P.AutoExposureApplyPhysicalCameraExposure=false;
        P.bOverride_AutoExposureBias=true;P.AutoExposureBias=.8f;
        P.bOverride_BloomIntensity=true;P.BloomIntensity=.12f;
        P.bOverride_VignetteIntensity=true;P.VignetteIntensity=.10f;
        P.bOverride_MotionBlurAmount=true;P.MotionBlurAmount=0.f;
        P.bOverride_ColorSaturation=true;P.ColorSaturation=FVector4(1,1,1,.94f);
        P.bOverride_AmbientOcclusionIntensity=true;P.AmbientOcclusionIntensity=.65f;
    }
    FString SurfaceFor(const FString& Name)
    {
        const FString N=Name.ToLower();
        if(N.Contains(TEXT("glass")) || N.Contains(TEXT("flower")) || N.Contains(TEXT("accent"))
            || N.Contains(TEXT("moss")) || N.Contains(TEXT("soil")) || N.Contains(TEXT("iron"))) return {};
        if(N.Contains(TEXT("plaster")) || N.Contains(TEXT("gable_cream")))
            return N.Contains(TEXT("sage_clay"))?TEXT("PlasterSage"):N.Contains(TEXT("ochre_slate"))?TEXT("PlasterOchre"):TEXT("PlasterWarm");
        if(N.Contains(TEXT("wood_dark")) || N.Contains(TEXT("timber_recess"))) return TEXT("TimberDark");
        if(N.Contains(TEXT("wood")) || N.Contains(TEXT("timber"))) return TEXT("Timber");
        if(N.Contains(TEXT("stone")) || N.Contains(TEXT("mortar"))) return TEXT("Stone");
        if(N.Contains(TEXT("slate"))) return TEXT("Slate");
        if(N.Contains(TEXT("terracotta")) || N.Contains(TEXT("clay_roof")) || N.Contains(TEXT("roof_tile"))) return TEXT("Terracotta");
        if(N.Contains(TEXT("leaf")) || N.Contains(TEXT("foliage"))) return TEXT("Leaf");
        return {};
    }
}

void HearthAincradStyle::ApplyToMesh(UStaticMeshComponent* Mesh)
{
    if(!IsValid(Mesh) || !Mesh->GetStaticMesh()) return;
    const FString Path=Mesh->GetStaticMesh()->GetPathName();
    // Preserve source UVs, material slots and cutout/character/unknown materials.
    if(!Path.StartsWith(TEXT("/Game/ThreeHearths/Generated/OrganicVillageMasters/"))
        && !Path.StartsWith(TEXT("/Game/ThreeHearths/Generated/MedievalLife/"))
        && !Path.StartsWith(TEXT("/Game/ThreeHearths/Generated/PublicWallKit/"))) return;
    const auto& Slots=Mesh->GetStaticMesh()->GetStaticMaterials();
    for(int32 I=0;I<Slots.Num();++I)
    {
        const FString Id=SurfaceFor(Slots[I].MaterialSlotName.ToString());
        if(Id.IsEmpty()) continue;
        if(auto* M=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/ThreeHearths/Materials/AincradStyle/MI_")+Id))) Mesh->SetMaterial(I,M);
    }
}

void HearthAincradStyle::ConfigureWorld(AHearthVillage& V,AActor& TerrainOwner)
{
    if(!V.IsOrganicVillage() || !V.GetWorld()) return;
    int32 Hidden=0,Suns=0,Skies=0;
    for(TActorIterator<AActor> It(V.GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(auto* Mesh:Meshes)
        {
            if(!Mesh->GetStaticMesh()) continue;
            const FString Path=Mesh->GetStaticMesh()->GetPathName();
            if(Path==TEXT("/Engine/BasicShapes/Plane.Plane") && Mesh->GetMaterial(0)
                && Mesh->GetMaterial(0)->GetName()==TEXT("M_Landscape"))
                if(auto* Grass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ThreeHearths/Materials/AincradStyle/MI_Grass"))) Mesh->SetMaterial(0,Grass);
            // Old water/fog were authored for the small flat Cropout island.
            if(Path.Contains(TEXT("/SM_WaterPlane.")) || Path.Contains(TEXT("/SM_BackgroundFog."))
                || Path.Contains(TEXT("/SM_CropoutBaseIsland.")))
            {Mesh->SetVisibility(false);Mesh->SetHiddenInGame(true);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);++Hidden;}
        }
        TArray<UDirectionalLightComponent*> Lights;It->GetComponents(Lights);
        for(auto* Sun:Lights)
        {
            Sun->SetIntensity(8.f);Sun->SetLightColor(FLinearColor(1.f,.94f,.82f));
            // The original miniature scene's cloud mask blackens broad areas
            // of the larger terrain. Daylight here is shared, readable light.
            Sun->SetLightFunctionMaterial(nullptr);
            Sun->SetWorldRotation(FRotator(-42.f,-32.f,0));
            Sun->LightSourceAngle=1.2f;Sun->DynamicShadowDistanceMovableLight=50000.f;++Suns;
        }
        TArray<USkyLightComponent*> Skylights;It->GetComponents(Skylights);
        for(auto* Sky:Skylights)
        {
            // A stable daylight environment also lights the shaded facades.
            // It does not depend on a first scene-capture frame's sky history.
            Sky->SetMobility(EComponentMobility::Movable);
            if(auto* Cube=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")))
            {Sky->SourceType=SLS_SpecifiedCubemap;Sky->SetCubemap(Cube);}
            Sky->SetIntensity(1.6f);Sky->SetLightColor(FLinearColor(.86f,.91f,1.f));Sky->RecaptureSky();++Skies;
        }
    }
    auto* Post=NewObject<UPostProcessComponent>(&TerrainOwner,TEXT("AincradDaylightGrade"));
    Post->SetupAttachment(TerrainOwner.GetRootComponent());Post->bUnbound=true;Post->Priority=100.f;
    Exposure(Post->Settings);Post->RegisterComponent();
    UE_LOG(LogTemp,Display,TEXT("AINCRAD_STYLE profile=hearth_aincrad_daylight_v1 old_island_layers_hidden=%d sun=%d sky=%d"),Hidden,Suns,Skies);
}

void HearthAincradStyle::ConfigureCapture(USceneCaptureComponent2D* Capture)
{if(Capture){Exposure(Capture->PostProcessSettings);Capture->PostProcessBlendWeight=1.f;}}

struct FHearthAincradStyleCapture
{
    static void Write(AHearthVillage& V,const FString& Name,const FVector& Eye,const FVector& Aim)
    {
        auto* Target=NewObject<UTextureRenderTarget2D>(&V);
        Target->RenderTargetFormat=RTF_RGBA8;Target->ClearColor=FLinearColor::Black;
        Target->InitAutoFormat(1600,1000);Target->UpdateResourceImmediate(true);
        auto* Capture=NewObject<USceneCaptureComponent2D>(&V);
        Capture->TextureTarget=Target;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
        Capture->bAlwaysPersistRenderingState=true;Capture->CaptureSource=SCS_FinalColorLDR;
        Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=65.f;
        for(const auto& M:V.PlanningMeshes) if(IsValid(M)) Capture->HiddenComponents.Add(M.Get());
        HearthAincradStyle::ConfigureCapture(Capture);
        Capture->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod=true;
        Capture->PostProcessSettings.DynamicGlobalIlluminationMethod=EDynamicGlobalIlluminationMethod::Lumen;
        Capture->RegisterComponent();Capture->SetWorldLocationAndRotation(Eye,(Aim-Eye).Rotation());
        // Let this view's exposure/render history initialise before reading
        // it, rather than evaluating the first uninitialised render target.
        for(int32 Warmup=0;Warmup<4;++Warmup){Capture->CaptureScene();FlushRenderingCommands();}
        TArray<FColor> Pixels;const bool Read=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);Capture->DestroyComponent();
        if(!Read || Pixels.Num()!=1600*1000) return;
        for(auto& Pixel:Pixels) Pixel.A=255;
        auto& Images=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        auto PNG=Images.CreateImageWrapper(EImageFormat::PNG);
        if(!PNG.IsValid() || !PNG->SetRaw(Pixels.GetData(),Pixels.Num()*sizeof(FColor),1600,1000,ERGBFormat::BGRA,8)) return;
        const auto& Compressed=PNG->GetCompressed();TArray<uint8> Bytes;Bytes.Append(Compressed.GetData(),int32(Compressed.Num()));
        const FString Path=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/AincradStyle")/(Name+TEXT(".png"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
        if(!FFileHelper::SaveArrayToFile(Bytes,*Path)) return;
        auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("world_id"),V.WorldId);
        Meta->SetStringField(TEXT("provenance"),TEXT("coordinator art validation; not NPC FOV; actual built scene only"));
        Meta->SetStringField(TEXT("eye"),Eye.ToString());Meta->SetStringField(TEXT("aim"),Aim.ToString());
        Meta->SetNumberField(TEXT("elapsed"),V.Elapsed);Meta->SetBoolField(TEXT("orthographic"),false);
        FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Path+TEXT(".json")));
        UE_LOG(LogTemp,Display,TEXT("AINCRAD_STYLE_CAPTURE name=%s bytes=%d path=%s"),*Name,Bytes.Num(),*Path);
    }
    static void Export(AHearthVillage& V)
    {
        if(!V.IsOrganicVillage() || !V.GetWorld()) return;
        Write(V,TEXT("town"),FVector(-11500,-13000,21000),FVector(3500,5500,1300));
        Write(V,TEXT("old-town"),FVector(-6500,-7800,5800),FVector(-1800,-700,200));
        for(int32 Index:{0,4,6})
        {
            if(!V.Residents.IsValidIndex(Index)) continue;const int32 Plot=V.Residents[Index].Plot;
            if(Plot<0 || Plot>=V.HousingPlotCount()) continue;
            const FVector Center=V.PlotPositions[Plot];const FRotator Facing(0,V.PlotYaws[Plot],0);
            FVector Eye=Center+Facing.RotateVector(FVector(-1800,-1200,0));
            double BestScore=TNumericLimits<double>::Max();
            for(int32 Try=0;Try<12;++Try)
            {
                const FVector Offset=FRotator(0,Try*30.f,0).RotateVector(Facing.RotateVector(FVector(-1800,-1200,0)));
                FVector Candidate=Center+Offset;Candidate.Z=V.GroundHeightAt(Candidate)+210;
                const double Score=FMath::Abs(Candidate.Z-(Center.Z+210))+Try*3;
                if(Score<BestScore){BestScore=Score;Eye=Candidate;}
            }
            Write(V,FString::Printf(TEXT("home-%d"),Index),Eye,Center+FVector(0,0,260));
        }
        for(int32 Index=0;Index<V.Residents.Num();++Index) V.ExportDesignObservation(Index);
    }
};
void HearthAincradStyle::ExportViews(AHearthVillage& Village){FHearthAincradStyleCapture::Export(Village);}
