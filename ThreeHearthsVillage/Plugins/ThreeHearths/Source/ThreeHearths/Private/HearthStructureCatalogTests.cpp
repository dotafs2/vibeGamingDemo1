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
    const auto* TerracottaRoof = HearthStructureCatalog::Find(TEXT("roof_slope_terracotta_2m"));
    TestNotNull(TEXT("Terracotta roof catalog entry exists"), TerracottaRoof);
    if (TerracottaRoof)
    {
        TestEqual(TEXT("Terracotta roof uses its native mesh"), TerracottaRoof->AssetPath, FString(TEXT("/Game/ThreeHearths/Generated/VillageKit/roof_slope_terracotta_2m/roof_slope_terracotta_2m.roof_slope_terracotta_2m")));
        TestTrue(TEXT("Terracotta roof keeps measured bounds"), TerracottaRoof->BoundsMin.Equals(FVector(-.04340045f, -1.f, -.13000008f), KINDA_SMALL_NUMBER) && TerracottaRoof->BoundsMax.Equals(FVector(2.25f, 1.f, 1.2841004f), KINDA_SMALL_NUMBER));
        TestTrue(TEXT("Terracotta roof declares beam support"), TerracottaRoof->SupportContacts.ContainsByPredicate([](const FHearthStructureSupportContact& Contact)
        { return Contact.ParentCatalogId == TEXT("beam_timber_2m") && Contact.ParentSocket == TEXT("end_x_plus") && Contact.ChildSocket == TEXT("ridge"); }));
    }
    const auto* Door = HearthStructureCatalog::Find(TEXT("wall_door_timber_2m"));
    TestNotNull(TEXT("Timber door catalog entry exists"), Door);
    if (Door)
    {
        TestTrue(TEXT("Door clearance is positive"), Door->bHasDoorClearance && Door->DoorClearanceMin.X < Door->DoorClearanceMax.X && Door->DoorClearanceMin.Y < Door->DoorClearanceMax.Y);
        TestTrue(TEXT("Door clearance width is reviewable"), FMath::IsNearlyEqual(Door->DoorClearanceMax.X - Door->DoorClearanceMin.X, .94f, .01f));
    }
    const auto* Canopy=HearthStructureCatalog::Find(TEXT("canopy_terracotta_2m"));
    const auto* Ridge=HearthStructureCatalog::Find(TEXT("roof_ridge_terracotta_2m"));
    const auto* Bench=HearthStructureCatalog::Find(TEXT("bench_timber"));
    TestNotNull(TEXT("Terracotta canopy catalog entry exists"),Canopy);
    TestNotNull(TEXT("Terracotta canopy ridge catalog entry exists"),Ridge);
    TestNotNull(TEXT("Timber bench catalog entry exists"),Bench);
    if (Canopy)
    {
        const FVector SizeCm=(Canopy->BoundsMax-Canopy->BoundsMin)*100.f;
        TestTrue(TEXT("Canopy keeps the measured two metre module bounds"),SizeCm.Equals(FVector(220.f,133.5404f,64.2153f),.01f));
        TestTrue(TEXT("Canopy retains the measured native bounds before default rotation"),
            (Canopy->BoundsMin*100.f).Equals(FVector(-110.0,-2.5403900146484375,-5.500005722045898),.01)
            && (Canopy->BoundsMax*100.f).Equals(FVector(110.0,131.0,58.715280532836914),.01));
        FBox PlannedBounds(ForceInit);
        for (int32 X=0;X<2;++X) for (int32 Y=0;Y<2;++Y) for (int32 Z=0;Z<2;++Z)
            PlannedBounds+=Canopy->DefaultRotation.RotateVector(FVector(X?Canopy->BoundsMax.X:Canopy->BoundsMin.X,
                Y?Canopy->BoundsMax.Y:Canopy->BoundsMin.Y,Z?Canopy->BoundsMax.Z:Canopy->BoundsMin.Z)*100.f);
        TestTrue(TEXT("Canopy native default rotation restores the source wall datum and outward eave"),
            Canopy->DefaultRotation.Equals(FRotator(0,180,0))
            && PlannedBounds.Min.Equals(FVector(-110,-131,-5.5),.01)
            && PlannedBounds.Max.Equals(FVector(110,2.5404,58.7153),.01));
        TestTrue(TEXT("Canopy declares a beam support contact"),Canopy->SupportContacts.ContainsByPredicate([](const FHearthStructureSupportContact& Contact)
        { return Contact.ParentCatalogId==TEXT("beam_timber_2m") && Contact.ChildSocket==TEXT("support_bottom"); }));
    }
    if (Ridge)
    {
        TestTrue(TEXT("Ridge retains its source roof datum above Z=1.22m"),FMath::IsNearlyEqual(Ridge->BoundsMin.Z,1.22,.0001)
            && FMath::IsNearlyEqual(Ridge->BoundsMax.Z,1.409,.0001));
        TestTrue(TEXT("Ridge declares canopy support"),Ridge->SupportContacts.ContainsByPredicate([](const FHearthStructureSupportContact& Contact)
        { return Contact.ParentCatalogId==TEXT("canopy_terracotta_2m") && Contact.ChildSocket==TEXT("support_bottom"); }));
    }
    // Dimensions alone cannot detect an incorrectly recentered catalog. Check
    // both native mesh corners, including the asymmetric canopy and raised ridge.
    const TCHAR* MeasuredAssets[]={TEXT("canopy_terracotta_2m"),TEXT("canopy_slateblue_2m"),TEXT("roof_ridge_terracotta_2m"),TEXT("roof_ridge_slateblue_2m"),TEXT("roof_ridge_timber_2m"),TEXT("bench_timber")};
    for (const TCHAR* AssetId:MeasuredAssets)
    {
        const auto* Entry=HearthStructureCatalog::Find(AssetId);
        UStaticMesh* Mesh=Entry?LoadObject<UStaticMesh>(nullptr,*Entry->AssetPath):nullptr;
        if (!TestNotNull(FString::Printf(TEXT("Measured native mesh: %s"),AssetId),Mesh)) continue;
        const FBox Bounds=Mesh->GetBoundingBox();
        TestTrue(FString::Printf(TEXT("Catalog min matches native mesh cm: %s actual=%s expected=%s"),AssetId,
            *Bounds.Min.ToString(),*(Entry->BoundsMin*100.f).ToString()),Bounds.Min.Equals(Entry->BoundsMin*100.f,.01f));
        TestTrue(FString::Printf(TEXT("Catalog max matches native mesh cm: %s actual=%s expected=%s"),AssetId,
            *Bounds.Max.ToString(),*(Entry->BoundsMax*100.f).ToString()),Bounds.Max.Equals(Entry->BoundsMax*100.f,.01f));
    }
    if (Bench)
        TestTrue(TEXT("Bench declares a usable seat socket and floor support"),Bench->Sockets.ContainsByPredicate([](const FHearthStructureCatalogSocket& Socket){return Socket.Role==TEXT("usable_seat");})
            && Bench->SupportContacts.ContainsByPredicate([](const FHearthStructureSupportContact& Contact){return Contact.ParentCatalogId==TEXT("floor_timber_2m");}));
    return true;
}
#endif
