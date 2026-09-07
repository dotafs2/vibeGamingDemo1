#include "HearthBotanicalCatalog.h"

namespace
{
    const TCHAR* CylinderMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
    const TCHAR* ConeMesh = TEXT("/Engine/BasicShapes/Cone.Cone");
    const TCHAR* SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* Grass01Mesh = TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_01.SM_GrassClump_01");
    const TCHAR* Grass02Mesh = TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_02.SM_GrassClump_02");
    const TCHAR* Grass03Mesh = TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_03.SM_GrassClump_03");
    const TCHAR* Grass04Mesh = TEXT("/Game/Environment/Meshes/Foliage/SM_GrassClump_04.SM_GrassClump_04");

    const FLinearColor OakGreen(0.20f, 0.42f, 0.12f, 1.0f);
    const FLinearColor BirchGreen(0.32f, 0.55f, 0.18f, 1.0f);
    const FLinearColor OrchardGreen(0.24f, 0.48f, 0.11f, 1.0f);
    const FLinearColor CypressGreen(0.10f, 0.30f, 0.18f, 1.0f);
    const FLinearColor ShrubGreen(0.28f, 0.50f, 0.16f, 1.0f);
    const FLinearColor GrassGreen(0.25f, 0.52f, 0.10f, 1.0f);
    const FLinearColor BarkBrown(0.28f, 0.13f, 0.06f, 1.0f);
    const FLinearColor BirchBark(0.72f, 0.68f, 0.52f, 1.0f);
    const FLinearColor BlossomPink(0.92f, 0.32f, 0.46f, 1.0f);
    const FLinearColor BlossomYellow(0.98f, 0.72f, 0.12f, 1.0f);
    const FLinearColor FruitRed(0.78f, 0.10f, 0.04f, 1.0f);

    FString Normalize(const FString& SpeciesName)
    {
        return SpeciesName.TrimStartAndEnd().ToLower();
    }

    FRandomStream MakeStream(const FString& Key, int32 Seed)
    {
        return FRandomStream(Seed ^ static_cast<int32>(GetTypeHash(Key)));
    }

    void AddPart(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream, const TCHAR* MeshPath, const FVector& Offset, const FVector& Scale, const FLinearColor& Color, float ScaleJitter = 0.025f, float BaseYaw = 0.0f)
    {
        FHearthPlantPart Part;
        Part.MeshPath = MeshPath;
        Part.Offset = Offset;
        Part.Scale = FVector(
            Scale.X * Stream.FRandRange(1.0f - ScaleJitter, 1.0f + ScaleJitter),
            Scale.Y * Stream.FRandRange(1.0f - ScaleJitter, 1.0f + ScaleJitter),
            Scale.Z);
        Part.Yaw = BaseYaw + Stream.FRandRange(-4.0f, 4.0f);
        Part.Color = Color;
        Parts.Add(MoveTemp(Part));
    }

    void BuildOak(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 175.0f), FVector(0.65f, 0.65f, 3.5f), BarkBrown, 0.02f);
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 300.0f), FVector(0.40f, 0.40f, 1.4f), BarkBrown, 0.035f, -12.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 0.0f, 410.0f), FVector(2.0f, 1.75f, 1.5f), OakGreen, 0.05f);
        AddPart(Parts, Stream, SphereMesh, FVector(-108.0f, 12.0f, 430.0f), FVector(1.30f, 1.15f, 1.25f), OakGreen, 0.06f, -14.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(112.0f, -8.0f, 425.0f), FVector(1.32f, 1.18f, 1.28f), OakGreen, 0.06f, 18.0f);
    }

    void BuildBirch(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 225.0f), FVector(0.34f, 0.34f, 4.5f), BirchBark, 0.02f);
        AddPart(Parts, Stream, CylinderMesh, FVector(62.0f, -6.0f, 212.5f), FVector(0.25f, 0.25f, 4.25f), BirchBark, 0.03f, -20.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 0.0f, 495.0f), FVector(1.18f, 1.00f, 1.30f), BirchGreen, 0.05f);
        AddPart(Parts, Stream, SphereMesh, FVector(-52.0f, 0.0f, 445.0f), FVector(0.88f, 0.78f, 0.98f), BirchGreen, 0.06f, -12.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(62.0f, -4.0f, 455.0f), FVector(0.82f, 0.74f, 0.92f), BirchGreen, 0.06f, 17.0f);
    }

    void BuildOrchard(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 140.0f), FVector(0.46f, 0.46f, 2.8f), BarkBrown, 0.025f);
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 250.0f), FVector(0.24f, 0.24f, 1.25f), BarkBrown, 0.04f, 25.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(-78.0f, 0.0f, 350.0f), FVector(1.10f, 1.00f, 1.04f), OrchardGreen, 0.05f, -18.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(78.0f, 0.0f, 350.0f), FVector(1.08f, 1.02f, 1.02f), OrchardGreen, 0.05f, 18.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 76.0f, 382.0f), FVector(1.08f, 1.02f, 1.12f), OrchardGreen, 0.05f, 8.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, -72.0f, 378.0f), FVector(1.02f, 0.98f, 1.08f), OrchardGreen, 0.05f, -8.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(-86.0f, -18.0f, 385.0f), FVector(0.20f, 0.20f, 0.20f), FruitRed, 0.08f);
        AddPart(Parts, Stream, SphereMesh, FVector(68.0f, 38.0f, 405.0f), FVector(0.19f, 0.19f, 0.19f), FruitRed, 0.08f);
        AddPart(Parts, Stream, SphereMesh, FVector(16.0f, -78.0f, 400.0f), FVector(0.18f, 0.18f, 0.18f), FruitRed, 0.08f);
    }

    void BuildCypress(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 200.0f), FVector(0.35f, 0.35f, 4.0f), BarkBrown, 0.02f);
        AddPart(Parts, Stream, ConeMesh, FVector(0.0f, 0.0f, 285.0f), FVector(1.45f, 1.45f, 5.3f), CypressGreen, 0.04f);
        AddPart(Parts, Stream, ConeMesh, FVector(0.0f, 0.0f, 430.0f), FVector(0.92f, 0.92f, 2.5f), CypressGreen, 0.05f, 7.0f);
    }

    void BuildFloweringShrub(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, CylinderMesh, FVector(0.0f, 0.0f, 65.0f), FVector(0.22f, 0.22f, 1.3f), BarkBrown, 0.04f, -25.0f);
        AddPart(Parts, Stream, CylinderMesh, FVector(52.0f, 0.0f, 62.0f), FVector(0.18f, 0.18f, 1.24f), BarkBrown, 0.04f, 28.0f);
        AddPart(Parts, Stream, CylinderMesh, FVector(-52.0f, 8.0f, 60.0f), FVector(0.18f, 0.18f, 1.20f), BarkBrown, 0.04f, -34.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 0.0f, 145.0f), FVector(0.90f, 0.78f, 0.80f), ShrubGreen, 0.06f);
        AddPart(Parts, Stream, SphereMesh, FVector(78.0f, 0.0f, 138.0f), FVector(0.68f, 0.62f, 0.70f), ShrubGreen, 0.06f, 14.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(-76.0f, 10.0f, 135.0f), FVector(0.70f, 0.64f, 0.68f), ShrubGreen, 0.06f, -16.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 62.0f, 158.0f), FVector(0.62f, 0.58f, 0.66f), ShrubGreen, 0.06f, 7.0f);
        AddPart(Parts, Stream, SphereMesh, FVector(-8.0f, 4.0f, 183.0f), FVector(0.20f, 0.20f, 0.20f), BlossomPink, 0.10f);
        AddPart(Parts, Stream, SphereMesh, FVector(78.0f, 0.0f, 173.0f), FVector(0.18f, 0.18f, 0.18f), BlossomYellow, 0.10f);
        AddPart(Parts, Stream, SphereMesh, FVector(-76.0f, 10.0f, 169.0f), FVector(0.18f, 0.18f, 0.18f), BlossomPink, 0.10f);
        AddPart(Parts, Stream, SphereMesh, FVector(0.0f, 62.0f, 191.0f), FVector(0.17f, 0.17f, 0.17f), BlossomYellow, 0.10f);
    }

    void BuildWildflowers(TArray<FHearthPlantPart>& Parts, FRandomStream& Stream)
    {
        AddPart(Parts, Stream, Grass01Mesh, FVector(-48.0f, -18.0f, 0.0f), FVector(0.35f, 0.35f, 0.35f), GrassGreen, 0.10f, -12.0f);
        AddPart(Parts, Stream, Grass02Mesh, FVector(18.0f, -42.0f, 0.0f), FVector(0.32f, 0.32f, 0.32f), GrassGreen, 0.10f, 8.0f);
        AddPart(Parts, Stream, Grass03Mesh, FVector(50.0f, 20.0f, 0.0f), FVector(0.34f, 0.34f, 0.34f), GrassGreen, 0.10f, 22.0f);
        AddPart(Parts, Stream, Grass04Mesh, FVector(-12.0f, 40.0f, 0.0f), FVector(0.30f, 0.30f, 0.30f), GrassGreen, 0.10f, -22.0f);
        AddPart(Parts, Stream, Grass01Mesh, FVector(-72.0f, 38.0f, 0.0f), FVector(0.27f, 0.27f, 0.27f), GrassGreen, 0.10f, 32.0f);
        AddPart(Parts, Stream, Grass03Mesh, FVector(78.0f, -32.0f, 0.0f), FVector(0.29f, 0.29f, 0.29f), GrassGreen, 0.10f, -30.0f);
        const FVector FlowerOffsets[] = { FVector(-48.0f, -18.0f, 62.0f), FVector(18.0f, -42.0f, 60.0f), FVector(50.0f, 20.0f, 62.0f), FVector(-12.0f, 40.0f, 58.0f) };
        const FLinearColor FlowerColors[] = { BlossomPink, BlossomYellow, BlossomPink, BlossomYellow };
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(FlowerOffsets); ++Index)
        {
            const FVector StemCenter(FlowerOffsets[Index].X, FlowerOffsets[Index].Y, FlowerOffsets[Index].Z / 2.0f);
            AddPart(Parts, Stream, CylinderMesh, StemCenter, FVector(0.06f, 0.06f, FlowerOffsets[Index].Z / 100.0f), GrassGreen, 0.08f);
            AddPart(Parts, Stream, SphereMesh, FlowerOffsets[Index], FVector(0.13f, 0.13f, 0.13f), FlowerColors[Index], 0.10f);
        }
    }
}

namespace HearthBotanicalCatalog
{
    TArray<FString> Species()
    {
        TArray<FString> Result;
        Result.Reserve(6);
        Result.Add(TEXT("oak"));
        Result.Add(TEXT("birch"));
        Result.Add(TEXT("orchard"));
        Result.Add(TEXT("cypress"));
        Result.Add(TEXT("flowering_shrub"));
        Result.Add(TEXT("wildflowers"));
        return Result;
    }

    TArray<FHearthPlantPart> Build(const FString& SpeciesName, int32 Seed)
    {
        const FString Key = Normalize(SpeciesName);
        FRandomStream Stream = MakeStream(Key, Seed);
        TArray<FHearthPlantPart> Result;
        Result.Reserve(16);

        if (Key == TEXT("oak")) BuildOak(Result, Stream);
        else if (Key == TEXT("birch")) BuildBirch(Result, Stream);
        else if (Key == TEXT("orchard")) BuildOrchard(Result, Stream);
        else if (Key == TEXT("cypress")) BuildCypress(Result, Stream);
        else if (Key == TEXT("flowering_shrub")) BuildFloweringShrub(Result, Stream);
        else if (Key == TEXT("wildflowers")) BuildWildflowers(Result, Stream);

        return Result;
    }

    float Radius(const FString& SpeciesName)
    {
        const FString Key = Normalize(SpeciesName);
        if (Key == TEXT("oak")) return 190.0f;
        if (Key == TEXT("birch")) return 135.0f;
        if (Key == TEXT("orchard")) return 165.0f;
        if (Key == TEXT("cypress")) return 100.0f;
        if (Key == TEXT("flowering_shrub")) return 135.0f;
        if (Key == TEXT("wildflowers")) return 90.0f;
        return 0.0f;
    }
}
