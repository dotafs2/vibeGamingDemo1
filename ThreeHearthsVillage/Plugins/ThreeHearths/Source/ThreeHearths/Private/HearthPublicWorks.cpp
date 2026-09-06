#include "HearthVillage.h"

namespace HearthPublicWorks
{
    void Populate(FHearthPublicProject& Project)
    {
        Project.TemplateId = TEXT("public_wall_6m");
        Project.Policy = TEXT("local_king_fixed_income_tax_25");
        if (Project.Id.IsEmpty()) Project.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        Project.Parts.Reset();
        auto Add = [&](const TCHAR* Kind, int32 Stage, float X, float Y, float Z, int32 Stone, int32 Plank, int32 Beam, int32 Bay)
        {
            FHearthPublicPart Part;
            Part.Id = FString::Printf(TEXT("%s:%s_%d"), *Project.Id, Kind, Bay);
            Part.Asset = FString::Printf(TEXT("public_wall_%s_2m"), Kind);
            Part.Offset = FVector(X, Y, Z); Part.Stage = Stage;
            Part.Required[0] = Stone; Part.Required[1] = Plank; Part.Required[2] = Beam;
            Project.Parts.Add(MoveTemp(Part));
        };
        for (int32 Bay = 0; Bay < 3; ++Bay) Add(TEXT("foundation"), 1, -200.f + 200.f * Bay, 0.f, 0.f, 4, 0, 0, Bay);
        for (int32 Bay = 0; Bay < 3; ++Bay) Add(TEXT("stone"), 2, -200.f + 200.f * Bay, 0.f, 28.f, 8, 0, 0, Bay);
        for (int32 Bay = 0; Bay < 3; ++Bay) Add(TEXT("walkway"), 3, -200.f + 200.f * Bay, 0.f, 267.7f, 0, 3, 2, Bay);
        for (int32 Bay = 0; Bay < 3; ++Bay)
        {
            const float X = -200.f + 200.f * Bay;
            Add(TEXT("parapet"), 4, X, -38.f, 289.7f, 0, 2, 1, Bay * 2);
            Add(TEXT("parapet"), 4, X, 38.f, 289.7f, 0, 2, 1, Bay * 2 + 1);
        }
    }
}

namespace
{
    FVector PublicDepotFor(const AHearthVillage& Village)
    {
        return Village.bUseCropoutMap ? FVector(-1650.f, -1050.f, 8.f) : FVector(-250.f, -400.f, 8.f);
    }
    const TCHAR* MaterialNames[] = { TEXT("stone"), TEXT("plank"), TEXT("beam") };

    void ClearPublicResident(FHearthResident& R)
    {
        R.ActiveTaskId.Empty(); R.Route.Reset(); R.Task = EHearthTask::LifeChoosing; R.Timer = 0.f;
        R.ProductionSite = -1; R.ProductionOp = -1; R.ProductionComponentId.Empty();
        R.CargoType = -1; R.CargoAmount = 0; R.WorkDuration = 0.f; R.NextLifeDecision = 0; R.LifeAction = -1;
    }

    FHearthPublicPart* FirstPart(FHearthPublicProject& P)
    {
        return P.Parts.FindByPredicate([](const FHearthPublicPart& Part)
        { return Part.Status != TEXT("completed"); });
    }

}

bool AHearthVillage::ApprovePublicProject(int32 King)
{
    if (Residents.Num() < 10 || !Residents.IsValidIndex(King) || !Residents[King].bKing
        || !CanAssignActivity(King) || TaxRatePercent != 25
        || PublicProject.Status != TEXT("unapproved")) return false;

    int32 SiteIndex = -1;
    float Best = FLT_MAX;
    for (int32 I = 0; I < ProductionSites.Num(); ++I)
    {
        const FHearthSite& S = ProductionSites[I];
        if (S.Kind != EHearthSiteKind::Empty || !S.bExpansion || S.ReservedBy >= 0 || !S.BuildPlanId.IsEmpty()) continue;
        if (FVector::DistSquared2D(S.Position, PublicDepotFor(*this)) < Best) { Best = FVector::DistSquared2D(S.Position, PublicDepotFor(*this)); SiteIndex = I; }
    }
    if (!ProductionSites.IsValidIndex(SiteIndex)) return false;
    const FHearthSite OriginalSite = ProductionSites[SiteIndex];
    ProductionSites[SiteIndex].Radius = 350.f;
    if (!ChooseSiteApproach(SiteIndex) || !ProductionSites[SiteIndex].bReachable) { ProductionSites[SiteIndex] = OriginalSite; return false; }
    TArray<FVector> Route;
    if (!FindActivityRoute(King, ProductionSites[SiteIndex].Approach, Route)) { ProductionSites[SiteIndex] = OriginalSite; return false; }

    FHearthDecisionRecord History;
    History.Run = CurrentRun; History.Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    History.Kind = TEXT("public_project_policy"); History.Source = TEXT("local"); History.Model.Empty();
    History.Resident = King; History.At = Elapsed; History.Status = TEXT("completed");
    History.Choice = TEXT("批准公共城墙工程"); History.Reason = TEXT("本地国王批准固定25%税收政策");
    History.Context = TEXT("ApprovalHistoryId=") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    History.Result = TEXT("批准公共城墙工程");
    const FString ApprovalId = History.Context.Mid(18);
    const int32 HistoryIndex = DecisionHistory.Add(MoveTemp(History));
    PublicProject = FHearthPublicProject(); PublicProject.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    PublicProject.Status = TEXT("building"); PublicProject.ApprovalHistoryId = ApprovalId; PublicProject.King = King;
    PublicProject.Site = SiteIndex; PublicProject.ApprovedAt = Elapsed; HearthPublicWorks::Populate(PublicProject);
    ProductionSites[SiteIndex].Owner = -1; ProductionSites[SiteIndex].ReservedBy = -1;
    Residents[King].HistoryIndex = HistoryIndex; ++HistoryRevision;
    VillageEvent = TEXT("国王批准公共城墙工程，固定25%税收用于工资。");
    return true;
}

bool AHearthVillage::StartPublicPart(int32 Worker)
{
    if (!Residents.IsValidIndex(Worker) || !CanAssignActivity(Worker)
        || (PublicProject.Status != TEXT("building") && PublicProject.Status != TEXT("approved"))) return false;
    FHearthPublicPart* Part = FirstPart(PublicProject);
    if (!Part || Part->Worker >= 0) return false;
    TArray<FVector> Route;
    if (!FindActivityRoute(Worker, PublicDepotFor(*this), Route)) return false;
    for (int32 M = 0; M < 3; ++M)
        if (Part->Required[M] > PublicProject.Stock[M]) return false;
    auto& R = Residents[Worker];
    const FString TaskId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    const FString PreviousTask=R.ActiveTaskId; R.ActiveTaskId=TaskId;
    if (!TryBorrowTool(Worker, 5) || !ReserveWage(Worker, TaskId, 2, true)) { ReturnTool(Worker); R.ActiveTaskId=PreviousTask; return false; }
    R.ProductionSite = -1; R.ProductionOp = -1; R.ProductionComponentId.Empty();
    R.CargoType = -1; R.CargoAmount = 0; R.Route = MoveTemp(Route); R.Task = EHearthTask::PublicTravel; R.Timer = 0.f; R.LifeAction = 1;
    Part->Worker = Worker; Part->TaskId = TaskId; Part->Status = TEXT("transporting");
    for (int32 M = 0; M < 3; ++M) { Part->Reserved[M] = Part->Required[M]; PublicProject.Stock[M] -= Part->Required[M]; }
    StartHistory(Worker,true,TEXT("local"));
    AcceptHistory(Worker,TEXT("安装公共城墙构件"),TEXT("公共工程按顺序调度了一个可达构件。"),TEXT("local"));
    R.LatestEvent = TEXT("前往公共仓库分批领取城墙材料。"); return true;
}

void AHearthVillage::AdvancePublicWorks(float Dt)
{
    if (!bAutonomousLifeEnabled) return;
    PublicScheduleTimer = FMath::Max(0.f, PublicScheduleTimer - FMath::Max(0.f, Dt));
    if (PublicScheduleTimer > 0.f) return;
    PublicScheduleTimer = 1.f;
    if(PublicProject.Status==TEXT("unapproved"))
    {
        for(int32 I=0;I<Residents.Num();++I) if(Residents[I].bKing && ApprovePublicProject(I)) return;
        return;
    }
    if(PublicProject.Status != TEXT("building")) return;
    // Physical depot grants are aggregate deductions from the real village depot.
    for (int32 M : {0, 2})
    {
        int32 Need = 0, Held=PublicProject.Stock[M];
        for (const auto& Part : PublicProject.Parts) if (Part.Status != TEXT("completed"))
        { Need+=Part.Required[M]; Held+=Part.Reserved[M]+Part.Delivered[M]; }
        for(const auto& Resident:Residents) if((Resident.Task==EHearthTask::PublicTravel || Resident.Task==EHearthTask::PublicWork) && Resident.CargoType==M+2) Held+=Resident.CargoAmount;
        int32& Depot = M == 0 ? StoneStock : BeamStock;
        const int32 Grant = FMath::Min(Depot, FMath::Max(0,Need-Held));
        if (Grant > 0) { Depot -= Grant; PublicProject.Stock[M] += Grant; PublicProject.Grants[M] += Grant; }
    }
    for (int32 I = 0; I < Residents.Num(); ++I)
        if (CanAssignActivity(I) && StartSupplyOrder(I)) return;
    for (int32 I = 0; I < Residents.Num(); ++I)
        if (CanAssignActivity(I) && StartPublicPart(I)) return;
}

void AHearthVillage::AdvancePublicWorker(int32 Worker, float Dt)
{
    if (!Residents.IsValidIndex(Worker)) return;
    auto& R = Residents[Worker];
    FHearthPublicPart* Part = PublicProject.Parts.FindByPredicate([&](const FHearthPublicPart& P)
    { return P.Worker == Worker && P.TaskId == R.ActiveTaskId && P.Status != TEXT("completed"); });
    if (!Part) return;
    if (R.Task == EHearthTask::PublicTravel)
    {
        if (!MoveResident(Worker, Dt)) return;
        if (R.LifeAction == 2 && R.CargoAmount > 0)
        {
            const int32 M = R.CargoType - 2;
            if (M < 0 || M >= 3) return;
            Part->Delivered[M] += R.CargoAmount; R.CargoType = -1; R.CargoAmount = 0;
            int32 Next = -1; for (int32 I = 0; I < 3; ++I) if (Part->Reserved[I] > 0) { Next = I; break; }
            if (Next >= 0)
            {
                TArray<FVector> Route;
                if (!FindActivityRoute(Worker, PublicDepotFor(*this), Route)) { CancelPublicWork(Worker); return; }
                R.Route = MoveTemp(Route); R.LifeAction = 1; R.LatestEvent = TEXT("材料已送达工地，返回公共仓库继续搬运。"); return;
            }
            R.Task = EHearthTask::PublicWork; R.Timer = 1.f; R.WorkDuration = 1.f; Part->Status = TEXT("installing"); return;
        }
        if (R.LifeAction != 1) { CancelPublicWork(Worker); return; }
        int32 M = -1; for (int32 I = 0; I < 3; ++I) if (Part->Reserved[I] > 0) { M = I; break; }
        if (M >= 0)
        {
            const int32 Amount = FMath::Min(6, Part->Reserved[M]); Part->Reserved[M] -= Amount;
            R.CargoType = M + 2; R.CargoAmount = Amount;
            TArray<FVector> Route; if (!FindActivityRoute(Worker, ProductionSites[PublicProject.Site].Approach, Route)) { Part->Reserved[M] += Amount; R.CargoType = -1; R.CargoAmount = 0; return; }
            R.Route = MoveTemp(Route); R.LifeAction = 2; R.LatestEvent = FString::Printf(TEXT("携带%d份%s前往工地。"), Amount, MaterialNames[M]); return;
        }
        R.Task = EHearthTask::PublicWork; R.Timer = 1.f; R.WorkDuration = 1.f; Part->Status = TEXT("installing"); return;
    }
    if (R.Task == EHearthTask::PublicWork)
    {
        // AHearthVillage::AdvanceSimulation decrements resident timers once per tick.
        if (R.Timer > 0.f) return;
        if (!SettleWage(Worker, R.ActiveTaskId)) { R.Timer = 0.1f; return; }
        for (int32 M = 0; M < 3; ++M) { if (M == 0) Spent[2] += Part->Delivered[M]; else ManufacturedSpent[M - 1] += Part->Delivered[M]; Part->Delivered[M] = 0; }
        Part->Status = TEXT("completed"); Part->Worker = -1; ++PublicProject.Completed;
        R.LatestEvent = TEXT("公共城墙构件已安装并结算税收工资。");
        CompleteHistory(Worker,R.LatestEvent); ReturnTool(Worker); ClearPublicResident(R);
        if (PublicProject.Completed >= PublicProject.Parts.Num()) PublicProject.Status = TEXT("completed");
        VillageEvent = R.Name + TEXT("：") + R.LatestEvent; return;
    }
}

bool AHearthVillage::CancelPublicWork(int32 Resident)
{
    if (!Residents.IsValidIndex(Resident)) return false;
    auto& R = Residents[Resident]; FHearthPublicPart* Part = PublicProject.Parts.FindByPredicate([&](const FHearthPublicPart& P)
    { return P.Worker == Resident && P.TaskId == R.ActiveTaskId && P.Status != TEXT("completed"); });
    if (!Part) return false;
    for (int32 M = 0; M < 3; ++M) { PublicProject.Stock[M] += Part->Reserved[M] + Part->Delivered[M]; Part->Reserved[M] = 0; Part->Delivered[M] = 0; }
    if (R.CargoType >= 2 && R.CargoType <= 4) PublicProject.Stock[R.CargoType - 2] += R.CargoAmount;
    CancelWage(R.ActiveTaskId); Part->Status = TEXT("waiting"); Part->Worker = -1; Part->TaskId.Empty();
    ReturnTool(Resident); R.LatestEvent = TEXT("公共工程取消，材料和税收工资预留已退回。"); CompleteHistory(Resident,R.LatestEvent); ClearPublicResident(R); return true;
}
