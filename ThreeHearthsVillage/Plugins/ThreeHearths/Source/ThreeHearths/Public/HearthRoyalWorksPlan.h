#pragma once

#include "CoreMinimal.h"
#include "HearthCastleRooms.h"

// Pure local manifest; no asset loads, world access or construction state.
struct THREEHEARTHS_API FHearthRoyalModule
{
    FString Id;
    FString MeshPath;
    FString PlantId;
    // v1: original asset origin. v2 meshes: actual bounds centre in cm.
    // Plants always use their catalog root on the ground.
    FVector Offset = FVector::ZeroVector;
    // Signed native scale: v2 VillageKit local Y is reflected from the UE
    // import frame to the authored assembly frame. World/site axes stay normal.
    // Abs(Scale) multiplies dimensions, including native floor thickness.
    FVector Scale = FVector::OneVector;
    bool bCenterMeshAtOffset = false;
    // Audit dimensions derived from source bounds and Scale. Rendering uses
    // loaded UStaticMesh bounds; this value is never a substitute for them.
    FVector BoundsSizeCm = FVector::ZeroVector;
    float Yaw = 0.f;
    // A dependency barrier: complete every earlier stage before starting this one.
    int32 Stage = 1;
    // X = stone, Y = planks, Z = beams; non-negative abstract inventory shares.
    // Garden bills pay for edging/stakes/bed boards.
    FIntVector Materials = FIntVector::ZeroValue;
    FLinearColor Color = FLinearColor::White;
};

struct THREEHEARTHS_API FHearthRoyalWorksPlan
{
    FString TemplateId;
    // Contains full horizontal mesh bounds and scaled plant crowns, in cm.
    float Radius = 850.f;
    TArray<FHearthRoyalModule> Modules;
    // Pure room index for structural review and future WorldState schema work.
    // Public construction still persists Modules through FHearthPublicPart.
    TArray<FHearthCastleRoom> Rooms;
};

namespace HearthRoyalWorksPlan
{
    // royal_keep_garden_v1: 44 modules, (73,46,40) materials, 13 dependency stages.
    // Foundations -> solid keep -> gate/curtain -> attached wings -> garden.
    // v2 canonical counts/bills come from Modules; the tests audit each asset
    // category and reject duplicate placements, rather than copying totals.
    THREEHEARTHS_API FHearthRoyalWorksPlan Build();

    // Build a canonical public-work manifest by template. Build() remains the
    // legacy v1 entry point so old worlds and their exact recipe are stable.
    THREEHEARTHS_API FHearthRoyalWorksPlan BuildForTemplate(const FString& TemplateId);

    // WorldOffset is site position + persisted part offset. This handles
    // bottom-origin walls and asymmetric/rebased roof and stair imports.
    THREEHEARTHS_API FTransform MeshTransform(const FHearthRoyalModule& Module,
        const FBox& MeshLocalBounds, const FVector& WorldOffset);

}
