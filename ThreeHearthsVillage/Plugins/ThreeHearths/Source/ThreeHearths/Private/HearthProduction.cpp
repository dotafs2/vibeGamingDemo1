#include "HearthVillage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HearthProduction
{
    const FString Crops=TEXT("/Game/Environment/Meshes/Crops/");
    const FString Buildings=TEXT("/Game/Environment/Meshes/Building/");
    const TCHAR* KindNames[]={TEXT("未开发空地"),TEXT("已开垦土地"),TEXT("玉米田"),TEXT("小麦田"),TEXT("生菜田"),TEXT("南瓜田"),TEXT("住宅"),TEXT("树木"),TEXT("浆果灌木"),TEXT("石矿"),TEXT("木工台")};
    const TCHAR* KindKeys[]={TEXT("empty"),TEXT("land"),TEXT("corn"),TEXT("wheat"),TEXT("lettuce"),TEXT("pumpkin"),TEXT("house"),TEXT("tree"),TEXT("shrub"),TEXT("stone"),TEXT("carpenter")};
    const TCHAR* OpKeys[]={TEXT("claim_land"),TEXT("build_corn"),TEXT("build_wheat"),TEXT("build_lettuce"),TEXT("build_pumpkin"),TEXT("build_house"),TEXT("plant_tree"),TEXT("plant_shrub"),TEXT("sow"),TEXT("harvest"),TEXT("chop"),TEXT("quarry"),TEXT("gather"),TEXT("mill_planks"),TEXT("frame_beams")};
    const TCHAR* ToolForOp(int32 Op)
    {
        if(Op==10) return TEXT("tool_axe");
        if(Op==11) return TEXT("tool_pickaxe");
        if(Op==5) return TEXT("tool_hammer");
        if(Op==0 || Op==6) return TEXT("tool_shovel");
        if(Op==7 || Op==12) return TEXT("tool_trowel");
        if((Op>=1 && Op<=4) || Op==8 || Op==9) return TEXT("tool_hoe");
        if(Op==13) return TEXT("tool_saw");
        if(Op==14) return TEXT("tool_mallet");
        return TEXT("");
    }
    const TCHAR* ResourceNames[]={TEXT("食物"),TEXT("原木"),TEXT("石材"),TEXT("木板"),TEXT("房梁")};
    bool IsCrop(EHearthSiteKind K) { return K>=EHearthSiteKind::Corn && K<=EHearthSiteKind::Pumpkin; }
    int32 Action(int32 Site,int32 Op) { return 100+Site*16+Op; }
    bool Decode(int32 Action,int32& Site,int32& Op)
    { if(Action<100) return false; Site=(Action-100)/16; Op=(Action-100)%16; return Op<=14; }
    void Cost(int32 Op,int32& Food,int32& Wood,int32& Stone)
    {
        Food=Wood=Stone=0;
        if(Op>=1 && Op<=4) { Food=10; Wood=10; }
        else if(Op==5) { }
        else if(Op==6) { Food=2; Wood=5; }
        else if(Op==7) { Food=5; Wood=2; }
        else if(Op==8) Food=1;
        else if(Op==13) Wood=2;
        else if(Op==14) Wood=3;
    }
    void CottagePart(int32 Stage,int32& CargoType,int32& Amount,const TCHAR*& Name)
    {
        CargoType=Stage==0?2:Stage==1?4:3; Amount=Stage==0?2:Stage==1?2:Stage==2?3:2;
        const TCHAR* Names[]={TEXT("石质地基层"),TEXT("木梁框架"),TEXT("灰泥墙体层"),TEXT("陶瓦屋顶层")}; Name=Names[FMath::Clamp(Stage,0,3)];
    }
    FString Json(const TSharedRef<FJsonObject>& Object) { FString S; FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&S)); return S; }
}

void AHearthVillage::InitializeProduction()
{
    for(const auto& M:ProductionMeshes) if(IsValid(M.Get())) M->DestroyComponent();
    ProductionMeshes.Reset(); ProductionSites.Reset(); ProductionTotals.Reset(); FixedObstacles.Reset();
    FoodStock=Residents.Num()*10; StoneStock=0; PlankStock=BeamStock=0;
    for(int32 I=0;I<3;++I) { Produced[I]=0; Spent[I]=0; }
    for(int32 I=0;I<2;++I) { Manufactured[I]=0; ManufacturedSpent[I]=0; }
    if(!bUseCropoutMap) { ProductionStatus=TEXT("生产与扩建技能在大岛地图开放"); return; }
    for(int32 I=0;I<HousingPlotCount();++I) FixedObstacles.Add(FVector(PlotPositions[I].X,PlotPositions[I].Y,230));
    FixedObstacles.Add(FVector(-1100,-1050,330));
    TArray<UStaticMeshComponent*> Existing; GetComponents(Existing);
    for(auto* M:Existing) if(M->GetStaticMesh())
    {
        const FString N=M->GetStaticMesh()->GetName();
        if(N.StartsWith(TEXT("SM_Tree")) || N.StartsWith(TEXT("SM_Stone")) || N.StartsWith(TEXT("SM_Shrub")))
        { const FVector P=M->GetComponentLocation(); FixedObstacles.Add(FVector(P.X,P.Y,N.StartsWith(TEXT("SM_Tree"))?150:100)); }
    }
    BuildLandGrid();
    auto AddSite=[this](EHearthSiteKind Kind,FVector Position,float Radius,bool Expansion=false)
    {
        if(!IsLand(Position) || !IsLand(Position+FVector(Radius,Radius,0)) || !IsLand(Position-FVector(Radius,Radius,0))
            || !IsLand(Position+FVector(-Radius,Radius,0)) || !IsLand(Position+FVector(Radius,-Radius,0))) return;
        FHearthSite Site; Site.Kind=Kind; Site.Position=Position; Site.Radius=Radius; Site.bExpansion=Expansion;
        Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        if(Kind==EHearthSiteKind::Tree) { Site.Stage=2; Site.Units=Site.Capacity=18; Site.GrowDuration=180; }
        if(Kind==EHearthSiteKind::Shrub) { Site.Stage=2; Site.Units=Site.Capacity=12; Site.GrowDuration=120; }
        if(Kind==EHearthSiteKind::Stone) { Site.Stage=2; Site.Units=Site.Capacity=36; }
        if(HearthProduction::IsCrop(Kind)) { const int32 Yields[]={12,14,10,16}; Site.Capacity=Yields[static_cast<int32>(Kind)-2]; }
        ProductionSites.Add(MoveTemp(Site));
    };
    for(int32 I=0;I<4;++I) AddSite(static_cast<EHearthSiteKind>(I+2),FVector(-1460+(I%2)*650,-3150+(I/2)*650,8),245);
    for(int32 I=0;I<3;++I) AddSite(EHearthSiteKind::Tree,FVector(-4300,-1900+I*1900,8),160);
    for(int32 I=0;I<3;++I) AddSite(EHearthSiteKind::Stone,FVector(-1000+I*800,3100,8),145);
    AddSite(EHearthSiteKind::Shrub,FVector(-3300,-2800,8),120);
    AddSite(EHearthSiteKind::Shrub,FVector(-2900,-3400,8),120);
    AddSite(EHearthSiteKind::Shrub,FVector(-3700,-3400,8),120);
    AddSite(EHearthSiteKind::Carpenter,FVector(-2250,-1050,8),190);
    int32 Plots=0;
    TArray<FVector> Candidates;
    for(int32 X=-6000;X<=5700;X+=900) for(int32 Y=-6000;Y<=5700;Y+=900) Candidates.Add(FVector(X,Y,8));
    Candidates.Sort([](const FVector& A,const FVector& B) { return FVector::DistSquared2D(A,FVector(-1100,-1050,8))<FVector::DistSquared2D(B,FVector(-1100,-1050,8)); });
    for(const FVector& P:Candidates)
    {
        if(Plots>=18) break;
        bool Clear=true;
        for(const auto& S:ProductionSites) if(FMath::Abs(P.X-S.Position.X)<S.Radius+350 && FMath::Abs(P.Y-S.Position.Y)<S.Radius+350) Clear=false;
        for(const auto& O:FixedObstacles) if(FMath::Abs(P.X-O.X)<O.Z+350 && FMath::Abs(P.Y-O.Y)<O.Z+350) Clear=false;
        if(!Clear) continue;
        const int32 Before=ProductionSites.Num(); AddSite(EHearthSiteKind::Empty,P,260,true); Plots+=ProductionSites.Num()>Before;
    }
    int32 Reachable=0;
    for(int32 I=0;I<ProductionSites.Num();++I) { if(ChooseSiteApproach(I)) ++Reachable; UpdateSiteVisual(I); }
    ProductionStatus=FString::Printf(TEXT("可达生产点 / 地块 %d / %d · 全员拥有全部技能"),Reachable,ProductionSites.Num());
}

bool AHearthVillage::IsProductionAllowed(int32 Index,int32 Action) const
{
    int32 Site,Op;
    if(!Residents.IsValidIndex(Index) || Residents[Index].BuildProgress<1 || !HearthProduction::Decode(Action,Site,Op) || !ProductionSites.IsValidIndex(Site)) return false;
    const auto& S=ProductionSites[Site]; if(!S.bReachable || S.ReservedBy>=0) return false;
    int32 Food,Wood,Stone; HearthProduction::Cost(Op,Food,Wood,Stone);
    if(FoodStock<Food || AvailableWood()<Wood || StoneStock<Stone) return false;
    if(!ToolAvailableFor(Index,Op)) return false;
    if(Op==0) return S.Kind==EHearthSiteKind::Empty;
    if(Op==5)
    {
        if(S.Kind!=EHearthSiteKind::Land || S.Stage>=4) return false;
        int32 Type,Amount; const TCHAR* Name; HearthProduction::CottagePart(S.Stage,Type,Amount,Name);
        return Type==2?StoneStock>=Amount:Type==3?PlankStock>=Amount:BeamStock>=Amount;
    }
    if(Op>=1 && Op<=7) return S.Kind==EHearthSiteKind::Land && S.BuildPlanId.IsEmpty();
    if(Op==8) return HearthProduction::IsCrop(S.Kind) && S.Stage==0;
    if(Op==9) return HearthProduction::IsCrop(S.Kind) && S.Stage==2 && S.Units>0;
    if(Op==10) return S.Kind==EHearthSiteKind::Tree && S.Stage==2 && S.Units>0;
    if(Op==11) return S.Kind==EHearthSiteKind::Stone && S.Units>0;
    if(Op==12) return S.Kind==EHearthSiteKind::Shrub && S.Stage==2 && S.Units>0;
    if(Op==13 || Op==14) return S.Kind==EHearthSiteKind::Carpenter;
    return false;
}

bool AHearthVillage::ToolAvailableFor(int32 Index,int32 Operation) const
{
    const FString Tool=HearthProduction::ToolForOp(Operation);
    if(Tool.IsEmpty()) return true;
    for(int32 I=0;I<Residents.Num();++I) if(I!=Index && Residents[I].HeldToolId==Tool) return false;
    return true;
}

bool AHearthVillage::TryBorrowTool(int32 Index,int32 Operation)
{
    if(!Residents.IsValidIndex(Index) || !ToolAvailableFor(Index,Operation)) return false;
    auto& R=Residents[Index]; const FString Tool=HearthProduction::ToolForOp(Operation);
    if(Tool.IsEmpty()) return true;
    if(!R.HeldToolId.IsEmpty() && R.HeldToolId!=Tool) return false;
    R.HeldToolId=Tool; R.HeldToolOperationId=R.ActiveTaskId;
    return true;
}

void AHearthVillage::ReturnTool(int32 Index)
{
    if(!Residents.IsValidIndex(Index)) return;
    Residents[Index].HeldToolId.Empty(); Residents[Index].HeldToolOperationId.Empty();
}

TArray<int32> AHearthVillage::AvailableProductionActions(int32 Index) const
{
    TArray<int32> Actions;
    if(!Residents.IsValidIndex(Index) || !IsValid(Residents[Index].Actor)) return Actions;
    // Keep model input compact: offer two nearby alternatives for each operation.
    for(int32 Op=0;Op<=14;++Op)
    {
        TArray<int32> Candidates;
        for(int32 S=0;S<ProductionSites.Num();++S) if(IsProductionAllowed(Index,HearthProduction::Action(S,Op))) Candidates.Add(S);
        const FVector P=Residents[Index].Actor->GetActorLocation();
        Candidates.Sort([this,P](int32 A,int32 B) { return FVector::DistSquared2D(P,ProductionSites[A].Approach)<FVector::DistSquared2D(P,ProductionSites[B].Approach); });
        for(int32 I=0;I<FMath::Min(2,Candidates.Num());++I) Actions.Add(HearthProduction::Action(Candidates[I],Op));
    }
    return Actions;
}

FString AHearthVillage::ProductionActionName(int32 Action) const
{
    int32 Site,Op; if(!HearthProduction::Decode(Action,Site,Op) || !ProductionSites.IsValidIndex(Site)) return TEXT("不可用生产任务");
    const auto& S=ProductionSites[Site];
    const TCHAR* Verbs[]={TEXT("开垦新土地"),TEXT("建设玉米田"),TEXT("建设小麦田"),TEXT("建设生菜田"),TEXT("建设南瓜田"),TEXT("建造新住宅"),TEXT("栽种树木"),TEXT("种植浆果灌木"),TEXT("播种耕作"),TEXT("收获作物并运回"),TEXT("伐木并运回"),TEXT("采石并运回"),TEXT("采集浆果并运回"),TEXT("锯制木板并运回"),TEXT("加工房梁并运回")};
    FString Name=FString::Printf(TEXT("%s · %d号%s"),Verbs[Op],Site+1,HearthProduction::KindNames[static_cast<int32>(S.Kind)]);
    int32 F,W,T; HearthProduction::Cost(Op,F,W,T);
    if(F+W+T>0) Name+=FString::Printf(TEXT("（食物%d / 木材%d / 石材%d）"),F,W,T);
    if(Op==5)
    {
        int32 Type,Amount; const TCHAR* Part; HearthProduction::CottagePart(S.Stage,Type,Amount,Part);
        Name+=FString::Printf(TEXT(" · 下一构件 %s（%s %d）"),Part,HearthProduction::ResourceNames[Type],Amount);
    }
    return Name;
}

int32 AHearthVillage::ChooseProductionLocally(int32 Index) const
{
    const auto Options=AvailableProductionActions(Index); int32 Best=-1; float BestScore=-FLT_MAX;
    for(int32 Action:Options)
    {
        int32 Site,Op; HearthProduction::Decode(Action,Site,Op); float Score=5;
        if(Op==9 || Op==12) Score=FoodStock<20?180:35;
        if(Op==10) Score=AvailableWood()<60?150:20;
        if(Op==11) Score=StoneStock<10?140:15;
        if(Op==13) Score=PlankStock<12?165:18;
        if(Op==14) Score=BeamStock<8?155:16;
        if(Op==8) Score=FoodStock<40?125:45;
        if(Op==0) { int32 Ready=0; for(const auto& S:ProductionSites) Ready+=S.Kind==EHearthSiteKind::Land; Score=Ready<2?90:2; }
        if(Op>=1 && Op<=7)
        {
            const FString Key=HearthProduction::OpKeys[Op]; Score=ProductionTotals.FindRef(Key)==0?100:10;
            if(Op==5 && AvailableWood()<70) Score-=50;
        }
        if(Index==0 && (Op==10 || Op==6)) Score+=10;
        if(Index==1 && (Op==8 || Op==9 || (Op>=1 && Op<=4))) Score+=10;
        if(Index==2 && (Op==11 || Op==5)) Score+=10;
        Score-=FVector::Dist2D(Residents[Index].Actor->GetActorLocation(),ProductionSites[Site].Approach)/2000.f;
        if(Score>BestScore) { BestScore=Score; Best=Action; }
    }
    return Best;
}

bool AHearthVillage::StartProduction(int32 Index,int32 Action,const FString& Reason,bool bFromApi)
{
    if(!IsProductionAllowed(Index,Action)) return false;
    int32 Site,Op; HearthProduction::Decode(Action,Site,Op);
    TArray<FVector> Route;
    if(!FindActivityRoute(Index,ProductionSites[Site].Approach,Route)) return false;
    auto& R=Residents[Index]; auto& S=ProductionSites[Site];
    if(!DecisionHistory.IsValidIndex(R.HistoryIndex) || DecisionHistory[R.HistoryIndex].Status!=TEXT("thinking")) StartHistory(Index,true,bFromApi?TEXT("api"):TEXT("local"));
    const FString Label=ProductionActionName(Action);
    R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    if(!TryBorrowTool(Index,Op)) { R.ActiveTaskId.Empty(); return false; }
    if(!ReserveWage(Index,R.ActiveTaskId,WageForOperation(Op)))
    {
        ReturnTool(Index); R.ActiveTaskId.Empty(); R.LatestEvent=TEXT("村库无法预留工资，这项工作暂不开始。"); return false;
    }
    int32 F,W,T; HearthProduction::Cost(Op,F,W,T);
    FoodStock-=F; StoneStock-=T; Spent[0]+=F; Spent[1]+=W; Spent[2]+=T;
    for(int32 I=0;I<3 && W>0;++I) { const int32 Used=FMath::Min(W,WoodStock[I]); WoodStock[I]-=Used; W-=Used; }
    S.ReservedBy=Index; S.Progress=0;
    R.ProductionSite=Site; R.ProductionOp=Op; R.CargoAmount=0; R.CargoType=-1;
    if(Op==5)
    {
        int32 Type,Amount; const TCHAR* Part; HearthProduction::CottagePart(S.Stage,Type,Amount,Part);
        if(S.BuildPlanId.IsEmpty()) { S.BuildPlanId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); S.Owner=Index; }
        R.CargoType=Type; R.CargoAmount=Amount;
        if(Type==2) StoneStock-=Amount; else if(Type==3) PlankStock-=Amount; else BeamStock-=Amount;
        R.LatestEvent=FString::Printf(TEXT("从公共库存领取 %d 份%s，前往工地安装%s。"),Amount,HearthProduction::ResourceNames[Type],Part);
    }
    R.LifeAction=Action; R.Reason=Reason; R.DecisionSource=bFromApi?TEXT("api"):TEXT("local");
    R.Route=MoveTemp(Route); R.Task=EHearthTask::ProductionTravel; if(Op!=5) R.LatestEvent=Label;
    R.MoveRetry=0; R.bMovementBlocked=false;
    R.WorkDuration=Op==5?12.f:Op==0?20.f:Op<=7?25.f:Op==8?20.f:Op==13?18.f:Op==14?22.f:12.f;
    const TCHAR* Hat=(Op==10 || Op==6)?TEXT("SKM_Woodcutter"):Op==11?TEXT("SKM_Miner"):(Op==0 || Op==8 || Op==9)?TEXT("SKM_Farmer"):Op==12?TEXT("SKM_Gatherer"):TEXT("SKP_Builder");
    if(auto* Asset=LoadObject<USkeletalMesh>(nullptr,*(FString(TEXT("/Game/Characters/Meshes/Hats/"))+Hat))) { R.Actor->Hat->SetSkeletalMesh(Asset); R.Actor->Hat->SetLeaderPoseComponent(R.Actor->Body,true); }
    AcceptHistory(Index,Label,Reason,R.DecisionSource); DecisionHistory[R.HistoryIndex].Kind=TEXT("production");
    DecisionHistory[R.HistoryIndex].Context+=TEXT("\n材料在开工时预扣；收获搬回村镇中心后才入库。");
    ++HistoryRevision; SaveHistory();
    VillageEvent=R.Name+TEXT("：")+Label; return true;
}

void AHearthVillage::AdvanceProductionWorld(float Dt)
{
    for(auto& S:ProductionSites) if(S.Growth>0 && S.ReservedBy<0)
    {
        S.Growth=FMath::Max(0.f,S.Growth-Dt);
        if(S.Growth<=0) { S.Stage=2; S.Units=S.Capacity; }
    }
}

void AHearthVillage::AdvanceProduction(int32 Index,float Dt)
{
    auto& R=Residents[Index]; if(!ProductionSites.IsValidIndex(R.ProductionSite)) return;
    auto& S=ProductionSites[R.ProductionSite]; const int32 Op=R.ProductionOp;
    if((R.Task==EHearthTask::ProductionTravel || R.Task==EHearthTask::ProductionWork) &&
       R.HeldToolId.IsEmpty() && !TryBorrowTool(Index,Op))
    {
        R.Timer+=Dt;
        R.LatestEvent=TEXT("所需工具正在由邻居使用，保留工地并等待归还。");
        return;
    }
    if(R.Task==EHearthTask::ProductionTravel)
    {
        if(MoveResident(Index,Dt)) { R.Task=EHearthTask::ProductionWork; R.Timer=R.WorkDuration; }
        return;
    }
    if(R.Task==EHearthTask::ProductionDeliver)
    {
        if(MoveResident(Index,Dt)) { R.Task=EHearthTask::ProductionDeposit; R.Timer=1.f; }
        return;
    }
    if(R.Task==EHearthTask::ProductionDeposit && R.Timer<=0)
    {
        const int32 Amount=R.CargoAmount;
        if(R.CargoType==0) FoodStock+=Amount;
        if(R.CargoType==1) WoodStock[0]+=Amount;
        if(R.CargoType==2) StoneStock+=Amount;
        if(R.CargoType==3) PlankStock+=Amount;
        if(R.CargoType==4) BeamStock+=Amount;
        const FString Result=FString::Printf(TEXT("已将 %d 份%s送达村镇中心并计入公共库存。"),Amount,HearthProduction::ResourceNames[FMath::Clamp(R.CargoType,0,4)]);
        CompleteCommitments(Index,Amount>0,Result);
        R.CargoAmount=0; R.CargoType=-1; FinishProduction(Index,Result); return;
    }
    if(R.Task!=EHearthTask::ProductionWork) return;
    S.Progress=FMath::Clamp(1.f-R.Timer/FMath::Max(1.f,R.WorkDuration),0.f,1.f);
    if(R.Timer>0) return;
    if(Op>=9)
    {
        // Reserve and validate the return path before touching any resource amount.
        TArray<FVector> Home;
        const FVector Depot=(bUseCropoutMap?FVector(-1650,-1050,8):FVector(-250,-400,8))+FVector(0,((Index%3)-1)*120,0);
        if(!FindActivityRoute(Index,Depot,Home))
        { R.Timer=3.f; R.LatestEvent=TEXT("交付路线暂不可用，保留资源并等待。"); return; }
        if(Op>=13)
        {
            R.CargoType=Op==13?3:4;
            R.CargoAmount=Op==13?3:2;
            const int32 Output=Op==13?4:2;
            Manufactured[R.CargoType-3]+=Output;
            if(Op==13) ++R.PersonalPlanks;
        }
        else
        {
            R.CargoType=Op==10?1:Op==11?2:0;
            R.CargoAmount=FMath::Min(Op==12?4:6,S.Units); S.Units-=R.CargoAmount; Produced[R.CargoType]+=R.CargoAmount;
            if(S.Units==0)
            {
                S.Stage=0;
                if(S.Kind==EHearthSiteKind::Tree || S.Kind==EHearthSiteKind::Shrub) S.Growth=S.GrowDuration;
            }
        }
        R.Route=MoveTemp(Home); R.Task=EHearthTask::ProductionDeliver;
        ReturnTool(Index);
        R.MoveRetry=0; R.bMovementBlocked=false;
        R.LatestEvent=FString::Printf(TEXT("携带 %d 份%s回村镇中心，尚未入库。"),R.CargoAmount,HearthProduction::ResourceNames[R.CargoType]);
        auto& H=DecisionHistory[R.HistoryIndex]; H.Result=R.LatestEvent; ++HistoryRevision; SaveHistory(); return;
    }
    FString Result;
    if(Op==0) { S.Kind=EHearthSiteKind::Land; S.Owner=Index; Result=TEXT("空地已开垦，可以建农田、房屋或种植树木、灌木。"); }
    else if(Op==5)
    {
        int32 Type,Amount; const TCHAR* Part; HearthProduction::CottagePart(S.Stage,Type,Amount,Part);
        if(R.CargoType!=Type || R.CargoAmount!=Amount) { R.Timer=1.f; R.LatestEvent=TEXT("构件材料记录不完整，暂停安装等待恢复。"); return; }
        if(Type==2) Spent[2]+=Amount; else ManufacturedSpent[Type-3]+=Amount;
        R.CargoType=-1; R.CargoAmount=0; ++S.Stage; if(S.Owner<0) S.Owner=Index;
        if(S.Stage>=4) { S.Kind=EHearthSiteKind::House; Result=TEXT("模块小住宅的地基、框架、墙体和屋顶已逐件安装完成。"); }
        else Result=FString::Printf(TEXT("已安装%s；住宅计划 %s 完成 %d / 4 个构件层。"),Part,*S.BuildPlanId,S.Stage);
    }
    else if(Op>=1 && Op<=7)
    {
        S.Kind=static_cast<EHearthSiteKind>(Op+1); S.Owner=Index; S.Stage=0; S.Units=0;
        if(HearthProduction::IsCrop(S.Kind))
        { const int32 Yields[]={12,14,10,16}; S.Capacity=Yields[Op-1]; Result=TEXT("农田建成，接下来可以播种耕作。"); }
        else
        { S.Capacity=Op==6?18:12; S.GrowDuration=Op==6?180.f:120.f; S.Growth=S.GrowDuration; Result=Op==6?TEXT("树苗已种下，长成后可持续供给木材。"):TEXT("浆果灌木已种下，长成后可以采集食物。"); }
    }
    else if(Op==8) { S.Stage=1; S.Growth=S.GrowDuration; Result=TEXT("播种耕作完成，作物开始生长；成熟后可收获并运回粮食。"); }
    FinishProduction(Index,Result);
}

void AHearthVillage::FinishProduction(int32 Index,const FString& Result)
{
    auto& R=Residents[Index]; auto& S=ProductionSites[R.ProductionSite];
    S.ReservedBy=-1; S.Progress=1.f; R.Energy=FMath::Max(0.f,R.Energy-8.f);
    ProductionTotals.FindOrAdd(HearthProduction::OpKeys[R.ProductionOp])++;
    const int32 Wage=WageForOperation(R.ProductionOp);
    const bool Paid=SettleWage(Index,R.ActiveTaskId);
    const FString Pay=Paid?FString::Printf(TEXT(" 已领取预留工资 %d 枚。"),Wage):TEXT(" 工资仍保留在应付账款中，等待恢复结算。");
    const FString Outcome=Result+Pay+FString::Printf(TEXT(" 当前库存：食物 %d、原木 %d、木板 %d、房梁 %d、石材 %d。"),FoodStock,AvailableWood(),PlankStock,BeamStock,StoneStock);
    R.LatestEvent=Outcome; CompleteHistory(Index,Outcome); ReturnTool(Index); R.Task=EHearthTask::LifeChoosing;
    R.ProductionSite=-1; R.ProductionOp=-1; VillageEvent=R.Name+TEXT("：")+Result;
}

bool AHearthVillage::TransferCoins(const FString& Kind,const FString& TaskId,int32 From,int32 To,int32 Amount,const FString& Item,int32 Quantity)
{
    FGuid ParsedTask;
    const bool Purchase=Kind==TEXT("food_purchase") && From>=0 && To==-1 && Item==TEXT("food") && Quantity==1 && Amount==1;
    const bool Trade=Kind==TEXT("plank_trade") && From>=0 && To>=0 && Item==TEXT("plank") && Quantity==1 && Amount==2;
    if(!FGuid::Parse(TaskId,ParsedTask) || !ParsedTask.IsValid() || From<-1 || To<-1 || From==To || From>=Residents.Num() || To>=Residents.Num()
        || (!Purchase && !Trade) || Transactions.Num()>=100000) return false;
    if(Transactions.ContainsByPredicate([&](const FHearthTransaction& T) { return T.Kind==Kind && T.TaskId==TaskId; })) return false;
    const int32 Balance=From<0?TreasuryCoins:Residents[From].Coins;
    if(Balance<Amount) return false;
    if(From<0) TreasuryCoins-=Amount; else Residents[From].Coins-=Amount;
    if(To<0) TreasuryCoins+=Amount; else Residents[To].Coins+=Amount;
    FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=Kind; T.TaskId=TaskId;
    T.From=From; T.To=To; T.Amount=Amount; T.Item=Item; T.Quantity=Quantity; T.At=Elapsed; Transactions.Add(MoveTemp(T));
    return true;
}

int32 AHearthVillage::WageForOperation(int32 Operation) const { return Operation>=13?3:2; }

bool AHearthVillage::ReserveWage(int32 Worker,const FString& TaskId,int32 Amount)
{
    FGuid ParsedTask;
    if(!Residents.IsValidIndex(Worker) || !FGuid::Parse(TaskId,ParsedTask) || !ParsedTask.IsValid() || (Amount!=2 && Amount!=3) || TreasuryCoins<Amount
        || WagePayables.ContainsByPredicate([&](const FHearthWagePayable& P) { return P.TaskId==TaskId; })) return false;
    TreasuryCoins-=Amount;
    FHearthWagePayable P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); P.TaskId=TaskId; P.Worker=Worker; P.Amount=Amount;
    WagePayables.Add(MoveTemp(P)); return true;
}

bool AHearthVillage::SettleWage(int32 Worker,const FString& TaskId)
{
    auto* P=WagePayables.FindByPredicate([&](const FHearthWagePayable& X) { return X.TaskId==TaskId; });
    if(!P || P->Worker!=Worker || !Residents.IsValidIndex(Worker) || Transactions.Num()>=100000) return false;
    if(P->Status==TEXT("unfunded"))
    {
        if(TreasuryCoins<P->Amount) { P->Status=TEXT("owed"); return false; }
        TreasuryCoins-=P->Amount; P->Status=TEXT("reserved");
    }
    if(P->Status==TEXT("owed"))
    {
        if(TreasuryCoins<P->Amount) return false;
        TreasuryCoins-=P->Amount; P->Status=TEXT("reserved");
    }
    if(P->Status!=TEXT("reserved")) return false;
    Residents[Worker].Coins+=P->Amount; P->Status=TEXT("paid");
    FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=TEXT("wage"); T.TaskId=TaskId;
    T.From=-1; T.To=Worker; T.Amount=P->Amount; T.Item=TEXT("labor"); T.Quantity=1; T.At=Elapsed; Transactions.Add(MoveTemp(T)); return true;
}

void AHearthVillage::AdvanceEconomy(float Dt)
{
    for(auto& P:WagePayables) if(P.Status==TEXT("owed") && TreasuryCoins>=P.Amount) SettleWage(P.Worker,P.TaskId);
    for(auto& P:WagePayables) if(P.Status==TEXT("unfunded") && TreasuryCoins>=P.Amount)
    {
        const bool StillWorking=Residents.IsValidIndex(P.Worker) && Residents[P.Worker].ActiveTaskId==P.TaskId
            && Residents[P.Worker].Task>=EHearthTask::ProductionTravel && Residents[P.Worker].Task<=EHearthTask::ProductionDeposit;
        if(StillWorking) { TreasuryCoins-=P.Amount; P.Status=TEXT("reserved"); }
    }
    for(auto& Offer:TradeOffers)
    {
        if(Offer.Status==TEXT("accepted") && Residents.IsValidIndex(Offer.Seller) && Residents[Offer.Seller].Task==EHearthTask::TradeTravel)
        {
            Offer.Remaining=FMath::Max(0.f,Offer.Remaining-Dt);
            if(MoveResident(Offer.Seller,Dt))
            {
                Offer.Status=TEXT("delivering"); Offer.Remaining=1.f;
                Residents[Offer.Seller].Task=EHearthTask::TradeWaiting; Residents[Offer.Seller].LatestEvent=TEXT("已与买方见面，正在交付木板。");
                Residents[Offer.Buyer].LatestEvent=TEXT("卖方已到达，验收木板后付款。");
            }
            else if(Offer.Remaining<=0)
            {
                Residents[Offer.Seller].PersonalPlanks+=Offer.ReservedQuantity; Offer.ReservedQuantity=0; Offer.Status=TEXT("cancelled");
                Offer.Result=TEXT("送货超时，交易取消，木板退回卖方。");
                for(int32 I:{Offer.Seller,Offer.Buyer}) if(Residents[I].Task==EHearthTask::TradeTravel || Residents[I].Task==EHearthTask::TradeWaiting) Residents[I].Task=EHearthTask::LifeChoosing;
            }
            continue;
        }
        if(Offer.Status!=TEXT("delivering")) continue;
        Offer.Remaining=FMath::Max(0.f,Offer.Remaining-Dt); if(Offer.Remaining>0) continue;
        if(TransferCoins(TEXT("plank_trade"),Offer.Id,Offer.Buyer,Offer.Seller,Offer.Price,TEXT("plank"),Offer.Quantity))
        {
            Residents[Offer.Buyer].PersonalPlanks+=Offer.ReservedQuantity; Offer.ReservedQuantity=0; Offer.Status=TEXT("completed");
            Offer.Result=Residents[Offer.Seller].Name+TEXT("向")+Residents[Offer.Buyer].Name+TEXT("交付1块自有木板，收到2枚钱。");
        }
        else
        {
            Residents[Offer.Seller].PersonalPlanks+=Offer.ReservedQuantity; Offer.ReservedQuantity=0; Offer.Status=TEXT("cancelled");
            Offer.Result=TEXT("结算时余额不足，交易取消，木板退回卖方。");
        }
        for(int32 I:{Offer.Seller,Offer.Buyer}) if(Residents[I].Task==EHearthTask::TradeTravel || Residents[I].Task==EHearthTask::TradeWaiting)
        { Residents[I].Task=EHearthTask::LifeChoosing; Residents[I].NextLifeDecision=Elapsed+LifeDecisionInterval; Residents[I].LatestEvent=Offer.Result; }
        auto& SellerBond=Residents[Offer.Seller].Bonds.FindOrAdd(Residents[Offer.Buyer].StableId);
        auto& BuyerBond=Residents[Offer.Buyer].Bonds.FindOrAdd(Residents[Offer.Seller].StableId);
        SellerBond.Memory=Offer.Result; BuyerBond.Memory=Offer.Result; VillageEvent=Offer.Result;
    }
}

void AHearthVillage::UpdateSiteVisual(int32 Index)
{
    auto& S=ProductionSites[Index];
    if(S.Kind==EHearthSiteKind::Empty && !S.bExpansion && !S.bReachable) return;
    // New cottages retain every installed module. BuildPlanId distinguishes them
    // from legacy saves whose completed house is still represented by one mesh.
    if(!S.BuildPlanId.IsEmpty())
    {
        const int32 Key=600+S.Stage;
        if(S.VisualStage==Key) return; S.VisualStage=Key;
        for(const auto& Weak:S.Meshes) if(auto* M=Weak.Get()) { ProductionMeshes.Remove(M); M->DestroyComponent(); }
        S.Meshes.Reset();
        auto AddPart=[this,&S](const TCHAR* Id,float X,float Y,float Z,float Yaw)
        {
            const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s"),Id,Id);
            if(auto* M=AddMesh(Path,S.Position+FVector(X,Y,Z),FVector::OneVector))
            { M->SetRelativeRotation(FRotator(0,Yaw,0)); S.Meshes.Add(M); ProductionMeshes.Add(M); }
        };
        if(S.Stage>=1) for(float X:{-100.f,100.f}) for(float Y:{-100.f,100.f})
        { AddPart(TEXT("foundation_stone_2m"),X,Y,0,0); AddPart(TEXT("floor_timber_2m"),X,Y,0,0); }
        if(S.Stage>=2)
        {
            for(float X:{-200.f,0.f,200.f}) for(float Y:{-200.f,0.f,200.f}) AddPart(TEXT("post_timber_2_4m"),X,Y,0,0);
            for(float X:{-100.f,100.f}) for(float Y:{-200.f,0.f,200.f}) AddPart(TEXT("beam_timber_2m"),X,Y,220,0);
            for(float X:{-200.f,0.f,200.f}) for(float Y:{-100.f,100.f}) AddPart(TEXT("beam_timber_2m"),X,Y,220,90);
        }
        if(S.Stage>=3)
        {
            AddPart(TEXT("wall_window_plaster_2m"),-200,-100,0,-90); AddPart(TEXT("wall_plaster_2m"),-200,100,0,-90);
            AddPart(TEXT("wall_door_plaster_2m"),-100,-200,0,0); AddPart(TEXT("wall_window_plaster_2m"),100,-200,0,0);
            AddPart(TEXT("wall_window_plaster_2m"),200,-100,0,90); AddPart(TEXT("wall_plaster_2m"),200,100,0,90);
            AddPart(TEXT("wall_window_plaster_2m"),-100,200,0,180); AddPart(TEXT("wall_window_plaster_2m"),100,200,0,180);
            AddPart(TEXT("gable_plaster_4m"),0,-200,240,0); AddPart(TEXT("gable_plaster_4m"),0,200,240,180);
        }
        if(S.Stage>=4) for(float Y:{-100.f,100.f})
        {
            AddPart(TEXT("roof_slope_terracotta_2m"),0,Y,240,0); AddPart(TEXT("roof_slope_terracotta_2m"),0,Y,240,180);
            AddPart(TEXT("roof_ridge_terracotta_2m"),0,Y,240,0);
        }
        if(S.Soil.IsValid()) S.Soil->SetVisibility(S.Stage==0);
        return;
    }
    int32 Visual=S.Stage;
    if(S.Growth>0) Visual=S.Growth<S.GrowDuration*.5f?11:10;
    if(S.ReservedBy>=0 && Residents.IsValidIndex(S.ReservedBy) && Residents[S.ReservedBy].ProductionOp==5)
        Visual=20+FMath::Min(3,FMath::FloorToInt(S.Progress*4));
    const int32 Key=static_cast<int32>(S.Kind)*100+Visual;
    if(S.VisualStage==Key) return; S.VisualStage=Key;
    if(!S.Soil.IsValid())
    {
        const FLinearColor Soil(0.30f,0.22f,0.12f);
        if(auto* M=AddMesh(TEXT("/Engine/BasicShapes/Cube"),S.Position+FVector(0,0,-3),FVector(S.Radius*2/100,S.Radius*2/100,.018f),&Soil))
        { S.Soil=M; ProductionMeshes.Add(M); }
        if(S.bExpansion)
        {
            const FLinearColor Edge(.62f,.65f,.35f);
            for(int32 Side=-1;Side<=1;Side+=2)
            {
                if(auto* M=AddMesh(TEXT("/Engine/BasicShapes/Cube"),S.Position+FVector(0,Side*S.Radius,0),FVector(S.Radius*2/100,.07f,.025f),&Edge)) ProductionMeshes.Add(M);
                if(auto* M=AddMesh(TEXT("/Engine/BasicShapes/Cube"),S.Position+FVector(Side*S.Radius,0,0),FVector(.07f,S.Radius*2/100,.025f),&Edge)) ProductionMeshes.Add(M);
            }
        }
    }
    if(S.Soil.IsValid()) S.Soil->SetVisibility(S.Kind!=EHearthSiteKind::Empty);
    FString Path; float Scale=1.f;
    if(HearthProduction::IsCrop(S.Kind))
    {
        const TCHAR* Types[]={TEXT("Corn"),TEXT("Wheat"),TEXT("Lettuce"),TEXT("Pumpkin")}; const int32 Type=static_cast<int32>(S.Kind)-2;
        const int32 Stage=S.Stage==2?3:S.Growth>0?(Visual==11?2:1):4;
        Path=HearthProduction::Crops+FString::Printf(TEXT("SM_Crop_%s_%02d"),Types[Type],Stage);
        Scale=Type==0?.7f:Type==3?.85f:.95f;
    }
    else if(S.Kind==EHearthSiteKind::House || Visual>=20)
    { Path=HearthProduction::Buildings+FString::Printf(TEXT("SM_House_%02d"),Visual>=20?Visual-19:4); Scale=.95f; }
    else if(S.Kind==EHearthSiteKind::Tree) { Path=HearthProduction::Crops+TEXT("SM_Tree_01"); Scale=S.Growth>0?(Visual==11?.65f:.35f):.9f; }
    else if(S.Kind==EHearthSiteKind::Shrub) { Path=HearthProduction::Crops+TEXT("SM_Shrub_01"); Scale=S.Growth>0?.45f:1.f; }
    else if(S.Kind==EHearthSiteKind::Stone) { Path=HearthProduction::Crops+TEXT("SM_Stone_02"); Scale=S.Units>0?1.f:.3f; }
    else if(S.Kind==EHearthSiteKind::Carpenter) { Path=TEXT("/Game/ThreeHearths/Generated/VillageKit/workbench_carpenter/workbench_carpenter"); Scale=1.f; }
    if(!Path.IsEmpty())
    {
        if(S.Meshes.IsEmpty()) if(auto* M=AddMesh(Path,S.Position,FVector(Scale))) { S.Meshes.Add(M); ProductionMeshes.Add(M); }
        for(const auto& Weak:S.Meshes) if(auto* M=Weak.Get())
        { if(auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path)) M->SetStaticMesh(Asset); M->SetWorldScale3D(FVector(Scale)); M->SetVisibility(true); }
    }
}

void AHearthVillage::RefreshProductionVisuals()
{
    for(int32 I=0;I<ProductionSites.Num();++I) UpdateSiteVisual(I);
    for(int32 I=0;I<3;++I) if(StockMeshes.IsValidIndex(I))
    { StockMeshes[I]->SetVisibility(WoodStock[I]>0); StockMeshes[I]->SetRelativeScale3D(FVector(1.1f,1.2f,FMath::Clamp(WoodStock[I]/12.f*.4f,.04f,1.2f))); }
}

FString AHearthVillage::GetProductionState() const
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("food"),FoodStock); Root->SetNumberField(TEXT("wood"),AvailableWood()); Root->SetNumberField(TEXT("raw_logs"),AvailableWood());
    Root->SetNumberField(TEXT("planks"),PlankStock); Root->SetNumberField(TEXT("beams"),BeamStock); Root->SetNumberField(TEXT("stone"),StoneStock);
    Root->SetStringField(TEXT("status"),ProductionStatus); Root->SetNumberField(TEXT("land_grid_cells"),LandGrid.Num());
    TArray<TSharedPtr<FJsonValue>> Sites;
    for(int32 I=0;I<ProductionSites.Num();++I)
    {
        const auto& S=ProductionSites[I]; auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("id"),I);
        J->SetStringField(TEXT("stable_id"),S.StableId);
        J->SetStringField(TEXT("build_plan_id"),S.BuildPlanId);
        J->SetStringField(TEXT("kind"),HearthProduction::KindKeys[static_cast<int32>(S.Kind)]); J->SetStringField(TEXT("name"),HearthProduction::KindNames[static_cast<int32>(S.Kind)]);
        J->SetNumberField(TEXT("stage"),S.Stage); J->SetNumberField(TEXT("units"),S.Units); J->SetNumberField(TEXT("growth_seconds"),S.Growth);
        J->SetNumberField(TEXT("reserved_by"),S.ReservedBy); J->SetNumberField(TEXT("owner"),S.Owner); J->SetBoolField(TEXT("reachable"),S.bReachable);
        J->SetNumberField(TEXT("x"),S.Position.X); J->SetNumberField(TEXT("y"),S.Position.Y); J->SetNumberField(TEXT("radius"),S.Radius);
        J->SetStringField(TEXT("approach"),S.Approach.ToString());
        TArray<TSharedPtr<FJsonValue>> Components;
        if(!S.BuildPlanId.IsEmpty()) for(int32 Stage=0;Stage<4;++Stage)
        {
            int32 Type,Amount; const TCHAR* Name; HearthProduction::CottagePart(Stage,Type,Amount,Name);
            const TCHAR* Keys[]={TEXT("food"),TEXT("raw_logs"),TEXT("stone"),TEXT("planks"),TEXT("beams")};
            const int32 Instances[]={8,21,10,6}; auto C=MakeShared<FJsonObject>();
            C->SetStringField(TEXT("id"),FString::Printf(TEXT("%s:%d"),*S.BuildPlanId,Stage+1));
            C->SetStringField(TEXT("name"),Name); C->SetNumberField(TEXT("stage"),Stage+1); C->SetNumberField(TEXT("instance_count"),Instances[Stage]);
            C->SetStringField(TEXT("material"),Keys[Type]); C->SetNumberField(TEXT("material_amount"),Amount); C->SetNumberField(TEXT("owner"),S.Owner);
            FString Status=Stage<S.Stage?TEXT("completed"):TEXT("waiting_material"); int32 ReservedBy=-1;
            if(Stage==S.Stage && S.ReservedBy>=0 && Residents.IsValidIndex(S.ReservedBy))
            {
                ReservedBy=S.ReservedBy; const auto Task=Residents[S.ReservedBy].Task;
                Status=Task==EHearthTask::ProductionTravel?TEXT("transporting"):Task==EHearthTask::ProductionWork?TEXT("installing"):TEXT("reserved");
            }
            C->SetNumberField(TEXT("reserved_by"),ReservedBy); C->SetStringField(TEXT("status"),Status);
            Components.Add(MakeShared<FJsonValueObject>(C));
        }
        J->SetArrayField(TEXT("components"),Components); Sites.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("sites"),Sites);
    auto Totals=MakeShared<FJsonObject>(); for(const auto& Pair:ProductionTotals) Totals->SetNumberField(Pair.Key,Pair.Value); Root->SetObjectField(TEXT("completed_operations"),Totals);
    for(int32 Type=0;Type<3;++Type)
    {
        int32 Carry=0; for(const auto& R:Residents) if(R.CargoType==Type) Carry+=R.CargoAmount;
        const int32 Stock=Type==0?FoodStock:Type==1?AvailableWood():StoneStock;
        int32 Accounted=Stock+Carry+Spent[Type]-Produced[Type];
        if(Type==1) for(const auto& R:Residents) Accounted+=R.CarriedWood+R.DeliveredWood;
        const TCHAR* Keys[]={TEXT("food"),TEXT("wood"),TEXT("stone")};
        Root->SetNumberField(FString(TEXT("accounted_"))+Keys[Type],Accounted);
        Root->SetNumberField(FString(TEXT("produced_"))+Keys[Type],Produced[Type]);
        Root->SetNumberField(FString(TEXT("spent_"))+Keys[Type],Spent[Type]);
        Root->SetNumberField(FString(TEXT("in_transit_"))+Keys[Type],Carry);
    }
    const TCHAR* ManufacturedKeys[]={TEXT("planks"),TEXT("beams")};
    for(int32 Type=0;Type<2;++Type)
    {
        int32 Carry=0; for(const auto& R:Residents) if(R.CargoType==Type+3) Carry+=R.CargoAmount;
        const int32 Stock=Type==0?PlankStock:BeamStock;
        int32 Personal=0,Reserved=0;
        if(Type==0)
        {
            for(const auto& R:Residents) Personal+=R.PersonalPlanks;
            for(const auto& T:TradeOffers) Reserved+=T.ReservedQuantity;
        }
        Root->SetNumberField(FString(TEXT("accounted_"))+ManufacturedKeys[Type],Stock+Carry+Personal+Reserved+ManufacturedSpent[Type]-Manufactured[Type]);
        Root->SetNumberField(FString(TEXT("produced_"))+ManufacturedKeys[Type],Manufactured[Type]);
        Root->SetNumberField(FString(TEXT("spent_"))+ManufacturedKeys[Type],ManufacturedSpent[Type]);
        Root->SetNumberField(FString(TEXT("in_transit_"))+ManufacturedKeys[Type],Carry);
        if(Type==0) { Root->SetNumberField(TEXT("resident_owned_planks"),Personal); Root->SetNumberField(TEXT("trade_reserved_planks"),Reserved); }
    }
    return HearthProduction::Json(Root);
}

void AHearthVillage::AppendProductionContext(const TSharedRef<FJsonObject>& Context) const
{
    auto Stock=MakeShared<FJsonObject>(); Stock->SetNumberField(TEXT("food"),FoodStock); Stock->SetNumberField(TEXT("wood"),AvailableWood()); Stock->SetNumberField(TEXT("raw_logs"),AvailableWood());
    Stock->SetNumberField(TEXT("planks"),PlankStock); Stock->SetNumberField(TEXT("beams"),BeamStock); Stock->SetNumberField(TEXT("stone"),StoneStock);
    Context->SetObjectField(TEXT("inventory"),Stock);
    auto Counts=MakeShared<FJsonObject>();
    for(int32 Kind=0;Kind<=10;++Kind)
    { int32 Count=0; for(const auto& S:ProductionSites) if(S.bReachable && static_cast<int32>(S.Kind)==Kind) ++Count; Counts->SetNumberField(HearthProduction::KindKeys[Kind],Count); }
    Context->SetObjectField(TEXT("production_sites"),Counts);
    auto Totals=MakeShared<FJsonObject>(); for(const auto& Pair:ProductionTotals) Totals->SetNumberField(Pair.Key,Pair.Value);
    Context->SetObjectField(TEXT("completed_production_operations"),Totals);
}

FString AHearthVillage::GetAvailableActivities(int32 Index) const
{
    auto Root=MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("can_assign"),CanAssignActivity(Index));
    TArray<TSharedPtr<FJsonValue>> Values;
    for(int32 Action:AvailableLifeActions(Index))
    { auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("id"),Action); J->SetStringField(TEXT("description"),LifeActionName(Index,Action)); Values.Add(MakeShared<FJsonValueObject>(J)); }
    Root->SetArrayField(TEXT("actions"),Values); return HearthProduction::Json(Root);
}

bool AHearthVillage::CanAssignActivity(int32 Index) const
{ return Residents.IsValidIndex(Index) && Residents[Index].Task==EHearthTask::LifeChoosing && Residents[Index].Route.IsEmpty() && !IsDecisionPending(Index); }
bool AHearthVillage::AssignActivity(int32 Index,int32 Action)
{
    if(!CanAssignActivity(Index) || !StartLifeAction(Index,Action,TEXT("按照你的安排，执行这项工作。"),false)) return false;
    auto& R=Residents[Index]; R.DecisionSource=TEXT("player"); R.DecisionNote=TEXT("你安排的任务");
    auto& H=DecisionHistory[R.HistoryIndex]; H.Source=TEXT("player"); ++HistoryRevision; SaveHistory(); WriteSnapshot(); return true;
}

FString AHearthVillage::ProductionSummary() const
{
    int32 Land=0,Fields=0,Houses=0,Empty=0;
    int32 CompletedTrades=0,ReservedWages=0;
    for(const auto& S:ProductionSites)
    { Land+=S.Kind==EHearthSiteKind::Land; Empty+=S.Kind==EHearthSiteKind::Empty && S.bReachable; Fields+=HearthProduction::IsCrop(S.Kind); Houses+=S.Kind==EHearthSiteKind::House; }
    for(const auto& T:TradeOffers) CompletedTrades+=T.Status==TEXT("completed");
    for(const auto& P:WagePayables) if(P.Status==TEXT("reserved")) ReservedWages+=P.Amount;
    return FString::Printf(TEXT("食物 %d · 原木 %d · 木板 %d · 房梁 %d · 石材 %d\n村库 %d 枚 · 预留工资 %d · 居民买卖 %d 笔\n农田 %d · 新住宅 %d · 空地 %d / 已开垦 %d"),FoodStock,AvailableWood(),PlankStock,BeamStock,StoneStock,TreasuryCoins,ReservedWages,CompletedTrades,Fields,Houses,Empty,Land);
}

FString AHearthVillage::CargoSummary(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return FString(); const auto& R=Residents[Index];
    if(R.CargoAmount>0) return FString::Printf(TEXT("携带 %d 份%s · 运往村镇中心"),R.CargoAmount,HearthProduction::ResourceNames[FMath::Clamp(R.CargoType,0,4)]);
    if(R.ProductionSite>=0) return FString::Printf(TEXT("正在处理 %d 号地块 · 材料已预扣"),R.ProductionSite+1);
    return FString::Printf(TEXT("%s · 建房木材 %d / %d"),*PlotNameFor(Index),R.DeliveredWood,CostFor(Index));
}
