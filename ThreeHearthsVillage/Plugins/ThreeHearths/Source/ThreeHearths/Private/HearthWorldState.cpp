#include "HearthWorldState.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#endif

namespace HearthWorld
{
    using FObject=TSharedPtr<FJsonObject>;
    using FArray=TArray<TSharedPtr<FJsonValue>>;
    constexpr int64 MaxBytes=64*1024*1024;
    FString Json(const TSharedRef<FJsonObject>& J)
    { FString S; FJsonSerializer::Serialize(J,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&S)); return S; }
    TSharedPtr<FJsonValue> Vec(const FVector& P)
    { return MakeShared<FJsonValueArray>(FArray{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}); }
    bool Guid(const FString& S,bool Optional=false)
    { FGuid G; return (Optional && S.IsEmpty()) || (FGuid::Parse(S,G) && G.IsValid()); }
    struct FRead
    {
        FObject J; bool Good=true;
        template<typename T> void Num(const TCHAR* K,T& V,double Min,double Max)
        {
            double N=0;
            if(!J.IsValid() || !J->HasTypedField<EJson::Number>(K) || !J->TryGetNumberField(K,N) || !FMath::IsFinite(N) || N<Min || N>Max
                || (TIsIntegral<T>::Value && N!=FMath::FloorToDouble(N))) { Good=false; return; }
            V=static_cast<T>(N);
        }
        void Str(const TCHAR* K,FString& V,int32 Max=4096)
        { if(!J.IsValid() || !J->TryGetStringField(K,V) || V.Len()>Max) Good=false; }
        void Bool(const TCHAR* K,bool& V)
        { if(!J.IsValid() || !J->TryGetBoolField(K,V)) Good=false; }
        const FArray& Array(const TCHAR* K,int32 Max)
        {
            const FArray* A=nullptr; static const FArray Empty;
            if(!J.IsValid() || !J->TryGetArrayField(K,A) || A->Num()>Max) { Good=false; return Empty; }
            return *A;
        }
        void Vector(const TSharedPtr<FJsonValue>& Value,FVector& P)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Array || Value->AsArray().Num()!=3) { Good=false; return; }
            for(int32 I=0;I<3;++I)
            { double N=0; if(!Value->AsArray()[I]->TryGetNumber(N) || !FMath::IsFinite(N) || FMath::Abs(N)>1000000) Good=false; else P[I]=N; }
        }
        void Vector(const TCHAR* K,FVector& P) { Vector(J.IsValid()?J->TryGetField(K):nullptr,P); }
    };
    FObject Object(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type==EJson::Object?V->AsObject():nullptr; }

    FString Encode(const FHearthWorldImage& W)
    {
        auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("schema"),5); J->SetNumberField(TEXT("plot_count"),W.PlotCount);
#define STR(Field) J->SetStringField(TEXT(#Field),W.Field)
#define NUM(Field) J->SetNumberField(TEXT(#Field),W.Field)
#define BOOL(Field) J->SetBoolField(TEXT(#Field),W.Field)
        STR(Id); STR(Run); STR(Event); NUM(Revision); NUM(Elapsed); NUM(Speed); NUM(Remainder);
        NUM(Selected); NUM(LastLife); NUM(Food); NUM(Stone); NUM(Planks); NUM(Beams); NUM(TreasuryCoins);
        J->SetNumberField(TEXT("ProducedPlanks"),W.Manufactured[0]); J->SetNumberField(TEXT("ProducedBeams"),W.Manufactured[1]);
        J->SetNumberField(TEXT("SpentPlanks"),W.ManufacturedSpent[0]); J->SetNumberField(TEXT("SpentBeams"),W.ManufacturedSpent[1]);
        BOOL(bIsland); BOOL(bPaused); BOOL(bAutonomy); BOOL(bComplete);
#undef STR
#undef NUM
#undef BOOL
        FArray Plots,Resources,People,Sites,History;
        for(int32 I=0;I<W.PlotCount;++I)
        {
            auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("id"),W.PlotIds[I]);
            P->SetField(TEXT("position"),Vec(W.Plots[I]));
            P->SetNumberField(TEXT("owner"),W.Owners[I]); P->SetNumberField(TEXT("cost"),W.Costs[I]);
            Plots.Add(MakeShared<FJsonValueObject>(P));
        }
        for(int32 I=0;I<3;++I)
        {
            auto P=MakeShared<FJsonObject>(); P->SetField(TEXT("stock_position"),Vec(W.Stocks[I])); P->SetNumberField(TEXT("wood"),W.Wood[I]);
            P->SetNumberField(TEXT("produced"),W.Produced[I]); P->SetNumberField(TEXT("spent"),W.Spent[I]); Resources.Add(MakeShared<FJsonValueObject>(P));
        }
        for(const auto& Saved:W.People)
        {
            const auto& R=Saved.Person; auto P=MakeShared<FJsonObject>();
#define STR(Field) P->SetStringField(TEXT(#Field),R.Field)
#define NUM(Field) P->SetNumberField(TEXT(#Field),R.Field)
            STR(StableId); STR(ActiveTaskId); STR(Name); STR(Personality); STR(Reason); STR(LatestEvent); STR(DecisionSource); STR(DecisionNote);
            STR(HouseBlueprint); STR(WallMaterial); STR(RoofMaterial);
            STR(HeldToolId); STR(HeldToolOperationId);
            STR(Role); NUM(Hunger); NUM(Mood); NUM(Age); P->SetBoolField(TEXT("king"),R.bKing);
            STR(ConversationId); STR(Speech); STR(ProductionComponentId); NUM(SpeechRemaining);
            FArray Bonds;
            for(const auto& Pair:R.Bonds)
            {
                auto B=MakeShared<FJsonObject>(); B->SetStringField(TEXT("other_id"),Pair.Key);
                B->SetNumberField(TEXT("affinity"),Pair.Value.Affinity); B->SetNumberField(TEXT("trust"),Pair.Value.Trust);
                B->SetNumberField(TEXT("meetings"),Pair.Value.Meetings); B->SetStringField(TEXT("memory"),Pair.Value.Memory); Bonds.Add(MakeShared<FJsonValueObject>(B));
            }
            P->SetArrayField(TEXT("bonds"),Bonds);
            NUM(Plot); NUM(CarriedWood); NUM(DeliveredWood); NUM(BuildProgress); NUM(Energy); NUM(SocialNeed); NUM(Coins); NUM(PersonalPlanks);
            NUM(Source); NUM(Trips); NUM(Timer); NUM(MoveSpeed); NUM(MoveRetry); NUM(HistoryIndex);
            NUM(LifeAction); NUM(ProductionSite); NUM(ProductionOp); NUM(CargoType); NUM(CargoAmount); NUM(WorkDuration);
#undef STR
#undef NUM
            P->SetNumberField(TEXT("Task"),static_cast<int32>(R.Task)); P->SetBoolField(TEXT("bMovementBlocked"),R.bMovementBlocked);
            P->SetField(TEXT("position"),Vec(Saved.Position)); P->SetNumberField(TEXT("yaw"),Saved.Yaw);
            P->SetNumberField(TEXT("decision_delay"),Saved.DecisionDelay); P->SetBoolField(TEXT("pending"),Saved.bPending);
            P->SetStringField(TEXT("pending_operation"),Saved.PendingOperation);
            FArray Route; for(const auto& Point:R.Route) Route.Add(Vec(Point)); P->SetArrayField(TEXT("route"),Route);
            People.Add(MakeShared<FJsonValueObject>(P));
        }
        for(const auto& S:W.Sites)
        {
            auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("id"),S.StableId);
            P->SetStringField(TEXT("build_plan_id"),S.BuildPlanId);
            P->SetNumberField(TEXT("kind"),static_cast<int32>(S.Kind)); P->SetField(TEXT("position"),Vec(S.Position)); P->SetField(TEXT("approach"),Vec(S.Approach));
#define NUM(Field) P->SetNumberField(TEXT(#Field),S.Field)
            NUM(Radius); NUM(Growth); NUM(GrowDuration); NUM(Progress); NUM(Stage); NUM(Units); NUM(Capacity); NUM(ReservedBy); NUM(Owner);
#undef NUM
            P->SetBoolField(TEXT("reachable"),S.bReachable); P->SetBoolField(TEXT("expansion"),S.bExpansion);
            FArray Components;
            for(const auto& C:S.CottageComponents)
            {
                auto Part=MakeShared<FJsonObject>(); Part->SetStringField(TEXT("id"),C.Id); Part->SetStringField(TEXT("asset_id"),C.AssetId);
                Part->SetStringField(TEXT("status"),C.Status); Part->SetStringField(TEXT("source"),C.Source); Part->SetStringField(TEXT("supply_policy"),C.SupplyPolicy);
                Part->SetField(TEXT("offset"),Vec(C.Offset)); Part->SetNumberField(TEXT("yaw"),C.Yaw); Part->SetNumberField(TEXT("stage"),C.Stage);
                Part->SetNumberField(TEXT("material_type"),C.MaterialType); Part->SetNumberField(TEXT("material_amount"),C.MaterialAmount);
                Part->SetNumberField(TEXT("owner"),C.Owner); Part->SetNumberField(TEXT("reserved_by"),C.ReservedBy); Components.Add(MakeShared<FJsonValueObject>(Part));
            }
            P->SetArrayField(TEXT("cottage_components"),Components);
            Sites.Add(MakeShared<FJsonValueObject>(P));
        }
        for(const auto& H:W.History)
        {
            auto P=MakeShared<FJsonObject>();
#define STR(Field) P->SetStringField(TEXT(#Field),H.Field)
#define NUM(Field) P->SetNumberField(TEXT(#Field),H.Field)
            STR(Run); STR(Timestamp); STR(Kind); STR(Context); STR(Choice); STR(Reason); STR(Result); STR(Source); STR(Model); STR(Status);
            NUM(Resident); NUM(Tokens); NUM(At); NUM(Latency); P->SetBoolField(TEXT("bHasUsage"),H.bHasUsage);
#undef STR
#undef NUM
            History.Add(MakeShared<FJsonValueObject>(P));
        }
        FArray Chats,Promises,Transactions;
        for(const auto& S:W.Conversations)
        {
            auto C=MakeShared<FJsonObject>();
#define STR(Field) C->SetStringField(TEXT(#Field),S.Field)
#define NUM(Field) C->SetNumberField(TEXT(#Field),S.Field)
            STR(Id); STR(FirstId); STR(SecondId); STR(Outcome); NUM(First); NUM(Second); NUM(Speaker); NUM(Offer); NUM(Proposer); NUM(OfferAction); NUM(TravelTime); NUM(TurnDelay);
#undef STR
#undef NUM
            C->SetBoolField(TEXT("met"),S.bMet); C->SetBoolField(TEXT("closed"),S.bClosed); C->SetBoolField(TEXT("accepted"),S.bAccepted);
            FArray Lines;
            for(const auto& L:S.Lines) { auto P=MakeShared<FJsonObject>(); P->SetNumberField(TEXT("speaker"),L.Speaker); P->SetNumberField(TEXT("intent"),L.Intent); P->SetNumberField(TEXT("at"),L.At); P->SetStringField(TEXT("text"),L.Text); P->SetStringField(TEXT("source"),L.Source); Lines.Add(MakeShared<FJsonValueObject>(P)); }
            C->SetArrayField(TEXT("lines"),Lines); Chats.Add(MakeShared<FJsonValueObject>(C));
        }
        for(const auto& S:W.Commitments)
        {
            auto C=MakeShared<FJsonObject>();
#define STR(Field) C->SetStringField(TEXT(#Field),S.Field)
#define NUM(Field) C->SetNumberField(TEXT(#Field),S.Field)
            STR(Id); STR(ConversationId); STR(TaskId); STR(Status); STR(Result); NUM(Worker); NUM(Beneficiary); NUM(Action);
#undef STR
#undef NUM
            Promises.Add(MakeShared<FJsonValueObject>(C));
        }
        for(const auto& T:W.Transactions)
        {
            auto Entry=MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("id"),T.Id); Entry->SetStringField(TEXT("kind"),T.Kind);
            Entry->SetStringField(TEXT("task_id"),T.TaskId); Entry->SetStringField(TEXT("item"),T.Item);
            Entry->SetNumberField(TEXT("from"),T.From); Entry->SetNumberField(TEXT("to"),T.To); Entry->SetNumberField(TEXT("amount"),T.Amount);
            Entry->SetNumberField(TEXT("quantity"),T.Quantity); Entry->SetNumberField(TEXT("at"),T.At); Transactions.Add(MakeShared<FJsonValueObject>(Entry));
        }
        FArray Payables,Trades;
        for(const auto& P:W.WagePayables)
        {
            auto Entry=MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("id"),P.Id); Entry->SetStringField(TEXT("task_id"),P.TaskId);
            Entry->SetStringField(TEXT("status"),P.Status); Entry->SetNumberField(TEXT("worker"),P.Worker); Entry->SetNumberField(TEXT("amount"),P.Amount);
            Payables.Add(MakeShared<FJsonValueObject>(Entry));
        }
        for(const auto& T:W.TradeOffers)
        {
            auto Entry=MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("id"),T.Id); Entry->SetStringField(TEXT("conversation_id"),T.ConversationId); Entry->SetStringField(TEXT("status"),T.Status); Entry->SetStringField(TEXT("result"),T.Result);
            Entry->SetNumberField(TEXT("seller"),T.Seller); Entry->SetNumberField(TEXT("buyer"),T.Buyer); Entry->SetNumberField(TEXT("quantity"),T.Quantity);
            Entry->SetNumberField(TEXT("price"),T.Price); Entry->SetNumberField(TEXT("reserved_quantity"),T.ReservedQuantity); Entry->SetNumberField(TEXT("remaining"),T.Remaining);
            Trades.Add(MakeShared<FJsonValueObject>(Entry));
        }
        J->SetArrayField(TEXT("conversations"),Chats); J->SetArrayField(TEXT("commitments"),Promises);
        J->SetArrayField(TEXT("transactions"),Transactions);
        J->SetArrayField(TEXT("wage_payables"),Payables); J->SetArrayField(TEXT("trade_offers"),Trades);
        auto Totals=MakeShared<FJsonObject>(); for(const auto& Pair:W.Totals) Totals->SetNumberField(Pair.Key,Pair.Value);
        J->SetObjectField(TEXT("totals"),Totals); J->SetArrayField(TEXT("plots"),Plots); J->SetArrayField(TEXT("resources"),Resources); J->SetArrayField(TEXT("people"),People);
        J->SetArrayField(TEXT("sites"),Sites); J->SetArrayField(TEXT("history"),History); return Json(J);
    }

    bool Decode(const FString& Text,FHearthWorldImage& Out,FString& Error)
    {
        Error=TEXT("世界存档格式或字段无效"); FObject Root;
        if(Text.Len()>MaxBytes || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid()) return false;
        FHearthWorldImage W; FRead C{Root}; C.Num(TEXT("schema"),W.Schema,1,5);
        const bool HasEconomy=W.Schema>=4 || Root->HasField(TEXT("TreasuryCoins"));
        if(W.Schema>=2) C.Num(TEXT("plot_count"),W.PlotCount,3,10);
        if(W.PlotCount!=3 && W.PlotCount!=10) return false;
#define STR(Field) C.Str(TEXT(#Field),W.Field)
#define NUM(Field,Min,Max) C.Num(TEXT(#Field),W.Field,Min,Max)
#define BOOL(Field) C.Bool(TEXT(#Field),W.Field)
        STR(Id); STR(Run); STR(Event); NUM(Revision,0,9007199254740991.0); NUM(Elapsed,0,1e9); NUM(Speed,1,1000); NUM(Remainder,0,300);
        NUM(Selected,0,W.PlotCount-1); NUM(LastLife,-1,W.PlotCount-1); NUM(Food,0,1e8); NUM(Stone,0,1e8);
        if(W.Schema>=4 || Root->HasField(TEXT("TreasuryCoins"))) NUM(TreasuryCoins,0,1e8);
        if(Root->HasField(TEXT("Planks"))) NUM(Planks,0,1e8);
        if(Root->HasField(TEXT("Beams"))) NUM(Beams,0,1e8);
        if(Root->HasField(TEXT("ProducedPlanks"))) C.Num(TEXT("ProducedPlanks"),W.Manufactured[0],0,1e8);
        if(Root->HasField(TEXT("ProducedBeams"))) C.Num(TEXT("ProducedBeams"),W.Manufactured[1],0,1e8);
        if(Root->HasField(TEXT("SpentPlanks"))) C.Num(TEXT("SpentPlanks"),W.ManufacturedSpent[0],0,1e8);
        if(Root->HasField(TEXT("SpentBeams"))) C.Num(TEXT("SpentBeams"),W.ManufacturedSpent[1],0,1e8);
        BOOL(bIsland); BOOL(bPaused); BOOL(bAutonomy); BOOL(bComplete);
#undef STR
#undef NUM
#undef BOOL
        const auto& Plots=C.Array(TEXT("plots"),10); if(Plots.Num()!=W.PlotCount) return false;
        for(int32 I=0;I<Plots.Num();++I)
        {
            FRead P{Object(Plots[I])}; P.Str(TEXT("id"),W.PlotIds[I]); P.Vector(TEXT("position"),W.Plots[I]);
            P.Num(TEXT("owner"),W.Owners[I],-1,W.PlotCount-1); P.Num(TEXT("cost"),W.Costs[I],1,1000000); C.Good&=P.Good;
        }
        const auto& Resources=W.Schema==1?Plots:C.Array(TEXT("resources"),3); if(Resources.Num()!=3) return false;
        for(int32 I=0;I<3;++I)
        {
            FRead P{Object(Resources[I])}; P.Vector(TEXT("stock_position"),W.Stocks[I]); P.Num(TEXT("wood"),W.Wood[I],0,1e8);
            P.Num(TEXT("produced"),W.Produced[I],0,1e8); P.Num(TEXT("spent"),W.Spent[I],0,1e8); C.Good&=P.Good;
        }
        for(const auto& V:C.Array(TEXT("people"),10))
        {
            FHearthSavedResident Saved; auto& R=Saved.Person; FRead P{Object(V)}; int32 Task=0;
#define STR(Field) P.Str(TEXT(#Field),R.Field)
#define NUM(Field,Min,Max) P.Num(TEXT(#Field),R.Field,Min,Max)
            STR(StableId); STR(ActiveTaskId); STR(Name); STR(Personality); STR(Reason); STR(LatestEvent); STR(DecisionSource); STR(DecisionNote);
            if(P.J.IsValid())
            {
                P.J->TryGetStringField(TEXT("HouseBlueprint"),R.HouseBlueprint);
                P.J->TryGetStringField(TEXT("WallMaterial"),R.WallMaterial);
                P.J->TryGetStringField(TEXT("RoofMaterial"),R.RoofMaterial);
                P.J->TryGetStringField(TEXT("HeldToolId"),R.HeldToolId);
                P.J->TryGetStringField(TEXT("HeldToolOperationId"),R.HeldToolOperationId);
                P.J->TryGetStringField(TEXT("ProductionComponentId"),R.ProductionComponentId);
            }
            if(W.Schema>=2) { STR(Role); NUM(Hunger,0,100); NUM(Mood,0,100); NUM(Age,18,120); P.Bool(TEXT("king"),R.bKing); }
            if(W.Schema>=3)
            {
                STR(ConversationId); STR(Speech); NUM(SpeechRemaining,0,60);
                for(const auto& BondValue:P.Array(TEXT("bonds"),100))
                {
                    FRead B{Object(BondValue)}; FString Other; FHearthBond Bond;
                    B.Str(TEXT("other_id"),Other); B.Num(TEXT("affinity"),Bond.Affinity,-100,100); B.Num(TEXT("trust"),Bond.Trust,0,100);
                    B.Num(TEXT("meetings"),Bond.Meetings,0,1e8); B.Str(TEXT("memory"),Bond.Memory);
                    P.Good&=B.Good && !R.Bonds.Contains(Other); R.Bonds.Add(Other,MoveTemp(Bond));
                }
            }
            NUM(Plot,-1,W.PlotCount-1); NUM(CarriedWood,0,3); NUM(DeliveredWood,0,1000000); NUM(BuildProgress,0,1); NUM(Energy,0,100); NUM(SocialNeed,0,100);
            if(W.Schema>=4 || (P.J.IsValid() && P.J->HasField(TEXT("Coins")))) NUM(Coins,0,100000000);
            if(W.Schema>=4) NUM(PersonalPlanks,0,100000000);
            NUM(Source,-1,2); NUM(Trips,0,1e8); NUM(Timer,-1e9,1e6); NUM(MoveSpeed,1,2000); NUM(MoveRetry,0,1e6); NUM(HistoryIndex,-1,49999);
            NUM(LifeAction,-1,100000); NUM(ProductionSite,-1,1023); NUM(ProductionOp,-1,14); NUM(CargoType,-1,4); NUM(CargoAmount,0,6); NUM(WorkDuration,0,1e6);
#undef STR
#undef NUM
            P.Num(TEXT("Task"),Task,0,static_cast<int32>(EHearthTask::TradeWaiting)); R.Task=static_cast<EHearthTask>(Task);
            P.Bool(TEXT("bMovementBlocked"),R.bMovementBlocked); P.Vector(TEXT("position"),Saved.Position); P.Num(TEXT("yaw"),Saved.Yaw,-360,360);
            P.Num(TEXT("decision_delay"),Saved.DecisionDelay,0,86400); P.Bool(TEXT("pending"),Saved.bPending); P.Str(TEXT("pending_operation"),Saved.PendingOperation);
            for(const auto& Point:P.Array(TEXT("route"),4096)) { FVector Position; P.Vector(Point,Position); R.Route.Add(Position); }
            C.Good&=P.Good; W.People.Add(MoveTemp(Saved));
        }
        for(const auto& V:C.Array(TEXT("sites"),1024))
        {
            FHearthSite S; FRead P{Object(V)}; int32 Kind=0; P.Str(TEXT("id"),S.StableId); if(P.J.IsValid()) P.J->TryGetStringField(TEXT("build_plan_id"),S.BuildPlanId); P.Num(TEXT("kind"),Kind,0,10); S.Kind=static_cast<EHearthSiteKind>(Kind);
            P.Vector(TEXT("position"),S.Position); P.Vector(TEXT("approach"),S.Approach);
#define NUM(Field,Min,Max) P.Num(TEXT(#Field),S.Field,Min,Max)
            NUM(Radius,1,10000); NUM(Growth,0,1e6); NUM(GrowDuration,1,1e6); NUM(Progress,0,1); NUM(Stage,0,4);
            NUM(Units,0,1e6); NUM(Capacity,0,1e6); NUM(ReservedBy,-1,W.PlotCount-1); NUM(Owner,-1,W.PlotCount-1);
#undef NUM
            P.Bool(TEXT("reachable"),S.bReachable); P.Bool(TEXT("expansion"),S.bExpansion);
            if(W.Schema>=5)
            {
                for(const auto& Value:P.Array(TEXT("cottage_components"),45))
                {
                    FHearthCottageComponent Part; FRead Q{Object(Value)}; Q.Str(TEXT("id"),Part.Id); Q.Str(TEXT("asset_id"),Part.AssetId);
                    Q.Str(TEXT("status"),Part.Status); Q.Str(TEXT("source"),Part.Source); Q.Str(TEXT("supply_policy"),Part.SupplyPolicy);
                    Q.Vector(TEXT("offset"),Part.Offset); Q.Num(TEXT("yaw"),Part.Yaw,-360,360); Q.Num(TEXT("stage"),Part.Stage,1,4);
                    Q.Num(TEXT("material_type"),Part.MaterialType,2,4); Q.Num(TEXT("material_amount"),Part.MaterialAmount,1,4);
                    Q.Num(TEXT("owner"),Part.Owner,-1,W.PlotCount-1); Q.Num(TEXT("reserved_by"),Part.ReservedBy,-1,W.PlotCount-1);
                    P.Good&=Q.Good; S.CottageComponents.Add(MoveTemp(Part));
                }
            }
            C.Good&=P.Good; W.Sites.Add(MoveTemp(S));
        }
        for(const auto& V:C.Array(TEXT("history"),50000))
        {
            FHearthDecisionRecord H; FRead P{Object(V)};
#define STR(Field) P.Str(TEXT(#Field),H.Field,32768)
            STR(Run); STR(Timestamp); STR(Kind); STR(Context); STR(Choice); STR(Reason); STR(Result); STR(Source); STR(Model); STR(Status);
#undef STR
            P.Num(TEXT("Resident"),H.Resident,0,9); P.Num(TEXT("Tokens"),H.Tokens,0,1e8); P.Num(TEXT("At"),H.At,0,1e9); P.Num(TEXT("Latency"),H.Latency,0,1e6);
            P.Bool(TEXT("bHasUsage"),H.bHasUsage); C.Good&=P.Good; W.History.Add(MoveTemp(H));
        }
        if(W.Schema>=3)
        {
            for(const auto& V:C.Array(TEXT("conversations"),10000))
            {
                FHearthConversation S; FRead P{Object(V)};
#define STR(Field) P.Str(TEXT(#Field),S.Field)
#define NUM(Field,Min,Max) P.Num(TEXT(#Field),S.Field,Min,Max)
                STR(Id); STR(FirstId); STR(SecondId); STR(Outcome);
                NUM(First,0,W.PlotCount-1); NUM(Second,0,W.PlotCount-1); NUM(Speaker,0,W.PlotCount-1);
                NUM(Offer,-1,3); NUM(Proposer,-1,W.PlotCount-1); NUM(OfferAction,-1,20000); NUM(TravelTime,0,1000); NUM(TurnDelay,0,60);
#undef STR
#undef NUM
                P.Bool(TEXT("met"),S.bMet); P.Bool(TEXT("closed"),S.bClosed); P.Bool(TEXT("accepted"),S.bAccepted);
                for(const auto& Value:P.Array(TEXT("lines"),8))
                {
                    FRead L{Object(Value)}; FHearthDialogueLine Line;
                    L.Num(TEXT("speaker"),Line.Speaker,0,W.PlotCount-1); L.Num(TEXT("intent"),Line.Intent,0,6); L.Num(TEXT("at"),Line.At,0,1e9);
                    L.Str(TEXT("text"),Line.Text,180); L.Str(TEXT("source"),Line.Source); P.Good&=L.Good; S.Lines.Add(MoveTemp(Line));
                }
                C.Good&=P.Good; W.Conversations.Add(MoveTemp(S));
            }
            for(const auto& V:C.Array(TEXT("commitments"),20000))
            {
                FHearthCommitment S; FRead P{Object(V)};
#define STR(Field) P.Str(TEXT(#Field),S.Field)
#define NUM(Field,Min,Max) P.Num(TEXT(#Field),S.Field,Min,Max)
                STR(Id); STR(ConversationId); STR(TaskId); STR(Status); STR(Result);
                NUM(Worker,0,W.PlotCount-1); NUM(Beneficiary,0,W.PlotCount-1); NUM(Action,0,20000);
#undef STR
#undef NUM
                C.Good&=P.Good; W.Commitments.Add(MoveTemp(S));
            }
            const FArray EmptyTransactions;
            const FArray& SavedTransactions=(W.Schema>=4 || Root->HasField(TEXT("transactions")))?C.Array(TEXT("transactions"),100000):EmptyTransactions;
            for(const auto& V:SavedTransactions)
            {
                FHearthTransaction T; FRead P{Object(V)};
                P.Str(TEXT("id"),T.Id); P.Str(TEXT("kind"),T.Kind); P.Str(TEXT("task_id"),T.TaskId); P.Str(TEXT("item"),T.Item);
                P.Num(TEXT("from"),T.From,-1,W.PlotCount-1); P.Num(TEXT("to"),T.To,-1,W.PlotCount-1);
                P.Num(TEXT("amount"),T.Amount,1,100000000); P.Num(TEXT("quantity"),T.Quantity,1,100000000); P.Num(TEXT("at"),T.At,0,1e9);
                C.Good&=P.Good; W.Transactions.Add(MoveTemp(T));
            }
            if(W.Schema>=4)
            {
                for(const auto& V:C.Array(TEXT("wage_payables"),100000))
                {
                    FHearthWagePayable S; FRead P{Object(V)}; P.Str(TEXT("id"),S.Id); P.Str(TEXT("task_id"),S.TaskId); P.Str(TEXT("status"),S.Status);
                    P.Num(TEXT("worker"),S.Worker,0,W.PlotCount-1); P.Num(TEXT("amount"),S.Amount,1,1000); C.Good&=P.Good; W.WagePayables.Add(MoveTemp(S));
                }
                for(const auto& V:C.Array(TEXT("trade_offers"),100000))
                {
                    FHearthTradeOffer S; FRead P{Object(V)}; P.Str(TEXT("id"),S.Id); if(P.J.IsValid()) P.J->TryGetStringField(TEXT("conversation_id"),S.ConversationId); P.Str(TEXT("status"),S.Status); P.Str(TEXT("result"),S.Result);
                    P.Num(TEXT("seller"),S.Seller,0,W.PlotCount-1); P.Num(TEXT("buyer"),S.Buyer,0,W.PlotCount-1);
                    P.Num(TEXT("quantity"),S.Quantity,1,1000); P.Num(TEXT("price"),S.Price,1,1000000); P.Num(TEXT("reserved_quantity"),S.ReservedQuantity,0,1000);
                    P.Num(TEXT("remaining"),S.Remaining,0,1000); C.Good&=P.Good; W.TradeOffers.Add(MoveTemp(S));
                }
            }
        }
        const FObject* Totals=nullptr;
        if(!Root->TryGetObjectField(TEXT("totals"),Totals) || (*Totals)->Values.Num()>1000) C.Good=false;
        else for(const auto& Pair:(*Totals)->Values)
        { int32 Count=0; FRead T{*Totals}; T.Num(*Pair.Key,Count,0,1e8); C.Good&=T.Good && Pair.Key.Len()<100; W.Totals.Add(FString(Pair.Key),Count); }
        if(!C.Good || W.People.Num()!=W.PlotCount || !Guid(W.Id) || !Guid(W.Run)) return false;
        if(W.Schema<5)
        {
            for(auto& Site:W.Sites) if(!Site.BuildPlanId.IsEmpty()) HearthCottage::Populate(Site);
            for(int32 I=0;I<W.People.Num();++I)
            {
                auto& R=W.People[I].Person; if(R.ProductionOp!=5 || !W.Sites.IsValidIndex(R.ProductionSite)) continue;
                auto& Site=W.Sites[R.ProductionSite]; auto* Part=Site.CottageComponents.FindByPredicate([](const auto& P){ return P.Status!=TEXT("completed"); });
                if(!Part) return false; Part->ReservedBy=I; Part->MaterialType=R.CargoType; Part->MaterialAmount=R.CargoAmount;
                Part->Status=R.Task==EHearthTask::ProductionWork?TEXT("installing"):TEXT("transporting"); R.ProductionComponentId=Part->Id;
            }
        }
        Error=TEXT("世界存档引用、任务或资源守恒校验失败");
        TSet<FString> Ids;
        TSet<FString> TransactionKeys;
        TSet<FString> HeldTools;
        auto Unique=[&Ids](const FString& Id) { if(!Guid(Id) || Ids.Contains(Id)) return false; Ids.Add(Id); return true; };
        if(!Unique(W.Id)) return false;
        for(int32 I=0;I<W.PlotCount;++I) if(!Unique(W.PlotIds[I])) return false;
        int64 Accounted[3]={W.Food+W.Spent[0]-W.Produced[0],W.Wood[0]+W.Wood[1]+W.Wood[2]+W.Spent[1]-W.Produced[1],W.Stone+W.Spent[2]-W.Produced[2]};
        int64 AccountedManufactured[2]={W.Planks+W.ManufacturedSpent[0]-W.Manufactured[0],W.Beams+W.ManufacturedSpent[1]-W.Manufactured[1]};
        for(int32 I=0;I<W.People.Num();++I)
        {
            const auto& Saved=W.People[I]; const auto& R=Saved.Person;
            if(!Unique(R.StableId) || (!R.ActiveTaskId.IsEmpty() && !Unique(R.ActiveTaskId)) || !Guid(Saved.PendingOperation,true) || R.Name.IsEmpty()) return false;
            const bool NoHouseStyle=R.HouseBlueprint.IsEmpty() && R.WallMaterial.IsEmpty() && R.RoofMaterial.IsEmpty();
            const bool Cottage=R.HouseBlueprint==TEXT("cottage_terracotta") && R.WallMaterial==TEXT("plaster") && R.RoofMaterial==TEXT("terracotta");
            const bool Longhouse=R.HouseBlueprint==TEXT("longhouse_slateblue") && R.WallMaterial==TEXT("timber") && R.RoofMaterial==TEXT("slateblue");
            const bool Townhouse=R.HouseBlueprint==TEXT("townhouse_terracotta") && R.WallMaterial==TEXT("stone") && R.RoofMaterial==TEXT("terracotta");
            if(!NoHouseStyle && !Cottage && !Longhouse && !Townhouse) return false;
            const TSet<FString> KnownTools={TEXT("tool_hammer"),TEXT("tool_mallet"),TEXT("tool_axe"),TEXT("tool_saw"),TEXT("tool_pickaxe"),TEXT("tool_shovel"),TEXT("tool_hoe"),TEXT("tool_trowel")};
            if(!R.HeldToolId.IsEmpty() && (!KnownTools.Contains(R.HeldToolId) || HeldTools.Contains(R.HeldToolId) || R.HeldToolOperationId!=R.ActiveTaskId
                || (R.Task!=EHearthTask::ProductionTravel && R.Task!=EHearthTask::ProductionWork))) return false;
            if(R.HeldToolId.IsEmpty()!=R.HeldToolOperationId.IsEmpty()) return false;
            if(!R.HeldToolId.IsEmpty()) HeldTools.Add(R.HeldToolId);
            if(Saved.bPending && (!Guid(Saved.PendingOperation) || (R.Task!=EHearthTask::Choosing && R.Task!=EHearthTask::LifeChoosing && !(R.Task==EHearthTask::LifeActivity && !R.ConversationId.IsEmpty())))) return false;
            if(R.Plot>=0 && (W.Owners[R.Plot]!=I || R.DeliveredWood>W.Costs[R.Plot])) return false;
            if(R.Task!=EHearthTask::Choosing && (R.Plot<0 || R.ActiveTaskId.IsEmpty())) return false;
            if(R.Task==EHearthTask::ToWood && R.Source<0) return false;
            if((R.Task==EHearthTask::LifeTravel || R.Task==EHearthTask::LifeActivity) && R.LifeAction!=50
                && (R.LifeAction<0 || R.LifeAction>=3+W.People.Num() || R.LifeAction==3+I)) return false;
            if((R.Task>=EHearthTask::Settled) && R.BuildProgress<1.f) return false;
            if(R.Plot<0 && (R.CarriedWood || R.DeliveredWood || R.BuildProgress>0)) return false;
            if(R.BuildProgress>0 && R.DeliveredWood!=W.Costs[R.Plot]) return false;
            if(R.HistoryIndex>=0 && (!W.History.IsValidIndex(R.HistoryIndex) || W.History[R.HistoryIndex].Resident!=I || W.History[R.HistoryIndex].Run!=W.Run)) return false;
            const bool Production=R.Task>=EHearthTask::ProductionTravel && R.Task<=EHearthTask::ProductionDeposit;
            if(Production && (!W.Sites.IsValidIndex(R.ProductionSite) || R.ProductionOp<0 || W.Sites[R.ProductionSite].ReservedBy!=I || R.WorkDuration<=0 || R.HistoryIndex<0)) return false;
            if(Production && R.ProductionOp==5)
            {
                const auto& Site=W.Sites[R.ProductionSite];
                const auto* Part=Site.CottageComponents.FindByPredicate([&](const auto& C){ return C.Id==R.ProductionComponentId; });
                if(Site.Kind!=EHearthSiteKind::Land || Site.Stage<0 || Site.Stage>=4 || Site.BuildPlanId.IsEmpty() || !Part || Part->ReservedBy!=I) return false;
                if(Part->Status==TEXT("reserved") && (R.Task!=EHearthTask::ProductionTravel || R.CargoAmount!=0 || R.CargoType!=-1)) return false;
                if(Part->Status==TEXT("transporting") && (R.Task!=EHearthTask::ProductionTravel || R.CargoType!=Part->MaterialType || R.CargoAmount!=Part->MaterialAmount)) return false;
                if(Part->Status==TEXT("installing") && (R.Task!=EHearthTask::ProductionWork || R.CargoType!=Part->MaterialType || R.CargoAmount!=Part->MaterialAmount)) return false;
            }
            if(!Production && (R.ProductionSite!=-1 || R.ProductionOp!=-1 || R.CargoAmount!=0 || !R.ProductionComponentId.IsEmpty())) return false;
            if(R.CargoAmount>0 && (R.CargoType<0 || (R.ProductionOp<9 && R.ProductionOp!=5)
                || (R.ProductionOp==5 && R.Task!=EHearthTask::ProductionTravel && R.Task!=EHearthTask::ProductionWork)
                || (R.ProductionOp>=9 && R.Task!=EHearthTask::ProductionDeliver && R.Task!=EHearthTask::ProductionDeposit))) return false;
            if(R.CargoType>=0 && R.CargoType<=2) Accounted[R.CargoType]+=R.CargoAmount;
            if(R.CargoType>=3) AccountedManufactured[R.CargoType-3]+=R.CargoAmount;
            AccountedManufactured[0]+=R.PersonalPlanks;
            Accounted[1]+=R.CarriedWood+R.DeliveredWood;
        }
        for(int32 I=0;I<W.PlotCount;++I) if(W.Owners[I]>=0 && W.People[W.Owners[I]].Person.Plot!=I) return false;
        TSet<FString> ComponentIds;
        for(int32 I=0;I<W.Sites.Num();++I)
        {
            const auto& S=W.Sites[I]; if(!Unique(S.StableId) || S.Units>S.Capacity || S.Growth>S.GrowDuration || (!S.BuildPlanId.IsEmpty() && !Guid(S.BuildPlanId))) return false;
            if(!S.BuildPlanId.IsEmpty() && (S.Stage<0 || S.Stage>4 || (S.Kind!=EHearthSiteKind::Land && S.Kind!=EHearthSiteKind::House))) return false;
            if(S.ReservedBy>=0 && W.People[S.ReservedBy].Person.ProductionSite!=I) return false;
            if(W.Schema>=5 && !S.BuildPlanId.IsEmpty() && S.CottageComponents.Num()!=45) return false;
            if(S.BuildPlanId.IsEmpty() && !S.CottageComponents.IsEmpty()) return false;
            int32 Completed=0,Active=0;
            for(const auto& Part:S.CottageComponents)
            {
                if(Part.Id.IsEmpty() || !Part.Id.StartsWith(S.BuildPlanId+TEXT(":")) || ComponentIds.Contains(Part.Id) || Part.AssetId.IsEmpty()
                    || Part.Stage<1 || Part.Stage>4 || Part.MaterialType<2 || Part.MaterialType>4 || Part.MaterialAmount<1
                    || Part.Owner!=S.Owner || Part.Source!=TEXT("public_depot") || Part.SupplyPolicy!=TEXT("village_construction_grant")) return false;
                ComponentIds.Add(Part.Id);
                const bool Done=Part.Status==TEXT("completed"),Waiting=Part.Status==TEXT("waiting_material");
                const bool Reserved=Part.Status==TEXT("reserved"),Transporting=Part.Status==TEXT("transporting"),Installing=Part.Status==TEXT("installing");
                if(!Done && !Waiting && !Reserved && !Transporting && !Installing) return false;
                if((Done || Waiting)!=(Part.ReservedBy<0) || ((Reserved || Transporting || Installing) && Part.ReservedBy<0)) return false;
                Completed+=Done; Active+=Reserved||Transporting||Installing;
                if(Reserved) { if(Part.MaterialType==2) Accounted[2]+=Part.MaterialAmount; else AccountedManufactured[Part.MaterialType-3]+=Part.MaterialAmount; }
            }
            if(!S.CottageComponents.IsEmpty() && (S.Units!=Completed || Active!=(S.ReservedBy>=0?1:0) || (S.Kind==EHearthSiteKind::House)!=(Completed==45))) return false;
        }
        TSet<FString> ActivePeople,ChatIds;
        for(const auto& S:W.Conversations)
        {
            if(!Unique(S.Id) || S.First==S.Second || S.FirstId!=W.People[S.First].Person.StableId || S.SecondId!=W.People[S.Second].Person.StableId
                || (S.Speaker!=S.First && S.Speaker!=S.Second) || S.Offer==0) return false;
            ChatIds.Add(S.Id);
            if(!S.bClosed)
            {
                for(int32 I:{S.First,S.Second})
                {
                    const auto& R=W.People[I].Person;
                    if(ActivePeople.Contains(R.StableId) || R.ConversationId!=S.Id || (R.Task!=EHearthTask::LifeTravel && R.Task!=EHearthTask::LifeActivity)) return false;
                    ActivePeople.Add(R.StableId);
                }
            }
            int32 Previous=-1;
            for(const auto& L:S.Lines)
            { if((L.Speaker!=S.First && L.Speaker!=S.Second) || L.Speaker==Previous || L.Text.IsEmpty()) return false; Previous=L.Speaker; }
            if(!S.bMet && !S.Lines.IsEmpty()) return false;
            if(!S.bClosed && !S.Lines.IsEmpty() && S.Speaker==S.Lines.Last().Speaker) return false;
            if(S.Offer>=0 && ((S.Proposer!=S.First && S.Proposer!=S.Second) || (S.Offer==1 && S.OfferAction!=50) || (S.Offer==2 && S.OfferAction<100) || (S.Offer==3 && S.OfferAction!=-1))) return false;
        }
        for(const auto& Saved:W.People)
        {
            const auto& R=Saved.Person;
            if(!R.ConversationId.IsEmpty() && !ActivePeople.Contains(R.StableId)) return false;
            for(const auto& Pair:R.Bonds) if(Pair.Key==R.StableId || !W.People.ContainsByPredicate([&Pair](const auto& P) { return P.Person.StableId==Pair.Key; })) return false;
        }
        TSet<int32> PromisedWorkers;
        for(const auto& P:W.Commitments)
        {
            if(!Unique(P.Id) || !ChatIds.Contains(P.ConversationId) || P.Worker==P.Beneficiary || !Guid(P.TaskId,true)
                || (P.Action!=50 && P.Action<100) || (P.Status!=TEXT("promised") && P.Status!=TEXT("active") && P.Status!=TEXT("fulfilled") && P.Status!=TEXT("broken"))) return false;
            const auto* Chat=W.Conversations.FindByPredicate([&P](const auto& S) { return S.Id==P.ConversationId; });
            if(!Chat || !Chat->bAccepted || P.Action!=Chat->OfferAction
                || (P.Worker!=Chat->First && P.Worker!=Chat->Second) || (P.Beneficiary!=Chat->First && P.Beneficiary!=Chat->Second)) return false;
            if(P.Status==TEXT("active") || P.Status==TEXT("promised"))
            {
                if(PromisedWorkers.Contains(P.Worker)) return false; PromisedWorkers.Add(P.Worker);
                const auto& R=W.People[P.Worker].Person;
                if(P.Status==TEXT("active") && (P.TaskId!=R.ActiveTaskId || R.LifeAction!=P.Action || (R.Task!=EHearthTask::LifeTravel && R.Task!=EHearthTask::LifeActivity && R.Task<EHearthTask::ProductionTravel))) return false;
                if(P.Status==TEXT("promised") && R.ConversationId!=P.ConversationId) return false;
            }
        }
        int64 ExpectedTreasury=500; TArray<int64> ExpectedWallets; ExpectedWallets.Init(12,W.PlotCount);
        for(const auto& T:W.Transactions)
        {
            const FString Key=T.Kind+TEXT("|")+T.TaskId;
            if(!Unique(T.Id) || !Guid(T.TaskId) || TransactionKeys.Contains(Key) || T.From==T.To
                || (T.Kind!=TEXT("wage") && T.Kind!=TEXT("food_purchase") && T.Kind!=TEXT("plank_trade"))
                || (T.Kind==TEXT("wage") && (T.From!=-1 || T.To<0 || T.Item!=TEXT("labor") || T.Quantity!=1))
                || (T.Kind==TEXT("food_purchase") && (T.From<0 || T.To!=-1 || T.Item!=TEXT("food") || T.Quantity!=1 || T.Amount!=1))
                || (T.Kind==TEXT("plank_trade") && (T.From<0 || T.To<0 || T.Item!=TEXT("plank") || T.Quantity!=1 || T.Amount!=2))) return false;
            TransactionKeys.Add(Key);
            if(T.From<0) ExpectedTreasury-=T.Amount; else ExpectedWallets[T.From]-=T.Amount;
            if(T.To<0) ExpectedTreasury+=T.Amount; else ExpectedWallets[T.To]+=T.Amount;
            if(ExpectedTreasury<0 || ExpectedWallets.ContainsByPredicate([](int64 Balance) { return Balance<0; })) return false;
        }
        TMap<FString,const FHearthWagePayable*> PayableByTask;
        for(const auto& P:W.WagePayables)
        {
            if(!Unique(P.Id) || !Guid(P.TaskId) || PayableByTask.Contains(P.TaskId) || P.Worker<0 || P.Worker>=W.PlotCount
                || (P.Amount!=2 && P.Amount!=3) || (P.Status!=TEXT("reserved") && P.Status!=TEXT("unfunded") && P.Status!=TEXT("owed") && P.Status!=TEXT("paid"))) return false;
            PayableByTask.Add(P.TaskId,&P);
            const auto* Wage=W.Transactions.FindByPredicate([&](const FHearthTransaction& T) { return T.Kind==TEXT("wage") && T.TaskId==P.TaskId; });
            if(P.Status==TEXT("paid") && (!Wage || Wage->To!=P.Worker || Wage->Amount!=P.Amount)) return false;
            if(P.Status!=TEXT("paid") && Wage) return false;
            if(P.Status==TEXT("reserved")) { ExpectedTreasury-=P.Amount; if(ExpectedTreasury<0) return false; }
        }
        if(W.Schema>=4)
        {
            for(const auto& T:W.Transactions) if(T.Kind==TEXT("wage"))
            {
                const auto* const* P=PayableByTask.Find(T.TaskId); if(!P || (*P)->Status!=TEXT("paid")) return false;
            }
            for(int32 I=0;I<W.People.Num();++I)
            {
                const auto& R=W.People[I].Person; const bool Production=R.Task>=EHearthTask::ProductionTravel && R.Task<=EHearthTask::ProductionDeposit;
                if(Production)
                {
                    const auto* const* P=PayableByTask.Find(R.ActiveTaskId);
                    if(!P || ((*P)->Status!=TEXT("reserved") && (*P)->Status!=TEXT("unfunded")) || (*P)->Worker!=I) return false;
                }
            }
        }
        TSet<FString> TradeIds;
        for(const auto& T:W.TradeOffers)
        {
            if(!Guid(T.Id) || TradeIds.Contains(T.Id) || T.Seller==T.Buyer || T.Quantity!=1 || T.Price!=2 || T.Remaining<0
                || (T.Status!=TEXT("proposed") && T.Status!=TEXT("accepted") && T.Status!=TEXT("delivering") && T.Status!=TEXT("completed") && T.Status!=TEXT("cancelled"))) return false;
            const bool Active=T.Status==TEXT("proposed") || T.Status==TEXT("accepted") || T.Status==TEXT("delivering");
            if(T.ReservedQuantity!=(Active?1:0)) return false;
            const auto* Sale=W.Transactions.FindByPredicate([&](const FHearthTransaction& X) { return X.Kind==TEXT("plank_trade") && X.TaskId==T.Id; });
            if((T.Status==TEXT("completed"))!=!!Sale || (Sale && (Sale->From!=T.Buyer || Sale->To!=T.Seller))) return false;
            if(Active)
            {
                if(!Guid(T.ConversationId) || !ChatIds.Contains(T.ConversationId)) return false;
                const auto* Chat=W.Conversations.FindByPredicate([&](const FHearthConversation& C) { return C.Id==T.ConversationId; });
                if(!Chat || Chat->Offer!=3 || Chat->Proposer!=T.Seller) return false;
                if(T.Status==TEXT("delivering") && (W.People[T.Seller].Person.Task!=EHearthTask::TradeWaiting || W.People[T.Buyer].Person.Task!=EHearthTask::TradeWaiting)) return false;
                if(T.Status==TEXT("accepted") && Chat->bClosed && (W.People[T.Seller].Person.Task!=EHearthTask::TradeTravel || W.People[T.Buyer].Person.Task!=EHearthTask::TradeWaiting)) return false;
                AccountedManufactured[0]+=T.ReservedQuantity;
            }
            TradeIds.Add(T.Id);
        }
        if(W.Schema>=4) for(const auto& T:W.Transactions) if(T.Kind==TEXT("plank_trade") && !TradeIds.Contains(T.TaskId)) return false;
        if(HasEconomy)
        {
            if(W.TreasuryCoins!=ExpectedTreasury) return false;
            for(int32 I=0;I<W.PlotCount;++I) if(W.People[I].Person.Coins!=ExpectedWallets[I]) return false;
        }
        if(Accounted[0]!=W.PlotCount*10 || Accounted[1]!=(W.PlotCount==3?36:99) || Accounted[2]!=0
            || AccountedManufactured[0]!=0 || AccountedManufactured[1]!=0) return false;
        Out=MoveTemp(W); Error.Empty(); return true;
    }

    FString Checksum(const FString& Text)
    { FTCHARToUTF8 Bytes(*Text); uint8 Digest[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.Get(),Bytes.Length(),Digest); return BytesToHex(Digest,FSHA1::DigestSize); }
    bool Read(const FString& Path,FString& Payload,FString& Error)
    {
        Error=TEXT("世界文件不存在、损坏或校验和不匹配"); auto& Files=FPlatformFileManager::Get().GetPlatformFile();
        if(Files.FileSize(*Path)<1 || Files.FileSize(*Path)>MaxBytes) return false;
        FString Text,Hash; FObject Envelope;
        if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Envelope) || !Envelope.IsValid()
            || !Envelope->TryGetStringField(TEXT("payload"),Payload) || !Envelope->TryGetStringField(TEXT("sha1"),Hash) || Hash!=Checksum(Payload)) return false;
        FHearthWorldImage Candidate; return Decode(Payload,Candidate,Error);
    }
    bool AtomicText(const FString& Path,const FString& Text)
    {
        auto& Files=FPlatformFileManager::Get().GetPlatformFile(); Files.CreateDirectoryTree(*FPaths::GetPath(Path));
        const FString Temp=Path+TEXT(".tmp-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        bool Written=false;
        {
            TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Temp)); FTCHARToUTF8 Bytes(*Text);
            Written=Handle.IsValid() && Handle->Write(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length()) && Handle->Flush(true);
        }
        if(Written)
        {
#if PLATFORM_WINDOWS
            Written=!!MoveFileExW(*FPaths::ConvertRelativePathToFull(Temp),*FPaths::ConvertRelativePathToFull(Path),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
            Written=std::rename(TCHAR_TO_UTF8(*Temp),TCHAR_TO_UTF8(*Path))==0;
#endif
        }
        if(!Written) Files.DeleteFile(*Temp); return Written;
    }
    bool Write(const FString& Path,const FString& Payload,FString& Error)
    {
        FHearthWorldImage Candidate; if(!Decode(Payload,Candidate,Error)) return false;
        auto& Files=FPlatformFileManager::Get().GetPlatformFile();
        if(Files.FileExists(*Path))
        {
            FString OldPayload,OldFile;
            if(!Read(Path,OldPayload,Error)) return false; // Never erase a damaged current file on an autosave.
            if(!FFileHelper::LoadFileToString(OldFile,*Path) || !AtomicText(Path+TEXT(".bak"),OldFile)) { Error=TEXT("无法保存上一版世界备份"); return false; }
        }
        auto Envelope=MakeShared<FJsonObject>(); Envelope->SetStringField(TEXT("payload"),Payload); Envelope->SetStringField(TEXT("sha1"),Checksum(Payload));
        if(!AtomicText(Path,Json(Envelope))) { Error=TEXT("无法原子写入世界文件"); return false; }
        Error.Empty(); return true;
    }
    bool Archive(const FString& Path,FString& Error)
    {
        auto& Files=FPlatformFileManager::Get().GetPlatformFile();
        if(!Files.FileExists(*Path)) return true;
        const FString Destination=Path+TEXT(".archive-")+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S-"))+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        if(!Files.MoveFile(*Destination,*Path)) { Error=TEXT("无法保留旧世界文件，未新建世界"); return false; }
        return true;
    }
}
