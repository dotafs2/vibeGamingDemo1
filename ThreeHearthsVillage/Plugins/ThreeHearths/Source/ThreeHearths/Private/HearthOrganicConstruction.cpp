#include "HearthOrganicConstruction.h"

bool FOrganicConstructionStock::IsNonNegative() const
{
    return Stone >= 0 && Planks >= 0 && Beams >= 0 && Tiles >= 0;
}

int32 FOrganicConstructionStock::Total() const
{
    return Stone + Planks + Beams + Tiles;
}

namespace HearthOrganicConstruction
{
    namespace
    {
        bool Fail(FString* OutError, const TCHAR* Error)
        {
            if (OutError) *OutError = Error;
            return false;
        }

        bool SameKeysAreUnique(const TArray<FString>& Keys, FString* OutError)
        {
            TSet<FString> Seen;
            for (const FString& Key : Keys)
            {
                if (Key.IsEmpty()) return Fail(OutError, TEXT("empty_installed_key"));
                if (Seen.Contains(Key)) return Fail(OutError, TEXT("duplicate_installed_key"));
                Seen.Add(Key);
            }
            return true;
        }

        bool AddWouldOverflow(const FOrganicConstructionStock& A, const FOrganicConstructionStock& B)
        {
            return int64(A.Stone) + B.Stone > MAX_int32 || int64(A.Planks) + B.Planks > MAX_int32
                || int64(A.Beams) + B.Beams > MAX_int32 || int64(A.Tiles) + B.Tiles > MAX_int32;
        }

        FOrganicConstructionStock MinStock(const FOrganicConstructionStock& Available,
            const FOrganicConstructionStock& Required)
        {
            FOrganicConstructionStock Result;
            Result.Stone = FMath::Min(Available.Stone, Required.Stone);
            Result.Planks = FMath::Min(Available.Planks, Required.Planks);
            Result.Beams = FMath::Min(Available.Beams, Required.Beams);
            Result.Tiles = FMath::Min(Available.Tiles, Required.Tiles);
            return Result;
        }

        FOrganicConstructionStock SubtractStock(const FOrganicConstructionStock& A,
            const FOrganicConstructionStock& B)
        {
            FOrganicConstructionStock Result;
            Result.Stone = A.Stone - B.Stone;
            Result.Planks = A.Planks - B.Planks;
            Result.Beams = A.Beams - B.Beams;
            Result.Tiles = A.Tiles - B.Tiles;
            return Result;
        }

        bool SameCost(const FOrganicConstructionStock& A, const FOrganicConstructionStock& B)
        {
            return A.Stone == B.Stone && A.Planks == B.Planks && A.Beams == B.Beams && A.Tiles == B.Tiles;
        }

        bool ValidMarketModule(const FString& ModuleId)
        {
            return ModuleId==TEXT("bench_low") || ModuleId==TEXT("work_table") || ModuleId==TEXT("tool_rack");
        }

        bool BuildDelta(const TArray<FString>& CurrentInstalledKeys,
            const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out)
        {
            Out = FOrganicConstructionDelta();
            FString Error;
            if (!SameKeysAreUnique(CurrentInstalledKeys, &Error)) return false;

            TSet<FString> TargetKeys;
            for (const FOrganicConstructionPiece& Piece : TargetPieces)
            {
                if (!Validate(Piece, &Error) || TargetKeys.Contains(Piece.Key))
                {
                    return false;
                }
                TargetKeys.Add(Piece.Key);
            }

            for (const FString& Key : CurrentInstalledKeys)
            {
                if (TargetKeys.Contains(Key)) Out.KeepKeys.Add(Key);
                else Out.RemoveKeys.Add(Key);
            }
            TSet<FString> CurrentKeys;
            for (const FString& Key : CurrentInstalledKeys) CurrentKeys.Add(Key);
            for (const FOrganicConstructionPiece& Piece : TargetPieces)
                if (!CurrentKeys.Contains(Piece.Key)) Out.AddPieces.Add(Piece);
            return true;
        }
    }

    bool Validate(const FOrganicConstructionStock& Stock, FString* OutError)
    {
        return Stock.IsNonNegative() ? true : Fail(OutError, TEXT("negative_stock"));
    }

    bool Validate(const FOrganicConstructionPiece& Piece, FString* OutError)
    {
        if (Piece.Key.IsEmpty()) return Fail(OutError, TEXT("missing_piece_key"));
        if (Piece.Module.IsEmpty()) return Fail(OutError, TEXT("missing_piece_module"));
        return Validate(Piece.Cost, OutError);
    }

    bool MarketKitCost(const FString& ModuleId, FOrganicConstructionStock& OutCost)
    {
        OutCost=FOrganicConstructionStock();
        if(ModuleId==TEXT("bench_low") || ModuleId==TEXT("tool_rack")) { OutCost.Planks=2; OutCost.Beams=1; return true; }
        if(ModuleId==TEXT("work_table")) { OutCost.Planks=3; OutCost.Beams=2; return true; }
        return false;
    }

    bool Validate(const FOrganicMarketKitRecord& Record, bool bPending, FString* OutError)
    {
        if(Record.RequestId.IsEmpty() || Record.RequestId.Len()>256) return Fail(OutError,TEXT("invalid_market_request_id"));
        FGuid WorkId;
        if(!FGuid::Parse(Record.TaskId,WorkId) || !WorkId.IsValid()) return Fail(OutError,TEXT("invalid_market_task_id"));
        if(!ValidMarketModule(Record.ModuleId)) return Fail(OutError,TEXT("unsupported_market_module"));
        const bool bKnownStatus=Record.Status==TEXT("pending") || Record.Status==TEXT("traveling")
            || Record.Status==TEXT("working") || Record.Status==TEXT("installed");
        if(!bKnownStatus || (bPending ? Record.Status==TEXT("installed") : Record.Status!=TEXT("installed")))
            return Fail(OutError,TEXT("invalid_market_status"));
        FOrganicConstructionStock Expected;
        if(!MarketKitCost(Record.ModuleId,Expected) || !SameCost(Record.Cost,Expected)) return Fail(OutError,TEXT("invalid_market_cost"));
        if(!FMath::IsFinite(Record.Anchor.X) || !FMath::IsFinite(Record.Anchor.Y) || !FMath::IsFinite(Record.Anchor.Z)
            || FMath::Abs(Record.Anchor.X)>1000000.f || FMath::Abs(Record.Anchor.Y)>1000000.f || FMath::Abs(Record.Anchor.Z)>1000000.f)
            return Fail(OutError,TEXT("invalid_market_anchor"));
        if(!FMath::IsFinite(Record.InstallPosition.X) || !FMath::IsFinite(Record.InstallPosition.Y) || !FMath::IsFinite(Record.InstallPosition.Z)
            || FMath::Abs(Record.InstallPosition.X)>1000000.f || FMath::Abs(Record.InstallPosition.Y)>1000000.f || FMath::Abs(Record.InstallPosition.Z)>1000000.f
            || !FMath::IsFinite(Record.InstallYaw) || FMath::Abs(Record.InstallYaw)>360.f)
            return Fail(OutError,TEXT("invalid_market_install_pose"));
        if(Record.InstallPosition.IsNearlyZero()) return Fail(OutError,TEXT("missing_market_install_position"));
        if(!FMath::IsFinite(Record.WorkProgress) || Record.WorkProgress<0.f || Record.WorkProgress>1.f)
            return Fail(OutError,TEXT("invalid_market_progress"));
        if(!bPending && !FMath::IsNearlyEqual(Record.WorkProgress,1.f)) return Fail(OutError,TEXT("installed_market_progress"));
        if(bPending && Record.Status!=TEXT("working") && !FMath::IsNearlyZero(Record.WorkProgress)) return Fail(OutError,TEXT("market_progress_outside_work"));
        if(Record.Revision<0) return Fail(OutError,TEXT("negative_market_revision"));
        return true;
    }

    bool Validate(const FOrganicConstructionHomeState& Home, FString* OutError)
    {
        if (!SameKeysAreUnique(Home.InstalledKeys, OutError)) return false;
        if (!Validate(Home.Reclaimed, OutError)) return false;
        for (const TPair<FString, FOrganicConstructionStock>& Entry : Home.InstalledCosts)
        {
            if (Entry.Key.IsEmpty() || !Home.InstalledKeys.Contains(Entry.Key))
                return Fail(OutError, TEXT("installed_cost_missing_key"));
            if (!Validate(Entry.Value, OutError)) return false;
        }
        if (Home.InstalledCosts.Num() != Home.InstalledKeys.Num())
            return Fail(OutError, TEXT("installed_cost_count_mismatch"));
        for (const FString& Key : Home.InstalledKeys)
            if (!Home.InstalledCosts.Contains(Key)) return Fail(OutError, TEXT("installed_key_missing_cost"));
        TSet<FString> MarketRequestIds;
        if(Home.MarketKitInstalled.Num()>MaxMarketKitsPerHome) return Fail(OutError,TEXT("market_capacity_exceeded"));
        for (const FOrganicMarketKitRecord& Record:Home.MarketKitInstalled)
        {
            if(!Validate(Record,false,OutError) || MarketRequestIds.Contains(Record.RequestId))
                return Fail(OutError,TEXT("duplicate_market_kit"));
            MarketRequestIds.Add(Record.RequestId);
        }
        if(Home.bHasMarketKitPending)
        {
            if(!Validate(Home.MarketKitPending,true,OutError) || MarketRequestIds.Contains(Home.MarketKitPending.RequestId)
                || Home.MarketKitInstalled.Num()>=MaxMarketKitsPerHome) return Fail(OutError,TEXT("invalid_market_pending"));
        }
        else if(!Home.MarketKitPending.RequestId.IsEmpty() || !Home.MarketKitPending.TaskId.IsEmpty() || !Home.MarketKitPending.ModuleId.IsEmpty()
            || Home.MarketKitPending.Status!=TEXT("pending") || !FMath::IsNearlyZero(Home.MarketKitPending.WorkProgress)
            || Home.MarketKitPending.Revision!=0 || !Home.MarketKitPending.Cost.IsNonNegative()
            || Home.MarketKitPending.Cost.Total()!=0 || !Home.MarketKitPending.Anchor.IsNearlyZero()
            || !Home.MarketKitPending.InstallPosition.IsNearlyZero() || !FMath::IsNearlyZero(Home.MarketKitPending.InstallYaw))
            return Fail(OutError,TEXT("orphan_market_pending"));
        if (!FMath::IsFinite(Home.WorkProgress) || Home.WorkProgress < 0.f || Home.WorkProgress > 1.f)
            return Fail(OutError, TEXT("invalid_work_progress"));
        if (Home.Revision < 0) return Fail(OutError, TEXT("negative_revision"));
        if (Home.ActivePieceKey.IsEmpty() && !FMath::IsNearlyZero(Home.WorkProgress))
            return Fail(OutError, TEXT("progress_without_active_piece"));
        return true;
    }

    bool PlanDelta(const TArray<FString>& CurrentInstalledKeys,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out)
    {
        return BuildDelta(CurrentInstalledKeys, TargetPieces, Out);
    }

    bool PlanDelta(const TArray<FOrganicConstructionPiece>& CurrentInstalledPieces,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out)
    {
        TArray<FString> CurrentKeys;
        CurrentKeys.Reserve(CurrentInstalledPieces.Num());
        for (const FOrganicConstructionPiece& Piece : CurrentInstalledPieces)
        {
            FString Error;
            if (!Validate(Piece, &Error)) { Out = FOrganicConstructionDelta(); return false; }
            CurrentKeys.Add(Piece.Key);
        }
        if (!BuildDelta(CurrentKeys, TargetPieces, Out)) return false;
        for (const FOrganicConstructionPiece& CurrentPiece : CurrentInstalledPieces)
        {
            const FOrganicConstructionPiece* Target = TargetPieces.FindByPredicate(
                [&CurrentPiece](const FOrganicConstructionPiece& Candidate) { return Candidate.Key == CurrentPiece.Key; });
            if (!Target || (Target->Module == CurrentPiece.Module && SameCost(Target->Cost, CurrentPiece.Cost))) continue;
            Out.KeepKeys.Remove(CurrentPiece.Key);
            Out.RemoveKeys.AddUnique(CurrentPiece.Key);
            if (Target) Out.AddPieces.Add(*Target);
        }
        return true;
    }

    bool PlanDelta(const FOrganicConstructionHomeState& Current,
        const TArray<FOrganicConstructionPiece>& TargetPieces, FOrganicConstructionDelta& Out)
    {
        FString Error;
        if (!Validate(Current, &Error)) { Out = FOrganicConstructionDelta(); return false; }
        return BuildDelta(Current.InstalledKeys, TargetPieces, Out);
    }

    bool InstallAtomic(const FOrganicConstructionPiece& Piece,
        FOrganicConstructionHomeState& Home, FOrganicConstructionStock& PublicStock,
        int32& OwnerCoins, int32& Treasury, FOrganicConstructionInstallResult& Out)
    {
        Out = FOrganicConstructionInstallResult();
        FString Error;
        if (!Validate(Piece, &Error)) { Out.FailureReason = Error; return false; }
        if (!Validate(Home, &Error)) { Out.FailureReason = Error; return false; }
        if (!Validate(PublicStock, &Error)) { Out.FailureReason = Error; return false; }
        if (OwnerCoins < 0) { Out.FailureReason = TEXT("negative_owner_coins"); return false; }
        if (Home.InstalledKeys.Contains(Piece.Key))
        {
            Out.FailureReason = TEXT("piece_already_installed");
            return false;
        }

        const FOrganicConstructionStock ReclaimedUse = MinStock(Home.Reclaimed, Piece.Cost);
        const FOrganicConstructionStock Remaining = SubtractStock(Piece.Cost, ReclaimedUse);
        if (Remaining.Stone > PublicStock.Stone || Remaining.Planks > PublicStock.Planks
            || Remaining.Beams > PublicStock.Beams || Remaining.Tiles > PublicStock.Tiles)
        {
            Out.FailureReason = TEXT("public_material_shortage");
            return false;
        }
        const int64 RequiredCoins64 = int64(Remaining.Stone) + Remaining.Planks + Remaining.Beams + Remaining.Tiles;
        if (RequiredCoins64 > MAX_int32 || RequiredCoins64 > OwnerCoins)
        {
            Out.FailureReason = TEXT("insufficient_owner_coins");
            return false;
        }
        if (Treasury > MAX_int32 - int32(RequiredCoins64))
        {
            Out.FailureReason = TEXT("treasury_overflow");
            return false;
        }

        // All checks have completed. From this point the transaction is a single
        // deterministic state transition with no partial material or coin debit.
        Home.Reclaimed = SubtractStock(Home.Reclaimed, ReclaimedUse);
        PublicStock = SubtractStock(PublicStock, Remaining);
        OwnerCoins -= int32(RequiredCoins64);
        Treasury += int32(RequiredCoins64);
        Home.InstalledKeys.Add(Piece.Key);
        Home.InstalledCosts.Add(Piece.Key, Piece.Cost);
        Home.ActivePieceKey = Piece.Key;
        Home.WorkProgress = 1.f;
        ++Home.Revision;

        Out.bSuccess = true;
        Out.ReclaimedConsumed = ReclaimedUse;
        Out.PublicConsumed = Remaining;
        Out.CoinsPaid = int32(RequiredCoins64);
        return true;
    }

    bool Dismantle(const FOrganicConstructionPiece& Piece,
        FOrganicConstructionHomeState& Home, FOrganicConstructionDismantleResult& Out)
    {
        Out = FOrganicConstructionDismantleResult();
        FString Error;
        if (!Validate(Piece, &Error)) { Out.FailureReason = Error; return false; }
        if (!Validate(Home, &Error)) { Out.FailureReason = Error; return false; }
        const int32 Index = Home.InstalledKeys.IndexOfByKey(Piece.Key);
        if (Index == INDEX_NONE)
        {
            Out.FailureReason = TEXT("piece_not_installed");
            return false;
        }
        const FOrganicConstructionStock* RecordedCost = Home.InstalledCosts.Find(Piece.Key);
        if (!RecordedCost)
        {
            Out.FailureReason = TEXT("installed_cost_record_missing");
            return false;
        }
        const FOrganicConstructionStock ActualCost = *RecordedCost;
        if (AddWouldOverflow(Home.Reclaimed, ActualCost))
        {
            Out.FailureReason = TEXT("reclaimed_stock_overflow");
            return false;
        }

        Home.InstalledKeys.RemoveAt(Index);
        Home.InstalledCosts.Remove(Piece.Key);
        Home.Reclaimed.Stone += ActualCost.Stone;
        Home.Reclaimed.Planks += ActualCost.Planks;
        Home.Reclaimed.Beams += ActualCost.Beams;
        Home.Reclaimed.Tiles += ActualCost.Tiles;
        if (Home.ActivePieceKey == Piece.Key)
        {
            Home.ActivePieceKey.Reset();
            Home.WorkProgress = 0.f;
        }
        ++Home.Revision;
        Out.bSuccess = true;
        Out.ReclaimedReturned = ActualCost;
        return true;
    }
}
