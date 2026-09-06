#pragma once

#include "CoreMinimal.h"

/** Authoritative metadata for independently imported Stage4/VillageKit pieces.
 * Bounds and socket positions use the authoring convention in metres (+Z up, -Y front).
 * TownKit/offline GLBs are intentionally absent until they have native imports.
 */
struct THREEHEARTHS_API FHearthStructureCatalogSocket
{
    FString Id;
    FVector LocalPosition = FVector::ZeroVector;
    FString Role;
};

struct THREEHEARTHS_API FHearthStructureSupportContact
{
    FString ParentCatalogId;
    FString ParentSocket;
    FString ChildSocket;
};

struct THREEHEARTHS_API FHearthStructureCatalogEntry
{
    FString CatalogId;
    FString AssetPath;
    FVector BoundsMin = FVector::ZeroVector;
    FVector BoundsMax = FVector::ZeroVector;
    FString OriginConvention;
    FRotator DefaultRotation = FRotator::ZeroRotator;
    TArray<FHearthStructureCatalogSocket> Sockets;
    TArray<FHearthStructureSupportContact> SupportContacts;
    bool bHasDoorClearance = false;
    FVector2D DoorClearanceMin = FVector2D::ZeroVector;
    FVector2D DoorClearanceMax = FVector2D::ZeroVector;
};

namespace HearthStructureCatalog
{
    THREEHEARTHS_API const TArray<FHearthStructureCatalogEntry>& Entries();
    THREEHEARTHS_API const FHearthStructureCatalogEntry* Find(const FString& CatalogId);
    THREEHEARTHS_API bool Validate(FString* OutError = nullptr);
    THREEHEARTHS_API bool HasFoundationToRoofSupportChain(FString* OutError = nullptr);
}
