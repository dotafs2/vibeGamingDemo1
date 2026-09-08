#if WITH_DEV_AUTOMATION_TESTS
#include "HearthOrganicConstruction.h"

#include "Misc/AutomationTest.h"

namespace
{
    FOrganicConstructionPiece Piece(const TCHAR* Key, const TCHAR* Module,
        int32 Stone, int32 Planks, int32 Beams, int32 Tiles)
    {
        FOrganicConstructionPiece Result;
        Result.Key = Key;
        Result.Module = Module;
        Result.Cost.Stone = Stone;
        Result.Cost.Planks = Planks;
        Result.Cost.Beams = Beams;
        Result.Cost.Tiles = Tiles;
        return Result;
    }

    bool SameStock(const FOrganicConstructionStock& A, const FOrganicConstructionStock& B)
    {
        return A.Stone == B.Stone && A.Planks == B.Planks && A.Beams == B.Beams && A.Tiles == B.Tiles;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicConstructionTest,
    "ThreeHearths.Construction.OrganicAtomicLedger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthOrganicConstructionTest::RunTest(const FString&)
{
    const FOrganicConstructionPiece Base = Piece(TEXT("base"), TEXT("family_growth.base"), 3, 2, 0, 0);
    const FOrganicConstructionPiece Wing = Piece(TEXT("wing"), TEXT("family_growth.wing"), 1, 0, 1, 2);
    const FOrganicConstructionPiece ChangedWing = Piece(TEXT("wing"), TEXT("family_growth.wing_large"), 2, 0, 1, 2);

    FOrganicConstructionPiece Invalid = Base;
    Invalid.Cost.Stone = -1;
    FString Error;
    TestFalse(TEXT("Negative costs are rejected"), HearthOrganicConstruction::Validate(Invalid, &Error));

    FOrganicConstructionHomeState PendingHome;
    PendingHome.ActivePieceKey = TEXT("pending-wing");
    PendingHome.WorkProgress = .5f;
    TestTrue(TEXT("Pending construction may reference an uninstalled key"),
        HearthOrganicConstruction::Validate(PendingHome, &Error));
    FOrganicConstructionHomeState EmptyActiveHome;
    EmptyActiveHome.WorkProgress = .5f;
    TestFalse(TEXT("Progress without an active pending key is rejected"),
        HearthOrganicConstruction::Validate(EmptyActiveHome, &Error));

    FOrganicConstructionHomeState Home;
    Home.Reclaimed.Stone = 2;
    FOrganicConstructionStock PublicStock;
    PublicStock.Stone = 4;
    PublicStock.Planks = 4;
    int32 OwnerCoins = 10;
    int32 Treasury = 2;
    FOrganicConstructionInstallResult Install;
    TestTrue(TEXT("First family growth piece installs atomically"),
        HearthOrganicConstruction::InstallAtomic(Base, Home, PublicStock, OwnerCoins, Treasury, Install));
    TestTrue(TEXT("Reclaimed material is consumed first"), Install.ReclaimedConsumed.Stone == 2
        && Install.PublicConsumed.Stone == 1 && Install.PublicConsumed.Planks == 2);
    TestEqual(TEXT("Public material is charged one coin per unit"), Install.CoinsPaid, 3);
    TestTrue(TEXT("Installation updates ledger and treasury"), Home.InstalledKeys.Num() == 1
        && Home.InstalledKeys[0] == TEXT("base") && OwnerCoins == 7 && Treasury == 5
        && Home.Reclaimed.Total() == 0 && Home.WorkProgress == 1.f);

    const FOrganicConstructionHomeState HomeBeforeDuplicate = Home;
    const FOrganicConstructionStock StockBeforeDuplicate = PublicStock;
    const int32 CoinsBeforeDuplicate = OwnerCoins;
    const int32 TreasuryBeforeDuplicate = Treasury;
    TestFalse(TEXT("Duplicate completion is rejected without mutation"),
        HearthOrganicConstruction::InstallAtomic(Base, Home, PublicStock, OwnerCoins, Treasury, Install));
    TestTrue(TEXT("Duplicate completion leaves every account unchanged"), Home.InstalledKeys == HomeBeforeDuplicate.InstalledKeys
        && SameStock(Home.Reclaimed, HomeBeforeDuplicate.Reclaimed) && SameStock(PublicStock, StockBeforeDuplicate)
        && OwnerCoins == CoinsBeforeDuplicate && Treasury == TreasuryBeforeDuplicate && Home.Revision == HomeBeforeDuplicate.Revision);

    FOrganicConstructionPiece TooExpensive = Piece(TEXT("too_expensive"), TEXT("family_growth.roof"), 0, 0, 1, 0);
    const FOrganicConstructionHomeState HomeBeforeShortage = Home;
    const FOrganicConstructionStock StockBeforeShortage = PublicStock;
    const int32 CoinsBeforeShortage = OwnerCoins;
    TestFalse(TEXT("Material shortage rejects the whole transaction"),
        HearthOrganicConstruction::InstallAtomic(TooExpensive, Home, PublicStock, OwnerCoins, Treasury, Install));
    TestTrue(TEXT("Material shortage does not debit any state"), Home.InstalledKeys == HomeBeforeShortage.InstalledKeys
        && SameStock(Home.Reclaimed, HomeBeforeShortage.Reclaimed) && SameStock(PublicStock, StockBeforeShortage)
        && OwnerCoins == CoinsBeforeShortage);

    FOrganicConstructionDismantleResult Dismantle;
    TestTrue(TEXT("Dismantling an installed piece returns its complete recipe"),
        HearthOrganicConstruction::Dismantle(Base, Home, Dismantle));
    TestTrue(TEXT("Dismantle returns all materials privately"), SameStock(Dismantle.ReclaimedReturned, Base.Cost)
        && SameStock(Home.Reclaimed, Base.Cost) && Home.InstalledKeys.Num() == 0);
    const FOrganicConstructionHomeState HomeBeforeRepeatDismantle = Home;
    TestFalse(TEXT("Repeated dismantle is rejected"), HearthOrganicConstruction::Dismantle(Base, Home, Dismantle));
    TestTrue(TEXT("Repeated dismantle does not duplicate reclaimed stock"), SameStock(Home.Reclaimed, HomeBeforeRepeatDismantle.Reclaimed)
        && Home.Revision == HomeBeforeRepeatDismantle.Revision);

    FOrganicConstructionHomeState MissingCostHome;
    MissingCostHome.InstalledKeys.Add(TEXT("orphan"));
    FOrganicConstructionStock MissingCostReclaimedBefore = MissingCostHome.Reclaimed;
    TestFalse(TEXT("Dismantle refuses an installed key without recorded cost"),
        HearthOrganicConstruction::Dismantle(Piece(TEXT("orphan"), TEXT("family_growth.orphan"), 99, 0, 0, 0), MissingCostHome, Dismantle));
    TestTrue(TEXT("Missing recorded cost does not create a refund"), SameStock(MissingCostHome.Reclaimed, MissingCostReclaimedBefore)
        && MissingCostHome.InstalledKeys.Num() == 1);

    FOrganicConstructionHomeState RefundHome;
    FOrganicConstructionStock RefundStock;
    RefundStock = Wing.Cost;
    int32 RefundCoins = 10;
    int32 RefundTreasury = 0;
    TestTrue(TEXT("A wing can be installed for refund provenance"),
        HearthOrganicConstruction::InstallAtomic(Wing, RefundHome, RefundStock, RefundCoins, RefundTreasury, Install));
    TestTrue(TEXT("Dismantle refunds the recorded cost even when target changed"),
        HearthOrganicConstruction::Dismantle(ChangedWing, RefundHome, Dismantle)
        && SameStock(Dismantle.ReclaimedReturned, Wing.Cost));

    FOrganicConstructionHomeState PendingInstallHome;
    PendingInstallHome.ActivePieceKey = TEXT("pending-wing");
    PendingInstallHome.WorkProgress = .5f;
    FOrganicConstructionStock PendingInstallStock = Base.Cost;
    int32 PendingInstallCoins = Base.Cost.Total();
    int32 PendingInstallTreasury = 0;
    TestTrue(TEXT("Install can complete while another key is pending"),
        HearthOrganicConstruction::InstallAtomic(Base, PendingInstallHome, PendingInstallStock,
            PendingInstallCoins, PendingInstallTreasury, Install));
    TestTrue(TEXT("Completed install records exact cost and progress"), PendingInstallHome.InstalledKeys.Contains(TEXT("base"))
        && PendingInstallHome.InstalledCosts.Contains(TEXT("base")) && PendingInstallHome.WorkProgress == 1.f);

    TArray<FString> CurrentKeys = { TEXT("base"), TEXT("old_wing") };
    TArray<FOrganicConstructionPiece> TargetPieces = { Base, Wing };
    FOrganicConstructionDelta Delta;
    TestTrue(TEXT("Delta keeps, removes, and adds by stable key"),
        HearthOrganicConstruction::PlanDelta(CurrentKeys, TargetPieces, Delta));
    TestTrue(TEXT("Delta retains base"), Delta.KeepKeys.Num() == 1 && Delta.KeepKeys[0] == TEXT("base"));
    TestTrue(TEXT("Delta removes changed-away key"), Delta.RemoveKeys.Num() == 1 && Delta.RemoveKeys[0] == TEXT("old_wing"));
    TestTrue(TEXT("Delta adds new family growth piece"), Delta.AddPieces.Num() == 1 && Delta.AddPieces[0].Key == TEXT("wing"));

    TArray<FOrganicConstructionPiece> CurrentPieces = { Base, Wing };
    TArray<FOrganicConstructionPiece> ChangedTarget = { Base, ChangedWing };
    TestTrue(TEXT("Target recipe change removes old key and adds replacement"),
        HearthOrganicConstruction::PlanDelta(CurrentPieces, ChangedTarget, Delta)
        && Delta.KeepKeys.Num() == 1 && Delta.KeepKeys[0] == TEXT("base")
        && Delta.AddPieces.Num() == 1 && Delta.AddPieces[0].Key == TEXT("wing")
        && Delta.RemoveKeys.Num() == 1 && Delta.RemoveKeys[0] == TEXT("wing"));
    return true;
}
#endif
