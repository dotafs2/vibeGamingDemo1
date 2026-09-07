#if WITH_DEV_AUTOMATION_TESTS
#include "HearthBotanicalCatalog.h"

#include "Misc/AutomationTest.h"

namespace
{
    bool IsFiniteColor(const FLinearColor& Color)
    {
        return FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) && FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A);
    }

    FString ContourKey(const TArray<FHearthPlantPart>& Parts)
    {
        FString Key;
        for (const FHearthPlantPart& Part : Parts)
        {
            Key += FString::Printf(TEXT("%s:%.0f,%.0f,%.0f:%.2f,%.2f,%.2f;"), *Part.MeshPath, Part.Offset.X, Part.Offset.Y, Part.Offset.Z, Part.Scale.X, Part.Scale.Y, Part.Scale.Z);
        }
        return Key;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthBotanicalCatalogTest, "ThreeHearths.BotanicalCatalog.NativeLowPolyPlants", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthBotanicalCatalogTest::RunTest(const FString&)
{
    const TArray<FString> CatalogSpecies = HearthBotanicalCatalog::Species();
    TestEqual(TEXT("Six plant species are catalogued"), CatalogSpecies.Num(), 6);

    TSet<FString> Contours;
    for (const FString& Species : CatalogSpecies)
    {
        const TArray<FHearthPlantPart> Parts = HearthBotanicalCatalog::Build(Species, 37);
        TestTrue(FString::Printf(TEXT("%s has parts"), *Species), Parts.Num() > 0);
        TestTrue(FString::Printf(TEXT("%s stays within the 16 mesh part budget"), *Species), Parts.Num() <= 16);
        TestTrue(FString::Printf(TEXT("%s has a positive placement radius"), *Species), HearthBotanicalCatalog::Radius(Species) > 0.0f);
        Contours.Add(ContourKey(Parts));

        int32 EstimatedCost = 0;
        for (const FHearthPlantPart& Part : Parts)
        {
            const bool bAllowedMesh = Part.MeshPath.StartsWith(TEXT("/Engine/BasicShapes/")) || Part.MeshPath.StartsWith(TEXT("/Game/Environment/Meshes/Foliage/"));
            TestTrue(FString::Printf(TEXT("%s uses an approved mesh path"), *Species), bAllowedMesh);
            TestTrue(FString::Printf(TEXT("%s keeps each part above ground"), *Species), Part.Offset.Z >= 0.0f && Part.Offset.Z <= 650.0f);
            TestTrue(FString::Printf(TEXT("%s has finite transforms and color"), *Species),
                FMath::IsFinite(Part.Offset.X) && FMath::IsFinite(Part.Offset.Y) && FMath::IsFinite(Part.Offset.Z) &&
                FMath::IsFinite(Part.Scale.X) && FMath::IsFinite(Part.Scale.Y) && FMath::IsFinite(Part.Scale.Z) &&
                FMath::IsFinite(Part.Yaw) && IsFiniteColor(Part.Color));
            TestTrue(FString::Printf(TEXT("%s has positive scale"), *Species), Part.Scale.X > 0.0f && Part.Scale.Y > 0.0f && Part.Scale.Z > 0.0f);
            EstimatedCost += Part.MeshPath.StartsWith(TEXT("/Game/Environment/Meshes/Foliage/")) ? 2 : 1;
        }

        TestTrue(FString::Printf(TEXT("%s stays within the assembly cost budget"), *Species), EstimatedCost <= 20);
    }

    TestEqual(TEXT("Each species has a distinct low-poly contour"), Contours.Num(), CatalogSpecies.Num());
    TestTrue(TEXT("Unknown species produces no parts"), HearthBotanicalCatalog::Build(TEXT("unknown"), 37).Num() == 0);
    TestTrue(TEXT("Unknown species has no placement radius"), HearthBotanicalCatalog::Radius(TEXT("unknown")) == 0.0f);

    const TArray<FHearthPlantPart> FirstBuild = HearthBotanicalCatalog::Build(TEXT("orchard"), 91);
    const TArray<FHearthPlantPart> RepeatBuild = HearthBotanicalCatalog::Build(TEXT("orchard"), 91);
    TestEqual(TEXT("The same seed is deterministic"), FirstBuild.Num(), RepeatBuild.Num());
    for (int32 Index = 0; Index < FirstBuild.Num() && Index < RepeatBuild.Num(); ++Index)
    {
        TestTrue(TEXT("Deterministic part transform"), FirstBuild[Index].Offset.Equals(RepeatBuild[Index].Offset) && FirstBuild[Index].Scale.Equals(RepeatBuild[Index].Scale) && FMath::IsNearlyEqual(FirstBuild[Index].Yaw, RepeatBuild[Index].Yaw));
    }
    return true;
}
#endif
