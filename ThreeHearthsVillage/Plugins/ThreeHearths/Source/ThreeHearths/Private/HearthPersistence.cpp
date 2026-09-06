#include "HearthWorldState.h"
#include "Dom/JsonObject.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

void AHearthVillage::InitializeWorldPersistence()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("HearthNoWorldPersistence")))
    { WorldSaveStatus=TEXT("隔离测试：不保存世界"); return; }
    WorldPath=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/World/current-world.json");
    FParse::Value(FCommandLine::Get(),TEXT("HearthWorld="),WorldPath);
    WorldPath=FPaths::ConvertRelativePathToFull(WorldPath);
    bWorldPersistenceEnabled=true;
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); Files.CreateDirectoryTree(*FPaths::GetPath(WorldPath));
    // Exclusive open blocks a second PIE/editor process from advancing this same world.
    WorldLease=MakeShareable(Files.OpenWrite(*(WorldPath+TEXT(".lock")),false,false));
    if(!WorldLease.IsValid())
    {
        bWorldWriteBlocked=true; bSimulationPaused=true; bApiReady=false;
        WorldSaveStatus=TEXT("此世界正在另一个进程中运行，当前模拟已暂停"); return;
    }
    if(Files.FileExists(*WorldPath) || Files.FileExists(*(WorldPath+TEXT(".bak"))))
    { if(!LoadWorld()) { bWorldWriteBlocked=true; bSimulationPaused=true; bApiReady=false; } }
    else SaveWorld();
}

FString AHearthVillage::ExportWorldState() const
{
    FHearthWorldImage W; W.Id=WorldId; W.Run=CurrentRun; W.Revision=WorldRevision;
    W.PlotCount=HousingPlotCount();
    W.Event=VillageEvent; W.Elapsed=Elapsed; W.Speed=SimulationSpeed; W.Remainder=SimulationRemainder;
    W.bIsland=bUseCropoutMap; W.bPaused=bSimulationPaused; W.bAutonomy=bAutonomousLifeEnabled; W.bComplete=bReportedComplete;
    W.Selected=SelectedResident; W.LastLife=LastLifeResident; W.Food=FoodStock; W.Stone=StoneStock; W.Planks=PlankStock; W.Beams=BeamStock; W.TreasuryCoins=TreasuryCoins;
    W.TaxProjectCoins=TaxProjectCoins; W.TaxRatePercent=TaxRatePercent; for(int32 I=0;I<10;++I) W.TaxRemainders[I]=TaxRemainders[I];
    for(int32 I=0;I<3;++I)
    {
        W.Wood[I]=WoodStock[I]; W.Stocks[I]=WoodPositions[I]; W.Produced[I]=Produced[I]; W.Spent[I]=Spent[I];
    }
    for(int32 I=0;I<2;++I) { W.Manufactured[I]=Manufactured[I]; W.ManufacturedSpent[I]=ManufacturedSpent[I]; }
    for(int32 I=0;I<W.PlotCount;++I) { W.Owners[I]=PlotOwners[I]; W.Costs[I]=PlotCosts[I]; W.PlotIds[I]=PlotIds[I]; W.Plots[I]=PlotPositions[I]; }
    for(int32 I=0;I<Residents.Num();++I)
    {
        FHearthSavedResident S; S.Person=Residents[I]; S.Person.Actor=nullptr;
        if(IsValid(Residents[I].Actor)) { S.Position=Residents[I].Actor->GetActorLocation(); S.Yaw=Residents[I].Actor->GetActorRotation().Yaw; }
        S.DecisionDelay=FMath::Max(0.0,Residents[I].NextLifeDecision-Elapsed);
        if(PendingDecisions.IsValidIndex(I)) { S.bPending=PendingDecisions[I].bActive; S.PendingOperation=PendingDecisions[I].OperationId; }
        W.People.Add(MoveTemp(S));
    }
    W.Sites=ProductionSites; W.Totals=ProductionTotals; W.History=DecisionHistory;
    W.Conversations=Conversations; W.Commitments=Commitments; W.Transactions=Transactions; W.TaxAssessments=TaxAssessments; W.WagePayables=WagePayables; W.TradeOffers=TradeOffers;
    W.PublicProject=PublicProject;
    W.StructurePlans=StructurePlans;
    return HearthWorld::Encode(W);
}

bool AHearthVillage::SaveWorld()
{
    if(!bWorldPersistenceEnabled || WorldPath.IsEmpty() || bWorldWriteBlocked || !WorldLease.IsValid()) return false;
    ++WorldRevision; FString Error;
    if(!HearthWorld::Write(WorldPath,ExportWorldState(),Error))
    {
        --WorldRevision; WorldSaveStatus=TEXT("存档失败：")+Error;
        UE_LOG(LogTemp,Warning,TEXT("WORLD_SAVE_REJECTED path=%s revision=%lld reason=%s"),*WorldPath,WorldRevision+1,*Error);
        bApiDisabledThisRun=true; bSimulationPaused=true; return false;
    }
    WorldSaveStatus=FString::Printf(TEXT("世界已保存 · 第 %lld 版 · 每 30 秒自动保存"),WorldRevision);
    WorldSaveTimer=0; return true;
}

bool AHearthVillage::ApplyWorldState(const FString& Text,FString& Error)
{
    FHearthWorldImage W; if(!HearthWorld::Decode(Text,W,Error)) return false;
    const int32 SourceSchema=W.Schema;
    if(W.Schema<6)
    {
        for(const auto& T:W.Transactions) if((T.Kind==TEXT("wage") || T.Kind==TEXT("plank_trade"))
            && !W.TaxAssessments.ContainsByPredicate([&](const FHearthTaxAssessment& A) { return A.SourceTransactionId==T.Id; }))
        {
            FHearthTaxAssessment A; A.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); A.SourceTransactionId=T.Id;
            A.Resident=T.To; A.Gross=T.Amount; A.Net=T.Amount; A.At=T.At; A.bLegacyExempt=true;
            A.RemainderBefore=W.TaxRemainders[T.To]; A.RemainderAfter=A.RemainderBefore; W.TaxAssessments.Add(MoveTemp(A));
        }
        W.Schema=6;
    }
    const bool Migrating=W.People.Num()!=Residents.Num();
    if(W.bIsland!=bUseCropoutMap) { Error=TEXT("存档的地图与当前场景不匹配"); return false; }
    if(!MigrateWorldPopulation(W,Error)) return false;
    for(int32 I=0;I<HousingPlotCount();++I) if(!W.Plots[I].Equals(PlotPositions[I],.01) || W.Costs[I]!=PlotCosts[I])
    { Error=TEXT("地图布局已变化，须先迁移存档"); return false; }
    for(int32 I=0;I<3;++I) if(!W.Stocks[I].Equals(WoodPositions[I],.01)) { Error=TEXT("材料站布局与存档不匹配"); return false; }
    for(const auto& R:Residents) if(!IsValid(R.Actor)) { Error=TEXT("居民表现对象未准备好"); return false; }
    // Everything above is read-only. Apply the validated image together on the game thread.
    StopDecisionRequests(); LoadApiConfig();
    WorldId=W.Id; CurrentRun=W.Run; WorldRevision=W.Revision; VillageEvent=W.Event; Elapsed=W.Elapsed;
    SimulationSpeed=W.Speed; SimulationRemainder=W.Remainder; bSimulationPaused=W.bPaused;
    // Loading a world must not silently keep the whole village idle because an
    // earlier session closed autonomy. LoadApiConfig above establishes the current
    // run policy; -HearthNoAutonomousLife remains the explicit headless/test opt-out.
    bReportedComplete=W.bComplete; LastLifeResident=W.LastLife;
    FoodStock=W.Food; StoneStock=W.Stone; PlankStock=W.Planks; BeamStock=W.Beams; DecisionHistory=MoveTemp(W.History); ++HistoryRevision;
    Conversations=MoveTemp(W.Conversations); Commitments=MoveTemp(W.Commitments); Transactions=MoveTemp(W.Transactions); TaxAssessments=MoveTemp(W.TaxAssessments);
    WagePayables=MoveTemp(W.WagePayables); TradeOffers=MoveTemp(W.TradeOffers); TreasuryCoins=W.TreasuryCoins; ++SocialRevision; bSocialOpen=false;
    PublicProject=MoveTemp(W.PublicProject);
    StructurePlans=MoveTemp(W.StructurePlans);
    TaxProjectCoins=W.TaxProjectCoins; TaxRatePercent=W.TaxRatePercent; for(int32 I=0;I<10;++I) TaxRemainders[I]=W.TaxRemainders[I];
    for(int32 I=0;I<3;++I)
    {
        WoodStock[I]=W.Wood[I]; Produced[I]=W.Produced[I]; Spent[I]=W.Spent[I];
        if(StockMeshes.IsValidIndex(I))
        { StockMeshes[I]->SetVisibility(WoodStock[I]>0); StockMeshes[I]->SetRelativeScale3D(FVector(1.1f,1.2f,FMath::Clamp(WoodStock[I]/12.f*.4f,.04f,1.2f))); }
    }
    for(int32 I=0;I<2;++I) { Manufactured[I]=W.Manufactured[I]; ManufacturedSpent[I]=W.ManufacturedSpent[I]; }
    for(int32 I=0;I<HousingPlotCount();++I)
    { PlotOwners[I]=W.Owners[I]; PlotIds[I]=W.PlotIds[I]; if(HouseMeshes.IsValidIndex(I)) HouseMeshes[I]->SetVisibility(false); }
    PendingDecisions.SetNum(Residents.Num()); bool Interrupted=false;
    for(int32 I=0;I<Residents.Num();++I)
    {
        auto* Actor=Residents[I].Actor.Get(); const auto& S=W.People[I]; Residents[I]=S.Person; auto& R=Residents[I];
        R.Actor=Actor; Actor->ResidentIndex=I; Actor->SetActorLocation(S.Position); Actor->SetActorRotation(FRotator(0,S.Yaw,0));
        if(R.Role.IsEmpty()) { FHearthResident Identity; InitializeResidentIdentity(I,Identity); R.Role=Identity.Role; R.Age=Identity.Age; }
        if(R.HouseBlueprint.IsEmpty()) AssignHouseStyle(I,R);
        if((R.Task==EHearthTask::ProductionTravel || R.Task==EHearthTask::ProductionWork) && R.HeldToolId.IsEmpty()) TryBorrowTool(I,R.ProductionOp);
        R.NextLifeDecision=Elapsed+S.DecisionDelay;
        if(R.Plot>=0) SetHouseStage(R.Plot,FMath::Min(3,FMath::FloorToInt(R.BuildProgress*3.f)));
        if(S.bPending)
        {
            Interrupted=true; R.DecisionSource=TEXT("local_fallback");
            R.DecisionNote=TEXT("重启时有未确认请求：")+S.PendingOperation+TEXT("；不自动重试，费用以独立账本为准");
            R.Timer=FMath::Min(R.Timer,1.f); R.NextLifeDecision=Elapsed+LifeDecisionInterval;
            if(DecisionHistory.IsValidIndex(R.HistoryIndex))
            { auto& H=DecisionHistory[R.HistoryIndex]; H.Status=TEXT("interrupted"); H.Result=R.DecisionNote; }
            for(auto& H:DecisionHistory) if(H.Run==CurrentRun && H.Resident==I && H.Kind==TEXT("social_turn") && H.Status==TEXT("thinking"))
            { H.Status=TEXT("interrupted"); H.Result=R.DecisionNote; }
        }
        PendingDecisions[I]=FHearthPendingDecision();
    }
    if(SourceSchema<4)
    {
        for(const auto& T:Transactions) if(T.Kind==TEXT("wage") && !WagePayables.ContainsByPredicate([&](const FHearthWagePayable& P) { return P.TaskId==T.TaskId; }))
        {
            FHearthWagePayable P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); P.TaskId=T.TaskId;
            P.Worker=T.To; P.Amount=T.Amount; P.Status=TEXT("paid"); WagePayables.Add(MoveTemp(P));
        }
        for(int32 I=0;I<Residents.Num();++I)
        {
            const auto& R=Residents[I];
            if(R.Task>=EHearthTask::ProductionTravel && R.Task<=EHearthTask::ProductionDeposit
                && !WagePayables.ContainsByPredicate([&](const FHearthWagePayable& P) { return P.TaskId==R.ActiveTaskId; }))
            {
                const int32 Wage=WageForOperation(R.ProductionOp);
                if(!ReserveWage(I,R.ActiveTaskId,Wage))
                {
                    FHearthWagePayable P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); P.TaskId=R.ActiveTaskId;
                    P.Worker=I; P.Amount=Wage; P.Status=TEXT("unfunded"); WagePayables.Add(MoveTemp(P));
                }
            }
        }
    }
    NextTradeAt=Elapsed+8.f;
    for(auto& M:ProductionMeshes) if(IsValid(M)) M->DestroyComponent();
    ProductionMeshes.Reset(); ProductionSites=MoveTemp(W.Sites); ProductionTotals=MoveTemp(W.Totals);
    if(!ProductionSites.ContainsByPredicate([](const FHearthSite& S) { return S.Kind==EHearthSiteKind::Carpenter; }))
    {
        FHearthSite S; S.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); S.Kind=EHearthSiteKind::Carpenter;
        S.Position=FVector(-2250,-1050,8); S.Radius=190; ProductionSites.Add(MoveTemp(S)); ChooseSiteApproach(ProductionSites.Num()-1);
    }
    for(auto& S:ProductionSites) { S.VisualStage=-99; S.Meshes.Reset(); S.Soil.Reset(); }
    if(Migrating) for(auto& R:Residents) if(!IsClearPoint(R.Actor->GetActorLocation()))
    {
        const FVector Before=R.Actor->GetActorLocation(); FVector Best=Before; double Distance=DBL_MAX;
        for(const auto& Cell:LandGrid)
        {
            const FVector Point(Cell.X*300,Cell.Y*300,8); const double D=FVector::DistSquared2D(Point,Before);
            if(D>=Distance || !IsClearPoint(Point)) continue;
            bool Occupied=false; for(const auto& Other:Residents) if(&Other!=&R && FVector::Dist2D(Other.Actor->GetActorLocation(),Point)<120) Occupied=true;
            if(!Occupied) { Best=Point; Distance=D; }
        }
        R.Actor->SetActorLocation(Best); R.LatestEvent=TEXT("新住宅地块划定后，移到附近空地继续原任务。");
    }
    RefreshProductionVisuals(); SelectResident(W.Selected); bHistoryOpen=false;
    if(Interrupted) { bApiDisabledThisRun=true; ApiStatus=TEXT("已恢复世界；未确认请求不重试，本轮使用本地规则"); }
    SaveHistory(); WorldSaveTimer=0; return true;
}

bool AHearthVillage::LoadWorld()
{
    if(!bWorldPersistenceEnabled || !WorldLease.IsValid()) return false;
    FString Payload,Error; bool Backup=false;
    if(!HearthWorld::Read(WorldPath,Payload,Error))
    {
        if(!HearthWorld::Read(WorldPath+TEXT(".bak"),Payload,Error)) { WorldSaveStatus=TEXT("主存档与备份均无法恢复：")+Error; return false; }
        Backup=true;
    }
    // Keep the invalid file verbatim before restoring the previous complete checkpoint.
    if(Backup && !HearthWorld::Archive(WorldPath,Error)) { WorldSaveStatus=Error; return false; }
    if(!ApplyWorldState(Payload,Error)) { WorldSaveStatus=Error; return false; }
    bWorldWriteBlocked=false;
    if(Backup && !SaveWorld()) return false;
    WorldSaveStatus=Backup?TEXT("已保留损坏文件并恢复上一版完整世界"):TEXT("已恢复世界、材料、任务与历史");
    WriteSnapshot(); return true;
}

void AHearthVillage::RestartVillage()
{
    if(bWorldPersistenceEnabled)
    {
        if(!WorldLease.IsValid()) { WorldSaveStatus=TEXT("无法新建：世界由另一个进程持有"); return; }
        FString Error;
        if(!HearthWorld::Archive(WorldPath,Error) || !HearthWorld::Archive(WorldPath+TEXT(".bak"),Error)) { WorldSaveStatus=Error; return; }
    }
    bWorldWriteBlocked=false; ResetVillageState();
    if(bWorldPersistenceEnabled) SaveWorld();
}
