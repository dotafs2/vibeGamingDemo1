#pragma once

#include "CoreMinimal.h"

/** The four material pools used by an atomic organic construction transaction. */
struct THREEHEARTHS_API FOrganicConstructionStock
{
    int32 Stone = 0;
    int32 Planks = 0;
    int32 Beams = 0;
    int32 Tiles = 0;

    bool IsNonNegative() const;
    int32 Total() const;
};

/** A single independently installable/removable construction module. */
struct THREEHEARTHS_API FOrganicConstructionPiece
{
    FString Key;
    FString Module;
    FOrganicConstructionStock Cost;
};

/** The key-level difference between an installed set and a target composition. */
struct THREEHEARTHS_API FOrganicConstructionDelta
{
    TArray<FString> KeepKeys;
    TArray<FString> RemoveKeys;
    TArray<FOrganicConstructionPiece> AddPieces;
};

/** Accounting returned by a successful or rejected InstallAtomic call. */
struct THREEHEARTHS_API FOrganicConstructionInstallResult
{
    bool bSuccess = false;
    FString FailureReason;
    FOrganicConstructionStock ReclaimedConsumed;
    FOrganicConstructionStock PublicConsumed;
    int32 CoinsPaid = 0;
};

/** Accounting returned by a successful or rejected Dismantle call. */
struct THREEHEARTHS_API FOrganicConstructionDismantleResult
{
    bool bSuccess = false;
    FString FailureReason;
    FOrganicConstructionStock ReclaimedReturned;
};

/** One persisted MarketLifeKit purchase/install, kept outside the house shell keys. */
struct THREEHEARTHS_API FOrganicMarketKitRecord
{
    FString RequestId;
    /** Runtime purchase task GUID; RequestId may be a human request key. */
    FString TaskId;
    FString ModuleId;
    FString Status = TEXT("pending");
    FOrganicConstructionStock Cost;
    /** Worker standing point; installation is kept separately to avoid spawning into the worker. */
    FVector Anchor = FVector::ZeroVector;
    FVector InstallPosition = FVector::ZeroVector;
    float InstallYaw = 0.f;
    float WorkProgress = 0.f;
    int32 Revision = 0;
};

/** Per-home construction ledger. Reclaimed is private to this home. */
struct THREEHEARTHS_API FOrganicConstructionHomeState
{
    TArray<FString> InstalledKeys;
    /** Original completed recipe cost by key, retained so changed target specs cannot alter refunds. */
    TMap<FString, FOrganicConstructionStock> InstalledCosts;
    FOrganicConstructionStock Reclaimed;
    FString ActivePieceKey;
    float WorkProgress = 0.f;
    FString CurrentRecipe;
    FString TargetRecipe;
    FString ChoiceReason;
    FString Source;
    FString Seed;
    int32 Revision = 0;
    /** Independent life-kit ledger; these keys never participate in shell PlanDelta. */
    TArray<FOrganicMarketKitRecord> MarketKitInstalled;
    FOrganicMarketKitRecord MarketKitPending;
    bool bHasMarketKitPending = false;
};

namespace HearthOrganicConstruction
{
    constexpr int32 MaxMarketKitsPerHome = 16;
    THREEHEARTHS_API bool Validate(const FOrganicConstructionStock& Stock, FString* OutError = nullptr);
    THREEHEARTHS_API bool Validate(const FOrganicConstructionPiece& Piece, FString* OutError = nullptr);
    THREEHEARTHS_API bool Validate(const FOrganicMarketKitRecord& Record, bool bPending, FString* OutError = nullptr);
    THREEHEARTHS_API bool Validate(const FOrganicConstructionHomeState& Home, FString* OutError = nullptr);

    /** Return the only supported construction/finish recipe for a MarketLifeKit module. */
    THREEHEARTHS_API bool MarketKitCost(const FString& ModuleId, FOrganicConstructionStock& OutCost);

    THREEHEARTHS_API bool PlanDelta(const TArray<FString>& CurrentInstalledKeys,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out);
    THREEHEARTHS_API bool PlanDelta(const TArray<FOrganicConstructionPiece>& CurrentInstalledPieces,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out);
    THREEHEARTHS_API bool PlanDelta(const FOrganicConstructionHomeState& Current,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out);

    /**
     * Complete one piece atomically. Public material is charged at one coin per
     * unit after this home's reclaimed material has been consumed.
     */
    THREEHEARTHS_API bool InstallAtomic(const FOrganicConstructionPiece& Piece,
        FOrganicConstructionHomeState& Home, FOrganicConstructionStock& PublicStock,
        int32& OwnerCoins, int32& Treasury, FOrganicConstructionInstallResult& Out);

    /** Remove one installed key and return the complete recipe cost privately to the home. */
    THREEHEARTHS_API bool Dismantle(const FOrganicConstructionPiece& Piece,
        FOrganicConstructionHomeState& Home, FOrganicConstructionDismantleResult& Out);
}
