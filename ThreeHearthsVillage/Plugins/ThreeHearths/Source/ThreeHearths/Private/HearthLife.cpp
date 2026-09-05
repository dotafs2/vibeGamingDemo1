#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"

namespace HearthLife
{
    TSharedRef<FJsonObject> RecordJson(const FHearthDecisionRecord& R)
    {
        auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("run"),R.Run); J->SetStringField(TEXT("timestamp"),R.Timestamp);
        J->SetNumberField(TEXT("resident"),R.Resident); J->SetNumberField(TEXT("simulation_time"),R.At);
        J->SetStringField(TEXT("kind"),R.Kind); J->SetStringField(TEXT("context"),R.Context);
        J->SetStringField(TEXT("choice"),R.Choice); J->SetStringField(TEXT("reason"),R.Reason);
        J->SetStringField(TEXT("result"),R.Result); J->SetStringField(TEXT("status"),R.Status);
        J->SetStringField(TEXT("source"),R.Source); J->SetStringField(TEXT("model"),R.Model);
        J->SetNumberField(TEXT("latency_seconds"),R.Latency); J->SetNumberField(TEXT("tokens"),R.Tokens);
        J->SetBoolField(TEXT("usage_available"),R.bHasUsage); return J;
    }
}

FString AHearthVillage::GetDecisionHistory(int32 Index) const
{
    auto Root=MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("version"),1);
    Root->SetStringField(TEXT("current_run"),CurrentRun);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& R:DecisionHistory) if(Index<0 || R.Resident==Index)
        Rows.Add(MakeShared<FJsonValueObject>(HearthLife::RecordJson(R)));
    Root->SetArrayField(TEXT("records"),Rows);
    FString Text; auto Writer=TJsonWriterFactory<>::Create(&Text); FJsonSerializer::Serialize(Root,Writer); return Text;
}

int32 AHearthVillage::HistoryCount(int32 Index) const
{
    int32 Count=0; for(const auto& R:DecisionHistory) if(R.Resident==Index) ++Count; return Count;
}

void AHearthVillage::LoadHistory()
{
    HistoryPath=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/decision-history.json");
    const bool bIsolated=FParse::Value(FCommandLine::Get(),TEXT("HearthHistory="),HistoryPath);
    FString Text; TSharedPtr<FJsonObject> Root;
    if(FPaths::FileExists(HistoryPath))
    {
        if(!FFileHelper::LoadFileToString(Text,*HistoryPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid() || !Root->HasTypedField<EJson::Array>(TEXT("records")))
        {
            // Preserve unreadable archives. New records go to a separate recovery file.
            HistoryPath+=TEXT(".recovery-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json");
            HistorySaveStatus=TEXT("旧档案读取失败，已保留原文件；本轮另存。");
            return;
        }
        for(const auto& Item:Root->GetArrayField(TEXT("records")))
        {
            if(Item->Type!=EJson::Object) continue;
            auto J=Item->AsObject(); double Index=-1; J->TryGetNumberField(TEXT("resident"),Index);
            if(Index<0 || Index>2 || Index!=FMath::FloorToDouble(Index)) continue;
            FHearthDecisionRecord R; R.Resident=static_cast<int32>(Index);
            J->TryGetStringField(TEXT("run"),R.Run); J->TryGetStringField(TEXT("timestamp"),R.Timestamp);
            J->TryGetNumberField(TEXT("simulation_time"),R.At); J->TryGetStringField(TEXT("kind"),R.Kind);
            J->TryGetStringField(TEXT("context"),R.Context); J->TryGetStringField(TEXT("choice"),R.Choice);
            J->TryGetStringField(TEXT("reason"),R.Reason); J->TryGetStringField(TEXT("result"),R.Result);
            J->TryGetStringField(TEXT("status"),R.Status); J->TryGetStringField(TEXT("source"),R.Source);
            J->TryGetStringField(TEXT("model"),R.Model); J->TryGetNumberField(TEXT("latency_seconds"),R.Latency);
            J->TryGetNumberField(TEXT("tokens"),R.Tokens); J->TryGetBoolField(TEXT("usage_available"),R.bHasUsage);
            if(R.Status==TEXT("thinking") || R.Status==TEXT("executing"))
            { R.Status=TEXT("cancelled"); R.Result=TEXT("上次运行已结束，执行结果未完整记录。"); }
            DecisionHistory.Add(MoveTemp(R));
        }
        HistorySaveStatus=TEXT("完整历史已从本机载入"); ++HistoryRevision; return;
    }
    // The previous implementation kept only one snapshot. Never invent older decisions.
    const FString Legacy=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/legacy-decision-snapshot.json");
    if(!bIsolated && FFileHelper::LoadFileToString(Text,*Legacy) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) && Root.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* People=nullptr;
        FString Model; Root->TryGetStringField(TEXT("model"),Model);
        if(Root->TryGetArrayField(TEXT("residents"),People)) for(const auto& Item:*People)
        {
            auto J=Item->AsObject(); if(!J.IsValid()) continue;
            int32 Index=-1,Plot=-1; J->TryGetNumberField(TEXT("id"),Index); J->TryGetNumberField(TEXT("plot"),Plot);
            if(Index<0 || Index>2 || Plot<0) continue;
            FHearthDecisionRecord R; R.Resident=Index; R.Run=TEXT("legacy"); R.Kind=TEXT("legacy");
            R.Timestamp=TEXT("旧版快照 · 原时间未记录"); R.Choice=TEXT("选择地块并建房");
            R.Context=TEXT("从启用历史前的最后一轮快照恢复；更早的决定和当时状态没有留存。");
            J->TryGetStringField(TEXT("reason"),R.Reason); J->TryGetStringField(TEXT("decision_source"),R.Source);
            R.Model=Model; R.Status=TEXT("archived"); R.Result=TEXT("旧版只保留最新状态，无法恢复完整执行过程。");
            DecisionHistory.Add(MoveTemp(R));
        }
    }
    ++HistoryRevision; SaveHistory();
}

void AHearthVillage::SaveHistory()
{
    if(HistoryPath.IsEmpty()) return;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(HistoryPath),true);
    const FString Temp=HistoryPath+TEXT(".tmp");
    const bool bSaved=FFileHelper::SaveStringToFile(GetDecisionHistory(),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && IFileManager::Get().Move(*HistoryPath,*Temp,true,true);
    HistorySaveStatus=bSaved?TEXT("历史已保存到本机 · 重开仍保留"):TEXT("历史保存失败，请检查磁盘与目录权限");
}

void AHearthVillage::CloseHistoryRun(const FString& Result)
{
    bool Changed=false;
    for(auto& R:DecisionHistory) if(R.Run==CurrentRun && (R.Status==TEXT("thinking") || R.Status==TEXT("executing")))
    { R.Status=TEXT("cancelled"); R.Result=Result; Changed=true; }
    if(Changed) { ++HistoryRevision; SaveHistory(); }
}

void AHearthVillage::StartHistory(int32 Index,bool bLife,const FString& Source)
{
    auto& Person=Residents[Index];
    FHearthDecisionRecord Record; Record.Resident=Index; Record.Run=CurrentRun;
    Record.Timestamp=FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")); Record.At=Elapsed;
    Record.Kind=bLife?TEXT("life"):TEXT("home"); Record.Source=Source; Record.Model=Source==TEXT("api")?ApiModel:FString();
    Record.Context=FString::Printf(TEXT("精力 %.0f / 100 · 社交需求 %.0f / 100 · 小屋 %s · 村庄木材 %d · 已建房屋 %d / 3"),
        Person.Energy,Person.SocialNeed,Person.BuildProgress>=1?TEXT("已完成"):TEXT("未完成"),AvailableWood(),CompletedHomes());
    Record.Context+=TEXT("\n人设：")+Person.Personality+TEXT("\n当时可选：");
    if(bLife)
    {
        for(int32 Action:AvailableLifeActions(Index)) Record.Context+=LifeActionName(Index,Action)+TEXT("；");
    }
    else
    {
        const TCHAR* Names[]={TEXT("林边小屋"),TEXT("花园小屋"),TEXT("紧凑小屋")};
        for(int32 P=0;P<3;++P) if(PlotOwners[P]<0) Record.Context+=FString::Printf(TEXT("%s（木材 %d）；"),Names[P],PlotCosts[P]);
    }
    Record.Context+=FString::Printf(TEXT("\n公共库存：食物 %d、木材 %d、石材 %d。"),FoodStock,AvailableWood(),StoneStock);
    Record.Choice=bLife?TEXT("考虑下一步生活"):TEXT("考虑在哪里建家");
    Person.HistoryIndex=DecisionHistory.Add(MoveTemp(Record)); ++HistoryRevision; SaveHistory();
}

void AHearthVillage::AcceptHistory(int32 Index,const FString& Choice,const FString& Reason,const FString& Source)
{
    auto& Person=Residents[Index];
    if(!DecisionHistory.IsValidIndex(Person.HistoryIndex) || DecisionHistory[Person.HistoryIndex].Status!=TEXT("thinking"))
        StartHistory(Index,Person.BuildProgress>=1,Source);
    auto& Record=DecisionHistory[Person.HistoryIndex]; Record.Choice=Choice; Record.Reason=Reason;
    Record.Source=Source; Record.Status=TEXT("executing"); Record.Result=TEXT("正在执行这个选择。");
    ++HistoryRevision; SaveHistory();
}

void AHearthVillage::CompleteHistory(int32 Index,const FString& Result)
{
    const int32 H=Residents[Index].HistoryIndex;
    if(!DecisionHistory.IsValidIndex(H)) return;
    auto& Record=DecisionHistory[H]; Record.Status=TEXT("completed"); Record.Result=Result;
    ++HistoryRevision; SaveHistory();
}

TArray<int32> AHearthVillage::AvailableLifeActions(int32 Index) const
{
    TArray<int32> Actions={0,1,2};
    Actions.Append(AvailableProductionActions(Index));
    for(int32 I=0;I<Residents.Num();++I) if(I!=Index && Residents[I].BuildProgress>=1.f) Actions.Add(3+I);
    return Actions;
}

FString AHearthVillage::LifeActionName(int32 Index,int32 Action) const
{
    if(Action>=100) return ProductionActionName(Action);
    if(Action==0) return TEXT("回家休息");
    if(Action==1) return TEXT("去农田观察作物");
    if(Action==2) return TEXT("巡查树林与木材站");
    if(Residents.IsValidIndex(Action-3)) return TEXT("拜访")+Residents[Action-3].Name;
    return TEXT("未知行动");
}

bool AHearthVillage::StartLifeAction(int32 Index,int32 Action,const FString& Reason,bool bFromApi)
{
    if(!Residents.IsValidIndex(Index) || Residents[Index].Task!=EHearthTask::LifeChoosing) return false;
    if(Action>=100) return StartProduction(Index,Action,Reason,bFromApi);
    if(!AvailableLifeActions(Index).Contains(Action)) return false;
    auto& R=Residents[Index];
    FVector Target=PlotPositions[R.Plot]+FVector(-245,0,0);
    if(Action==1) Target=bUseCropoutMap?FVector(-1850,-2400,8):PlotPositions[1]+FVector(-245,0,0);
    if(Action==2) Target=WoodPositions[Index]+FVector(80,Index*120-120,0);
    if(Action>=3) Target=PlotPositions[Residents[Action-3].Plot]+FVector(-245,0,0);
    if(!bUseCropoutMap && Action!=2) Target.Y+=(Index-1)*120;
    TArray<FVector> Route;
    if(bUseCropoutMap && !ProductionSites.IsEmpty() && !FindActivityRoute(Index,Target,Route)) return false;
    R.LifeAction=Action; R.Reason=Reason; R.DecisionSource=bFromApi?TEXT("api"):TEXT("local");
    R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    R.MoveRetry=0; R.bMovementBlocked=false;
    R.Task=EHearthTask::LifeTravel; R.LatestEvent=LifeActionName(Index,Action)+TEXT("。");
    AcceptHistory(Index,LifeActionName(Index,Action),Reason,R.DecisionSource);
    if(Route.IsEmpty()) SetRoute(Index,Target); else R.Route=MoveTemp(Route);
    VillageEvent=R.Name+TEXT("：")+R.LatestEvent; return true;
}

void AHearthVillage::DecideLifeLocally(int32 Index,const FString& Failure)
{
    const auto& Person=Residents[Index]; int32 Action=0;
    if(Person.Energy<45) Action=0;
    else
    {
        const int32 Work=ChooseProductionLocally(Index);
        if(Work>=0) Action=Work;
        else
        {
            for(int32 Other=0;Other<Residents.Num();++Other) if(Other!=Index && Residents[Other].BuildProgress>=1 && Person.SocialNeed>60) { Action=Other+3; break; }
        }
    }
    const FString Reason=Action>=100?TEXT("村庄需要生产和建设，我准备")+ProductionActionName(Action)+TEXT("。"):Action==0?TEXT("先回家歇一会儿，恢复精力。"):Action>=3?TEXT("想找邻居聊聊，看看大家过得怎么样。"):Action==1?TEXT("去看看田里的作物，熟悉村庄的粮食来源。"):TEXT("去树林和木材站看看，了解村庄的材料情况。");
    if(StartLifeAction(Index,Action,Reason,false))
    {
        Residents[Index].DecisionSource=Failure.IsEmpty()?TEXT("local"):TEXT("local_fallback");
        Residents[Index].DecisionNote=Failure;
        auto& Record=DecisionHistory[Residents[Index].HistoryIndex]; Record.Source=Residents[Index].DecisionSource;
        if(!Failure.IsEmpty() && !Record.Context.Contains(Failure)) Record.Context+=TEXT("\n采用备用规则：")+Failure;
        if(!Failure.IsEmpty()) Record.Result=TEXT("本地备用选择：")+Failure+TEXT("；正在执行。");
        ++HistoryRevision; SaveHistory();
    }
}

void AHearthVillage::RequestLifeDecision(int32 Index)
{
    if(!HasDecisionCapacity(Index)) return;
    auto& R=Residents[Index];
    if(!bApiReady || ApiBackend==TEXT("codex_spark") || bApiDisabledThisRun || ApiRequests>=ApiMaxRequests)
    {
        const FString Failure=!bApiConfigured?FString():ApiRequests>=ApiMaxRequests?TEXT("本轮模型预算已用完"):ApiBackend==TEXT("codex_spark")?TEXT("旧 Spark 通路暂只支持选址"):ApiStatus;
        DecideLifeLocally(Index,Failure); return;
    }
    auto Context=MakeShared<FJsonObject>();
    auto Person=MakeShared<FJsonObject>(); Person->SetNumberField(TEXT("id"),Index);
    Person->SetStringField(TEXT("name"),R.Name); Person->SetStringField(TEXT("personality"),R.Personality);
    Person->SetNumberField(TEXT("energy"),R.Energy); Person->SetNumberField(TEXT("social_need"),R.SocialNeed);
    Context->SetObjectField(TEXT("resident"),Person); Context->SetNumberField(TEXT("completed_homes"),CompletedHomes());
    Context->SetNumberField(TEXT("available_wood"),AvailableWood());
    AppendProductionContext(Context);
    TArray<TSharedPtr<FJsonValue>> Actions,Memory;
    for(int32 Action:AvailableLifeActions(Index))
    {
        auto A=MakeShared<FJsonObject>(); A->SetNumberField(TEXT("id"),Action);
        A->SetStringField(TEXT("description"),LifeActionName(Index,Action)); Actions.Add(MakeShared<FJsonValueObject>(A));
    }
    for(int32 H=DecisionHistory.Num()-1;H>=0 && Memory.Num()<3;--H)
    {
        const auto& D=DecisionHistory[H]; if(D.Resident!=Index || D.Run!=CurrentRun || D.Status!=TEXT("completed")) continue;
        auto M=MakeShared<FJsonObject>(); M->SetStringField(TEXT("choice"),D.Choice);
        M->SetStringField(TEXT("reason"),D.Reason); M->SetStringField(TEXT("result"),D.Result); Memory.Add(MakeShared<FJsonValueObject>(M));
    }
    Context->SetArrayField(TEXT("available_actions"),Actions); Context->SetArrayField(TEXT("recent_decisions_newest_first"),Memory);
    const FString Prompt=TEXT("Choose one next activity for this medieval villager. All villagers have ALL skills; personality is a preference, not a restriction. Choose exactly one supplied available_actions id. Help create a productive village: gather/chop/quarry and deliver food/wood/stone, claim vacant land, construct corn/wheat/lettuce/pumpkin fields and houses, plant trees/shrubs, sow and harvest. No monument or terrain creation exists. Prioritize sustainable stocks (food around 30, wood around 60, stone around 10), sow idle fields, harvest ripe crops, and diversify expansion using completed_production_operations; try each build/plant type when affordable instead of endlessly collecting. Costs and site availability are enforced by the game. Rest when energy is low; visits reduce social need. Old observation actions 1/2 produce nothing. Use recent completed choices to avoid needless repetition. Return ONLY JSON with exactly action_id (integer) and reason (brief first-person Chinese, at most 60 Chinese characters). Do not invent actions or resources.");
    SendDecisionRequest(Index,Context,Prompt,true);
}

void AHearthVillage::UpdateLifeDecisions()
{
    if(!bAutonomousLifeEnabled || bSimulationPaused) return;
    const double Now=FPlatformTime::Seconds();
    const int32 StartAfter=LastLifeResident;
    for(int32 Offset=1;Offset<=Residents.Num();++Offset)
    {
        const int32 Index=(StartAfter+Offset)%Residents.Num();
        auto& R=Residents[Index];
        if(R.Task!=EHearthTask::LifeChoosing || Now<R.NextLifeDecision || !HasDecisionCapacity(Index)) continue;
        LastLifeResident=Index; R.NextLifeDecision=Now+LifeDecisionInterval;
        RequestLifeDecision(Index);
    }
}

void AHearthVillage::AdvanceLife(int32 Index,float Dt)
{
    auto& R=Residents[Index];
    if(R.Task==EHearthTask::LifeTravel && MoveResident(Index,Dt))
    { R.Task=EHearthTask::LifeActivity; R.Timer=R.LifeAction==0?20.f:15.f; }
    else if(R.Task==EHearthTask::LifeActivity && R.Timer<=0)
    {
        if(R.LifeAction==0) R.Energy=FMath::Min(100.f,R.Energy+35.f);
        else if(R.LifeAction>=3) { R.SocialNeed=FMath::Max(0.f,R.SocialNeed-45.f); R.Energy=FMath::Max(0.f,R.Energy-6.f); }
        else R.Energy=FMath::Max(0.f,R.Energy-10.f);
        const FString Result=FString::Printf(TEXT("%s已完成。精力 %.0f，社交需求 %.0f。%s"),*LifeActionName(Index,R.LifeAction),R.Energy,R.SocialNeed,
            R.LifeAction==1||R.LifeAction==2?TEXT("本次只观察，没有改变资源库存。"):TEXT(""));
        R.LatestEvent=Result; CompleteHistory(Index,Result); R.Task=EHearthTask::LifeChoosing;
        VillageEvent=R.Name+TEXT("完成了活动，准备下一次选择。");
    }
}

void AHearthVillage::ToggleAutonomy() { bAutonomousLifeEnabled=!bAutonomousLifeEnabled; WriteSnapshot(); }

FString AHearthVillage::LifeSummary() const
{
    if(!bAutonomousLifeEnabled) return TEXT("自主生活已关闭 · 已开始的行动仍会完成");
    TArray<FString> Thinking;
    for(int32 I=0;I<Residents.Num();++I) if(IsDecisionPending(I)) Thinking.Add(Residents[I].Name);
    if(!Thinking.IsEmpty()) return FString::Join(Thinking,TEXT("、"))+TEXT("正在各自思考，回复后独立执行");
    if(ApiRequests>=ApiMaxRequests && bApiReady) return TEXT("本轮模型预算已用完 · 后续采用本地规则");
    return FString::Printf(TEXT("自主生活开启 · 每人独立思考 · 每人间隔 %d 秒"),FMath::RoundToInt(LifeDecisionInterval));
}
