#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HearthSocial
{
    const TCHAR* IntentNames[]={TEXT("回应与闲聊"),TEXT("邀请一起吃饭"),TEXT("请求采集帮助"),TEXT("接受提议"),TEXT("婉拒提议"),TEXT("告别")};
    FString Json(const TSharedRef<FJsonObject>& J) { FString Text; FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text)); return Text; }
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
        if((S->Offer==1 && FoodStock>=2 && Residents[Index].Coins>0 && Residents[Other].Coins>0) || (S->Offer==2 && IsProductionAllowed(Index,S->OfferAction))) Choices.Insert(3,0);
        return Choices;
    }
    TArray<int32> Choices={0,5};
    if(S->Lines.Num()>=2 && S->Offer<0)
    {
        const int32 Other=S->First==Index?S->Second:S->First;
        if(FoodStock>=2 && Residents[Index].Coins>0 && Residents[Other].Coins>0) Choices.Add(1);
        if(FindHelpActivity(Other)>=0) Choices.Add(2);
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
    FHearthDialogueLine Line; Line.Speaker=Index; Line.Intent=Intent; Line.Text=Words; Line.Source=Source; Line.At=Elapsed;
    S->Lines.Add(MoveTemp(Line)); auto& R=Residents[Index]; auto& Listener=Residents[Other];
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
    else if(Intent==3)
    {
        S->bAccepted=true;
        const TArray<int32> Workers=S->Offer==1?TArray<int32>{Index,Other}:TArray<int32>{Index};
        for(int32 Worker:Workers)
        {
            FHearthCommitment P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); P.ConversationId=S->Id;
            P.Worker=Worker; P.Beneficiary=Worker==Index?Other:Index; P.Action=S->OfferAction;
            Commitments.Add(MoveTemp(P));
        }
        S->Outcome=S->Offer==1?TEXT("两人约好一起去吃饭。"):TEXT("对方答应完成一项采集与运输工作。");
    }
    else if(Intent==4)
    {
        Reciprocal.Affinity=FMath::Max(-100.f,Reciprocal.Affinity-.5f);
        Listener.Mood=FMath::Max(0.f,Listener.Mood-1.f); S->Outcome=TEXT("提议被婉拒，双方保留自己的安排。");
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
        Words=WillAccept?(S->Offer==1?TEXT("好啊，一起去吃饭。我也想歇一会儿。"):TEXT("好，我来做这项采集，把东西运回村里。")):TEXT("抱歉，我现在有些累，这次先不答应了。");
    }
    else if(S->Lines.Num()==0) Words=Residents[Other].Name+TEXT("，最近过得怎么样？新家住得还习惯吗？");
    else if(S->Lines.Num()==1) Words=TEXT("还不错，家已经安顿好了。谢谢你过来，见到邻居真好。");
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
    Context->SetStringField(TEXT("pending_offer"),S->Offer<0?TEXT("无提议"):S->Offer==1?TEXT("一起去吃饭，每人消耗1份库存食物"):ProductionActionName(S->OfferAction));
    const int32 HelpAction=FindHelpActivity(Other);
    Context->SetStringField(TEXT("available_help_job"),HelpAction<0?TEXT("无可用采集工作"):ProductionActionName(HelpAction));
    AppendProductionContext(Context);
    const FString Prompt=TEXT("You are the named medieval resident (your_id) speaking directly to the other present resident, who has a separate mind and may refuse. Continue in natural first-person Chinese, respecting current participant facts, needs and remembered relationship. A completed home already belongs to its resident; do not claim they still need their first home. Select exactly one allowed_intents id and make your spoken words match it: 0=chat only, without invitations, requests for work or promises; 1=invite both to eat; 2=ask the other to perform the supplied available_help_job; 3=accept the supplied pending_offer; 4=politely decline that offer; 5=say goodbye. A request must use 1 or 2, never 0. Previous words alone do not create an offer: accept or decline only when pending_offer exists and that intent is allowed. Accepted promises cause real tasks after the conversation and change trust only when performed or failed. Do not claim completed work or transferred resources. Never invent quantities, trades, marriage, money, tools or additional obligations. No new offer while responding to an existing offer. Return only JSON with action_id (integer) and reason (your spoken words, Chinese, at most 60 Chinese characters). reason is actual dialogue, not a description of your decision. Keep each turn brief and give the other person room to reply.");
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
    auto Root=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Chats,Promises;
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
    Root->SetArrayField(TEXT("conversations"),Chats); Root->SetArrayField(TEXT("commitments"),Promises); return HearthSocial::Json(Root);
}
