#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HearthAincradResidentRuntime.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradCharacterAnimationTest,
    "ThreeHearths.AincradCharacters.IdentityAppearanceAndAnimation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthAincradCharacterAnimationTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("temporary character fixture world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    for (const FString Role : { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("carpenter") })
    {
        AHearthAincradResidentVisual* Visual = World->SpawnActor<AHearthAincradResidentVisual>();
        if (!TestNotNull(Role + TEXT(" actor"), Visual)) continue;
        Visual->SetActorLocation(FVector(127, 465000, 92));
        Visual->SetActorRotation(FRotator(0, 37, 0));
        const FTransform ActorBefore = Visual->GetActorTransform();
        const FVector EyeBasisBefore = Visual->Body->GetRelativeLocation();
        const float RadiusBefore = Visual->Capsule->GetUnscaledCapsuleRadius();
        const float HalfHeightBefore = Visual->Capsule->GetUnscaledCapsuleHalfHeight();
        if (!TestTrue(Role + TEXT(" complete appearance loads"), Visual->ConfigureIdentityAppearance(Role, -90))) continue;
        TestTrue(Role + TEXT(" actor position and facing preserved"), Visual->GetActorTransform().Equals(ActorBefore));
        TestTrue(Role + TEXT(" body offset preserved"), Visual->Body->GetRelativeLocation().Equals(EyeBasisBefore));
        TestEqual(Role + TEXT(" capsule radius preserved"), Visual->Capsule->GetUnscaledCapsuleRadius(), RadiusBefore);
        TestEqual(Role + TEXT(" capsule height preserved"), Visual->Capsule->GetUnscaledCapsuleHalfHeight(), HalfHeightBefore);
        TestEqual(Role + TEXT(" relative mesh yaw"), Visual->Body->GetRelativeRotation().Yaw, -90.0);
        TestEqual(Role + TEXT(" persisted material slots"), Visual->Body->GetSkeletalMeshAsset()->GetMaterials().Num(), 12);
        for (int32 Index = 0; Index < Visual->Body->GetNumMaterials(); ++Index)
            TestNotNull(Role + TEXT(" material"), Visual->Body->GetMaterial(Index));

        TArray<FVector> IdleProbe;
        TArray<FVector> WalkProbe;
        for (const bool Walking : { false, true })
        {
            Visual->SetWalking(Walking);
            Visual->Body->SetPosition(.25f, false);
            Visual->Body->TickAnimation(0.f, false);
            Visual->Body->RefreshBoneTransforms();
            TestTrue(Role + TEXT(" sampled actual single node position"),
                FMath::IsNearlyEqual(Visual->Body->GetPosition(), .25f, .001f));
            TArray<FVector>& Probe = Walking ? WalkProbe : IdleProbe;
            for (const FName Bone : { FName(TEXT("hand_L")), FName(TEXT("hand_R")), FName(TEXT("foot_L")), FName(TEXT("foot_R")) })
            {
                TestTrue(Role + TEXT(" bone exists"), Visual->Body->GetBoneIndex(Bone) >= 0);
                Probe.Add(Visual->Body->GetSocketTransform(Bone, RTS_Component).GetLocation());
            }
        }
        const double HandDelta = FMath::Max(FVector::Distance(IdleProbe[0], WalkProbe[0]), FVector::Distance(IdleProbe[1], WalkProbe[1]));
        const double FootDelta = FMath::Max(FVector::Distance(IdleProbe[2], WalkProbe[2]), FVector::Distance(IdleProbe[3], WalkProbe[3]));
        AddInfo(FString::Printf(TEXT("%s component pose difference: hand %.3f cm, foot %.3f cm"), *Role, HandDelta, FootDelta));
        TestTrue(Role + TEXT(" actual idle/walk hands differ"), HandDelta >= .5);
        TestTrue(Role + TEXT(" actual idle/walk feet differ"), FootDelta >= .5);
        const USkeletalMesh* Installed = Visual->Body->GetSkeletalMeshAsset();
        AddExpectedError(TEXT("unsupported_role_or_yaw"), EAutomationExpectedErrorFlags::Contains, 1);
        TestFalse(Role + TEXT(" unsupported appearance refuses change"), Visual->ConfigureIdentityAppearance(TEXT("unknown"), 0));
        TestTrue(Role + TEXT(" rejected swap keeps installed mesh"), Visual->Body->GetSkeletalMeshAsset() == Installed);
        TestTrue(Role + TEXT(" rejected swap keeps actor"), Visual->GetActorTransform().Equals(ActorBefore));
        TestEqual(Role + TEXT(" rejected swap keeps mesh yaw"), Visual->Body->GetRelativeRotation().Yaw, -90.0);
        // The fixture world owns cleanup; it intentionally has no engine world context.
    }
    return true;
}
#endif
