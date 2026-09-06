#include "HearthVillage.h"

bool AHearthVillage::StartTileOrder(int32 Customer,int32 Potter,const FString& ConversationId)
{
    if(!Residents.IsValidIndex(Customer) || !Residents.IsValidIndex(Potter) || Customer==Potter
        || !Residents[Potter].Role.Contains(TEXT("陶工")) || Residents[Customer].RoofMaterial!=TEXT("terracotta")
        || Residents[Customer].Coins<4 || ClayStock<4 || AvailableWood()<2
        || !ProductionSites.ContainsByPredicate([](const FHearthSite& S){return S.Kind==EHearthSiteKind::TileKiln && S.bReachable;})
        || TileOrders.ContainsByPredicate([&](const FHearthTileOrder& O){return O.ConversationId==ConversationId;})) return false;
    const auto* Bond=Residents[Customer].Bonds.Find(Residents[Potter].StableId);
    if(Bond && Bond->Trust<35.f) return false;
    FHearthTileOrder O; O.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); O.ConversationId=ConversationId;
    O.Customer=Customer; O.Potter=Potter; O.Status=TEXT("active"); O.Escrow=O.Price; O.Remaining=180.f;
    Residents[Customer].Coins-=O.Escrow;
    O.Result=TEXT("客户已托管4枚钱；陶工将使用4份黏土和2份原木燃料烧制6片陶瓦。");
    TileOrders.Add(MoveTemp(O)); return true;
}

bool AHearthVillage::RejectTileOrder(int32 Customer,int32 Potter,const FString& ConversationId)
{
    if(!Residents.IsValidIndex(Customer) || !Residents.IsValidIndex(Potter) || Customer==Potter
        || TileOrders.ContainsByPredicate([&](const FHearthTileOrder& O){return O.ConversationId==ConversationId;})) return false;
    FHearthTileOrder O; O.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); O.ConversationId=ConversationId;
    O.Customer=Customer; O.Potter=Potter; O.Status=TEXT("rejected"); O.Result=TEXT("客户拒绝报价；没有钱款或材料被预留。");
    const FString Result=O.Result; TileOrders.Add(MoveTemp(O));
    auto& B=Residents[Potter].Bonds.FindOrAdd(Residents[Customer].StableId); B.Affinity=FMath::Max(-100.f,B.Affinity-.5f); B.Memory=Result;
    return true;
}

bool AHearthVillage::CancelTileOrder(const FString& OrderId)
{
    auto* O=TileOrders.FindByPredicate([&](const FHearthTileOrder& X){return X.Id==OrderId;});
    if(!O) return false;
    if(O->Status==TEXT("cancelled")) return true;
    if(O->Status!=TEXT("active") || O->ReservedClay || O->ReservedTiles || !Residents.IsValidIndex(O->Customer)) return false;
    Residents[O->Customer].Coins+=O->Escrow; O->Escrow=0; O->Status=TEXT("cancelled"); O->Result=TEXT("开工前取消订单，托管钱已原路退回。");
    return true;
}

bool AHearthVillage::SettleTileOrder(FHearthTileOrder& O)
{
    if(O.Status==TEXT("completed")) return true;
    if(O.Status!=TEXT("delivering") || O.ReservedTiles!=O.TileQuantity || O.Escrow!=O.Price
        || !Residents.IsValidIndex(O.Customer) || !Residents.IsValidIndex(O.Potter)) return false;
    const FString TransactionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FHearthTaxAssessment Tax; if(!PrepareIncomeTax(O.Potter,O.Price,TransactionId,false,Tax)) return false;
    if(Residents[O.Customer].PersonalTiles>100000000-O.TileQuantity || Residents[O.Potter].Coins>100000000-O.Price) return false;
    FHearthTransaction Sale; Sale.Id=TransactionId; Sale.Kind=TEXT("tile_order"); Sale.TaskId=O.Id; Sale.From=O.Customer; Sale.To=O.Potter;
    Sale.Amount=O.Price; Sale.Quantity=O.TileQuantity; Sale.Item=TEXT("tiles"); Sale.At=Elapsed;
    Residents[O.Potter].Coins+=O.Price; Residents[O.Customer].PersonalTiles+=O.TileQuantity; Transactions.Add(MoveTemp(Sale)); CommitIncomeTax(Tax);
    auto& Potter=Residents[O.Potter]; auto& Customer=Residents[O.Customer];
    Potter.CargoType=-1; Potter.CargoAmount=0; Potter.Route.Reset(); Potter.Task=EHearthTask::LifeChoosing; Potter.ActiveTaskId.Empty();
    if(Customer.Task==EHearthTask::TradeWaiting) { Customer.Task=EHearthTask::LifeChoosing; Customer.ActiveTaskId.Empty(); }
    O.ReservedTiles=0; O.Escrow=0; O.Status=TEXT("completed"); O.Result=TEXT("6片真实陶瓦已交付，4枚托管钱只结算一次并计税。");
    auto& B=Customer.Bonds.FindOrAdd(Potter.StableId); B.Trust=FMath::Min(100.f,B.Trust+6.f); B.Memory=O.Result;
    VillageEvent=Potter.Name+TEXT("向")+Customer.Name+TEXT("交付了陶瓦。"); return true;
}

void AHearthVillage::AdvanceTileOrders(float Dt)
{
    for(auto& O:TileOrders)
    {
        if(O.Status==TEXT("active") && O.ReservedClay==0)
        {
            O.Remaining=FMath::Max(0.f,O.Remaining-Dt);
            if(O.Remaining<=0.f) { CancelTileOrder(O.Id); continue; }
        }
        if(O.Status==TEXT("active") && Residents.IsValidIndex(O.Potter) && Residents[O.Potter].Task==EHearthTask::LifeChoosing)
        {
            const int32 Site=ProductionSites.IndexOfByPredicate([](const FHearthSite& S){return S.Kind==EHearthSiteKind::TileKiln && S.bReachable && S.ReservedBy<0;});
            if(Site>=0) StartProduction(O.Potter,100+Site*16+15,TEXT("履行已接受的制瓦订单。"),false);
        }
        else if(O.Status==TEXT("delivering") && Residents.IsValidIndex(O.Potter) && Residents.IsValidIndex(O.Customer))
        {
            auto& Potter=Residents[O.Potter]; auto& Customer=Residents[O.Customer];
            if(Potter.Task!=EHearthTask::TradeTravel || Potter.ActiveTaskId!=O.Id) continue;
            if(Potter.Route.IsEmpty())
            {
                TArray<FVector> Route; const FVector Target=Customer.Actor->GetActorLocation()+FVector(0,120,0);
                if(!FindActivityRoute(O.Potter,Target,Route)) { O.Result=TEXT("交货路线暂不可用，陶瓦保持预留并等待。"); continue; }
                Potter.Route=MoveTemp(Route); Customer.Task=EHearthTask::TradeWaiting; Customer.ActiveTaskId=O.Id;
            }
            if(MoveResident(O.Potter,Dt)) SettleTileOrder(O);
        }
    }
}
