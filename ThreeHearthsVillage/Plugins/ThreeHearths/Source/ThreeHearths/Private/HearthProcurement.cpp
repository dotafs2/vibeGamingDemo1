#include "HearthVillage.h"

// Shared only by the three construction-supply translation units. No gameplay
// interface or persisted fields are needed: commitments already have task IDs.
namespace HearthCastleSupply
{
    FIntVector CurrentBatchDeficit(const FHearthPublicProject& Project,const TArray<FHearthResident>& Residents)
    {
        FIntVector Need(-Project.Stock[0],-Project.Stock[1],-Project.Stock[2]);
        int32 Stage=MAX_int32,Count=0;
        for(const auto& Part:Project.Parts)
            if(Part.Status!=TEXT("completed")) Stage=FMath::Min(Stage,Part.Stage);
        for(const auto& Part:Project.Parts)
        {
            if(Part.Status==TEXT("completed") || Part.Stage!=Stage) continue;
            for(int32 M=0;M<3;++M) Need[M]+=Part.Required[M]-Part.Reserved[M]-Part.Delivered[M];
            if(Residents.IsValidIndex(Part.Worker))
            {
                const auto& R=Residents[Part.Worker];
                if(R.ActiveTaskId==Part.TaskId && (R.Task==EHearthTask::PublicTravel || R.Task==EHearthTask::PublicWork)
                    && R.CargoType>=2 && R.CargoType<=4) Need[R.CargoType-2]-=R.CargoAmount;
            }
            if(++Count==3) break;
        }
        for(const auto& Order:Project.Orders)
            if(Order.ProjectId==Project.Id && Order.Status==TEXT("transporting")) Need.Y-=Order.ReservedQuantity;
        for(int32 M=0;M<3;++M) Need[M]=FMath::Max(0,Need[M]);
        return Need;
    }

    int32 OwnedOrProcessingPlanks(const TArray<FHearthResident>& Residents)
    {
        int32 Available=0;
        for(const auto& R:Residents)
        {
            Available+=R.PersonalPlanks;
            // One private share is earned on finishing milling. Delivery cargo
            // is communal; its private share is already in PersonalPlanks.
            if(R.ProductionOp==13 && (R.Task==EHearthTask::ProductionTravel || R.Task==EHearthTask::ProductionWork)) ++Available;
        }
        return Available;
    }
}

namespace
{
    FVector SupplyDepotFor(const AHearthVillage& Village)
    {
        // Use the same warehouse as milling and public construction in each map.
        return Village.bUseCropoutMap ? FVector(-1650.f,-1050.f,8.f) : FVector(-250.f,-400.f,8.f);
    }

    int32 UnfilledPlanks(const FHearthPublicProject& Project,const TArray<FHearthResident>& Residents,const FString& ExcludedOrder=FString())
    {
        int32 Required = 0, Committed=Project.Stock[1];
        for (const FHearthPublicPart& Part : Project.Parts)
        {
            if(Part.Status!=TEXT("completed"))
            {
                Required+=Part.Required[1];
                Committed+=Part.Reserved[1]+Part.Delivered[1];
            }
        }
        for(const auto& Resident:Residents) if((Resident.Task==EHearthTask::PublicTravel || Resident.Task==EHearthTask::PublicWork) && Resident.CargoType==3)
            Committed+=Resident.CargoAmount;
        for(const auto& Order:Project.Orders) if(Order.Id!=ExcludedOrder && Order.Status==TEXT("transporting")) Committed+=Order.ReservedQuantity;
        return FMath::Max(0,Required-Committed);
    }

    void ClearSupplyWorker(FHearthResident& Resident)
    {
        Resident.ActiveTaskId.Empty();
        Resident.Route.Reset();
        Resident.Task = EHearthTask::LifeChoosing;
        Resident.Timer = 0.f;
        Resident.ProductionSite = -1;
        Resident.ProductionOp = -1;
        Resident.ProductionComponentId.Empty();
        Resident.NextLifeDecision = 0;
    }
}

bool AHearthVillage::StartSupplyOrder(int32 Seller)
{
    const bool bCastle=PublicProject.TemplateId==TEXT("royal_keep_garden_v2");
    const int32 Missing=bCastle?HearthCastleSupply::CurrentBatchDeficit(PublicProject,Residents).Y:UnfilledPlanks(PublicProject,Residents);
    if (!Residents.IsValidIndex(Seller) || !CanAssignActivity(Seller)
        || (PublicProject.Status != TEXT("approved") && PublicProject.Status != TEXT("building"))
        || PublicProject.Completed >= PublicProject.Parts.Num() || Missing <= 0
        || Residents[Seller].PersonalPlanks < 1 || TreasuryCoins < 2 || TaxProjectCoins < (bCastle?4:2)
        || PublicProject.Orders.Num() >= 100000) return false;

    TArray<FVector> Route;
    const FVector SupplyDepot=SupplyDepotFor(*this);
    if (bUseCropoutMap)
    {
        if (!FindActivityRoute(Seller, SupplyDepot, Route)) return false;
    }
    else Route={SupplyDepot}; // Isolated unit world; public approval itself remains island-only.

    auto& Resident = Residents[Seller];
    const FString OrderId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FHearthSupplyOrder Order;
    Order.Id = OrderId;
    Order.ProjectId = PublicProject.Id;
    Order.Seller = Seller;
    Order.Quantity = 1;
    Order.Price = 2;
    Order.ReservedQuantity = 1;
    Order.Escrow = 2;
    Order.Status = TEXT("transporting");
    Order.Remaining = 300.f;

    // All checks, including route availability, happen before these reservations.
    Resident.PersonalPlanks -= 1;
    TreasuryCoins -= Order.Escrow;
    TaxProjectCoins -= Order.Escrow;
    Resident.ActiveTaskId = OrderId;
    Resident.Route = MoveTemp(Route);
    Resident.Task = EHearthTask::SupplyTravel;
    Resident.Timer = 0.f;
    Resident.LatestEvent = TEXT("携带自有木板前往公共仓库交货。");
    FHearthDecisionRecord History;
    History.Run = CurrentRun; History.Resident = Seller; History.At = Elapsed;
    History.Kind = TEXT("procurement"); History.Source = TEXT("local");
    History.Status = TEXT("executing"); History.Choice = TEXT("公共工程供货");
    History.Context = TEXT("来源：resident_owned_sawmill_share_or_completed_trade；价格固定为2枚钱、1块木板。");
    Resident.HistoryIndex = DecisionHistory.Add(MoveTemp(History));
    PublicProject.Orders.Add(MoveTemp(Order));
    return true;
}

bool AHearthVillage::SettleSupplyOrder(FHearthSupplyOrder& Order)
{
    // The batch cap applies to new orders. Honor previously escrowed deliveries
    // against the original project demand, including orders from older saves.
    if (Order.Status == TEXT("completed")) return true;
    if (Order.Status == TEXT("cancelled")) return false;
    FGuid OrderGuid;
    if (!Residents.IsValidIndex(Order.Seller) || Order.ProjectId != PublicProject.Id
        || Order.Id.IsEmpty() || !FGuid::Parse(Order.Id, OrderGuid)
        || Order.Quantity != 1 || Order.Price != 2 || Order.ReservedQuantity != 1 || Order.Escrow != 2
        || Order.Origin != TEXT("resident_owned_sawmill_share_or_completed_trade")
        || PublicProject.Status == TEXT("completed") || UnfilledPlanks(PublicProject,Residents,Order.Id) <= 0
        || TreasuryCoins < 0 || TaxProjectCoins < 0) return false;

    const FString TransactionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FHearthTaxAssessment Tax;
    // PrepareIncomeTax validates transaction and account capacity without mutating state.
    if (!PrepareIncomeTax(Order.Seller, Order.Price, TransactionId, false, Tax)) return false;
    if (TreasuryCoins > 100000000 - Tax.Tax
        || TaxProjectCoins > 100000000 - Tax.Tax
        || PublicProject.Stock[1] > 100000000 - Order.Quantity) return false;

    // Escrow already left both treasury balances at reservation. Completion pays it
    // to the seller; only the assessed tax returns to the protected treasury.
    FHearthTransaction Sale;
    Sale.Id = TransactionId; Sale.Kind = TEXT("public_purchase"); Sale.TaskId = Order.Id;
    Sale.From = -1; Sale.To = Order.Seller; Sale.Amount = Order.Price;
    Sale.Item = TEXT("plank"); Sale.Quantity = 1; Sale.At = Elapsed;
    Residents[Order.Seller].Coins += Order.Price;
    Transactions.Add(MoveTemp(Sale));
    CommitIncomeTax(Tax);
    PublicProject.Stock[1] += Order.Quantity;
    Order.ReservedQuantity = 0;
    Order.Escrow = 0;
    Order.Status = TEXT("completed");
    Order.Result = TEXT("公共工程收到1块来源真实、价格为2枚钱的木板。");
    if (Residents[Order.Seller].ActiveTaskId == Order.Id)
    {
        Residents[Order.Seller].LatestEvent = Order.Result;
        CompleteHistory(Order.Seller, Order.Result);
        ClearSupplyWorker(Residents[Order.Seller]);
    }
    VillageEvent = Order.Result;
    return true;
}

bool AHearthVillage::CancelSupplyOrder(int32 Seller)
{
    if (!Residents.IsValidIndex(Seller)) return false;
    FHearthSupplyOrder* Order = PublicProject.Orders.FindByPredicate([&](const FHearthSupplyOrder& Candidate)
    {
        return Candidate.Seller == Seller && Candidate.Status == TEXT("transporting");
    });
    if (!Order)
    {
        // Repeating cancellation is harmless once the order is closed.
        return PublicProject.Orders.ContainsByPredicate([&](const FHearthSupplyOrder& Candidate)
        { return Candidate.Seller == Seller && Candidate.Status == TEXT("cancelled"); });
    }
    const int32 Goods = Order->ReservedQuantity;
    const int32 Cash = Order->Escrow;
    Residents[Seller].PersonalPlanks += Goods;
    TreasuryCoins += Cash;
    TaxProjectCoins += Cash;
    Order->ReservedQuantity = 0;
    Order->Escrow = 0;
    Order->Status = TEXT("cancelled");
    Order->Result = TEXT("公共工程供货取消，木板和托管钱已原路退回。");
    if (Residents[Seller].ActiveTaskId == Order->Id)
    {
        Residents[Seller].LatestEvent = Order->Result;
        CompleteHistory(Seller, Order->Result);
        ClearSupplyWorker(Residents[Seller]);
    }
    return true;
}

void AHearthVillage::AdvanceSupplyWorker(int32 Seller, float Dt)
{
    if (!Residents.IsValidIndex(Seller)) return;
    FHearthResident& Resident = Residents[Seller];
    FHearthSupplyOrder* Order = PublicProject.Orders.FindByPredicate([&](const FHearthSupplyOrder& Candidate)
    { return Candidate.Id == Resident.ActiveTaskId && Candidate.Seller == Seller && Candidate.Status == TEXT("transporting"); });
    if (!Order) return;

    Order->Remaining = FMath::Max(0.f, Order->Remaining - FMath::Max(0.f, Dt));
    if (Resident.Task == EHearthTask::SupplyTravel)
    {
        if (MoveResident(Seller, Dt))
        {
            Resident.Task = EHearthTask::SupplyHandover;
            Order->Remaining = 1.f;
            Resident.LatestEvent = TEXT("已到公共仓库，正在交接木板。");
        }
        else if (Order->Remaining <= 0.f)
        {
            CancelSupplyOrder(Seller);
        }
        return;
    }
    if (Resident.Task == EHearthTask::SupplyHandover && Order->Remaining <= 0.f)
    {
        // A failed settlement leaves the order and all balances intact so a later tick can retry.
        if (!SettleSupplyOrder(*Order)) Order->Remaining = 0.1f;
    }
}
