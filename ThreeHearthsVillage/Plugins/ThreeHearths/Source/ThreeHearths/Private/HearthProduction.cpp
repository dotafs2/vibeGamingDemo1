#include "HearthVillage.h"
#include "HearthPlannedConstructionAdapter.h"
#include "HearthResidentBuildingPlanner.h"
#include "HearthTownLayout.h"
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
    FString Json(const TSharedRef<FJsonObject>& Object) { FString S; FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&S)); return S; }
    const FHearthCottageComponent* NextComponent(const FHearthSite& Site)
    { return Site.CottageComponents.FindByPredicate([](const auto& C){ return C.Status!=TEXT("completed"); }); }

    FHearthResidentBuildingInput PlanningInput(const FHearthResident& Resident, const FHearthSite& Site,
        int32 Stone, int32 Planks, int32 Beams, int32 ConstructionGrantBudget)
    {
        FHearthResidentBuildingInput Input;
        Input.ResidentId=Resident.StableId;
        Input.StableSeed=Resident.StableId+TEXT("|")+Resident.Personality+TEXT("|")+Resident.Role;
        Input.Need=Resident.Hunger>=65.f?TEXT("urgent shelter near food and neighbors"):
            Resident.SocialNeed>=60.f?TEXT("social family shelter"):TEXT("shelter");
        Input.Occupation=Resident.Role;
        Input.FriendsNearby=0;
        for(const auto& Bond:Resident.Bonds) if(Bond.Value.Meetings>0 && Bond.Value.Affinity>=0.f) ++Input.FriendsNearby;
        Input.HouseholdSize=FMath::Clamp(1+Input.FriendsNearby/2,1,4);
        // Cottage materials and wages are supplied by the existing village-construction grant,
        // so its currently spendable treasury balance is the plan's real feasibility ceiling.
        Input.Budget=FMath::Max(0,ConstructionGrantBudget);
        Input.bRoadAccessible=Site.bReachable;
        const FVector Road=Site.Approach-Site.Position;
        Input.RoadYaw=Road.IsNearlyZero()?0.f:FMath::RadiansToDegrees(FMath::Atan2(Road.Y,Road.X));
        Input.Origin=Site.Position;
        Input.Stone=FMath::Max(0,Stone);
        Input.Planks=FMath::Max(0,Planks);
        Input.Beams=FMath::Max(0,Beams);
        return Input;
    }

    bool PrepareExpansion(const FHearthResident& Resident,const FHearthSite& Site,const FHearthStructurePlan& Current,
        int32 Stone,int32 Planks,int32 Beams,int32 Budget,FHearthStructurePlan& OutPlan,FHearthPlannedConstructionResult& OutComponents)
    {
        FHearthResidentBuildingPlan Existing; Existing.Plan=Current; Existing.Expansion.ResultingPlan=Current; Existing.bBuildable=true;
        FHearthResidentBuildingInput Input=PlanningInput(Resident,Site,Stone,Planks,Beams,Budget);
        Input.ExtensionKey=FString::Printf(TEXT("%s:extension:%d"),*Current.PlanId,Current.Rooms.Num());
        if(!HearthResidentBuildingPlanner::AppendExpansion(Existing,Input)) return false;
        OutPlan=Existing.Expansion.ResultingPlan;
        OutComponents=HearthPlannedConstructionAdapter::Convert(OutPlan,Site.Owner,Site.CottageComponents);
        return OutComponents.bAccepted && OutComponents.Components.ContainsByPredicate([](const auto& Part){return Part.Status!=TEXT("completed");});
    }
}

void HearthCottage::Populate(FHearthSite& S)
{
    if(S.BuildPlanId.IsEmpty() || !S.CottageComponents.IsEmpty()) return;
    int32 Serial=0;
    auto Add=[&](const TCHAR* Asset,int32 Stage,int32 Material,float X,float Y,float Z,float Yaw)
    {
        FHearthCottageComponent C; C.AssetId=Asset; C.Stage=Stage; C.MaterialType=Material; C.Offset=FVector(X,Y,Z); C.Yaw=Yaw; C.Owner=S.Owner;
        C.Id=FString::Printf(TEXT("%s:%02d:%s"),*S.BuildPlanId,++Serial,Asset); C.Status=Stage<=S.Stage?TEXT("completed"):TEXT("waiting_material");
        S.CottageComponents.Add(MoveTemp(C));
    };
    for(float X:{-100.f,100.f}) for(float Y:{-100.f,100.f}) Add(TEXT("foundation_stone_2m"),1,2,X,Y,0,0);
    for(float X:{-100.f,100.f}) for(float Y:{-100.f,100.f}) Add(TEXT("floor_timber_2m"),1,3,X,Y,0,0);
    for(float X:{-200.f,0.f,200.f}) for(float Y:{-200.f,0.f,200.f}) Add(TEXT("post_timber_2_4m"),2,4,X,Y,0,0);
    for(float X:{-100.f,100.f}) for(float Y:{-200.f,0.f,200.f}) Add(TEXT("beam_timber_2m"),2,4,X,Y,220,0);
    for(float X:{-200.f,0.f,200.f}) for(float Y:{-100.f,100.f}) Add(TEXT("beam_timber_2m"),2,4,X,Y,220,90);
    Add(TEXT("wall_window_timber_2m"),3,3,-200,-100,0,-90); Add(TEXT("wall_timber_2m"),3,3,-200,100,0,-90);
    Add(TEXT("wall_door_timber_2m"),3,3,-100,-200,0,0); Add(TEXT("wall_window_timber_2m"),3,3,100,-200,0,0);
    Add(TEXT("wall_window_timber_2m"),3,3,200,-100,0,90); Add(TEXT("wall_timber_2m"),3,3,200,100,0,90);
    Add(TEXT("wall_window_timber_2m"),3,3,-100,200,0,180); Add(TEXT("wall_window_timber_2m"),3,3,100,200,0,180);
    Add(TEXT("gable_timber_4m"),3,3,0,-200,240,0); Add(TEXT("gable_timber_4m"),3,3,0,200,240,180);
    for(float Y:{-100.f,100.f})
    {
        Add(TEXT("roof_slope_timber_2m"),4,3,0,Y,240,0); Add(TEXT("roof_slope_timber_2m"),4,3,0,Y,240,180);
        Add(TEXT("roof_ridge_timber_2m"),4,3,0,Y,240,0);
    }
    S.Capacity=45; S.Units=0; for(const auto& C:S.CottageComponents) S.Units+=C.Status==TEXT("completed");
}

void AHearthVillage::EnsureCottageComponents(FHearthSite& S) const
{
    if(const FHearthStructurePlan* Plan=StructurePlans.FindByPredicate([&](const FHearthStructurePlan& P){ return P.PlanId==S.BuildPlanId; }))
    {
        const auto Converted=HearthPlannedConstructionAdapter::Convert(*Plan,S.Owner,S.CottageComponents);
        if(Converted.bAccepted)
        {
            S.CottageComponents=Converted.Components;
            S.Capacity=S.CottageComponents.Num();
            S.Units=0;
            for(const auto& Component:S.CottageComponents) S.Units+=Component.Status==TEXT("completed");
        }
        return;
    }
    HearthCottage::Populate(S);
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
    FHearthTownLayoutInput TownInput;
    TownInput.IslandMin=FVector2D(-6000,-5700); TownInput.IslandMax=FVector2D(5700,5700);
    TownInput.Roads.Add({FVector(-2130,-5100,8),FVector(-2130,5100,8),340.f});
    TownInput.Markets={FVector(-1100,-1050,8),FVector(-1650,-1050,8)};
    TownInput.Workpoints={FVector(-2250,-1050,8)};
    for(const FVector& Obstacle:FixedObstacles)
    {
        FHearthTownRect Blocker; Blocker.Center=FVector(Obstacle.X,Obstacle.Y,8); Blocker.HalfExtent=FVector2D(Obstacle.Z); Blocker.Clearance=80.f;
        TownInput.TerrainBlockers.Add(Blocker);
    }
    for(const FHearthSite& ExistingSite:ProductionSites)
    {
        FHearthTownRect Blocker; Blocker.Center=ExistingSite.Position; Blocker.HalfExtent=FVector2D(ExistingSite.Radius); Blocker.Clearance=80.f;
        TownInput.TerrainBlockers.Add(Blocker);
        if(ExistingSite.Kind==EHearthSiteKind::Carpenter) TownInput.Workpoints.Add(ExistingSite.Position);
    }
    TownInput.RequestedHomes=18; TownInput.Seed=583;
    const FHearthTownLayoutPlan TownPlan=HearthTownLayout::Build(TownInput);
    int32 Plots=0;
    for(const FHearthTownFootprint& Home:TownPlan.Homes)
    {
        if(Home.bExisting || Plots>=TownInput.RequestedHomes) continue;
        constexpr float PlotRadius=260.f;
        const FVector P=Home.Center;
        if(!IsLand(P) || !IsLand(P+FVector(PlotRadius,PlotRadius,0)) || !IsLand(P-FVector(PlotRadius,PlotRadius,0))
            || !IsLand(P+FVector(-PlotRadius,PlotRadius,0)) || !IsLand(P+FVector(PlotRadius,-PlotRadius,0))) continue;
        FHearthSite Site; Site.Kind=EHearthSiteKind::Empty; Site.Position=P; Site.Approach=Home.Door;
        Site.Radius=PlotRadius; Site.bExpansion=true; Site.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        ProductionSites.Add(MoveTemp(Site)); ++Plots;
    }
    int32 Reachable=0;
    for(int32 I=0;I<ProductionSites.Num();++I) { if(ChooseSiteApproach(I)) ++Reachable; UpdateSiteVisual(I); }
    ProductionStatus=FString::Printf(TEXT("连续街区住宅位 %d · 可达生产点 / 地块 %d / %d · 全员拥有全部技能"),Plots,Reachable,ProductionSites.Num());
}

bool AHearthVillage::IsProductionAllowed(int32 Index,int32 Action) const
{
    int32 Site,Op;
    if(!Residents.IsValidIndex(Index) || Residents[Index].BuildProgress<1 || !HearthProduction::Decode(Action,Site,Op) || !ProductionSites.IsValidIndex(Site)) return false;
    const auto& S=ProductionSites[Site]; if(!S.bReachable || S.ReservedBy>=0 || (!PublicProject.Id.IsEmpty() && PublicProject.Site==Site)) return false;
    int32 Food,Wood,Stone; HearthProduction::Cost(Op,Food,Wood,Stone);
    if(FoodStock<Food || AvailableWood()<Wood || StoneStock<Stone) return false;
    if(!ToolAvailableFor(Index,Op)) return false;
    if(Op==0) return S.Kind==EHearthSiteKind::Empty;
    if(Op==5)
    {
        if(S.Kind!=EHearthSiteKind::Land && S.Kind!=EHearthSiteKind::House) return false;
        TArray<FHearthCottageComponent> CandidateComponents=S.CottageComponents;
        if(S.BuildPlanId.IsEmpty())
        {
            if(S.Kind!=EHearthSiteKind::Land) return false;
            const auto Proposed=HearthResidentBuildingPlanner::Build(HearthProduction::PlanningInput(Residents[Index],S,StoneStock,PlankStock,BeamStock,GeneralFunds()));
            if(!Proposed.bBuildable || StructurePlans.ContainsByPredicate([&](const FHearthStructurePlan& P){ return P.PlanId==Proposed.Plan.PlanId; })) return false;
            const auto Converted=HearthPlannedConstructionAdapter::Convert(Proposed.Plan,Index,{});
            if(!Converted.bAccepted) return false;
            CandidateComponents=Converted.Components;
        }
        else if(const FHearthStructurePlan* Plan=StructurePlans.FindByPredicate([&](const FHearthStructurePlan& P){ return P.PlanId==S.BuildPlanId; }))
        {
            const auto Converted=HearthPlannedConstructionAdapter::Convert(*Plan,S.Owner,CandidateComponents);
            if(!Converted.bAccepted) return false;
            CandidateComponents=Converted.Components;
            if(!CandidateComponents.ContainsByPredicate([](const auto& C){return C.Status!=TEXT("completed");}))
            {
                if(S.Owner!=Index) return false; // Neighbors may build parts, but only the owner chooses an extension.
                FHearthStructurePlan Expanded; FHearthPlannedConstructionResult ExpansionComponents;
                if(!HearthProduction::PrepareExpansion(Residents[Index],S,*Plan,StoneStock,PlankStock,BeamStock,GeneralFunds(),Expanded,ExpansionComponents)) return false;
                CandidateComponents=MoveTemp(ExpansionComponents.Components);
            }
        }
        const auto* Part=CandidateComponents.FindByPredicate([](const auto& C){ return C.Status!=TEXT("completed"); });
        if(!Part) return false;
        const int32 Type=Part->MaterialType,Amount=Part->MaterialAmount;
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
        const auto* Part=HearthProduction::NextComponent(S); const int32 Type=Part?Part->MaterialType:2,Amount=Part?Part->MaterialAmount:1;
        const FString PartName=Part?Part->AssetId:TEXT("foundation_stone_2m");
        Name+=FString::Printf(TEXT(" · 下一构件 %s（%s %d）"),*PartName,HearthProduction::ResourceNames[Type],Amount);
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
        const bool PublicBuilding=!PublicProject.Id.IsEmpty() && PublicProject.Status==TEXT("building");
        int32 PublicNeed[3]={0,0,0};
        if(PublicBuilding) for(const auto& Part:PublicProject.Parts) if(Part.Status!=TEXT("completed"))
            for(int32 M=0;M<3;++M) PublicNeed[M]+=Part.Required[M]-Part.Reserved[M]-Part.Delivered[M];
        if(Op==11) Score=PublicBuilding && PublicProject.Stock[0]<PublicNeed[0]?235:(StoneStock<10?140:15);
        if(Op==13) Score=PublicBuilding && PublicProject.Stock[1]<PublicNeed[1]?245:(PlankStock<12?165:18);
        if(Op==14) Score=PublicBuilding && PublicProject.Stock[2]<PublicNeed[2]?240:(BeamStock<8?155:16);
        if(Op==8) Score=FoodStock<40?125:45;
        if(Op==0) { int32 Ready=0; for(const auto& S:ProductionSites) Ready+=S.Kind==EHearthSiteKind::Land; Score=Ready<2?90:2; }
        if(Op>=1 && Op<=7)
        {
            const FString Key=HearthProduction::OpKeys[Op]; Score=ProductionTotals.FindRef(Key)==0?100:10;
            if(Op==5)
            {
                const auto& CandidateSite=ProductionSites[Site];
                const bool HasPendingParts=CandidateSite.CottageComponents.ContainsByPredicate([](const auto& Part){return Part.Status!=TEXT("completed");});
                const auto& Person=Residents[Index];
                const bool OwnsStructure=StructurePlans.ContainsByPredicate([&](const FHearthStructurePlan& Plan){return Plan.PlanId.Contains(Person.StableId);});
                const bool WorkshopRole=Person.Role.Contains(TEXT("木匠"))||Person.Role.Contains(TEXT("铁匠"))||Person.Role.Contains(TEXT("陶工"))||Person.Role.Contains(TEXT("织工"));
                const float NeedPressure=FMath::Max(Person.Hunger,Person.SocialNeed);
                if(!CandidateSite.BuildPlanId.IsEmpty() && HasPendingParts) Score=260; // Complete the current material-backed assembly before opening another frame.
                else if(CandidateSite.BuildPlanId.IsEmpty()) Score=!OwnsStructure?(WorkshopRole?205:(NeedPressure>=45.f?195:85)):20;
                else if(CandidateSite.Owner==Index) Score=NeedPressure>=75.f?190:(WorkshopRole?175:50);
                else Score=20;
            }
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
    FHearthResidentBuildingPlan ProposedPlan;
    FHearthPlannedConstructionResult ProposedComponents;
    const bool bNeedsNewPlan=Op==5 && S.BuildPlanId.IsEmpty();
    bool bNeedsExpansion=false;
    int32 ExpansionPlanIndex=INDEX_NONE;
    if(bNeedsNewPlan)
    {
        ProposedPlan=HearthResidentBuildingPlanner::Build(HearthProduction::PlanningInput(R,S,StoneStock,PlankStock,BeamStock,GeneralFunds()));
        if(!ProposedPlan.bBuildable || StructurePlans.ContainsByPredicate([&](const FHearthStructurePlan& P){ return P.PlanId==ProposedPlan.Plan.PlanId; }))
        {
            R.LatestEvent=ProposedPlan.Reason.IsEmpty()?TEXT("当前需求、资源或道路条件无法生成可执行住宅计划。"):ProposedPlan.Reason;
            return false;
        }
        ProposedComponents=HearthPlannedConstructionAdapter::Convert(ProposedPlan.Plan,Index,{});
        if(!ProposedComponents.bAccepted) { R.LatestEvent=ProposedComponents.Reason; return false; }
    }
    else if(Op==5 && !S.CottageComponents.ContainsByPredicate([](const auto& C){return C.Status!=TEXT("completed");}))
    {
        ExpansionPlanIndex=StructurePlans.IndexOfByPredicate([&](const FHearthStructurePlan& P){return P.PlanId==S.BuildPlanId;});
        if(!StructurePlans.IsValidIndex(ExpansionPlanIndex) || !HearthProduction::PrepareExpansion(R,S,StructurePlans[ExpansionPlanIndex],StoneStock,PlankStock,BeamStock,GeneralFunds(),ProposedPlan.Plan,ProposedComponents))
        { R.LatestEvent=TEXT("当前真实库存或预算不足以扩建住宅。"); return false; }
        bNeedsExpansion=true;
    }
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
        if(bNeedsNewPlan)
        {
            StructurePlans.Add(ProposedPlan.Plan);
            S.BuildPlanId=ProposedPlan.Plan.PlanId;
            S.Owner=Index;
            S.CottageComponents=MoveTemp(ProposedComponents.Components);
            S.Capacity=S.CottageComponents.Num();
            S.Units=0;
        }
        else if(bNeedsExpansion)
        {
            StructurePlans[ExpansionPlanIndex]=ProposedPlan.Plan;
            S.CottageComponents=MoveTemp(ProposedComponents.Components);
            S.Capacity=S.CottageComponents.Num();
        }
        EnsureCottageComponents(S); auto* Part=S.CottageComponents.FindByPredicate([](const auto& C){ return C.Status!=TEXT("completed"); });
        if(!Part) { ReturnTool(Index); CancelWage(R.ActiveTaskId); R.ActiveTaskId.Empty(); return false; }
        const int32 Type=Part->MaterialType,Amount=Part->MaterialAmount; Part->Status=TEXT("reserved"); Part->ReservedBy=Index; Part->Owner=S.Owner;
        R.ProductionComponentId=Part->Id;
        if(Type==2) StoneStock-=Amount; else if(Type==3) PlankStock-=Amount; else BeamStock-=Amount;
        R.LatestEvent=FString::Printf(TEXT("村庄批准公共建材供给；前往库存点领取 %d 份%s，再安装 %s。"),Amount,HearthProduction::ResourceNames[Type],*Part->AssetId);
    }
    R.LifeAction=Action; R.Reason=Reason; R.DecisionSource=bFromApi?TEXT("api"):TEXT("local");
    if(Op==5)
    {
        const FVector Depot=(bUseCropoutMap?FVector(-1650,-1050,8):FVector(-250,-400,8))+FVector(0,((Index%3)-1)*120,0);
        if(!FindActivityRoute(Index,Depot,Route)) { CancelProduction(Index); return false; }
    }
    R.Route=MoveTemp(Route); R.Task=EHearthTask::ProductionTravel; if(Op!=5) R.LatestEvent=Label;
    R.MoveRetry=0; R.bMovementBlocked=false;
    R.WorkDuration=Op==5?3.f:Op==0?20.f:Op<=7?25.f:Op==8?20.f:Op==13?18.f:Op==14?22.f:12.f;
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
        if(Op==5)
        {
            auto* Part=S.CottageComponents.FindByPredicate([&](const auto& C){ return C.Id==R.ProductionComponentId; });
            if(!Part) { CancelProduction(Index); return; }
            if(MoveResident(Index,Dt))
            {
                if(Part->Status==TEXT("reserved"))
                {
                    R.CargoType=Part->MaterialType; R.CargoAmount=Part->MaterialAmount; Part->Status=TEXT("transporting");
                    TArray<FVector> ToSite;
                    if(!FindActivityRoute(Index,S.Approach,ToSite)) { CancelProduction(Index); return; }
                    R.Route=MoveTemp(ToSite); R.LatestEvent=FString::Printf(TEXT("已在公共库存点领取 %s，正搬往住宅工地。"),*Part->AssetId);
                }
                else if(Part->Status==TEXT("transporting")) { Part->Status=TEXT("installing"); R.Task=EHearthTask::ProductionWork; R.Timer=R.WorkDuration; }
                else CancelProduction(Index);
            }
            return;
        }
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
        auto* Part=S.CottageComponents.FindByPredicate([&](const auto& C){ return C.Id==R.ProductionComponentId; });
        if(!Part || Part->Status!=TEXT("installing") || R.CargoType!=Part->MaterialType || R.CargoAmount!=Part->MaterialAmount)
        { R.Timer=1.f; R.LatestEvent=TEXT("构件材料记录不完整，暂停安装等待恢复。"); return; }
        if(Part->MaterialType==2) Spent[2]+=Part->MaterialAmount; else ManufacturedSpent[Part->MaterialType-3]+=Part->MaterialAmount;
        Part->Status=TEXT("completed"); Part->ReservedBy=-1; R.CargoType=-1; R.CargoAmount=0; R.ProductionComponentId.Empty(); ++S.Units;
        S.Stage=0; for(int32 Group=1;Group<=4;++Group)
            if(!S.CottageComponents.ContainsByPredicate([Group](const auto& C){ return C.Stage==Group && C.Status!=TEXT("completed"); })) S.Stage=Group; else break;
        if(S.Owner<0) S.Owner=Index;
        if(S.Units>=S.CottageComponents.Num())
        {
            S.Kind=EHearthSiteKind::House; S.Stage=4; S.Capacity=S.CottageComponents.Num();
            Result=FString::Printf(TEXT("住宅计划 %s 的 %d 个构件已逐件搬运并安装完成。"),*S.BuildPlanId,S.CottageComponents.Num());
        }
        else Result=FString::Printf(TEXT("已安装 %s；住宅计划 %s 完成 %d / %d 个构件。"),*Part->AssetId,*S.BuildPlanId,S.Units,S.CottageComponents.Num());
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

bool AHearthVillage::CancelProduction(int32 Index)
{
    if(!Residents.IsValidIndex(Index)) return false; auto& R=Residents[Index];
    if(R.ProductionOp!=5 || !ProductionSites.IsValidIndex(R.ProductionSite)) return false; auto& S=ProductionSites[R.ProductionSite];
    auto* Part=S.CottageComponents.FindByPredicate([&](const auto& C){ return C.Id==R.ProductionComponentId; });
    if(!Part || Part->Status==TEXT("completed")) return false;
    const int32 Type=Part->MaterialType,Amount=Part->MaterialAmount;
    if(Type==2) StoneStock+=Amount; else if(Type==3) PlankStock+=Amount; else if(Type==4) BeamStock+=Amount;
    Part->Status=TEXT("waiting_material"); Part->ReservedBy=-1;
    CancelWage(R.ActiveTaskId);
    S.ReservedBy=-1; S.Progress=0; R.CargoType=-1; R.CargoAmount=0; R.ProductionComponentId.Empty(); R.Route.Reset(); ReturnTool(Index);
    R.Task=EHearthTask::LifeChoosing; R.ProductionSite=-1; R.ProductionOp=-1; R.WorkDuration=0; R.NextLifeDecision=Elapsed+LifeDecisionInterval;
    R.LatestEvent=TEXT("施工取消：未安装构件已退回公共库存，未赚取的工资也已解除预留。");
    if(DecisionHistory.IsValidIndex(R.HistoryIndex)) { auto& H=DecisionHistory[R.HistoryIndex]; H.Status=TEXT("failed"); H.Result=R.LatestEvent; ++HistoryRevision; SaveHistory(); }
    VillageEvent=R.Name+TEXT("：")+R.LatestEvent; return true;
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
    R.ProductionSite=-1; R.ProductionOp=-1; R.ProductionComponentId.Empty(); R.WorkDuration=0; VillageEvent=R.Name+TEXT("：")+Result;
}

void AHearthVillage::AdvanceEconomy(float Dt)
{
    for(auto& P:WagePayables) if(P.Status==TEXT("owed") && (P.bTaxFunded?TaxProjectCoins:GeneralFunds())>=P.Amount) SettleWage(P.Worker,P.TaskId);
    for(auto& P:WagePayables) if(P.Status==TEXT("unfunded") && (P.bTaxFunded?TaxProjectCoins:GeneralFunds())>=P.Amount)
    {
        const bool StillWorking=Residents.IsValidIndex(P.Worker) && Residents[P.Worker].ActiveTaskId==P.TaskId
            && Residents[P.Worker].Task>=EHearthTask::ProductionTravel && Residents[P.Worker].Task<=EHearthTask::ProductionDeposit;
        if(StillWorking) { TreasuryCoins-=P.Amount; if(P.bTaxFunded) TaxProjectCoins-=P.Amount; P.Status=TEXT("reserved"); }
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
        const int32 Key=600+S.Units;
        if(S.VisualStage==Key) return; S.VisualStage=Key;
        for(const auto& Weak:S.Meshes) if(auto* M=Weak.Get()) { ProductionMeshes.Remove(M); M->DestroyComponent(); }
        S.Meshes.Reset();
        auto AddPart=[this,&S](const FHearthCottageComponent& Part)
        {
            const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s"),*Part.AssetId,*Part.AssetId);
            if(auto* M=AddMesh(Path,S.Position+Part.Offset,FVector::OneVector))
            { M->SetRelativeRotation(FRotator(0,Part.Yaw,0)); S.Meshes.Add(M); ProductionMeshes.Add(M); }
        };
        for(const auto& Part:S.CottageComponents) if(Part.Status==TEXT("completed")) AddPart(Part);
        if(S.Soil.IsValid()) S.Soil->SetVisibility(S.Units==0);
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
        if(!S.BuildPlanId.IsEmpty()) for(const auto& Part:S.CottageComponents)
        {
            const TCHAR* Keys[]={TEXT("food"),TEXT("raw_logs"),TEXT("stone"),TEXT("planks"),TEXT("beams")};
            auto C=MakeShared<FJsonObject>(); C->SetStringField(TEXT("id"),Part.Id); C->SetStringField(TEXT("asset_id"),Part.AssetId);
            C->SetNumberField(TEXT("stage"),Part.Stage); C->SetNumberField(TEXT("instance_count"),1);
            C->SetStringField(TEXT("material"),Keys[Part.MaterialType]); C->SetNumberField(TEXT("material_amount"),Part.MaterialAmount); C->SetNumberField(TEXT("owner"),Part.Owner);
            C->SetNumberField(TEXT("reserved_by"),Part.ReservedBy); C->SetStringField(TEXT("status"),Part.Status);
            C->SetStringField(TEXT("source"),Part.Source); C->SetStringField(TEXT("supply_policy"),Part.SupplyPolicy);
            Components.Add(MakeShared<FJsonValueObject>(C));
        }
        J->SetArrayField(TEXT("components"),Components); Sites.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("sites"),Sites);
    TArray<TSharedPtr<FJsonValue>> Plans;
    for(const FHearthStructurePlan& Plan:StructurePlans)
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("plan_id"),Plan.PlanId); J->SetStringField(TEXT("stable_seed"),Plan.StableSeed);
        J->SetNumberField(TEXT("revision"),Plan.Revision); J->SetNumberField(TEXT("room_count"),Plan.Rooms.Num()); J->SetNumberField(TEXT("component_count"),Plan.Components.Num());
        auto Footprint=MakeShared<FJsonObject>(); Footprint->SetNumberField(TEXT("origin_x"),Plan.Footprint.Origin.X); Footprint->SetNumberField(TEXT("origin_y"),Plan.Footprint.Origin.Y); Footprint->SetNumberField(TEXT("origin_z"),Plan.Footprint.Origin.Z);
        Footprint->SetNumberField(TEXT("size_x"),Plan.Footprint.Size.X); Footprint->SetNumberField(TEXT("size_y"),Plan.Footprint.Size.Y); Footprint->SetNumberField(TEXT("yaw"),Plan.Footprint.Orientation.Yaw); J->SetObjectField(TEXT("footprint"),Footprint);
        auto Reasons=MakeShared<FJsonObject>(); Reasons->SetStringField(TEXT("need"),Plan.Reasons.Need); Reasons->SetStringField(TEXT("occupation"),Plan.Reasons.Occupation); Reasons->SetStringField(TEXT("budget"),Plan.Reasons.Budget); Reasons->SetStringField(TEXT("relationship"),Plan.Reasons.Relationship); Reasons->SetStringField(TEXT("road_access"),Plan.Reasons.RoadAccess); J->SetObjectField(TEXT("reasons"),Reasons);
        TArray<TSharedPtr<FJsonValue>> Rooms; for(const auto& Room:Plan.Rooms){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("id"),Room.Id);R->SetStringField(TEXT("label"),Room.Label);Rooms.Add(MakeShared<FJsonValueObject>(R));} J->SetArrayField(TEXT("rooms"),Rooms);
        TSet<FString> ExtensionIds; TArray<TSharedPtr<FJsonValue>> Components;
        for(const auto& Part:Plan.Components)
        {
            if(!Part.ExtensionId.IsEmpty()) ExtensionIds.Add(Part.ExtensionId);
            auto C=MakeShared<FJsonObject>(); C->SetStringField(TEXT("id"),Part.Id); C->SetStringField(TEXT("catalog_id"),Part.CatalogId); C->SetStringField(TEXT("extension_id"),Part.ExtensionId);
            C->SetNumberField(TEXT("offset_x"),Part.Offset.X); C->SetNumberField(TEXT("offset_y"),Part.Offset.Y); C->SetNumberField(TEXT("offset_z"),Part.Offset.Z); C->SetNumberField(TEXT("yaw"),Part.Orientation.Yaw);
            C->SetNumberField(TEXT("bounds_min_x"),Part.BoundsMin.X); C->SetNumberField(TEXT("bounds_min_y"),Part.BoundsMin.Y); C->SetNumberField(TEXT("bounds_min_z"),Part.BoundsMin.Z); C->SetNumberField(TEXT("bounds_max_x"),Part.BoundsMax.X); C->SetNumberField(TEXT("bounds_max_y"),Part.BoundsMax.Y); C->SetNumberField(TEXT("bounds_max_z"),Part.BoundsMax.Z);
            Components.Add(MakeShared<FJsonValueObject>(C));
        }
        TArray<FString> SortedExtensionIds=ExtensionIds.Array(); SortedExtensionIds.Sort(); TArray<TSharedPtr<FJsonValue>> Extensions; for(const FString& ExtensionId:SortedExtensionIds) Extensions.Add(MakeShared<FJsonValueString>(ExtensionId)); J->SetArrayField(TEXT("extension_ids"),Extensions); J->SetArrayField(TEXT("components"),Components);
        const int32 SiteIndex=ProductionSites.IndexOfByPredicate([&](const FHearthSite& Site){return Site.BuildPlanId==Plan.PlanId;});
        J->SetNumberField(TEXT("site_id"),SiteIndex); if(ProductionSites.IsValidIndex(SiteIndex)){const auto& Site=ProductionSites[SiteIndex];int32 Installed=0;for(const auto& Part:Site.CottageComponents)Installed+=Part.Status==TEXT("completed");J->SetNumberField(TEXT("owner"),Site.Owner);J->SetStringField(TEXT("site_status"),HearthProduction::KindKeys[static_cast<int32>(Site.Kind)]);J->SetNumberField(TEXT("installed_components"),Installed);}
        Plans.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("structure_plans"),Plans);
    auto Totals=MakeShared<FJsonObject>(); for(const auto& Pair:ProductionTotals) Totals->SetNumberField(Pair.Key,Pair.Value); Root->SetObjectField(TEXT("completed_operations"),Totals);
    for(int32 Type=0;Type<3;++Type)
    {
        int32 Carry=0; for(const auto& R:Residents) if(R.CargoType==Type) Carry+=R.CargoAmount;
        for(const auto& S:ProductionSites) for(const auto& C:S.CottageComponents) if(C.MaterialType==Type && C.Status==TEXT("reserved")) Carry+=C.MaterialAmount;
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
        for(const auto& S:ProductionSites) for(const auto& C:S.CottageComponents) if(C.MaterialType==Type+3 && C.Status==TEXT("reserved")) Carry+=C.MaterialAmount;
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
    return FString::Printf(TEXT("食物 %d · 原木 %d · 木板 %d · 房梁 %d · 石材 %d\n村库 %d 枚 · 税收工程金 %d 枚 · 税率 %d%% · 预留工资 %d\n居民买卖 %d 笔 · 税单 %d 笔\n农田 %d · 新住宅 %d · 空地 %d / 已开垦 %d\n%s"),FoodStock,AvailableWood(),PlankStock,BeamStock,StoneStock,TreasuryCoins,TaxProjectCoins,TaxRatePercent,ReservedWages,CompletedTrades,TaxAssessments.Num(),Fields,Houses,Empty,Land,*PublicWorksSummary());
}

FString AHearthVillage::CargoSummary(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return FString(); const auto& R=Residents[Index];
    if(R.CargoAmount>0) return FString::Printf(TEXT("携带 %d 份%s · 运往村镇中心"),R.CargoAmount,HearthProduction::ResourceNames[FMath::Clamp(R.CargoType,0,4)]);
    if(R.ProductionSite>=0) return FString::Printf(TEXT("正在处理 %d 号地块 · 材料已预扣"),R.ProductionSite+1);
    return FString::Printf(TEXT("%s · 建房木材 %d / %d"),*PlotNameFor(Index),R.DeliveredWood,CostFor(Index));
}
