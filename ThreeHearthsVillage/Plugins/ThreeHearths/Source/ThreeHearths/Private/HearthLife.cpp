#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"

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
            if(Index<0 || Index>9 || Index!=FMath::FloorToDouble(Index)) continue;
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
        const FString SegmentDirectory=HistoryPath+TEXT(".segments");
        HistorySaveStatus=IFileManager::Get().DirectoryExists(*SegmentDirectory)?TEXT("近期历史已载入 · 早期记录保存在只读分卷"):TEXT("完整历史已从本机载入");
        ++HistoryRevision; return;
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

bool AHearthVillage::ArchiveCompletedHistoryIfNeeded()
{
    constexpr int32 ArchiveThreshold=12000,RetainedTarget=9000;
    if(DecisionHistory.Num()<=ArchiveThreshold || HistoryPath.IsEmpty()) return true;

    TSet<int32> Protected;
    for(const auto& R:Residents) if(DecisionHistory.IsValidIndex(R.HistoryIndex)) Protected.Add(R.HistoryIndex);
    for(const auto& P:PendingDecisions) if(P.bActive && DecisionHistory.IsValidIndex(P.HistoryIndex)) Protected.Add(P.HistoryIndex);
    for(int32 I=0;I<DecisionHistory.Num();++I) if(DecisionHistory[I].Kind==TEXT("public_project_policy")) Protected.Add(I);

    TBitArray<> Archive(false,DecisionHistory.Num());
    int32 Needed=DecisionHistory.Num()-RetainedTarget,Selected=0;
    for(int32 I=0;I<DecisionHistory.Num() && Selected<Needed;++I)
    {
        const FString& Status=DecisionHistory[I].Status;
        const bool bTerminal=Status==TEXT("completed") || Status==TEXT("failed") || Status==TEXT("cancelled")
            || Status==TEXT("interrupted") || Status==TEXT("archived") || Status==TEXT("completed_late") || Status==TEXT("failed_late");
        if(bTerminal && !Protected.Contains(I)) { Archive[I]=true; ++Selected; }
    }
    if(Selected<=0) return true;

    auto Root=MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("version"),1); Root->SetNumberField(TEXT("record_count"),Selected);
    Root->SetStringField(TEXT("source_history"),FPaths::GetCleanFilename(HistoryPath));
    TArray<TSharedPtr<FJsonValue>> Rows; Rows.Reserve(Selected);
    for(int32 I=0;I<DecisionHistory.Num();++I) if(Archive[I]) Rows.Add(MakeShared<FJsonValueObject>(HearthLife::RecordJson(DecisionHistory[I])));
    Root->SetArrayField(TEXT("records"),Rows);
    FString SegmentText; auto Writer=TJsonWriterFactory<>::Create(&SegmentText); FJsonSerializer::Serialize(Root,Writer);
    const FString Directory=HistoryPath+TEXT(".segments"); IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Run=Rows.IsEmpty()?TEXT("unknown"):DecisionHistory[Archive.Find(true)].Run.Replace(TEXT("/"),TEXT("_"));
    const FString Name=FString::Printf(TEXT("segment-%s-%08x-%d.json"),*Run,FCrc::StrCrc32(*SegmentText),Selected);
    const FString Path=Directory/Name,Temp=Path+TEXT(".tmp");
    if(!FFileHelper::SaveStringToFile(SegmentText,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !IFileManager::Get().Move(*Path,*Temp,true,true))
    { HistorySaveStatus=TEXT("历史分卷失败 · 未删除任何记录"); return false; }

    TArray<int32> Remap; Remap.Init(INDEX_NONE,DecisionHistory.Num());
    TArray<FHearthDecisionRecord> Kept; Kept.Reserve(DecisionHistory.Num()-Selected);
    for(int32 I=0;I<DecisionHistory.Num();++I) if(!Archive[I]) { Remap[I]=Kept.Num(); Kept.Add(MoveTemp(DecisionHistory[I])); }
    for(auto& R:Residents) if(R.HistoryIndex>=0) R.HistoryIndex=Remap.IsValidIndex(R.HistoryIndex)?Remap[R.HistoryIndex]:INDEX_NONE;
    for(auto& P:PendingDecisions) if(P.HistoryIndex>=0) P.HistoryIndex=Remap.IsValidIndex(P.HistoryIndex)?Remap[P.HistoryIndex]:INDEX_NONE;
    DecisionHistory=MoveTemp(Kept); ++HistoryRevision;
    HistorySaveStatus=FString::Printf(TEXT("较早历史已分卷 %d 条 · 主存档保留 %d 条"),Selected,DecisionHistory.Num());
    return true;
}

void AHearthVillage::SaveHistory()
{
    if(HistoryPath.IsEmpty()) return;
    const double Now=FPlatformTime::Seconds();
    // At high simulation speeds several residents can finish decisions in one
    // rendered frame. Batch the separate audit archive instead of serializing
    // the entire accumulated history once per resident; the world checkpoint
    // still contains every in-memory record and keeps its 30-real-second cadence.
    if(SimulationSpeed>10.f && LastHistoryDiskWriteAt>=0.0 && Now-LastHistoryDiskWriteAt<2.0)
    { HistorySaveStatus=TEXT("历史有更新 · 正在批量写入"); return; }
    if(!ArchiveCompletedHistoryIfNeeded()) return;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(HistoryPath),true);
    const FString Temp=HistoryPath+TEXT(".tmp");
    const bool bSaved=FFileHelper::SaveStringToFile(GetDecisionHistory(),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && IFileManager::Get().Move(*HistoryPath,*Temp,true,true);
    if(bSaved) LastHistoryDiskWriteAt=Now;
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
    Record.Context=FString::Printf(TEXT("精力 %.0f · 饥饿 %.0f · 心情 %.0f · 社交需求 %.0f · 小屋 %s · 村庄木材 %d · 已建房屋 %d / %d"),
        Person.Energy,Person.Hunger,Person.Mood,Person.SocialNeed,Person.BuildProgress>=1?TEXT("已完成"):TEXT("未完成"),AvailableWood(),CompletedHomes(),Residents.Num());
    Record.Context+=TEXT("\n人设：")+Person.Personality+TEXT("\n当时可选：");
    if(bLife)
    {
        for(int32 Action:AvailableLifeActions(Index)) Record.Context+=LifeActionName(Index,Action)+TEXT("；");
    }
    else
    {
        for(int32 P=0;P<HousingPlotCount();++P) if(PlotOwners[P]<0) Record.Context+=FString::Printf(TEXT("%s（木材 %d）；"),*PlotLabel(P),PlotCosts[P]);
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
    if(FoodStock>0 && Residents.IsValidIndex(Index) && Residents[Index].Coins>0) Actions.Add(50);
    Actions.Append(AvailableProductionActions(Index));
    for(int32 I=0;I<Residents.Num();++I) if(I!=Index && IsSociallyAvailable(I)) Actions.Add(3+I);
    return Actions;
}

FString AHearthVillage::LifeActionName(int32 Index,int32 Action) const
{
    if(Action>=100) return ProductionActionName(Action);
    if(Action==0) return TEXT("回家休息");
    if(Action==1) return TEXT("去农田观察作物");
    if(Action==2) return TEXT("巡查树林与木材站");
    if(Action==50) return TEXT("去村镇中心吃饭");
    if(Residents.IsValidIndex(Action-3)) return TEXT("拜访")+Residents[Action-3].Name;
    return TEXT("未知行动");
}

bool AHearthVillage::StartLifeAction(int32 Index,int32 Action,const FString& Reason,bool bFromApi)
{
    if(!Residents.IsValidIndex(Index) || Residents[Index].Task!=EHearthTask::LifeChoosing) return false;
    if(Action>=100) return StartProduction(Index,Action,Reason,bFromApi);
    if(!AvailableLifeActions(Index).Contains(Action)) return false;
    if(Residents.IsValidIndex(Action-3)) return BeginConversation(Index,Action-3,Reason,bFromApi);
    auto& R=Residents[Index];
    FVector Target=PlotPositions[R.Plot]+FVector(-245,0,0);
    if(Action==1) Target=bUseCropoutMap?FVector(-1850,-2400,8):PlotPositions[1]+FVector(-245,0,0);
    if(Action==2) Target=WoodPositions[Index%3]+FVector(80,(Index%3)*120-120,0);
    if(Action==50) Target=bUseCropoutMap?FVector(-1650,-1050,8):FVector(-250,-400,0);
    if(Residents.IsValidIndex(Action-3)) Target=PlotPositions[Residents[Action-3].Plot]+FVector(-245,0,0);
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
    const auto& Person=Residents[Index]; int32 Action=0; FString LocalReason;
    if(Person.Hunger>=60 && FoodStock>0 && Person.Coins>0) Action=50;
    else if(Person.Energy<45) Action=0;
    else
    {
        const auto HasOpenTileOrder=[](const TArray<FHearthTileOrder>& Orders,int32 Resident)
        {
            return Orders.ContainsByPredicate([Resident](const FHearthTileOrder& O)
            {
                return (O.Customer==Resident || O.Potter==Resident) && (O.Status==TEXT("active") || O.Status==TEXT("delivering"));
            });
        };
        if(ClayStock>=4 && AvailableWood()>=2 && !HasOpenTileOrder(TileOrders,Index))
        {
            const bool bPotter=Person.Role.Contains(TEXT("陶工"));
            for(int32 Other=0;Other<Residents.Num();++Other)
            {
                if(Other==Index || !IsSociallyAvailable(Other) || HasOpenTileOrder(TileOrders,Other)) continue;
                const auto& Candidate=Residents[Other];
                const bool bMatchingCustomer=bPotter && Candidate.RoofMaterial==TEXT("terracotta") && Candidate.PersonalTiles<12 && Candidate.Coins>=4;
                const bool bMatchingPotter=!bPotter && Person.RoofMaterial==TEXT("terracotta") && Person.PersonalTiles<12 && Person.Coins>=4 && Candidate.Role.Contains(TEXT("陶工"));
                if(!bMatchingCustomer && !bMatchingPotter) continue;
                const auto* Bond=Person.Bonds.Find(Candidate.StableId);
                if(Bond && Bond->Trust<35.f) continue;
                Action=Other+3;
                LocalReason=bPotter?TEXT("邻居需要陶瓦，我去按库存、价格和彼此信任报价。")
                                   :TEXT("我的陶瓦还不够，我去找可信的陶工谈一笔真实订单。");
                break;
            }
        }
        if(Action==0 && Person.PersonalPlanks>1)
            for(int32 Other=0;Other<Residents.Num();++Other) if(Other!=Index && Residents[Other].PersonalPlanks==0 && Residents[Other].Coins>=2 && IsSociallyAvailable(Other)) { Action=Other+3; break; }
        if(Action==0 && Person.SocialNeed>55)
        {
            float Best=-FLT_MAX;
            for(int32 Other=0;Other<Residents.Num();++Other) if(Other!=Index && IsSociallyAvailable(Other))
            {
                const auto* Bond=Person.Bonds.Find(Residents[Other].StableId);
                const float Score=(Bond?Bond->Affinity-Bond->Meetings*2:12.f)-FVector::Dist2D(Person.Actor->GetActorLocation(),Residents[Other].Actor->GetActorLocation())/1000;
                if(Score>Best) { Best=Score; Action=Other+3; }
            }
        }
        if(Action==0) { const int32 Work=ChooseProductionLocally(Index); if(Work>=0) Action=Work; }
    }
    const FString Reason=!LocalReason.IsEmpty()?LocalReason:Action>=100?TEXT("村庄需要生产和建设，我准备")+ProductionActionName(Action)+TEXT("。"):Action==50?TEXT("肚子饿了，去吃一份库存里的食物。"):Action==0?TEXT("先回家歇一会儿，恢复精力。"):Action>=3?TEXT("想找邻居聊聊，看看大家过得怎么样。"):Action==1?TEXT("去看看田里的作物，熟悉村庄的粮食来源。"):TEXT("去树林和木材站看看，了解村庄的材料情况。");
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
    Person->SetStringField(TEXT("stable_id"),R.StableId); Person->SetStringField(TEXT("role"),R.Role);
    Person->SetBoolField(TEXT("king"),R.bKing); Person->SetNumberField(TEXT("age"),R.Age);
    Person->SetNumberField(TEXT("hunger"),R.Hunger); Person->SetNumberField(TEXT("mood"),R.Mood);
    Person->SetNumberField(TEXT("coins"),R.Coins); Context->SetNumberField(TEXT("treasury_coins"),TreasuryCoins);
    Person->SetStringField(TEXT("remembered_relationships"),RelationshipSummary(Index));
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
    const FString Prompt=TEXT("Choose one next activity for this medieval villager. All villagers have ALL skills; personality is a preference, not a restriction. Choose exactly one supplied available_actions id. Help create a productive village: gather/chop/quarry and deliver food/wood/stone, claim vacant land, construct corn/wheat/lettuce/pumpkin fields and houses, plant trees/shrubs, sow and harvest. A house is a persistent four-job plan: transport and install its stone foundation, timber frame, plaster walls, then terracotta roof; choose its offered next stage when the required public material is available. No monument or terrain creation exists. Prioritize sustainable stocks (food around 30, wood around 60, stone around 10), sow idle fields, harvest ripe crops, and diversify expansion using completed_production_operations; try each build/plant type when affordable instead of endlessly collecting. Costs and site availability are enforced by the game. Completed production earns a real wage from the village treasury. Rest when energy is low; eat action 50 when hungry, buying one real food for one coin. A resident who owns a plank may visit an idle neighbor to offer it for two coins. Visits start a real two-way conversation: each person can invite, offer a plank sale, ask help, accept or refuse, and accepted obligations become actual tasks. Remember relationships and vary whom you meet. Old observation actions 1/2 produce nothing. Use recent completed choices to avoid needless repetition. Return ONLY JSON with exactly action_id (integer) and reason (brief first-person Chinese, at most 60 Chinese characters). Do not invent actions or resources.");
    SendDecisionRequest(Index,Context,Prompt,true);
}

void AHearthVillage::UpdateLifeDecisions()
{
    if(!bAutonomousLifeEnabled || bSimulationPaused) return;
    const double Now=Elapsed;
    const int32 StartAfter=LastLifeResident;
    for(int32 Offset=1;Offset<=Residents.Num();++Offset)
    {
        const int32 Index=(StartAfter+Offset)%Residents.Num();
        auto& R=Residents[Index];
        if(R.Task!=EHearthTask::LifeChoosing || !R.Route.IsEmpty() || Now<R.NextLifeDecision || !HasDecisionCapacity(Index)) continue;
        LastLifeResident=Index; R.NextLifeDecision=Now+LifeDecisionInterval;
        RequestLifeDecision(Index);
    }
}

void AHearthVillage::AdvanceLife(int32 Index,float Dt)
{
    auto& R=Residents[Index];
    if(!R.ConversationId.IsEmpty())
    {
        if(R.Task==EHearthTask::LifeTravel && MoveResident(Index,Dt)) R.Task=EHearthTask::LifeActivity;
        return;
    }
    if(R.Task==EHearthTask::LifeTravel && MoveResident(Index,Dt))
    { R.Task=EHearthTask::LifeActivity; R.Timer=R.LifeAction==50?5.f:R.LifeAction==0?20.f:15.f; }
    else if(R.Task==EHearthTask::LifeActivity && R.Timer<=0)
    {
        FString Extra; bool Ate=false;
        if(R.LifeAction==50)
        {
            if(FoodStock>0 && TransferCoins(TEXT("food_purchase"),R.ActiveTaskId,Index,-1,1,TEXT("food"),1))
            { --FoodStock; ++Spent[0]; Ate=true; R.Hunger=FMath::Max(0.f,R.Hunger-55.f); R.Mood=FMath::Min(100.f,R.Mood+5.f); Extra=TEXT("花1枚钱购买并吃掉1份食物，交易与消耗均已入账。"); }
            else Extra=FoodStock<=0?TEXT("到达时食物已用完，这次没有吃到饭。"):TEXT("钱包不足或这笔餐食已经结算。");
        }
        else if(R.LifeAction==0) R.Energy=FMath::Min(100.f,R.Energy+35.f);
        // Legacy visit timers without a conversation do not invent a mutual encounter.
        else R.Energy=FMath::Max(0.f,R.Energy-10.f);
        const FString Result=FString::Printf(TEXT("%s已完成。精力 %.0f，饥饿 %.0f，社交需求 %.0f。%s%s"),*LifeActionName(Index,R.LifeAction),R.Energy,R.Hunger,R.SocialNeed,
            R.LifeAction==1||R.LifeAction==2?TEXT("本次只观察，没有改变资源库存。"):TEXT(""),*Extra);
        R.LatestEvent=Result; CompleteHistory(Index,Result);
        if(R.LifeAction==50) CompleteCommitments(Index,Ate,Result);
        R.Task=EHearthTask::LifeChoosing;
        VillageEvent=R.Name+TEXT("完成了活动，准备下一次选择。");
    }
}

void AHearthVillage::ToggleAutonomy() { bAutonomousLifeEnabled=!bAutonomousLifeEnabled; WriteSnapshot(); }

FString AHearthVillage::LifeSummary() const
{
    if(!bAutonomousLifeEnabled) return TEXT("自主生活已关闭 · 已开始的行动仍会完成");
    TArray<FString> Thinking;
    for(int32 I=0;I<Residents.Num();++I) if(IsDecisionPending(I) && !PendingDecisions[I].bGameplayReleased) Thinking.Add(Residents[I].Name);
    if(!Thinking.IsEmpty()) return FString::Join(Thinking,TEXT("、"))+TEXT("正在各自思考，回复后独立执行");
    if(ApiRequests>=ApiMaxRequests && bApiReady) return TEXT("本轮模型预算已用完 · 后续采用本地规则");
    const float RealWait=LifeDecisionInterval/FMath::Max(1.f,SimulationSpeed);
    return FString::Printf(TEXT("自主生活开启 · 每人独立思考 · 间隔 %d 秒模拟时间（当前约 %.2f 秒现实时间）"),FMath::RoundToInt(LifeDecisionInterval),RealWait);
}
