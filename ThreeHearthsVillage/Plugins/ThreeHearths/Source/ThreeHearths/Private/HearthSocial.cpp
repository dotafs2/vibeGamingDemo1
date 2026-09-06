#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HearthSocial
{
    const TCHAR* IntentNames[]={TEXT("回应与闲聊"),TEXT("邀请一起吃饭"),TEXT("请求采集帮助"),TEXT("接受提议"),TEXT("婉拒提议"),TEXT("告别"),TEXT("出售一块自有木板"),TEXT("委托陶工制作一箱陶瓦"),TEXT("向有需要的居民提出制瓦报价")};
    FString Json(const TSharedRef<FJsonObject>& J) { FString Text; FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text)); return Text; }

    bool HasTileNeed(const FHearthResident& Customer)
    {
        // One modeled room uses two six-tile roof slopes. A first delivered
        // batch remains useful private property, but the customer may order a
        // second batch until the complete roof recipe is executable.
        return Customer.RoofMaterial==TEXT("terracotta") && Customer.PersonalTiles<12;
    }

    bool IsOpenTileOrder(const FHearthTileOrder& Order)
    {
        return Order.Status!=TEXT("completed") && Order.Status!=TEXT("cancelled") && Order.Status!=TEXT("rejected") && Order.Status!=TEXT("broken");
    }

    bool TileParties(const AHearthVillage& Village,const FHearthConversation& Conversation,int32& Customer,int32& Potter)
    {
        if(!Village.Residents.IsValidIndex(Conversation.First) || !Village.Residents.IsValidIndex(Conversation.Second)) return false;
        const bool FirstPotter=Village.Residents[Conversation.First].Role==TEXT("陶工");
        const bool SecondPotter=Village.Residents[Conversation.Second].Role==TEXT("陶工");
        if(FirstPotter==SecondPotter) return false;
        Potter=FirstPotter?Conversation.First:Conversation.Second;
        Customer=FirstPotter?Conversation.Second:Conversation.First;
        return true;
    }

    bool CanOfferTiles(const AHearthVillage& Village,const FHearthConversation& Conversation)
    {
        int32 Customer=-1,Potter=-1;
        if(!TileParties(Village,Conversation,Customer,Potter)) return false;
        const auto& Buyer=Village.Residents[Customer]; const auto& Maker=Village.Residents[Potter];
        const auto* Bond=Buyer.Bonds.Find(Maker.StableId);
        const float Trust=Bond?Bond->Trust:50.f;
        const bool Busy=Village.TileOrders.ContainsByPredicate([&](const FHearthTileOrder& Order)
        { return IsOpenTileOrder(Order) && (Order.Customer==Customer || Order.Potter==Potter || Order.ConversationId==Conversation.Id); });
        return HasTileNeed(Buyer) && Maker.Role.Contains(TEXT("陶工")) && Buyer.Coins>=4 && Maker.Energy>=25.f && Trust>=35.f
            && Village.ClayStock>=4 && Village.AvailableWood()>=2 && !Busy;
    }
}

bool AHearthVillage::IsSociallyAvailable(int32 Index) const
{
    return Residents.IsValidIndex(Index) && Residents[Index].BuildProgress>=1 && Residents[Index].Task==EHearthTask::LifeChoosing
        && Residents[Index].ConversationId.IsEmpty() && Residents[Index].Route.IsEmpty() && !IsDecisionPending(Index);
}

bool AHearthVillage::BeginConversation(int32 Index,int32 Other,const FString& Reason,bool bFromApi)
{
    if(Index==Other || !IsSociallyAvailable(Index) || !IsSociallyAvailable(Other)) return false;
    TArray<FVector> Route; const FVector HostPosition=Residents[Other].Actor->GetActorLocation();
    if(bUseCropoutMap)
    { if(!FindActivityRoute(Index,HostPosition,Route)) return false; }
    else Route={HostPosition+FVector(0,120,0)};
    FHearthConversation S; S.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    S.First=Index; S.Second=Other; S.FirstId=Residents[Index].StableId; S.SecondId=Residents[Other].StableId; S.Speaker=Index;
    for(int32 I:{Index,Other})
    {
        auto& R=Residents[I]; R.ConversationId=S.Id; R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        R.LifeAction=3+(I==Index?Other:Index); R.Task=I==Index?EHearthTask::LifeTravel:EHearthTask::LifeActivity;
        R.Timer=0; R.Route.Reset(); R.MoveRetry=0; R.bMovementBlocked=false;
        R.DecisionSource=I==Index && bFromApi?TEXT("api"):TEXT("social_event");
        if(I==Other) StartHistory(I,true,TEXT("social_event"));
        AcceptHistory(I,TEXT("与")+Residents[I==Index?Other:Index].Name+TEXT("交谈"),Reason,R.DecisionSource);
        R.LatestEvent=I==Index?TEXT("走过去，等见面后再开口。"):TEXT("有人来访，等对方走近再决定聊些什么。");
    }
    Residents[Index].Route=MoveTemp(Route); Conversations.Add(MoveTemp(S)); ++SocialRevision;
    return true;
}

int32 AHearthVillage::FindHelpActivity(int32 Worker) const
{
    // Help is an actual available gathering/transport job, never free inventory.
    int32 Best=-1; float Score=-FLT_MAX;
    for(int32 Action:AvailableProductionActions(Worker))
    {
        const int32 Op=(Action-100)%16; if(Op<9 || Op>12) continue;
        const float Value=(Op==10 && AvailableWood()<60?100.f:Op==12 && FoodStock<Residents.Num()*5?110.f:Op==11 && StoneStock<10?90.f:40.f)-Action*.0001f;
        if(Value>Score) { Score=Value; Best=Action; }
    }
    return Best;
}

TArray<int32> AHearthVillage::AvailableSocialIntents(int32 Index) const
{
    const auto* S=Conversations.FindByPredicate([Index](const auto& C) { return !C.bClosed && C.bMet && C.Speaker==Index; });
    if(!S) return {};
    if(S->Lines.IsEmpty()) return {0};
    if(S->bAccepted) return {0,5};
    if(S->Offer>=0 && S->Proposer!=Index)
    {
        TArray<int32> Choices={4};
        const int32 Other=S->First==Index?S->Second:S->First;
        const auto* Trade=TradeOffers.FindByPredicate([&](const FHearthTradeOffer& T) { return T.ConversationId==S->Id && T.Status==TEXT("proposed"); });
        if((S->Offer==1 && FoodStock>=2 && Residents[Index].Coins>0 && Residents[Other].Coins>0) || (S->Offer==2 && IsProductionAllowed(Index,S->OfferAction))
            || (S->Offer==3 && Trade && Trade->Buyer==Index && Residents[Index].PersonalPlanks==0 && Residents[Index].Coins>=Trade->Price)
            || (S->Offer==4 && HearthSocial::CanOfferTiles(*this,*S))) Choices.Insert(3,0);
        return Choices;
    }
    TArray<int32> Choices={0,5};
    if(S->Lines.Num()>=2 && S->Offer<0)
    {
        const int32 Other=S->First==Index?S->Second:S->First;
        if(FoodStock>=2 && Residents[Index].Coins>0 && Residents[Other].Coins>0) Choices.Add(1);
        if(FindHelpActivity(Other)>=0) Choices.Add(2);
        if(Residents[Index].PersonalPlanks>1 && Residents[Other].PersonalPlanks==0 && Residents[Other].Coins>=2) Choices.Add(6);
        if(HearthSocial::CanOfferTiles(*this,*S))
        {
            int32 Customer=-1,Potter=-1; HearthSocial::TileParties(*this,*S,Customer,Potter);
            Choices.Add(Index==Customer?7:8);
        }
    }
    return Choices;
}

bool AHearthVillage::ResolveSocialTurn(int32 Index,int32 Intent,const FString& Words,const FString& Source)
{
    if(!AvailableSocialIntents(Index).Contains(Intent) || Words.TrimStartAndEnd().IsEmpty() || Words.Len()>180) return false;
    auto* S=Conversations.FindByPredicate([Index](const auto& C) { return !C.bClosed && C.bMet && C.Speaker==Index; });
    if(!S) return false;
    const int32 Other=S->First==Index?S->Second:S->First;
    if(FVector::Dist2D(Residents[Index].Actor->GetActorLocation(),Residents[Other].Actor->GetActorLocation())>300) return false;
    auto& R=Residents[Index]; auto& Listener=Residents[Other]; FHearthTradeOffer* PendingTrade=nullptr;
    int32 TileCustomer=-1,TilePotter=-1;
    if(Intent==6 && (R.PersonalPlanks<=1 || Listener.PersonalPlanks!=0 || Listener.Coins<2
        || TradeOffers.ContainsByPredicate([&](const FHearthTradeOffer& T) { return T.ConversationId==S->Id; }))) return false;
    if(Intent==3 && S->Offer==3)
    {
        PendingTrade=TradeOffers.FindByPredicate([&](const FHearthTradeOffer& T) { return T.ConversationId==S->Id && T.Status==TEXT("proposed"); });
        if(!PendingTrade || PendingTrade->Buyer!=Index || R.PersonalPlanks!=0 || R.Coins<PendingTrade->Price) return false;
    }
    if((Intent==7 || Intent==8) && (!HearthSocial::CanOfferTiles(*this,*S) || !HearthSocial::TileParties(*this,*S,TileCustomer,TilePotter)
        || (Intent==7 && Index!=TileCustomer) || (Intent==8 && Index!=TilePotter))) return false;
    if((Intent==3 || Intent==4) && S->Offer==4)
    {
        if(!HearthSocial::TileParties(*this,*S,TileCustomer,TilePotter)) return false;
        if(Intent==3 && (!HearthSocial::CanOfferTiles(*this,*S) || !StartTileOrder(TileCustomer,TilePotter,S->Id))) return false;
        if(Intent==4 && !RejectTileOrder(TileCustomer,TilePotter,S->Id)) return false;
    }
    FHearthDialogueLine Line; Line.Speaker=Index; Line.Intent=Intent; Line.Text=Words; Line.Source=Source; Line.At=Elapsed;
    S->Lines.Add(MoveTemp(Line));
    R.Speech=Words; R.SpeechRemaining=4.5f; Listener.SpeechRemaining=0;
    R.LatestEvent=TEXT("对")+Listener.Name+TEXT("说：")+Words;
    R.SocialNeed=FMath::Max(0.f,R.SocialNeed-15); Listener.SocialNeed=FMath::Max(0.f,Listener.SocialNeed-10);
    auto& Bond=R.Bonds.FindOrAdd(Listener.StableId); auto& Reciprocal=Listener.Bonds.FindOrAdd(R.StableId);
    Bond.Affinity=FMath::Min(100.f,Bond.Affinity+.5f); Reciprocal.Affinity=FMath::Min(100.f,Reciprocal.Affinity+.3f);
    Bond.Memory=TEXT("我说：")+Words; Reciprocal.Memory=R.Name+TEXT("说：")+Words;
    R.Mood=FMath::Min(100.f,R.Mood+1.f); Listener.Mood=FMath::Min(100.f,Listener.Mood+.5f);
    if(Intent==1 || Intent==2)
    {
        S->Offer=Intent; S->Proposer=Index; S->OfferAction=Intent==1?50:FindHelpActivity(Other);
    }
    else if(Intent==6)
    {
        --R.PersonalPlanks; FHearthTradeOffer T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.ConversationId=S->Id;
        T.Seller=Index; T.Buyer=Other; T.ReservedQuantity=1; T.Result=R.Name+TEXT("提出以2枚钱出售1块自有木板，木板已预留。"); TradeOffers.Add(MoveTemp(T));
        S->Offer=3; S->Proposer=Index; S->OfferAction=-1;
    }
    else if(Intent==7 || Intent==8)
    {
        S->Offer=4; S->Proposer=Index; S->OfferAction=-1;
    }
    else if(Intent==3)
    {
        S->bAccepted=true;
        if(S->Offer==3)
        {
            PendingTrade->Status=TEXT("accepted"); PendingTrade->Result=Index==PendingTrade->Buyer?TEXT("买方同意价格，卖方将在谈话后送货。"):TEXT("交易已接受。");
            S->Outcome=TEXT("双方同意以2枚钱交易1块自有木板。");
        }
        else if(S->Offer==4)
        {
            S->Outcome=TEXT("双方同意制瓦订单；客户钱款已托管，陶土与燃料只在真实开工时扣除，陶瓦交付后才结算。");
        }
        else
        {
            const TArray<int32> Workers=S->Offer==1?TArray<int32>{Index,Other}:TArray<int32>{Index};
            for(int32 Worker:Workers)
            {
                FHearthCommitment P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); P.ConversationId=S->Id;
                P.Worker=Worker; P.Beneficiary=Worker==Index?Other:Index; P.Action=S->OfferAction;
                Commitments.Add(MoveTemp(P));
            }
            S->Outcome=S->Offer==1?TEXT("两人约好一起去吃饭。"):TEXT("对方答应完成一项采集与运输工作。");
        }
    }
    else if(Intent==4)
    {
        if(S->Offer==3) if(auto* Trade=TradeOffers.FindByPredicate([&](const FHearthTradeOffer& T) { return T.ConversationId==S->Id && T.Status==TEXT("proposed"); }))
        { Residents[Trade->Seller].PersonalPlanks+=Trade->ReservedQuantity; Trade->ReservedQuantity=0; Trade->Status=TEXT("cancelled"); Trade->Result=TEXT("买方拒绝，预留木板已退回卖方。"); }
        if(S->Offer==4)
        {
            auto& CustomerBond=Residents[TileCustomer].Bonds.FindOrAdd(Residents[TilePotter].StableId);
            auto& PotterBond=Residents[TilePotter].Bonds.FindOrAdd(Residents[TileCustomer].StableId);
            CustomerBond.Trust=FMath::Max(0.f,CustomerBond.Trust-1.f);
            CustomerBond.Memory=TEXT("这次制瓦合作没有谈成；没有预留或转移钱和陶土。"); PotterBond.Memory=CustomerBond.Memory;
            S->Outcome=TEXT("制瓦提议被拒绝；没有预留钱或陶土，双方记住了这次未谈成的合作。");
        }
        Reciprocal.Affinity=FMath::Max(-100.f,Reciprocal.Affinity-.5f);
        Listener.Mood=FMath::Max(0.f,Listener.Mood-1.f); if(S->Offer!=4) S->Outcome=TEXT("提议被婉拒，双方保留自己的安排。");
        S->Offer=-1; S->Proposer=-1; S->OfferAction=-1;
        CloseConversation(*S,S->Outcome); ++SocialRevision; return true;
    }
    if(Intent==5 || S->Lines.Num()>=6 || (S->bAccepted && Index==S->Proposer))
        CloseConversation(*S,S->Outcome.IsEmpty()?TEXT("聊过近况，双方继续自己的生活。"):S->Outcome);
    else { S->Speaker=Other; S->TurnDelay=4.5f; }
    ++SocialRevision; return true;
}

void AHearthVillage::CloseConversation(FHearthConversation& S,const FString& Outcome)
{
    if(S.bClosed) return;
    S.bClosed=true; S.Outcome=Outcome;
    for(int32 Index:{S.First,S.Second})
    {
        auto& R=Residents[Index]; R.ConversationId.Empty(); R.Route.Reset(); R.Task=EHearthTask::LifeChoosing;
        R.Timer=0; R.NextLifeDecision=Elapsed+LifeDecisionInterval; R.LatestEvent=Outcome;
        CompleteHistory(Index,Outcome);
    }
    // Commitments only count as fulfilled after the real task deposits output or consumes food.
    for(auto& P:Commitments) if(P.ConversationId==S.Id && P.Status==TEXT("promised"))
    {
        const bool Started=StartLifeAction(P.Worker,P.Action,TEXT("履行和")+Residents[P.Beneficiary].Name+TEXT("的约定。"),false);
        P.TaskId=Residents[P.Worker].ActiveTaskId; P.Status=Started?TEXT("active"):TEXT("broken");
        if(!Started)
        {
            P.Result=TEXT("约定执行时资源或工作地点已不可用，未创造资源。");
            auto& B=Residents[P.Beneficiary].Bonds.FindOrAdd(Residents[P.Worker].StableId);
            B.Trust=FMath::Max(0.f,B.Trust-3); B.Memory=P.Result;
        }
    }
    if(S.Offer==3 && S.bAccepted)
    {
        auto* Trade=TradeOffers.FindByPredicate([&](const FHearthTradeOffer& T) { return T.ConversationId==S.Id && T.Status==TEXT("accepted"); });
        if(Trade)
        {
            TArray<FVector> Route; const FVector Target=Residents[Trade->Buyer].Actor->GetActorLocation()+FVector(0,120,0);
            const bool Routed=!bUseCropoutMap?true:FindActivityRoute(Trade->Seller,Target,Route);
            if(!bUseCropoutMap) Route={Target};
            if(Routed)
            {
                auto& Seller=Residents[Trade->Seller]; auto& Buyer=Residents[Trade->Buyer]; Seller.Task=EHearthTask::TradeTravel; Buyer.Task=EHearthTask::TradeWaiting;
                Seller.ActiveTaskId=Trade->Id; Seller.Route=MoveTemp(Route); Trade->Remaining=90.f;
                Seller.LatestEvent=TEXT("携带预留木板前往买方交货。"); Buyer.LatestEvent=TEXT("等待卖方送来木板，交货后才付款。");
            }
            else
            {
                Residents[Trade->Seller].PersonalPlanks+=Trade->ReservedQuantity; Trade->ReservedQuantity=0; Trade->Status=TEXT("cancelled"); Trade->Result=TEXT("无法找到交货路线，交易取消并退回木板。");
            }
        }
    }
    VillageEvent=Residents[S.First].Name+TEXT("和")+Residents[S.Second].Name+TEXT("：")+Outcome; ++SocialRevision;
}

void AHearthVillage::CompleteCommitments(int32 Worker,bool bSuccess,const FString& Result)
{
    for(auto& P:Commitments) if(P.Worker==Worker && P.Status==TEXT("active") && P.TaskId==Residents[Worker].ActiveTaskId)
    {
        P.Status=bSuccess?TEXT("fulfilled"):TEXT("broken"); P.Result=Result;
        auto& B=Residents[P.Beneficiary].Bonds.FindOrAdd(Residents[Worker].StableId);
        B.Trust=FMath::Clamp(B.Trust+(bSuccess?6.f:-6.f),0.f,100.f);
        B.Affinity=FMath::Clamp(B.Affinity+(bSuccess?3.f:-2.f),-100.f,100.f);
        B.Memory=Residents[Worker].Name+(bSuccess?TEXT("履行了约定："):TEXT("未能履行约定："))+Result;
        Residents[Worker].Bonds.FindOrAdd(Residents[P.Beneficiary].StableId).Memory=B.Memory; ++SocialRevision;
    }
}

void AHearthVillage::DecideSocialLocally(int32 Index,const FString& Failure)
{
    const auto* S=Conversations.FindByPredicate([Index](const auto& C) { return !C.bClosed && C.bMet && C.Speaker==Index; });
    if(!S) return;
    const int32 Other=S->First==Index?S->Second:S->First; const auto Choices=AvailableSocialIntents(Index);
    int32 Intent=0; FString Words;
    if(S->bAccepted) { Intent=5; Words=TEXT("谢谢，我们照约定去做，回头再聊。"); }
    else if(S->Offer>=0 && S->Proposer!=Index)
    {
        const auto* B=Residents[Index].Bonds.Find(Residents[Other].StableId);
        const bool WillAccept=Choices.Contains(3) && Residents[Index].Energy>=25 && (!B || B->Trust>=35);
        Intent=WillAccept?3:4;
        Words=WillAccept?(S->Offer==1?TEXT("好啊，一起去吃饭。我也想歇一会儿。"):S->Offer==3?TEXT("两枚钱可以，我等你把木板送来再付款。"):S->Offer==4?TEXT("四枚钱可以，按订单把陶土制成陶瓦后再交付吧。"):TEXT("好，我来做这项采集，把东西运回村里。")):TEXT("抱歉，我现在有些累，这次先不答应了。");
    }
    else if(S->Lines.Num()==0) Words=Residents[Other].Name+TEXT("，最近过得怎么样？新家住得还习惯吗？");
    else if(S->Lines.Num()==1) Words=TEXT("还不错，家已经安顿好了。谢谢你过来，见到邻居真好。");
    else if(Choices.Contains(6)) { Intent=6; Words=TEXT("我有一块自己的木板，两枚钱卖给你，需要吗？"); }
    else if(Choices.Contains(7)) { Intent=7; Words=TEXT("我需要一箱陶瓦，四枚钱请你用四份陶土制作六片，可以吗？"); }
    else if(Choices.Contains(8)) { Intent=8; Words=TEXT("你家需要陶瓦的话，四枚钱我可以把四份陶土制成六片。"); }
    else if(Choices.Contains(2) && AvailableWood()<60) { Intent=2; Words=TEXT("村里还需要材料，你愿意帮忙采集一趟、运回来吗？"); }
    else if(Choices.Contains(1)) { Intent=1; Words=TEXT("我们一起去村镇中心吃点东西，边走边聊吧？"); }
    else { Intent=5; Words=TEXT("聊得很开心，我先去忙了，下次再见。"); }
    ResolveSocialTurn(Index,Intent,Words,Failure.IsEmpty()?TEXT("local"):TEXT("local_fallback"));
}

void AHearthVillage::RequestSocialDecision(int32 Index)
{
    if(!HasDecisionCapacity(Index)) return;
    if(!bApiReady || bApiDisabledThisRun || ApiBackend==TEXT("codex_spark") || ApiRequests>=ApiMaxRequests)
    { DecideSocialLocally(Index,bApiConfigured?ApiStatus:FString()); return; }
    const auto* S=Conversations.FindByPredicate([Index](const auto& C) { return !C.bClosed && C.bMet && C.Speaker==Index; }); if(!S) return;
    const int32 Other=S->First==Index?S->Second:S->First;
    auto Context=MakeShared<FJsonObject>(); Context->SetStringField(TEXT("conversation_id"),S->Id);
    TArray<TSharedPtr<FJsonValue>> People,Choices,Lines;
    for(int32 I:{Index,Other})
    {
        const auto& R=Residents[I]; auto P=MakeShared<FJsonObject>();
        P->SetNumberField(TEXT("id"),I); P->SetStringField(TEXT("stable_id"),R.StableId); P->SetStringField(TEXT("name"),R.Name);
        P->SetStringField(TEXT("role"),R.Role); P->SetStringField(TEXT("personality"),R.Personality); P->SetNumberField(TEXT("age"),R.Age);
        P->SetNumberField(TEXT("energy"),R.Energy); P->SetNumberField(TEXT("hunger"),R.Hunger); P->SetNumberField(TEXT("mood"),R.Mood);
        P->SetNumberField(TEXT("coins"),R.Coins); P->SetNumberField(TEXT("personally_owned_planks"),R.PersonalPlanks); P->SetNumberField(TEXT("personally_owned_tiles"),R.PersonalTiles);
        P->SetStringField(TEXT("roof_material_need"),R.RoofMaterial);
        P->SetBoolField(TEXT("home_completed"),R.BuildProgress>=1.f); P->SetStringField(TEXT("home_plot"),PlotLabel(R.Plot));
        P->SetStringField(TEXT("latest_event"),R.LatestEvent);
        P->SetStringField(TEXT("relationships"),RelationshipSummary(I)); People.Add(MakeShared<FJsonValueObject>(P));
    }
    Context->SetArrayField(TEXT("participants_speaker_first"),People); Context->SetNumberField(TEXT("your_id"),Index);
    for(int32 Intent:AvailableSocialIntents(Index))
    { auto C=MakeShared<FJsonObject>(); C->SetNumberField(TEXT("id"),Intent); C->SetStringField(TEXT("meaning"),HearthSocial::IntentNames[Intent]); Choices.Add(MakeShared<FJsonValueObject>(C)); }
    for(const auto& Line:S->Lines)
    { auto L=MakeShared<FJsonObject>(); L->SetStringField(TEXT("speaker"),Residents[Line.Speaker].Name); L->SetNumberField(TEXT("intent_id"),Line.Intent); L->SetStringField(TEXT("words"),Line.Text); Lines.Add(MakeShared<FJsonValueObject>(L)); }
    Context->SetArrayField(TEXT("allowed_intents"),Choices); Context->SetArrayField(TEXT("conversation_so_far"),Lines);
    Context->SetStringField(TEXT("pending_offer"),S->Offer<0?TEXT("无提议"):S->Offer==1?TEXT("一起去吃饭，每人消耗1份库存食物"):S->Offer==3?TEXT("卖方以2枚钱出售1块自有木板，交货后付款"):S->Offer==4?TEXT("制瓦订单：公共库存提供4份有来源陶土和2份原木燃料，客户托管4枚钱，陶工真实制作并交付6片陶瓦后结算"):ProductionActionName(S->OfferAction));
    Context->SetNumberField(TEXT("village_clay_stock"),ClayStock);
    Context->SetStringField(TEXT("tile_order_source"),TEXT("陶土和原木燃料来自村庄已记账库存；陶瓦只由订单的真实制作阶段产生；货币只在交付结算后成为陶工收入。"));
    if(const auto* Order=TileOrders.FindByPredicate([&](const FHearthTileOrder& Candidate){return Candidate.ConversationId==S->Id;}))
    {
        Context->SetStringField(TEXT("tile_order_status"),Order->Status); Context->SetStringField(TEXT("tile_order_result"),Order->Result);
        Context->SetNumberField(TEXT("tile_order_reserved_clay"),Order->ReservedClay); Context->SetNumberField(TEXT("tile_order_reserved_tiles"),Order->ReservedTiles);
        Context->SetNumberField(TEXT("tile_order_escrow"),Order->Escrow);
    }
    else Context->SetStringField(TEXT("tile_order_status"),S->Offer==4?TEXT("awaiting_response"):TEXT("none"));
    const int32 HelpAction=FindHelpActivity(Other);
    Context->SetStringField(TEXT("available_help_job"),HelpAction<0?TEXT("无可用采集工作"):ProductionActionName(HelpAction));
    AppendProductionContext(Context);
    const FString Prompt=TEXT("You are the named medieval resident (your_id) speaking directly to the other present resident, who has a separate mind and may refuse. Continue in natural first-person Chinese, respecting current participant facts, needs and remembered relationship. A completed home already belongs to its resident; a terracotta roof may still create demand for owned replacement tiles. Select exactly one allowed_intents id and make your spoken words match it: 0=chat only; 1=invite both to eat; 2=ask the other to perform the supplied available_help_job; 3=accept the supplied pending_offer; 4=politely decline that offer; 5=say goodbye; 6=offer one personally owned plank for exactly two coins; 7=as the customer, commission the potter to turn four village-stock clay and two village-stock logs into six tiles for four coins; 8=as the potter, quote those same fixed terms to a customer with a terracotta tile roof need. Previous words alone do not create an offer. An accepted tile order only reserves customer escrow; real production later consumes recorded clay and logs, creates the tiles, and delivery settles payment. Do not claim completed work, income, ownership or transferred resources before the real order state records it. No new offer while responding to an existing offer. Return only JSON with action_id (integer) and reason (your spoken words, Chinese, at most 60 Chinese characters). reason is actual dialogue, not a description of your decision. Keep each turn brief and give the other person room to reply.");
    SendDecisionRequest(Index,Context,Prompt,true,true);
}

void AHearthVillage::AdvanceSocial(float SimulationDt)
{
    for(auto& R:Residents) R.SpeechRemaining=FMath::Max(0.f,R.SpeechRemaining-SimulationDt);
    for(auto& S:Conversations) if(!S.bClosed)
    {
        auto& A=Residents[S.First]; auto& B=Residents[S.Second];
        if(!S.bMet)
        {
            S.TravelTime+=SimulationDt;
            if(A.Task==EHearthTask::LifeActivity && FVector::Dist2D(A.Actor->GetActorLocation(),B.Actor->GetActorLocation())<=300)
            {
                S.bMet=true; S.TurnDelay=.2f;
                ++A.Bonds.FindOrAdd(B.StableId).Meetings; ++B.Bonds.FindOrAdd(A.StableId).Meetings; ++SocialRevision;
            }
            else if(S.TravelTime>90) CloseConversation(S,TEXT("路上耽搁，未能见面；这次没有完成社交。"));
            continue;
        }
        const FVector Direction=B.Actor->GetActorLocation()-A.Actor->GetActorLocation();
        A.Actor->SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0)); B.Actor->SetActorRotation(FRotator(0,Direction.Rotation().Yaw+180,0));
        S.TurnDelay=FMath::Max(0.f,S.TurnDelay-SimulationDt);
        if(S.TurnDelay<=0 && !IsDecisionPending(S.Speaker)) RequestSocialDecision(S.Speaker);
    }
}

FString AHearthVillage::RelationshipSummary(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return FString(); TArray<FString> Lines;
    for(int32 Other=0;Other<Residents.Num();++Other) if(Other!=Index)
        if(const auto* B=Residents[Index].Bonds.Find(Residents[Other].StableId))
            Lines.Add(FString::Printf(TEXT("%s：好感 %.0f，信任 %.0f，见面 %d 次。%s"),*Residents[Other].Name,B->Affinity,B->Trust,B->Meetings,*B->Memory));
    return Lines.IsEmpty()?TEXT("还没有交谈过的邻居。"):FString::Join(Lines,TEXT("\n"));
}

FString AHearthVillage::GetSocialState(int32 Index) const
{
    auto Root=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Chats,Promises,Orders;
    Root->SetStringField(TEXT("relationships"),Index>=0?RelationshipSummary(Index):TEXT(""));
    for(const auto& S:Conversations) if(Index<0 || S.First==Index || S.Second==Index)
    {
        auto C=MakeShared<FJsonObject>(); C->SetStringField(TEXT("id"),S.Id); C->SetNumberField(TEXT("first"),S.First); C->SetNumberField(TEXT("second"),S.Second);
        C->SetBoolField(TEXT("met"),S.bMet); C->SetBoolField(TEXT("closed"),S.bClosed); C->SetStringField(TEXT("outcome"),S.Outcome);
        TArray<TSharedPtr<FJsonValue>> Lines;
        for(const auto& L:S.Lines) { auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("speaker"),L.Speaker); J->SetNumberField(TEXT("intent"),L.Intent); J->SetNumberField(TEXT("at"),L.At); J->SetStringField(TEXT("text"),L.Text); J->SetStringField(TEXT("source"),L.Source); Lines.Add(MakeShared<FJsonValueObject>(J)); }
        C->SetArrayField(TEXT("lines"),Lines); Chats.Add(MakeShared<FJsonValueObject>(C));
    }
    for(const auto& P:Commitments) if(Index<0 || P.Worker==Index || P.Beneficiary==Index)
    { auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),P.Id); J->SetNumberField(TEXT("worker"),P.Worker); J->SetNumberField(TEXT("beneficiary"),P.Beneficiary); J->SetNumberField(TEXT("action"),P.Action); J->SetStringField(TEXT("status"),P.Status); J->SetStringField(TEXT("result"),P.Result); Promises.Add(MakeShared<FJsonValueObject>(J)); }
    for(const auto& O:TileOrders) if(Index<0 || O.Customer==Index || O.Potter==Index)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("id"),O.Id); J->SetStringField(TEXT("conversation_id"),O.ConversationId); J->SetStringField(TEXT("status"),O.Status); J->SetStringField(TEXT("result"),O.Result);
        J->SetNumberField(TEXT("customer"),O.Customer); J->SetNumberField(TEXT("potter"),O.Potter); J->SetNumberField(TEXT("clay_quantity"),O.ClayQuantity); J->SetNumberField(TEXT("tile_quantity"),O.TileQuantity); J->SetNumberField(TEXT("price"),O.Price);
        J->SetNumberField(TEXT("reserved_clay"),O.ReservedClay); J->SetNumberField(TEXT("reserved_tiles"),O.ReservedTiles); J->SetNumberField(TEXT("escrow"),O.Escrow); J->SetStringField(TEXT("source"),TEXT("村庄陶土和原木库存 → 陶工制瓦 → 客户个人陶瓦；客户托管款在交付后结算")); Orders.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("conversations"),Chats); Root->SetArrayField(TEXT("commitments"),Promises); Root->SetArrayField(TEXT("tile_orders"),Orders); return HearthSocial::Json(Root);
}
