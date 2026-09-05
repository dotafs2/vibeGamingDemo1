#include "HearthImportPreviewLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

AStaticMeshActor* UHearthImportPreviewLibrary::SpawnPreview(UStaticMeshComponent* Original, UStaticMesh* Mesh)
{
    if (!IsValid(Original) || !IsValid(Mesh)) return nullptr;
    UWorld* World = Original->GetWorld();
    if (!World || World->WorldType != EWorldType::PIE) return nullptr;
    FActorSpawnParameters Parameters;
    Parameters.ObjectFlags |= RF_Transient;
    Parameters.Owner = Original->GetOwner();
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
        Original->GetComponentTransform(), Parameters);
    if (!Actor) return nullptr;
    auto* Component = Actor->GetStaticMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!Component->SetStaticMesh(Mesh))
    {
        Actor->Destroy();
        return nullptr;
    }
    return Actor;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPreviewWorldTest, "ThreeHearths.ImportPreview.WorldIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPreviewWorldTest::RunTest(const FString&)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Load the test mesh"), Mesh)) return false;
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false);
    for (EWorldType::Type Type : {EWorldType::Editor, EWorldType::PIE})
    {
        UWorld* World = UWorld::CreateWorld(Type, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!TestNotNull(TEXT("Create isolated world"), World)) return false;
        auto* Original = World->SpawnActor<AStaticMeshActor>();
        if (!TestNotNull(TEXT("Create original actor"), Original)) { World->DestroyWorld(false); return false; }
        auto* Component = Original->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Mesh);
        Original->SetActorTransform(FTransform(FRotator(0,90,0), FVector(-1500,1200,2.8), FVector(1.25)));
        const auto Collision = Component->GetCollisionEnabled();
        auto* Preview = UHearthImportPreviewLibrary::SpawnPreview(Component, Mesh);
        if (Type == EWorldType::Editor)
            TestNull(TEXT("Never spawn a preview in the saved editor world"), Preview);
        else if (TestNotNull(TEXT("Spawn a visual in PIE"), Preview))
        {
            TestTrue(TEXT("Use the original transform"), Preview->GetActorTransform().Equals(Component->GetComponentTransform()));
            TestEqual(TEXT("No second collision body"), Preview->GetStaticMeshComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            TestTrue(TEXT("Preview is transient"), Preview->HasAnyFlags(RF_Transient));
            Preview->Destroy();
        }
        TestEqual(TEXT("Original collision unchanged"), Component->GetCollisionEnabled(), Collision);
        TestTrue(TEXT("Original mesh unchanged"), Component->GetStaticMesh() == Mesh);
        World->DestroyWorld(false);
    }
    TestNull(TEXT("Reject missing original"), UHearthImportPreviewLibrary::SpawnPreview(nullptr, Mesh));
    return true;
}
#endif
