#include "HearthAincradCharacterReview.h"

#include "HearthAincradLevel.h"
#include "HearthAincradResidentRuntime.h"
#include "HearthAincradViewGrade.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/CommandLine.h"
#include "CoreGlobals.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "SingleAnimationPlayData.h"

namespace
{
    constexpr float SampleSeconds = .25f;
    constexpr int32 CaptureSize = 900;
    constexpr double AutomaticSampleWindowSeconds = .2;
    constexpr double AutomaticSampleTimeoutSeconds = 2.0;

    struct FAutomaticPlaybackEntry
    {
        TWeakObjectPtr<AHearthAincradResidentVisual> Visual;
        FSingleAnimationPlayData SavedAnimation;
        bool bWasWalking = false;
        FTransform ActorBefore;
        FTransform CapsuleBefore;
        uint64 StartFrame = 0;
        double StartWallSeconds = 0;
        float StartAnimationSeconds = 0;
        FString StartAsset;
        bool bStartPlaying = false;
        TMap<FName, FVector> StartBones;
    };

    struct FAutomaticPlaybackSession
    {
        TWeakObjectPtr<AHearthAincradLevel> Owner;
        FString VerificationWorldPath;
        uint64 CommandFrame = 0;
        double CommandWallSeconds = 0;
        bool bBaselineRecorded = false;
        TArray<FAutomaticPlaybackEntry> Entries;
    };

    TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
    {
        return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
    }

    TSharedPtr<FJsonObject> TransformJson(const FTransform& Value)
    {
        const auto Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("location_cm"), VectorJson(Value.GetLocation()));
        Result->SetArrayField(TEXT("rotation_quaternion"),
            {MakeShared<FJsonValueNumber>(Value.GetRotation().X), MakeShared<FJsonValueNumber>(Value.GetRotation().Y),
             MakeShared<FJsonValueNumber>(Value.GetRotation().Z), MakeShared<FJsonValueNumber>(Value.GetRotation().W)});
        Result->SetArrayField(TEXT("scale"), VectorJson(Value.GetScale3D()));
        return Result;
    }

    FString SafeName(FString Value)
    {
        Value.RemoveFromStart(TEXT("SK_"));
        for (TCHAR& C : Value) if (!FChar::IsAlnum(C) && C != TEXT('_')) C = TEXT('_');
        return Value.IsEmpty() ? TEXT("unknown") : Value;
    }

    TMap<FName, FVector> SamplePose(AHearthAincradResidentVisual* Visual, const bool bWalking)
    {
        TMap<FName, FVector> Result;
        Visual->SetWalking(bWalking);
        Visual->Body->SetPosition(SampleSeconds, false);
        Visual->Body->TickAnimation(0.f, false);
        Visual->Body->RefreshBoneTransforms();
        for (const FName Bone : {FName(TEXT("hand_L")), FName(TEXT("hand_R")), FName(TEXT("foot_L")), FName(TEXT("foot_R"))})
            Result.Add(Bone, Visual->Body->GetSocketLocation(Bone));
        return Result;
    }

    TMap<FName, FVector> ReadReviewBones(USkeletalMeshComponent* Body)
    {
        TMap<FName, FVector> Result;
        for (const FName Bone : {FName(TEXT("hand_L")), FName(TEXT("hand_R")), FName(TEXT("foot_L")), FName(TEXT("foot_R"))})
            Result.Add(Bone, Body->GetSocketLocation(Bone));
        return Result;
    }

    bool AdvanceAutomaticPlaybackReview(AHearthAincradLevel* Owner, const FString& VerificationWorldPath,
        TArray<TSharedPtr<FJsonValue>>& OutRows, bool& bOutPassed)
    {
        static TUniquePtr<FAutomaticPlaybackSession> Session;
        const double Now = FPlatformTime::Seconds();
        if (!Session.IsValid())
        {
            Session = MakeUnique<FAutomaticPlaybackSession>();
            Session->Owner = Owner;
            Session->VerificationWorldPath = VerificationWorldPath;
            Session->CommandFrame = GFrameCounter;
            Session->CommandWallSeconds = Now;
            for (TActorIterator<AHearthAincradResidentVisual> It(Owner->GetWorld()); It; ++It)
            {
                if (!IsValid(*It) || !IsValid(It->Body) || !IsValid(It->Capsule)) continue;
                FAutomaticPlaybackEntry Entry;
                Entry.Visual = *It;
                Entry.SavedAnimation = It->Body->AnimationData;
                if (UAnimSingleNodeInstance* Instance = It->Body->GetSingleNodeInstance()) Entry.SavedAnimation.PopulateFrom(Instance);
                Entry.bWasWalking = Entry.SavedAnimation.AnimToPlay
                    && Entry.SavedAnimation.AnimToPlay->GetName().Contains(TEXT("_Walk"), ESearchCase::IgnoreCase);
                Entry.ActorBefore = It->GetActorTransform();
                Entry.CapsuleBefore = It->Capsule->GetComponentTransform();
                It->SetWalking(true);
                It->Body->SetPosition(0.f, false);
                It->Body->Play(true);
                Session->Entries.Add(MoveTemp(Entry));
            }
            return false;
        }
        if (Session->Owner.Get() != Owner || Session->VerificationWorldPath != VerificationWorldPath) return false;

        if (!Session->bBaselineRecorded && GFrameCounter > Session->CommandFrame)
        {
            Session->bBaselineRecorded = true;
            for (FAutomaticPlaybackEntry& Entry : Session->Entries)
            {
                if (!Entry.Visual.IsValid()) continue;
                USkeletalMeshComponent* Body = Entry.Visual->Body;
                UAnimSingleNodeInstance* Instance = Body->GetSingleNodeInstance();
                Entry.StartFrame = GFrameCounter;
                Entry.StartWallSeconds = Now;
                Entry.StartAnimationSeconds = Body->GetPosition();
                Entry.StartAsset = GetPathNameSafe(Instance ? Instance->GetCurrentAsset() : nullptr);
                Entry.bStartPlaying = Instance && Instance->IsPlaying();
                Entry.StartBones = ReadReviewBones(Body);
            }
            return false;
        }

        const bool bTimedOut = Now - Session->CommandWallSeconds >= AutomaticSampleTimeoutSeconds;
        bool bWindowReady = Session->bBaselineRecorded;
        if (bWindowReady)
            for (const FAutomaticPlaybackEntry& Entry : Session->Entries)
                bWindowReady &= GFrameCounter >= Entry.StartFrame + 2 && Now - Entry.StartWallSeconds >= AutomaticSampleWindowSeconds;
        if (!bWindowReady && !bTimedOut) return false;

        bOutPassed = Session->Entries.Num() == 3 && bWindowReady && !bTimedOut;
        for (FAutomaticPlaybackEntry& Entry : Session->Entries)
        {
            const auto Row = MakeShared<FJsonObject>();
            AHearthAincradResidentVisual* Visual = Entry.Visual.Get();
            Row->SetStringField(TEXT("character"), Visual ? SafeName(GetNameSafe(Visual->Body->GetSkeletalMeshAsset())) : TEXT("invalid"));
            Row->SetNumberField(TEXT("command_frame"), static_cast<double>(Session->CommandFrame));
            Row->SetNumberField(TEXT("start_frame"), static_cast<double>(Entry.StartFrame));
            Row->SetNumberField(TEXT("end_frame"), static_cast<double>(GFrameCounter));
            Row->SetNumberField(TEXT("start_position_seconds"), Entry.StartAnimationSeconds);
            Row->SetNumberField(TEXT("elapsed_wall_seconds"), Now - Entry.StartWallSeconds);
            bool bEntryPassed = Visual && Entry.StartFrame > Session->CommandFrame && !bTimedOut;
            if (Visual)
            {
                USkeletalMeshComponent* Body = Visual->Body;
                UAnimSingleNodeInstance* Instance = Body->GetSingleNodeInstance();
                const float EndPosition = Body->GetPosition();
                const FString EndAsset = GetPathNameSafe(Instance ? Instance->GetCurrentAsset() : nullptr);
                const bool bEndPlaying = Instance && Instance->IsPlaying();
                const TMap<FName, FVector> EndBones = ReadReviewBones(Body);
                float MaxBoneDelta = 0.f;
                for (const auto& Bone : Entry.StartBones)
                    MaxBoneDelta = FMath::Max(MaxBoneDelta, FVector::Distance(Bone.Value, EndBones.FindRef(Bone.Key)));
                auto StartBoneObject = MakeShared<FJsonObject>();
                auto EndBoneObject = MakeShared<FJsonObject>();
                for (const auto& Bone : Entry.StartBones) StartBoneObject->SetArrayField(Bone.Key.ToString(), VectorJson(Bone.Value));
                for (const auto& Bone : EndBones) EndBoneObject->SetArrayField(Bone.Key.ToString(), VectorJson(Bone.Value));
                Row->SetObjectField(TEXT("start_bones_world_cm"), StartBoneObject);
                Row->SetObjectField(TEXT("end_bones_world_cm"), EndBoneObject);
                Row->SetNumberField(TEXT("start_wall_seconds"), Entry.StartWallSeconds);
                Row->SetNumberField(TEXT("end_wall_seconds"), Now);
                Row->SetNumberField(TEXT("end_position_seconds"), EndPosition);
                Row->SetNumberField(TEXT("position_advance_seconds"), EndPosition - Entry.StartAnimationSeconds);
                Row->SetStringField(TEXT("start_animation_asset"), Entry.StartAsset);
                Row->SetStringField(TEXT("end_animation_asset"), EndAsset);
                Row->SetBoolField(TEXT("start_playing"), Entry.bStartPlaying);
                Row->SetBoolField(TEXT("end_playing"), bEndPlaying);
                Row->SetNumberField(TEXT("max_required_bone_delta_cm"), MaxBoneDelta);
                const bool bActorUnchanged = Visual->GetActorTransform().Equals(Entry.ActorBefore, KINDA_SMALL_NUMBER);
                const bool bCapsuleUnchanged = Visual->Capsule->GetComponentTransform().Equals(Entry.CapsuleBefore, KINDA_SMALL_NUMBER);
                Row->SetBoolField(TEXT("actor_transform_unchanged"), bActorUnchanged);
                Row->SetBoolField(TEXT("capsule_transform_unchanged"), bCapsuleUnchanged);
                bEntryPassed &= GFrameCounter >= Entry.StartFrame + 2
                    && Now - Entry.StartWallSeconds >= AutomaticSampleWindowSeconds
                    && EndPosition - Entry.StartAnimationSeconds >= .1f
                    && Entry.StartAsset.Contains(TEXT("_Walk")) && EndAsset == Entry.StartAsset
                    && Entry.bStartPlaying && bEndPlaying && MaxBoneDelta > .5f
                    && bActorUnchanged && bCapsuleUnchanged;

                Visual->SetWalking(Entry.bWasWalking);
                Body->OverrideAnimationData(Entry.SavedAnimation.AnimToPlay, Entry.SavedAnimation.bSavedLooping,
                    Entry.SavedAnimation.bSavedPlaying, Entry.SavedAnimation.SavedPosition, Entry.SavedAnimation.SavedPlayRate);
            }
            Row->SetBoolField(TEXT("timed_out"), bTimedOut);
            Row->SetBoolField(TEXT("passed"), bEntryPassed);
            bOutPassed &= bEntryPassed;
            OutRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Session.Reset();
        return true;
    }

    void SubmitPoseForReviewCapture(AHearthAincradLevel* Owner, USkeletalMeshComponent* Body)
    {
        // RefreshBoneTransforms updates the CPU/read bone buffer. Multiple
        // manual samples in one GFrameCounter share a bone revision, so recreate
        // this review-only proxy to upload the current pose before capture.
        Body->MarkRenderStateDirty();
        Owner->GetWorld()->SendAllEndOfFrameUpdates();
        FlushRenderingCommands();
    }

    bool CapturePng(AHearthAincradLevel* Owner, AHearthAincradResidentVisual* Visual,
        const FVector& Eye, const FVector& Aim, const FString& Path, const bool bIsolateCharacter,
        const bool bApplyViewGrade = true)
    {
        UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Owner);
        USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Owner);
        UPointLightComponent* Fill = bIsolateCharacter ? NewObject<UPointLightComponent>(Owner) : nullptr;
        if (!Target || !Capture || (bIsolateCharacter && !Fill)) return false;

        Target->RenderTargetFormat = RTF_RGBA8;
        Target->ClearColor = FLinearColor(.025f, .03f, .04f, 1.f);
        Target->InitAutoFormat(CaptureSize, CaptureSize);
        Target->UpdateResourceImmediate(true);
        Capture->TextureTarget = Target;
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->bAlwaysPersistRenderingState = true;
        Capture->CaptureSource = SCS_FinalColorLDR;
        Capture->ProjectionType = ECameraProjectionMode::Perspective;
        Capture->FOVAngle = 36.f;
        if (bApplyViewGrade)
        {
            HearthAincradViewGrade::Apply(Capture->PostProcessSettings);
            Capture->PostProcessBlendWeight = 1.f;
        }
        if (bIsolateCharacter)
        {
            Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->ShowOnlyActorComponents(Visual, true);
        }
        else Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
        Capture->RegisterComponent();
        Capture->SetWorldLocationAndRotation(Eye, (Aim - Eye).Rotation());

        if (Fill)
        {
            Fill->SetIntensity(550.f);
            Fill->SetAttenuationRadius(700.f);
            Fill->SetLightColor(FLinearColor(1.f, .88f, .76f));
            Fill->RegisterComponent();
            Fill->SetWorldLocation(FMath::Lerp(Eye, Aim, .18f) + FVector(0.f, 0.f, 35.f));
        }

        for (int32 Warmup = 0; Warmup < 3; ++Warmup) { Capture->CaptureScene(); FlushRenderingCommands(); }
        TArray<FColor> Pixels;
        const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        Capture->DestroyComponent();
        if (Fill) Fill->DestroyComponent();
        if (!bRead || Pixels.Num() != CaptureSize * CaptureSize) return false;
        for (FColor& Pixel : Pixels) Pixel.A = 255;

        IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
        if (!Png.IsValid() || !Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), CaptureSize, CaptureSize, ERGBFormat::BGRA, 8)) return false;
        const TArray64<uint8>& Compressed = Png->GetCompressed();
        if (Compressed.IsEmpty() || Compressed.Num() > MAX_int32) return false;
        TArray<uint8> Bytes;
        Bytes.Append(Compressed.GetData(), static_cast<int32>(Compressed.Num()));
        return FFileHelper::SaveArrayToFile(Bytes, *Path);
    }

    bool IsAllowedVerificationPath(FString Path)
    {
        Path = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(Path);
        FPaths::CollapseRelativeDirectories(Path);
        FString Allowed = FPaths::ConvertRelativePathToFull(
            FPaths::ProjectSavedDir() / TEXT("ThreeHearths/AincradLevel0/VerificationWorlds"));
        FPaths::NormalizeFilename(Allowed);
        FPaths::CollapseRelativeDirectories(Allowed);
        return FPaths::IsUnderDirectory(Path, Allowed) && IFileManager::Get().FileExists(*Path);
    }

    TSharedPtr<FJsonObject> CaptureWorkshopReview(AHearthAincradLevel* Owner, const FString& Directory)
    {
        const auto Review = MakeShared<FJsonObject>();
        Review->SetStringField(TEXT("source"), TEXT("local_verification"));
        Review->SetStringField(TEXT("review_role"), TEXT("coordinator"));
        Review->SetBoolField(TEXT("npc_vision"), false);
        Review->SetStringField(TEXT("target_static_mesh"), TEXT("smithy_cold_forge"));
        Review->SetBoolField(TEXT("view_grade_applied"), true);
        Review->SetBoolField(TEXT("temporary_fill_light"), false);

        TArray<UInstancedStaticMeshComponent*> Components;
        Owner->GetComponents<UInstancedStaticMeshComponent>(Components);
        UInstancedStaticMeshComponent* Match = nullptr;
        int32 MatchingComponentCount = 0;
        int32 MatchingInstanceCount = 0;
        for (UInstancedStaticMeshComponent* Component : Components)
        {
            if (!IsValid(Component) || !IsValid(Component->GetStaticMesh())
                || Component->GetStaticMesh()->GetName() != TEXT("smithy_cold_forge")) continue;
            if (!Match || (Match->GetInstanceCount() == 0 && Component->GetInstanceCount() > 0)) Match = Component;
            ++MatchingComponentCount;
            MatchingInstanceCount += Component->GetInstanceCount();
        }
        Review->SetNumberField(TEXT("matching_component_count"), MatchingComponentCount);
        Review->SetNumberField(TEXT("matching_instance_count"), MatchingInstanceCount);

        bool bAssetValid = IsValid(Match) && IsValid(Match->GetStaticMesh()) && MatchingInstanceCount == 1;
        Review->SetBoolField(TEXT("asset_present"), IsValid(Match) && IsValid(Match->GetStaticMesh()));
        Review->SetBoolField(TEXT("instance_count_valid"), MatchingInstanceCount == 1);
        if (!Match)
        {
            Review->SetStringField(TEXT("status"), TEXT("failed"));
            Review->SetStringField(TEXT("failure"), TEXT("no UInstancedStaticMeshComponent uses smithy_cold_forge"));
            Review->SetBoolField(TEXT("render_capture_completed"), false);
            Review->SetBoolField(TEXT("render_visually_confirmed"), false);
            Review->SetBoolField(TEXT("passed"), false);
            return Review;
        }

        const UStaticMesh* StaticMesh = Match->GetStaticMesh();
        Review->SetStringField(TEXT("static_mesh_path"), GetPathNameSafe(StaticMesh));
        TArray<TSharedPtr<FJsonValue>> Materials;
        bool bMaterialsValid = StaticMesh->GetStaticMaterials().Num() > 0;
        for (const FStaticMaterial& Slot : StaticMesh->GetStaticMaterials())
        {
            const UMaterialInterface* Material = Slot.MaterialInterface;
            Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Material)));
            bMaterialsValid &= Material != nullptr;
        }
        Review->SetArrayField(TEXT("materials"), Materials);
        Review->SetNumberField(TEXT("material_slot_count"), Materials.Num());
        Review->SetBoolField(TEXT("materials_valid"), bMaterialsValid);
        bAssetValid &= bMaterialsValid;

        FTransform InstanceTransform;
        const bool bTransformRead = Match->GetInstanceTransform(0, InstanceTransform, true);
        const FVector Scale = bTransformRead ? InstanceTransform.GetScale3D() : FVector::ZeroVector;
        const bool bScaleValid = bTransformRead && FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z)
            && Scale.X > KINDA_SMALL_NUMBER && Scale.Y > KINDA_SMALL_NUMBER && Scale.Z > KINDA_SMALL_NUMBER;
        Review->SetBoolField(TEXT("instance_transform_read"), bTransformRead);
        if (bTransformRead) Review->SetObjectField(TEXT("instance_transform_world"), TransformJson(InstanceTransform));
        Review->SetBoolField(TEXT("scale_valid"), bScaleValid);
        bAssetValid &= bScaleValid;

        const FVector Eye = InstanceTransform.TransformPosition(FVector(-160.f, -320.f, 225.f));
        const FVector Aim = InstanceTransform.TransformPosition(FVector(-10.f, 0.f, 70.f));
        const FString ImagePath = Directory / TEXT("cold_forge_context.png");
        const bool bCapture = bTransformRead && CapturePng(Owner, nullptr, Eye, Aim, ImagePath, false)
            && IFileManager::Get().FileSize(*ImagePath) > 0;
        Review->SetStringField(TEXT("image"), ImagePath);
        Review->SetArrayField(TEXT("camera_eye_world_cm"), VectorJson(Eye));
        Review->SetArrayField(TEXT("camera_aim_world_cm"), VectorJson(Aim));
        Review->SetNumberField(TEXT("fov_degrees"), 36.f);
        Review->SetBoolField(TEXT("render_capture_completed"), bCapture);
        Review->SetBoolField(TEXT("render_visually_confirmed"), false);
        Review->SetBoolField(TEXT("passed"), false);
        Review->SetStringField(TEXT("status"), bAssetValid && bCapture ? TEXT("captured; requires visual confirmation") : TEXT("failed"));
        return Review;
    }

    TSharedPtr<FJsonObject> CaptureTradeSignsReview(AHearthAincradLevel* Owner, const FString& Directory)
    {
        struct FTradeSignSpec
        {
            const TCHAR* Role;
            const TCHAR* MeshName;
        };
        const FTradeSignSpec Specs[] = {
            {TEXT("inn"), TEXT("inn_trade_sign")},
            {TEXT("smithy"), TEXT("smithy_trade_sign")},
            {TEXT("carpentry"), TEXT("carpentry_trade_sign")}
        };

        const auto Review = MakeShared<FJsonObject>();
        Review->SetStringField(TEXT("source"), TEXT("local_verification"));
        Review->SetStringField(TEXT("review_role"), TEXT("coordinator"));
        Review->SetBoolField(TEXT("npc_vision"), false);
        Review->SetBoolField(TEXT("scene_environment_hidden"), false);
        Review->SetBoolField(TEXT("view_grade_applied"), true);
        Review->SetBoolField(TEXT("temporary_fill_light"), false);
        Review->SetBoolField(TEXT("render_visually_confirmed"), false);
        Review->SetBoolField(TEXT("passed"), false);

        TArray<UInstancedStaticMeshComponent*> Components;
        Owner->GetComponents<UInstancedStaticMeshComponent>(Components);
        TArray<TSharedPtr<FJsonValue>> Rows;
        int32 CapturedCount = 0;
        for (const FTradeSignSpec& Spec : Specs)
        {
            const auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("role"), Spec.Role);
            Row->SetStringField(TEXT("target_static_mesh"), Spec.MeshName);
            Row->SetBoolField(TEXT("render_visually_confirmed"), false);
            Row->SetBoolField(TEXT("passed"), false);

            UInstancedStaticMeshComponent* Match = nullptr;
            int32 MatchingComponentCount = 0;
            int32 MatchingInstanceCount = 0;
            for (UInstancedStaticMeshComponent* Component : Components)
            {
                if (!IsValid(Component) || !IsValid(Component->GetStaticMesh())
                    || Component->GetStaticMesh()->GetName() != Spec.MeshName) continue;
                if (!Match || (Match->GetInstanceCount() == 0 && Component->GetInstanceCount() > 0)) Match = Component;
                ++MatchingComponentCount;
                MatchingInstanceCount += Component->GetInstanceCount();
            }
            Row->SetNumberField(TEXT("matching_component_count"), MatchingComponentCount);
            Row->SetNumberField(TEXT("matching_instance_count"), MatchingInstanceCount);
            Row->SetBoolField(TEXT("instance_count_valid"), MatchingInstanceCount == 1);

            bool bAssetValid = IsValid(Match) && IsValid(Match->GetStaticMesh()) && MatchingInstanceCount == 1;
            if (Match)
            {
                const UStaticMesh* StaticMesh = Match->GetStaticMesh();
                Row->SetStringField(TEXT("static_mesh_path"), GetPathNameSafe(StaticMesh));
                TArray<TSharedPtr<FJsonValue>> Materials;
                bool bMaterialsValid = StaticMesh->GetStaticMaterials().Num() > 0;
                for (int32 SlotIndex = 0; SlotIndex < StaticMesh->GetStaticMaterials().Num(); ++SlotIndex)
                {
                    const FStaticMaterial& StaticSlot = StaticMesh->GetStaticMaterials()[SlotIndex];
                    const UMaterialInterface* EffectiveMaterial = Match->GetMaterial(SlotIndex);
                    const auto Slot = MakeShared<FJsonObject>();
                    Slot->SetNumberField(TEXT("slot_index"), SlotIndex);
                    Slot->SetStringField(TEXT("slot_name"), StaticSlot.MaterialSlotName.ToString());
                    Slot->SetStringField(TEXT("asset_material_path"), GetPathNameSafe(StaticSlot.MaterialInterface));
                    Slot->SetStringField(TEXT("effective_material_path"), GetPathNameSafe(EffectiveMaterial));
                    Materials.Add(MakeShared<FJsonValueObject>(Slot));
                    bMaterialsValid &= EffectiveMaterial != nullptr;
                }
                Row->SetArrayField(TEXT("material_slots"), Materials);
                Row->SetNumberField(TEXT("material_slot_count"), Materials.Num());
                Row->SetBoolField(TEXT("materials_valid"), bMaterialsValid);
                bAssetValid &= bMaterialsValid;
            }

            FTransform InstanceTransform;
            const bool bTransformRead = Match && MatchingInstanceCount == 1
                && Match->GetInstanceTransform(0, InstanceTransform, true);
            Row->SetBoolField(TEXT("instance_transform_read"), bTransformRead);
            if (bTransformRead) Row->SetObjectField(TEXT("instance_transform_world"), TransformJson(InstanceTransform));

            const FVector Eye = bTransformRead
                ? InstanceTransform.TransformPosition(FVector(70.f, -250.f, 25.f)) : FVector::ZeroVector;
            const FVector Aim = bTransformRead
                ? InstanceTransform.TransformPosition(FVector(0.f, 0.f, 15.f)) : FVector::ZeroVector;
            const FString ImagePath = Directory / FString::Printf(TEXT("%s_trade_sign_context.png"), Spec.Role);
            const bool bCapture = bTransformRead && CapturePng(Owner, nullptr, Eye, Aim, ImagePath, false)
                && IFileManager::Get().FileSize(*ImagePath) > 0;
            Row->SetStringField(TEXT("image"), ImagePath);
            Row->SetArrayField(TEXT("camera_eye_world_cm"), VectorJson(Eye));
            Row->SetArrayField(TEXT("camera_aim_world_cm"), VectorJson(Aim));
            Row->SetNumberField(TEXT("fov_degrees"), 36.f);
            Row->SetBoolField(TEXT("render_capture_completed"), bCapture);
            Row->SetStringField(TEXT("status"), bAssetValid && bTransformRead && bCapture
                ? TEXT("captured; requires visual confirmation") : TEXT("failed"));
            if (bAssetValid && bTransformRead && bCapture) ++CapturedCount;
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Review->SetArrayField(TEXT("signs"), Rows);
        Review->SetNumberField(TEXT("expected_count"), UE_ARRAY_COUNT(Specs));
        Review->SetNumberField(TEXT("captured_count"), CapturedCount);
        Review->SetBoolField(TEXT("all_three_captured"), CapturedCount == UE_ARRAY_COUNT(Specs));
        Review->SetStringField(TEXT("status"), CapturedCount == UE_ARRAY_COUNT(Specs)
            ? TEXT("captured; requires visual confirmation") : TEXT("failed"));
        return Review;
    }

    TSharedPtr<FJsonObject> CaptureAxeReview(AHearthAincradLevel* Owner, const FString& Directory,
        const FString& VerificationWorldPath)
    {
        const auto Review = MakeShared<FJsonObject>();
        Review->SetStringField(TEXT("source"), TEXT("local_verification"));
        Review->SetStringField(TEXT("review_role"), TEXT("coordinator"));
        Review->SetBoolField(TEXT("npc_vision"), false);
        Review->SetBoolField(TEXT("kimi_action"), false);
        Review->SetBoolField(TEXT("item_state_changed"), false);
        Review->SetBoolField(TEXT("graded_view_grade_applied"), true);
        Review->SetBoolField(TEXT("legacy_capture_component_grade_applied"), false);
        Review->SetStringField(TEXT("exposure_comparison_scope"), TEXT("Both inherit the existing unbound scene grade; comparison adds an explicit identical component grade, not an ungraded scene."));
        Review->SetBoolField(TEXT("temporary_fill_light"), false);
        Review->SetBoolField(TEXT("render_visually_confirmed"), false);
        Review->SetBoolField(TEXT("passed"), false);

        TSharedPtr<FJsonObject> CopyState;
        TSharedPtr<FJsonObject> Item;
        FString CopyJson;
        if (FFileHelper::LoadFileToString(CopyJson, *VerificationWorldPath)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CopyJson), CopyState)
            && CopyState.IsValid())
        {
            const TSharedPtr<FJsonObject>* Life = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
            if (CopyState->TryGetObjectField(TEXT("life"), Life) && Life && (*Life).IsValid()
                && (*Life)->TryGetArrayField(TEXT("items"), Items) && Items)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Items)
                {
                    const TSharedPtr<FJsonObject> Candidate = Value.IsValid() ? Value->AsObject() : nullptr;
                    if (Candidate.IsValid() && Candidate->GetStringField(TEXT("id")) == TEXT("aincrad_axe_erin"))
                    {
                        Item = Candidate;
                        break;
                    }
                }
            }
        }
        Review->SetBoolField(TEXT("item_found_in_verification_copy"), Item.IsValid());
        double Edge = 0, Handle = 0;
        if (Item.IsValid())
        {
            Item->TryGetNumberField(TEXT("edge"), Edge);
            Item->TryGetNumberField(TEXT("handle"), Handle);
            Review->SetStringField(TEXT("item_id"), TEXT("aincrad_axe_erin"));
            Review->SetStringField(TEXT("owner_id"), Item->GetStringField(TEXT("owner_id")));
            Review->SetStringField(TEXT("custodian_id"), Item->GetStringField(TEXT("custodian_id")));
            Review->SetNumberField(TEXT("edge"), Edge);
            Review->SetNumberField(TEXT("handle"), Handle);
        }

        AActor* Tool = nullptr;
        int32 TaggedActorCount = 0;
        for (TActorIterator<AActor> It(Owner->GetWorld()); It; ++It)
        {
            if (!IsValid(*It) || !It->ActorHasTag(FName(TEXT("aincrad_axe_erin")))) continue;
            if (!Tool) Tool = *It;
            ++TaggedActorCount;
        }
        Review->SetNumberField(TEXT("tagged_actor_count"), TaggedActorCount);
        Review->SetBoolField(TEXT("tagged_actor_valid"), TaggedActorCount == 1 && Tool != nullptr);

        TArray<TSharedPtr<FJsonValue>> Parts;
        int32 AxePartCount = 0;
        int32 VisibleMeshComponentCount = 0;
        bool bPartsValid = TaggedActorCount == 1;
        const FString ExpectedHandle = Handle >= 100 ? TEXT("axe_handle_sound") : TEXT("axe_handle_split");
        const FString ExpectedHead = Edge >= 100 ? TEXT("axe_head_sharp") : TEXT("axe_head_chipped");
        bool bExpectedHandle = false, bExpectedHead = false;
        if (Tool)
        {
            TArray<UStaticMeshComponent*> Components;
            Tool->GetComponents<UStaticMeshComponent>(Components);
            for (UStaticMeshComponent* Component : Components)
            {
                if (!IsValid(Component) || !IsValid(Component->GetStaticMesh())) continue;
                if (Component->IsVisible()) ++VisibleMeshComponentCount;
                const FString MeshName = Component->GetStaticMesh()->GetName();
                if (!MeshName.StartsWith(TEXT("axe_handle_")) && !MeshName.StartsWith(TEXT("axe_head_"))) continue;
                ++AxePartCount;
                bExpectedHandle |= MeshName == ExpectedHandle;
                bExpectedHead |= MeshName == ExpectedHead;
                const auto Part = MakeShared<FJsonObject>();
                Part->SetStringField(TEXT("component"), Component->GetName());
                Part->SetStringField(TEXT("mesh_path"), GetPathNameSafe(Component->GetStaticMesh()));
                Part->SetObjectField(TEXT("relative_transform"), TransformJson(Component->GetRelativeTransform()));
                Part->SetObjectField(TEXT("world_transform"), TransformJson(Component->GetComponentTransform()));
                TArray<TSharedPtr<FJsonValue>> Materials;
                bool bMaterialsValid = Component->GetNumMaterials() > 0;
                for (int32 SlotIndex = 0; SlotIndex < Component->GetNumMaterials(); ++SlotIndex)
                {
                    const UMaterialInterface* Material = Component->GetMaterial(SlotIndex);
                    const auto Slot = MakeShared<FJsonObject>();
                    Slot->SetNumberField(TEXT("slot_index"), SlotIndex);
                    if (Component->GetStaticMesh()->GetStaticMaterials().IsValidIndex(SlotIndex))
                        Slot->SetStringField(TEXT("slot_name"),
                            Component->GetStaticMesh()->GetStaticMaterials()[SlotIndex].MaterialSlotName.ToString());
                    Slot->SetStringField(TEXT("material_path"), GetPathNameSafe(Material));
                    Materials.Add(MakeShared<FJsonValueObject>(Slot));
                    bMaterialsValid &= Material != nullptr;
                }
                Part->SetArrayField(TEXT("material_slots"), Materials);
                Part->SetNumberField(TEXT("material_slot_count"), Materials.Num());
                Part->SetBoolField(TEXT("materials_valid"), bMaterialsValid);
                bPartsValid &= bMaterialsValid;
                Parts.Add(MakeShared<FJsonValueObject>(Part));
            }
        }
        bPartsValid &= AxePartCount == 2 && VisibleMeshComponentCount == 2 && bExpectedHandle && bExpectedHead;
        Review->SetArrayField(TEXT("parts"), Parts);
        Review->SetNumberField(TEXT("part_count"), AxePartCount);
        Review->SetNumberField(TEXT("visible_mesh_component_count"), VisibleMeshComponentCount);
        Review->SetStringField(TEXT("expected_handle_mesh"), ExpectedHandle);
        Review->SetStringField(TEXT("expected_head_mesh"), ExpectedHead);
        Review->SetBoolField(TEXT("parts_valid_for_condition"), bPartsValid);

        const FTransform ToolBefore = Tool ? Tool->GetActorTransform() : FTransform::Identity;
        const FVector Eye = ToolBefore.TransformPosition(FVector(30.f, -140.f, 15.f));
        const FVector Aim = ToolBefore.TransformPosition(FVector(6.f, 0.f, 5.f));
        const FString LegacyImagePath = Directory / TEXT("axe_context_legacy_exposure.png");
        const FString ImagePath = Directory / TEXT("axe_context.png");
        const bool bLegacyCapture = Item.IsValid() && Tool && bPartsValid
            && CapturePng(Owner, nullptr, Eye, Aim, LegacyImagePath, false, false)
            && IFileManager::Get().FileSize(*LegacyImagePath) > 0;
        const bool bGradedCapture = Item.IsValid() && Tool && bPartsValid
            && CapturePng(Owner, nullptr, Eye, Aim, ImagePath, false)
            && IFileManager::Get().FileSize(*ImagePath) > 0;
        const bool bTransformUnchanged = Tool && Tool->GetActorTransform().Equals(ToolBefore, KINDA_SMALL_NUMBER);
        Review->SetObjectField(TEXT("tool_actor_transform"), TransformJson(ToolBefore));
        Review->SetBoolField(TEXT("tool_actor_transform_unchanged"), bTransformUnchanged);
        Review->SetStringField(TEXT("legacy_exposure_image"), LegacyImagePath);
        Review->SetStringField(TEXT("image"), ImagePath);
        Review->SetArrayField(TEXT("camera_eye_world_cm"), VectorJson(Eye));
        Review->SetArrayField(TEXT("camera_aim_world_cm"), VectorJson(Aim));
        Review->SetNumberField(TEXT("fov_degrees"), 36.f);
        Review->SetBoolField(TEXT("legacy_exposure_capture_completed"), bLegacyCapture);
        Review->SetBoolField(TEXT("graded_capture_completed"), bGradedCapture);
        Review->SetBoolField(TEXT("comparison_same_actor_geometry_lighting_camera"), true);
        Review->SetBoolField(TEXT("render_capture_completed"), bLegacyCapture && bGradedCapture);
        Review->SetStringField(TEXT("status"), Item.IsValid() && bLegacyCapture && bGradedCapture && bTransformUnchanged
            ? TEXT("captured; requires visual confirmation") : TEXT("failed"));
        return Review;
    }
}

bool HearthAincradCharacterReview::TryCapture(AHearthAincradLevel* Owner, const FString& VerificationWorldPath)
{
    static TSet<TWeakObjectPtr<AHearthAincradLevel>> Attempted;
    if (!Owner) return false;
    const TWeakObjectPtr<AHearthAincradLevel> OwnerKey(Owner);
    if (Attempted.Contains(OwnerKey)) return false;
    if (!FParse::Param(FCommandLine::Get(), TEXT("HearthDisableApi"))
        || !FParse::Param(FCommandLine::Get(), TEXT("AincradCharactersV2"))
        || !FParse::Param(FCommandLine::Get(), TEXT("AincradCharacterReview"))
        || !IsAllowedVerificationPath(VerificationWorldPath)) return false;
    TArray<TSharedPtr<FJsonValue>> AutomaticPlaybackRows;
    bool bAutomaticPlaybackPassed = false;
    if (!AdvanceAutomaticPlaybackReview(Owner, VerificationWorldPath, AutomaticPlaybackRows, bAutomaticPlaybackPassed)) return false;
    Attempted.Add(OwnerKey);

    TArray<AHearthAincradResidentVisual*> Visuals;
    for (TActorIterator<AHearthAincradResidentVisual> It(Owner->GetWorld()); It; ++It)
        if (IsValid(*It) && IsValid(It->Body) && IsValid(It->Capsule)) Visuals.Add(*It);
    Visuals.Sort([](const AHearthAincradResidentVisual& A, const AHearthAincradResidentVisual& B)
    {
        const USkeletalMesh* AMesh = A.Body->GetSkeletalMeshAsset();
        const USkeletalMesh* BMesh = B.Body->GetSkeletalMeshAsset();
        return GetNameSafe(AMesh) < GetNameSafe(BMesh);
    });

    const FString CopyName = SafeName(FPaths::GetBaseFilename(VerificationWorldPath));
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("ThreeHearths/AincradLevel0/CharacterReview") / CopyName;
    IFileManager::Get().MakeDirectory(*Directory, true);
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("source"), TEXT("local_verification"));
    Root->SetStringField(TEXT("sampling_method"), TEXT("explicit SetWalking, SetPosition(0.25), TickAnimation(0), RefreshBoneTransforms"));
    Root->SetStringField(TEXT("verification_world"), VerificationWorldPath);
    Root->SetStringField(TEXT("copy_name"), CopyName);
    Root->SetBoolField(TEXT("npc_vision"), false);
    Root->SetBoolField(TEXT("kimi_action"), false);
    Root->SetBoolField(TEXT("automatic_animation_playback_validated"), bAutomaticPlaybackPassed);
    Root->SetStringField(TEXT("automatic_animation_playback_status"), bAutomaticPlaybackPassed ? TEXT("passed") : TEXT("failed"));
    Root->SetStringField(TEXT("automatic_animation_playback_method"),
        TEXT("Walk reset to zero, then sampled across at least 0.2 wall seconds and two natural frames without manual TickAnimation or RefreshBoneTransforms"));
    Root->SetArrayField(TEXT("automatic_animation_playback"), AutomaticPlaybackRows);
    Root->SetStringField(TEXT("cpu_pose_status"), TEXT("validated from required bone transforms at 0.25 seconds"));
    Root->SetStringField(TEXT("render_pose_submission"), TEXT("review-only render-state recreation and end-of-frame render update after each sampled pose"));
    Root->SetStringField(TEXT("render_pose_status"), TEXT("captured; requires visual confirmation"));
    Root->SetBoolField(TEXT("shared_view_grade_applied"), true);
    Root->SetBoolField(TEXT("isolated_character_temporary_fill_light"), true);
    Root->SetBoolField(TEXT("context_temporary_fill_light"), false);
    Root->SetStringField(TEXT("prior_same_tick_evidence_interpretation"),
        TEXT("CPU pose passed; render pose remained unconfirmed because same-frame bone revisions can be reused"));
    Root->SetBoolField(TEXT("isolated_pose_frames_environment_hidden"), true);
    Root->SetStringField(TEXT("context_frame_policy"), TEXT("one additional idle front frame per character renders the scene environment at the unchanged actor location"));

    bool bPassed = Visuals.Num() == 3 && bAutomaticPlaybackPassed;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (AHearthAincradResidentVisual* Visual : Visuals)
    {
        const FTransform ActorBefore = Visual->GetActorTransform();
        const FTransform CapsuleBefore = Visual->Capsule->GetComponentTransform();
        FSingleAnimationPlayData AnimationBefore = Visual->Body->AnimationData;
        if (UAnimSingleNodeInstance* Instance = Visual->Body->GetSingleNodeInstance())
            AnimationBefore.PopulateFrom(Instance);
        const bool bWasWalking = AnimationBefore.AnimToPlay
            && AnimationBefore.AnimToPlay->GetName().Contains(TEXT("_Walk"), ESearchCase::IgnoreCase);

        const FString Name = SafeName(GetNameSafe(Visual->Body->GetSkeletalMeshAsset()));
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("character"), Name);
        Row->SetObjectField(TEXT("actor_before"), TransformJson(ActorBefore));
        Row->SetObjectField(TEXT("capsule_before"), TransformJson(CapsuleBefore));
        Row->SetNumberField(TEXT("capsule_radius_cm"), Visual->Capsule->GetScaledCapsuleRadius());
        Row->SetNumberField(TEXT("capsule_half_height_cm"), Visual->Capsule->GetScaledCapsuleHalfHeight());
        Row->SetArrayField(TEXT("mesh_bounds_origin_cm"), VectorJson(Visual->Body->Bounds.Origin));
        Row->SetArrayField(TEXT("mesh_bounds_box_extent_cm"), VectorJson(Visual->Body->Bounds.BoxExtent));
        Row->SetNumberField(TEXT("material_slot_count"), Visual->Body->GetNumMaterials());
        TArray<TSharedPtr<FJsonValue>> Materials;
        bool bMaterialsValid = Visual->Body->GetNumMaterials() == 12;
        for (int32 Slot = 0; Slot < Visual->Body->GetNumMaterials(); ++Slot)
        {
            const UMaterialInterface* Material = Visual->Body->GetMaterial(Slot);
            Materials.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Material)));
            bMaterialsValid &= Material != nullptr;
        }
        Row->SetArrayField(TEXT("materials"), Materials);
        Row->SetBoolField(TEXT("materials_valid"), bMaterialsValid);

        bool bBonesValid = true;
        for (const FName Bone : {FName(TEXT("hand_L")), FName(TEXT("hand_R")), FName(TEXT("foot_L")), FName(TEXT("foot_R"))})
            bBonesValid &= Visual->Body->GetBoneIndex(Bone) != INDEX_NONE;
        Row->SetBoolField(TEXT("required_bones_valid"), bBonesValid);

        TMap<FName, FVector> Poses[2];
        bool bImagesValid = true;
        bool bSampleTimesValid = true;
        for (int32 State = 0; State < 2; ++State)
        {
            const bool bWalking = State == 1;
            const FString StateName = bWalking ? TEXT("Walk") : TEXT("Idle");
            Poses[State] = SamplePose(Visual, bWalking);
            const float ActualSampleTime = Visual->Body->GetPosition();
            Row->SetNumberField(StateName + TEXT("_actual_sample_time_seconds"), ActualSampleTime);
            bSampleTimesValid &= FMath::Abs(ActualSampleTime - SampleSeconds) <= .01f;
            SubmitPoseForReviewCapture(Owner, Visual->Body);
            const FVector Aim = Visual->Body->Bounds.Origin + FVector(0.f, 0.f, 5.f);
            const FVector Forward = Visual->GetActorForwardVector();
            const FVector Right = Visual->GetActorRightVector();
            for (int32 View = 0; View < 2; ++View)
            {
                const FString ViewName = View == 0 ? TEXT("front") : TEXT("side");
                const FVector Eye = Aim + (View == 0 ? Forward : Right) * 360.f + FVector(0.f, 0.f, 12.f);
                const FString ImagePath = Directory / FString::Printf(TEXT("%s_%s_%s.png"), *Name, *StateName, *ViewName);
                const bool bImage = CapturePng(Owner, Visual, Eye, Aim, ImagePath, true)
                    && IFileManager::Get().FileSize(*ImagePath) > 0;
                Row->SetStringField(FString::Printf(TEXT("%s_%s_image"), *StateName, *ViewName), ImagePath);
                Row->SetBoolField(FString::Printf(TEXT("%s_%s_exists"), *StateName, *ViewName), bImage);
                bImagesValid &= bImage;
            }
            if (!bWalking)
            {
                const FVector Eye = Aim + Forward * 360.f + FVector(0.f, 0.f, 12.f);
                const FString ContextPath = Directory / FString::Printf(TEXT("%s_Idle_front_context.png"), *Name);
                const bool bContext = CapturePng(Owner, Visual, Eye, Aim, ContextPath, false)
                    && IFileManager::Get().FileSize(*ContextPath) > 0;
                Row->SetStringField(TEXT("Idle_front_context_image"), ContextPath);
                Row->SetBoolField(TEXT("Idle_front_context_exists"), bContext);
                Row->SetBoolField(TEXT("Idle_front_context_environment_hidden"), false);
                bImagesValid &= bContext;
            }
            auto PoseObject = MakeShared<FJsonObject>();
            for (const auto& Bone : Poses[State]) PoseObject->SetArrayField(Bone.Key.ToString(), VectorJson(Bone.Value));
            Row->SetObjectField(StateName + TEXT("_bones_world_cm"), PoseObject);
        }

        float HandDelta = 0.f, FootDelta = 0.f;
        for (const FName Bone : {FName(TEXT("hand_L")), FName(TEXT("hand_R")), FName(TEXT("foot_L")), FName(TEXT("foot_R"))})
        {
            const float Delta = FVector::Distance(Poses[0].FindRef(Bone), Poses[1].FindRef(Bone));
            if (Bone.ToString().StartsWith(TEXT("hand"))) HandDelta = FMath::Max(HandDelta, Delta);
            else FootDelta = FMath::Max(FootDelta, Delta);
        }
        Row->SetNumberField(TEXT("max_hand_idle_walk_delta_cm"), HandDelta);
        Row->SetNumberField(TEXT("max_foot_idle_walk_delta_cm"), FootDelta);

        // SetWalking also restores the visual's private mode latch. Override then
        // reinstates the exact pre-review asset, time, rate and playback flags.
        Visual->SetWalking(bWasWalking);
        Visual->Body->OverrideAnimationData(AnimationBefore.AnimToPlay, AnimationBefore.bSavedLooping,
            AnimationBefore.bSavedPlaying, AnimationBefore.SavedPosition, AnimationBefore.SavedPlayRate);
        Visual->Body->TickAnimation(0.f, false);
        Visual->Body->RefreshBoneTransforms();
        SubmitPoseForReviewCapture(Owner, Visual->Body);
        const bool bActorUnchanged = Visual->GetActorTransform().Equals(ActorBefore, KINDA_SMALL_NUMBER);
        const bool bCapsuleUnchanged = Visual->Capsule->GetComponentTransform().Equals(CapsuleBefore, KINDA_SMALL_NUMBER);
        Row->SetObjectField(TEXT("actor_after"), TransformJson(Visual->GetActorTransform()));
        Row->SetObjectField(TEXT("capsule_after"), TransformJson(Visual->Capsule->GetComponentTransform()));
        Row->SetBoolField(TEXT("actor_transform_unchanged"), bActorUnchanged);
        Row->SetBoolField(TEXT("capsule_transform_unchanged"), bCapsuleUnchanged);
        Row->SetBoolField(TEXT("sample_times_valid"), bSampleTimesValid);
        const bool bCpuPosePassed = bMaterialsValid && bBonesValid && bSampleTimesValid
            && HandDelta > .5f && FootDelta > .5f
            && bActorUnchanged && bCapsuleUnchanged;
        Row->SetBoolField(TEXT("cpu_pose_passed"), bCpuPosePassed);
        Row->SetBoolField(TEXT("render_pose_capture_completed"), bImagesValid);
        Row->SetBoolField(TEXT("render_pose_visually_confirmed"), false);
        Row->SetBoolField(TEXT("passed"), false);
        bPassed &= bCpuPosePassed && bImagesValid;
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("characters"), Rows);
    Root->SetNumberField(TEXT("character_count"), Visuals.Num());
    Root->SetBoolField(TEXT("cpu_pose_and_capture_pipeline_passed"), bPassed);
    Root->SetBoolField(TEXT("render_pose_visually_confirmed"), false);
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("awaiting_render_pose_visual_confirmation") : TEXT("failed"));
    Root->SetBoolField(TEXT("passed"), false);
    if (FParse::Param(FCommandLine::Get(), TEXT("AincradWorkshopReview")))
        Root->SetObjectField(TEXT("workshop_review"), CaptureWorkshopReview(Owner, Directory));
    if (FParse::Param(FCommandLine::Get(), TEXT("AincradTradeSignsReview")))
        Root->SetObjectField(TEXT("trade_sign_review"), CaptureTradeSignsReview(Owner, Directory));
    if (FParse::Param(FCommandLine::Get(), TEXT("AincradAxeReview")))
        Root->SetObjectField(TEXT("axe_review"), CaptureAxeReview(Owner, Directory, VerificationWorldPath));
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    const FString IndexPath = Directory / TEXT("index.json");
    const bool bWrote = FFileHelper::SaveStringToFile(Json, *IndexPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bPassed && bWrote)
    {
        UE_LOG(LogTemp, Display, TEXT("AINCRAD_CHARACTER_REVIEW capture_pipeline=1 visual_confirmation=pending path=%s"), *IndexPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AINCRAD_CHARACTER_REVIEW passed=%d wrote=%d path=%s"), bPassed, bWrote, *IndexPath);
    }
    return bPassed && bWrote;
}
