#include "HearthWorldState.h"
#include "HAL/PlatformTime.h"

void AHearthVillage::InitializeResidentIdentity(int32 I,FHearthResident& R) const
{
    const TCHAR* Names[]={TEXT("林恩"),TEXT("米拉"),TEXT("伯恩"),TEXT("阿尔登"),TEXT("伊芙"),TEXT("罗莎"),TEXT("奥斯卡"),TEXT("梅芙"),TEXT("托马斯"),TEXT("塞琳"),
        TEXT("艾达"),TEXT("布兰"),TEXT("卡斯"),TEXT("德拉"),TEXT("埃文"),TEXT("菲娅"),TEXT("格伦"),TEXT("海蒂"),TEXT("伊恩"),TEXT("茱莉"),
        TEXT("凯尔"),TEXT("莉亚"),TEXT("莫恩"),TEXT("诺拉"),TEXT("奥林"),TEXT("佩特拉"),TEXT("昆恩"),TEXT("瑞恩"),TEXT("萨拉"),TEXT("维克")};
    const TCHAR* Roles[]={TEXT("木匠"),TEXT("农民"),TEXT("石匠"),TEXT("国王"),TEXT("商人"),TEXT("陶工"),TEXT("铁匠"),TEXT("织工"),TEXT("采集者"),TEXT("学徒"),
        TEXT("木匠"),TEXT("农民"),TEXT("石匠"),TEXT("商人"),TEXT("陶工"),TEXT("铁匠"),TEXT("织工"),TEXT("采集者"),TEXT("学徒"),TEXT("农民"),
        TEXT("木匠"),TEXT("商人"),TEXT("陶工"),TEXT("铁匠"),TEXT("织工"),TEXT("采集者"),TEXT("学徒"),TEXT("石匠"),TEXT("农民"),TEXT("木匠")};
    const TCHAR* Personas[]={TEXT("内向木匠 · 喜欢树林"),TEXT("热心农民 · 喜欢邻居"),TEXT("节俭石匠 · 够住就好"),
        TEXT("好大喜功的国王 · 以重税维持宫廷，渴望美丽王国"),TEXT("精明商人 · 利润与信誉之间摇摆"),TEXT("耐心陶工 · 喜欢漂亮的红瓦"),
        TEXT("野心铁匠 · 想扩大工坊、获得权贵订单"),TEXT("温和织工 · 珍惜友情与温暖的家"),TEXT("好奇采集者 · 喜欢树林和新鲜事"),TEXT("勤奋学徒 · 想建自己的家和学手艺"),
        TEXT("谨慎木匠 · 记录每一根梁的来处"),TEXT("爽朗农民 · 把街坊当作家人"),TEXT("沉默石匠 · 相信耐心胜过力气"),TEXT("精明商人 · 经营一间可靠的店"),
        TEXT("爱美陶工 · 研究不同窑火的颜色"),TEXT("坚定铁匠 · 想让工具用得更久"),TEXT("温柔织工 · 喜欢在门廊聊天"),TEXT("自由采集者 · 熟悉每条林间小路"),
        TEXT("好学徒弟 · 盼着第一次独立工作"),TEXT("务实农民 · 让每块地都不浪费"),TEXT("建筑木匠 · 想做一间有光的工坊"),TEXT("远行商人 · 重视客人与信誉"),
        TEXT("耐心陶工 · 想给街区铺上好看的瓦"),TEXT("勤勉铁匠 · 计划扩建锻造棚"),TEXT("安静织工 · 盼望温暖的邻里"),TEXT("观察采集者 · 把故事写在地图上"),
        TEXT("机灵学徒 · 喜欢拆解再重装"),TEXT("严谨石匠 · 关心城墙的每个接缝"),TEXT("开朗农民 · 愿意分享收成"),TEXT("细心木匠 · 想留下自己的手艺")};
    const int32 Ages[]={34,29,46,51,32,38,43,27,36,22,31,41,37,28,44,35,26,39,21,48,30,33,40,45,24,42,23,47,29,36};
    I=FMath::Clamp(I,0,UE_ARRAY_COUNT(Names)-1); R.Name=Names[I]; R.Role=Roles[I]; R.Personality=Personas[I]; R.Age=Ages[I]; R.bKing=I==3;
    R.Hunger=10.f+(I%4)*5; R.Mood=60.f+(I%3)*8; R.SocialNeed=I==1 || I==4 || I==7?65.f:25.f;
}

void AHearthVillage::InitializeServiceResident(int32 I,FHearthResident& R) const
{
    static const TCHAR* Names[]={TEXT("埃德温"),TEXT("罗兰"),TEXT("马丁")};
    static const TCHAR* Roles[]={TEXT("门卫"),TEXT("护卫"),TEXT("车夫")};
    static const TCHAR* RoleKeys[]={TEXT("gatekeeper"),TEXT("royal_guard"),TEXT("carter")};
    const int32 Slot=FMath::Clamp(I-10,0,2);
    if(R.StableId.IsEmpty()) R.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    R.Name=Names[Slot]; R.Role=Roles[Slot]; R.ServiceRoleKey=RoleKeys[Slot];
    R.ResidenceId=TEXT("v4_public_life_rest_anchor"); R.Personality=Slot==0?TEXT("警觉门卫 · 熟悉每个来访者"):Slot==1?TEXT("守序护卫 · 先保护村民再休息"):TEXT("稳重车夫 · 记得每批货物的来路");
    R.InnerStory=FString::Printf(TEXT("我是%s，职责是%s，住在村庄公共生活区；我的职责、工资和物资都必须通过真实账本记录。"),*R.Name,*R.Role);
    R.Age=Slot==1?34:Slot==2?41:38; R.Hunger=15.f; R.Mood=68.f; R.SocialNeed=32.f; R.Energy=75.f;
    R.Plot=-1; R.Coins=0; R.PersonalPlanks=0; R.PersonalTiles=0; R.CarriedWood=0; R.DeliveredWood=0; R.BuildProgress=0.f;
    R.HouseBlueprint.Empty(); R.WallMaterial.Empty(); R.RoofMaterial.Empty(); R.BuildingArchetype.Empty(); R.DesignGoal.Empty();
    R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.LifeAction=-1; R.ProductionSite=-1; R.ProductionOp=-1;
    R.CargoType=-1; R.CargoAmount=0; R.ProductionComponentId.Empty(); R.Route.Reset(); R.ConversationId.Empty();
    R.Reason=TEXT("住在村庄公共生活区，等待适合本职的职责。"); R.LatestEvent=TEXT("作为服务居民来到村庄公共生活区。");
}

bool AHearthVillage::IsSharedServiceResident(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return false;
    const auto& R=Residents[Index];
    return R.ServiceRoleKey==TEXT("gatekeeper") || R.ServiceRoleKey==TEXT("royal_guard") || R.ServiceRoleKey==TEXT("carter");
}

FVector AHearthVillage::SharedResidenceAnchor(int32 Index) const
{
    const int32 Slot=FMath::Clamp(Index-10,0,2);
    // This is the existing v4 public life/depot area used by eating and tavern
    // routes; shared residents do not claim a private plot or create a house.
    const FVector Base=bUseCropoutMap?FVector(-1650.f,-1050.f,8.f):FVector(-250.f,-400.f,0.f);
    FVector Candidate=Base+FVector((Slot-1)*85.f,Slot==1?35.f:-35.f,0.f);
    if(bUseCropoutMap) Candidate.Z=GroundHeightAt(Candidate)+5.2f;
    if(bUseCropoutMap && !IsClearPoint(Candidate))
    {
        for(int32 Ring=1;Ring<=3 && !IsClearPoint(Candidate);++Ring)
        {
            FVector Try=FVector(Candidate.X+Ring*90.f,Candidate.Y+((Ring&1)?90.f:-90.f),Candidate.Z);
            Try.Z=GroundHeightAt(Try)+5.2f;
            if(IsClearPoint(Try)) { Candidate=Try; break; }
        }
    }
    return Candidate;
}

FString AHearthVillage::PlotLabel(int32 Plot) const
{
    const TCHAR* Names[]={TEXT("林边地块"),TEXT("花园旁地块"),TEXT("紧凑地块"),TEXT("西街北宅"),TEXT("西街中宅"),TEXT("西街南宅"),TEXT("东街北宅"),TEXT("西街北巷"),TEXT("西街南巷"),TEXT("花园南侧")};
    if(IsTownLayoutVersion3() && Plot>=0 && Plot<HousingPlotCount()) return FString::Printf(TEXT("Town3第%02d号街坊"),Plot+1);
    return Plot>=0 && Plot<HousingPlotCount()?Names[Plot]:TEXT("正在选址");
}

void AHearthVillage::AdvanceNeeds(float Dt)
{
    for(auto& R:Residents)
    {
        R.Hunger=FMath::Min(100.f,R.Hunger+Dt*.04f);
        const float Target=FMath::Clamp(85.f-R.Hunger*.35f-R.SocialNeed*.2f-(100.f-R.Energy)*.15f,5.f,95.f);
        R.Mood=FMath::FInterpTo(R.Mood,Target,Dt,.02f);
        // Hunger has a consequence before a future health/death system is introduced.
        if(R.Hunger>85.f) R.Energy=FMath::Max(0.f,R.Energy-Dt*.1f);
    }
}

bool AHearthVillage::MigrateWorldPopulation(FHearthWorldImage& W,FString& Error) const
{
    if(W.People.Num()==Residents.Num())
    {
        if(IsOrganicVillage() && Residents.Num()==13)
        {
            if(W.PlotCount!=10) { Error=TEXT("有机村服务居民存档必须保留十个私宅地块"); return false; }
            W.PopulationCount=13; W.Schema=FMath::Max(W.Schema,12);
        }
        return true;
    }
    if(IsOrganicVillage() && W.People.Num()==10 && Residents.Num()==13 && W.PlotCount==10)
    {
        TSet<FString> ExistingIds;
        for(const auto& Saved:W.People) ExistingIds.Add(Saved.Person.StableId);
        for(int32 I=10;I<13;++I)
        {
            const auto& Source=Residents[I];
            if(Source.ServiceRoleKey.IsEmpty() || Source.ResidenceId.IsEmpty() || Source.StableId.IsEmpty() || ExistingIds.Contains(Source.StableId))
            { Error=TEXT("有机村服务居民身份或重复迁移标记无效"); return false; }
            // The old image has no service records. Rebuild a clean catalog
            // record instead of copying a transient task, cargo, conversation,
            // or relationship from the runtime staging residents.
            FHearthSavedResident Saved; InitializeServiceResident(I,Saved.Person); Saved.Person.StableId=Source.StableId; Saved.Person.Actor=nullptr;
            Saved.Position=SharedResidenceAnchor(I); Saved.Yaw=180.f;
            Saved.DecisionDelay=FMath::Max(0.0,Source.NextLifeDecision-Elapsed); W.People.Add(MoveTemp(Saved)); ExistingIds.Add(Source.StableId);
        }
        W.PopulationCount=13; W.Schema=12; W.bComplete=false;
        W.Event=TEXT("三位服务居民加入公共生活区；原有十位居民、私宅、库存和钱币保持不变。");
        return HearthWorld::Decode(HearthWorld::Encode(W),W,Error);
    }
    if(!bUseCropoutMap || W.People.Num()!=3 || Residents.Num()!=10 || W.PlotCount!=3)
    { Error=TEXT("不支持该人口存档迁移，原文件已保留"); return false; }
    // Seven new adults arrive with explicitly accounted food/wood supplies. Existing
    // people, task IDs, site IDs, ownership, history and in-transit resources remain.
    for(int32 I=3;I<10;++I) for(auto& Site:W.Sites)
    {
        const FVector P=PlotPositions[I];
        if(FMath::Abs(P.X-Site.Position.X)>=Site.Radius+270 || FMath::Abs(P.Y-Site.Position.Y)>=Site.Radius+270) continue;
        if(Site.Kind!=EHearthSiteKind::Empty || Site.Owner>=0 || Site.ReservedBy>=0)
        { Error=TEXT("新增住宅覆盖已有生产或产权地块，须先调整布局；保留原世界"); return false; }
        // Retire only vacant, unowned future expansion markers; retain their IDs
        // and array positions so every existing historical site reference stays valid.
        Site.bExpansion=false; Site.bReachable=false;
    }
    for(int32 I=3;I<10;++I)
    {
        FHearthSavedResident S; S.Person=Residents[I]; S.Person.Actor=nullptr;
        S.Position=Residents[I].Actor->GetActorLocation(); S.Yaw=180;
        W.People.Add(MoveTemp(S)); W.PlotIds[I]=PlotIds[I]; W.Plots[I]=PlotPositions[I]; W.Costs[I]=PlotCosts[I]; W.Owners[I]=-1;
        W.PlotYaws[I]=PlotYaws[I]; W.PlotEntrances[I]=PlotEntrances[I];
    }
    W.Food+=70; for(int32 I=0;I<3;++I) W.Wood[I]+=21;
    W.PlotCount=10; W.PopulationCount=10; W.Schema=3; W.bComplete=false;
    W.Event=TEXT("七位新居民抵达，带来70份食物和63份木材；原居民的家与工作都保留。");
    FHearthDecisionRecord Arrival; Arrival.Run=W.Run; Arrival.Timestamp=FDateTime::Now().ToString(); Arrival.Resident=3;
    Arrival.At=W.Elapsed; Arrival.Kind=TEXT("population_migration"); Arrival.Source=TEXT("world_rules"); Arrival.Status=TEXT("completed");
    Arrival.Choice=TEXT("七位成年居民抵达"); Arrival.Result=W.Event; W.History.Add(MoveTemp(Arrival));
    FHearthWorldImage Verified;
    return HearthWorld::Decode(HearthWorld::Encode(W),Verified,Error);
}
