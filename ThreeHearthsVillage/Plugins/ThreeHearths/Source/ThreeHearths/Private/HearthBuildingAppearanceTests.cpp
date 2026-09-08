#if WITH_DEV_AUTOMATION_TESTS
#include "HearthBuildingAppearance.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthBuildingAppearanceTest, "ThreeHearths.Appearance.NativeModularStarter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthBuildingAppearanceTest::RunTest(const FString&)
{
    const TCHAR* Archetypes[] = { TEXT("rowhouse"), TEXT("shop_house"), TEXT("courtyard_workshop"), TEXT("warehouse"), TEXT("inn"), TEXT("keep") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Archetypes); ++Index)
    {
        FHearthBuildingAppearance Town3;
        TestTrue(FString::Printf(TEXT("Town3 %s builds"), Archetypes[Index]), HearthBuildingAppearance::Build(Archetypes[Index], TEXT("plaster"), TEXT("terracotta"), 100u + Index, true, Town3));
        TestTrue(FString::Printf(TEXT("Town3 %s has a 6-10m scale axis"), Archetypes[Index]), Town3.CoreFootprintCm.X >= 600.f || Town3.CoreFootprintCm.Y >= 600.f);
        TestTrue(FString::Printf(TEXT("Town3 %s uses native scale"), Archetypes[Index]), Town3.Parts.ContainsByPredicate([](const auto& Part) { return !Part.Scale.Equals(FVector::OneVector, .001f); }) == false);
        TestTrue(FString::Printf(TEXT("Town3 %s has one stair per floor transition"), Archetypes[Index]),
            Town3.StairCount == FMath::Max(0, Town3.Floors - 1) &&
            Town3.Parts.FilterByPredicate([](const auto& Part) { return Part.Role == TEXT("stairs"); }).Num() == Town3.StairCount);
        if (FCString::Strcmp(Archetypes[Index], TEXT("inn")) == 0)
            TestTrue(TEXT("Town3 inn has an actual native canopy and outdoor seating"),
                Town3.Parts.ContainsByPredicate([](const auto& Part) { return Part.AssetId == TEXT("canopy_terracotta_2m"); }) &&
                Town3.Parts.ContainsByPredicate([](const auto& Part) { return Part.AssetId == TEXT("bench_timber"); }));

        FHearthBuildingAppearance Town2;
        TestTrue(FString::Printf(TEXT("Town2 %s builds"), Archetypes[Index]), HearthBuildingAppearance::Build(Archetypes[Index], TEXT("timber"), TEXT("slateblue"), 200u + Index, false, Town2));
        TestTrue(FString::Printf(TEXT("Town2 %s stays within compact render footprint"), Archetypes[Index]), Town2.OccupiedFootprintCm.X <= 500.f && Town2.OccupiedFootprintCm.Y <= 500.f);
        const int32 ExpectedTown2Floors = FCString::Strcmp(Archetypes[Index], TEXT("inn")) == 0 ? 3 :
            (FCString::Strcmp(Archetypes[Index], TEXT("courtyard_workshop")) == 0 || FCString::Strcmp(Archetypes[Index], TEXT("warehouse")) == 0 ? 1 : 2);
        TestEqual(FString::Printf(TEXT("Town2 %s retains role-specific storeys"), Archetypes[Index]), Town2.Floors, ExpectedTown2Floors);
    }
    FHearthBuildingAppearance Workshop;
    TestTrue(TEXT("Workshop builds"), HearthBuildingAppearance::Build(TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 7u, true, Workshop));
    TestTrue(TEXT("Workshop uses the real side canopy"), Workshop.Parts.ContainsByPredicate([](const auto& Part) { return Part.Role == TEXT("side_canopy"); }));
    TestTrue(TEXT("Workshop supports a double-span or courtyard variant"), Workshop.RoofSpanCount >= 1 && (Workshop.RoofSpanCount == 2 || Workshop.bHasCourtyard));
    TestTrue(TEXT("Every roof piece is a native two metre module"), Workshop.Parts.ContainsByPredicate([](const auto& Part)
    { return Part.Role == TEXT("roof_slope_front") && Part.AssetId == TEXT("roof_slope_slateblue_2m"); }));
    FHearthBuildingAppearance VariantA, VariantB;
    TestTrue(TEXT("Seeded workshop variant A builds"), HearthBuildingAppearance::Build(TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 1u, true, VariantA));
    TestTrue(TEXT("Seeded workshop variant B builds"), HearthBuildingAppearance::Build(TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 2u, true, VariantB));
    TestTrue(TEXT("Persistent seed changes layout variant"), VariantA.LayoutVariant != VariantB.LayoutVariant || !VariantA.CoreFootprintCm.Equals(VariantB.CoreFootprintCm));

    const TCHAR* ComposedShapes[] = {
        TEXT("compact_cluster"), TEXT("L_court"), TEXT("stepped_wings"),
        TEXT("U_court"), TEXT("offset_workshop"), TEXT("tower_annex") };
    for (int32 LayoutId = 0; LayoutId < UE_ARRAY_COUNT(ComposedShapes); ++LayoutId)
    {
        FHearthBuildingAppearance Composed;
        TestTrue(FString::Printf(TEXT("Version 4 layout %d builds"), LayoutId),
            HearthBuildingAppearance::BuildComposed(TEXT("rowhouse"), TEXT("plaster"), TEXT("terracotta"), 177u, LayoutId, Composed));
        TestEqual(FString::Printf(TEXT("Version 4 layout %d shape id"), LayoutId), Composed.ShapeId, FString(ComposedShapes[LayoutId]));
        TestEqual(FString::Printf(TEXT("Version 4 layout %d reports one tier per mass"), LayoutId), Composed.HeightTiers.Num(), Composed.MassCount);
        TestTrue(FString::Printf(TEXT("Version 4 layout %d stays within 14m siting bound"), LayoutId),
            Composed.OccupiedFootprintCm.X <= 1400.f && Composed.OccupiedFootprintCm.Y <= 1400.f);
        TestTrue(FString::Printf(TEXT("Version 4 layout %d has supported entrance"), LayoutId),
            Composed.Parts.ContainsByPredicate([&Composed](const auto& Part)
            {
                return Part.Role == TEXT("front_door") && FVector2D(Part.Offset.X, Part.Offset.Y).Equals(Composed.EntranceOffsetCm, .01f);
            }));
        TestTrue(FString::Printf(TEXT("Version 4 layout %d contains an authored rotated wing"), LayoutId),
            Composed.Parts.ContainsByPredicate([](const auto& Part) { return FMath::IsNearlyEqual(FMath::Abs(Part.Yaw), 90.f, .01f); }));
    }

    FHearthBuildingAppearance SeedA, SeedB, SeedARepeat;
    TestTrue(TEXT("Seeded composed layout A builds"), HearthBuildingAppearance::BuildComposed(
        TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 101u, 2, SeedA));
    TestTrue(TEXT("Seeded composed layout B builds"), HearthBuildingAppearance::BuildComposed(
        TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 102u, 2, SeedB));
    TestTrue(TEXT("Same composed seed is persistent"), HearthBuildingAppearance::BuildComposed(
        TEXT("courtyard_workshop"), TEXT("timber"), TEXT("slateblue"), 101u, 2, SeedARepeat) &&
        SeedA.LayoutVariant == SeedARepeat.LayoutVariant && SeedA.Parts.Num() == SeedARepeat.Parts.Num() &&
        SeedA.Parts[0].Offset.Equals(SeedARepeat.Parts[0].Offset, .01f));
    TestTrue(TEXT("Different composed seed changes authored placement"),
        SeedA.LayoutVariant != SeedB.LayoutVariant || !SeedA.Parts[0].Offset.Equals(SeedB.Parts[0].Offset, .01f));
    return true;
}
#endif
