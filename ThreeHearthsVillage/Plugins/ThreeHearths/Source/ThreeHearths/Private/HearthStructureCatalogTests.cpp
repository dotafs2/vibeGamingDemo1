#if WITH_DEV_AUTOMATION_TESTS
#include "HearthStructureCatalog.h"
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthStructureCatalogTest, "ThreeHearths.StructureCatalog.NativeStage4Assets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthStructureCatalogTest::RunTest(const FString&)
{
    FString Error;
    TestTrue(TEXT("Stage4 catalog metadata is finite and bounded"), HearthStructureCatalog::Validate(&Error));
    if (!Error.IsEmpty()) AddInfo(Error);
    TestTrue(TEXT("Foundation-to-roof support chain is describable"), HearthStructureCatalog::HasFoundationToRoofSupportChain(&Error));
    if (!Error.IsEmpty()) AddInfo(Error);
    TestTrue(TEXT("Required native asset count is present"), HearthStructureCatalog::Entries().Num() >= 7);
    for (const FHearthStructureCatalogEntry& Entry : HearthStructureCatalog::Entries())
    {
        TestTrue(FString::Printf(TEXT("Asset path is native: %s"), *Entry.CatalogId), Entry.AssetPath.StartsWith(TEXT("/Game/ThreeHearths/Generated/VillageKit/")));
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Entry.AssetPath);
        TestTrue(FString::Printf(TEXT("Native asset exists: %s"), *Entry.CatalogId), Mesh != nullptr);
        TestTrue(FString::Printf(TEXT("Sockets exist: %s"), *Entry.CatalogId), Entry.Sockets.Num() > 0);
    }
    const auto* Door = HearthStructureCatalog::Find(TEXT("wall_door_timber_2m"));
    TestNotNull(TEXT("Timber door catalog entry exists"), Door);
    if (Door)
    {
        TestTrue(TEXT("Door clearance is positive"), Door->bHasDoorClearance && Door->DoorClearanceMin.X < Door->DoorClearanceMax.X && Door->DoorClearanceMin.Y < Door->DoorClearanceMax.Y);
        TestTrue(TEXT("Door clearance width is reviewable"), FMath::IsNearlyEqual(Door->DoorClearanceMax.X - Door->DoorClearanceMin.X, .94f, .01f));
    }
    return true;
}
#endif
