#pragma once

#include "CoreMinimal.h"
#include "HearthStructurePlan.generated.h"

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureMaterialQuantity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MaterialId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureMaterialRecipe
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RecipeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CatalogId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureMaterialQuantity> Inputs;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureFootprint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Size = FVector2D(100.f, 100.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Origin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Orientation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureReasonFields
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Need;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Occupation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Budget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Relationship;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoadAccess;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureRoom
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> OpeningIds;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureOpening
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RoomId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Offset = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D AccessDirection = FVector2D(1.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Width = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDoor = false;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureComponent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CatalogId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ExtensionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Height = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Orientation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Size = FVector2D(100.f, 20.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsMin = FVector(-50.f, -10.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsMax = FVector(50.f, 10.f, 100.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaterialCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RecipeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureMaterialQuantity> Materials;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CollisionRadius = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresSupport = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SupportsComponentId;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ParentComponentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CatalogId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Socket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Height = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Orientation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureConnection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString FromComponentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToComponentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString FromSocket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToSocket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLoadBearing = false;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureOccupiedVolume
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Center = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Z = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Height = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString OwnerId;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructurePlan
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlanId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StableSeed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FHearthStructureFootprint Footprint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FHearthStructureReasonFields Reasons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureRoom> Rooms;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureOpening> Openings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureComponent> Components;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureAttachment> Attachments;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureConnection> Connections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureMaterialRecipe> MaterialRecipes;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureComponentSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CatalogId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SemanticKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Height = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Orientation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Size = FVector2D(100.f, 20.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsMin = FVector(-50.f, -10.f, 0.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BoundsMax = FVector(50.f, 10.f, 100.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaterialCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RecipeId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureMaterialQuantity> Materials;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CollisionRadius = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresSupport = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SupportsComponentKey;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureValidationContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvailableBudget = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRoadAccessible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureOccupiedVolume> Occupied;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthStructureMaterialQuantity> AvailableMaterials;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthStructureValidationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bValid = true;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Issues;
};

namespace HearthStructurePlan
{
    THREEHEARTHS_API FHearthStructurePlan MakePlan(const FString& PlanId, const FString& StableSeed,
        const FHearthStructureFootprint& Footprint, const FHearthStructureReasonFields& Reasons);
    THREEHEARTHS_API bool RegisterRecipe(FHearthStructurePlan& Plan, const FHearthStructureMaterialRecipe& Recipe);
    THREEHEARTHS_API FString StableId(const FHearthStructurePlan& Plan, const FString& Namespace, const FString& Key);
    THREEHEARTHS_API bool AppendComponent(FHearthStructurePlan& Plan, const FHearthStructureComponentSpec& Spec,
        const FString& ExtensionId = FString());
    THREEHEARTHS_API bool AppendRoom(FHearthStructurePlan& Plan, const FString& Key, const FString& Label,
        const FString& ExtensionId = FString());
    THREEHEARTHS_API bool AppendOpening(FHearthStructurePlan& Plan, const FString& Key, const FString& RoomKey,
        const FHearthStructureOpening& Opening, const FString& ExtensionId = FString());
    THREEHEARTHS_API bool AppendConnection(FHearthStructurePlan& Plan, const FString& Key,
        const FString& FromComponentKey, const FString& ToComponentKey, bool bLoadBearing,
        const FString& ExtensionId = FString());
    THREEHEARTHS_API bool AppendExtension(FHearthStructurePlan& Plan, const FString& ExtensionKey,
        TConstArrayView<FHearthStructureComponentSpec> Components);
    THREEHEARTHS_API FHearthStructureValidationResult Validate(const FHearthStructurePlan& Plan,
        const FHearthStructureValidationContext& Context);
}
