#pragma once

#include "CoreMinimal.h"

/** Source-space metres mapped into Unreal centimetres by the catalog matrix. */
struct THREEHEARTHS_API FHearthOrganicSourceMatrix
{
    float Values[3][3] = {{100.f, 0.f, 0.f}, {0.f, 100.f, 0.f}, {0.f, 0.f, 100.f}};

    FVector TransformMeters(const FVector& SourceMeters) const;
    FVector TransformDirection(const FVector& SourceDirection) const;
    /** Relative yaw: mapped(source yaw basis) minus mapped(source +X basis). */
    float TransformYawDegrees(float SourceYawDegrees) const;
};

struct THREEHEARTHS_API FHearthOrganicAsset
{
    FString ModuleId;
    FString Palette;
    FString Layer;
    FString Mesh;
    FVector BoundsMinCm = FVector::ZeroVector;
    FVector BoundsMaxCm = FVector::ZeroVector;
};

struct THREEHEARTHS_API FHearthOrganicPiece
{
    FString OriginalKey;
    FString ModuleId;
    FString Palette;
    FString Purpose;
    TArray<FString> Layers;
    FVector SourceTranslationM = FVector::ZeroVector;
    FVector TranslationCm = FVector::ZeroVector;
    float SourceYawDegrees = 0.f;
    float UnrealYawDegrees = 0.f;
};

struct THREEHEARTHS_API FHearthOrganicOccupiedCell
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    float Zm = 0.f;
    float WallHeightM = 0.f;
};

struct THREEHEARTHS_API FHearthOrganicRecipe
{
    FString Id;
    FString Label;
    FString Palette;
    FString Kind;
    bool bGrowth = false;
    TArray<FHearthOrganicPiece> Pieces;
    TArray<FHearthOrganicOccupiedCell> OccupiedCells;
    FVector SourceBoundsMinM = FVector::ZeroVector;
    FVector SourceBoundsMaxM = FVector::ZeroVector;
    FVector BoundsMinCm = FVector::ZeroVector;
    FVector BoundsMaxCm = FVector::ZeroVector;
};

struct THREEHEARTHS_API FHearthOrganicGrowthTransition
{
    FString From;
    FString To;
    TArray<FString> RetainKeys;
    TArray<FString> DismantleKeys;
    TArray<FString> AddKeys;
};

/** Growth is an authored sequence, not a sixth renderable building recipe. */
struct THREEHEARTHS_API FHearthOrganicGrowth
{
    FString Id;
    FString Source;
    TArray<FString> Stages;
    TArray<FString> Rules;
    TArray<FHearthOrganicGrowthTransition> Transitions;
    FString RawJson;
};

struct THREEHEARTHS_API FHearthOrganicCatalog
{
    int32 SchemaVersion = 0;
    FHearthOrganicSourceMatrix SourceToUnrealMatrix;
    TArray<FHearthOrganicAsset> Assets;
    TArray<FHearthOrganicRecipe> Recipes;
    FHearthOrganicGrowth Growth;
    bool bHasGrowth = false;
};

namespace HearthOrganicCatalog
{
    /** Parse and validate a complete schema_version=1 manifest. */
    THREEHEARTHS_API bool LoadFromJson(const FString& JsonText, FHearthOrganicCatalog& OutCatalog, FString& OutError);

    /** Load Content/ThreeHearths/Data/OrganicMasterCatalog.json. */
    THREEHEARTHS_API bool Load(FHearthOrganicCatalog& OutCatalog, FString& OutError);

    THREEHEARTHS_API FString DefaultPath();
    THREEHEARTHS_API const FHearthOrganicRecipe* FindRecipe(const FHearthOrganicCatalog& Catalog, const FString& RecipeId);
    THREEHEARTHS_API const FHearthOrganicGrowth* FindGrowth(const FHearthOrganicCatalog& Catalog);
    THREEHEARTHS_API const FHearthOrganicAsset* FindAsset(const FHearthOrganicCatalog& Catalog, const FString& ModuleId,
        const FString& Palette, const FString& Layer);

    /** Return the manifest mesh path for one retained material layer. */
    THREEHEARTHS_API FString ResolveLayerPath(const FHearthOrganicCatalog& Catalog, const FString& ModuleId,
        const FString& Palette, const FString& Layer);
}
