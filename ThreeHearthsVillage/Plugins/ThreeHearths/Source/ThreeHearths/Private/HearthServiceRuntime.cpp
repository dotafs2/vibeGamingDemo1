#include "HearthWorldState.h"

namespace
{
    constexpr float ServiceShiftSeconds=600.f;

    FHearthServiceDutyRecord* ActiveDuty(TArray<FHearthServiceDutyRecord>& Duties,int32 Resident,const FString& TaskId)
    {
        return Duties.FindByPredicate([Resident,&TaskId](const FHearthServiceDutyRecord& D)
        { return D.Resident==Resident && D.Status==TEXT("active") && D.TaskId==TaskId; });
    }

    const TCHAR* DutyKind(const FHearthResident& Resident)
    {
        if(Resident.ServiceRoleKey==TEXT("gatekeeper")) return TEXT("gate_watch");
        if(Resident.ServiceRoleKey==TEXT("royal_guard")) return TEXT("guard_patrol");
        return TEXT("stable_check");
    }

    FVector GuardPatrolAnchor(const AHearthVillage& Village,int32 ResidentIndex,int32 Point,TFunctionRef<bool(const FVector&)> Clear)
    {
        FVector Base=Village.GetServiceDutyAnchor(ResidentIndex);
        const FVector Offsets[]={FVector::ZeroVector,FVector(220,100,0),FVector(-220,200,0)};
        FVector Result=Base+Village.GetServiceGateFrame().TransformVectorNoScale(Offsets[FMath::Clamp(Point,0,2)]);
        if(Village.IsOrganicVillage()) Result.Z=Village.GroundHeightAt(Result)+5.2f;
        if(Village.IsOrganicVillage() && !Clear(Result))
        {
            // Check nearby clear ground in increasing distance. A facade can
            // span the old point and its entire outward strip, so search all
            // directions while retaining normal land, obstacle and route rules.
            for(float Radius:{120.f,240.f,360.f,520.f,700.f,900.f}) for(int32 Angle=0;Angle<12;++Angle)
            {
                FVector Candidate=Result+FRotator(0,Angle*30.f,0).RotateVector(FVector(Radius,0,0));
                Candidate.Z=Village.GroundHeightAt(Candidate)+5.2f;
                if(Point>0 && (FVector::Dist2D(Candidate,Base)<120.f || FVector::Dist2D(Candidate,Village.Residents[ResidentIndex].Actor->GetActorLocation())<120.f)) continue;
                if(Clear(Candidate))
                {UE_LOG(LogTemp,Display,TEXT("SERVICE_PATROL_RELOCATED resident=%d point=%d distance=%.0f"),ResidentIndex,Point,Radius);return Candidate;}
            }
        }
        return Result;
    }
}

FTransform AHearthVillage::GetServiceGateFrame() const
{
    // Local +Y faces away from the actual angled home, with room for both
    // piers and the watch shelter outside its occupied footprint.
    const int32 Plot=Residents.IsValidIndex(3)?Residents[3].Plot:3;
    if(Plot<0 || Plot>=HousingPlotCount()) return FTransform(FVector(-2200,-4000,0));
    const FVector Entry=PlotEntrances[Plot];
    FVector Outward=Entry-PlotPositions[Plot]; Outward.Z=0;
    if(!Outward.Normalize()) Outward=FRotator(0,PlotYaws[Plot],0).RotateVector(FVector(0,-1,0));
    return FTransform(FRotator(0,Outward.Rotation().Yaw-90.f,0),Entry+Outward*350.f);
}

FVector AHearthVillage::GetServiceDutyAnchor(int32 ResidentIndex) const
{
    if(!IsSharedServiceResident(ResidentIndex)) return FVector::ZeroVector;
    const auto& R=Residents[ResidentIndex];
    const FTransform GateFrame=GetServiceGateFrame();
    const FVector GateBase=GateFrame.GetLocation();
    // The gate and public stable serve different spaces. Keep the carter at
    // the public life/depot area even when the king entrance is nearby, with
    // a deterministic clear fallback at least twelve metres from the gate.
    FVector Base=R.ServiceRoleKey==TEXT("carter")?FVector(-1550,-1950,0):GateFrame.TransformPosition(R.ServiceRoleKey==TEXT("gatekeeper")?FVector(-280,150,0):FVector(0,360,0));
    if(R.ServiceRoleKey==TEXT("carter") && FVector::Dist2D(Base,GateBase)<1200.f) Base=GateBase+FVector(0,1200,0);
    if(bUseCropoutMap) Base.Z=GroundHeightAt(Base)+5.2f;
    if(IsClearPoint(Base)) return Base;
    for(int32 Ring=1;Ring<=4;++Ring)
    {
        for(const FVector& CandidateOffset:{FVector(Ring*90.f,0,0),FVector(-Ring*90.f,0,0),FVector(0,Ring*90.f,0),FVector(0,-Ring*90.f,0)})
        {
            FVector Candidate=Base+CandidateOffset;
            if(bUseCropoutMap) Candidate.Z=GroundHeightAt(Candidate)+5.2f;
            if(IsClearPoint(Candidate)) return Candidate;
        }
    }
    return Base;
}

bool AHearthVillage::StartServiceDuty(int32 ResidentIndex)
{
    if(!IsSharedServiceResident(ResidentIndex) || Residents[ResidentIndex].Task!=EHearthTask::LifeChoosing || Elapsed+.1f<Residents[ResidentIndex].NextLifeDecision) return false;
    auto& R=Residents[ResidentIndex];
    if(ServiceDuties.ContainsByPredicate([ResidentIndex](const FHearthServiceDutyRecord& D){return D.Resident==ResidentIndex && D.Status==TEXT("active");})) return false;
    const FString TaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    // Public staff are a real use of collected royal taxes. Once the general
    // purse is exhausted, use only unreserved tax cash; never mint wages or
    // spend money already held for an existing building/transport contract.
    const bool bTaxFunded=GeneralFunds()<3 && TaxProjectCoins>=3;
    if(!ReserveWage(ResidentIndex,TaskId,3,bTaxFunded,-1)) { R.NextLifeDecision=Elapsed+120.f; return false; }
    const auto ClearPatrol=[this,ResidentIndex](const FVector& P){TArray<FVector> Probe;return IsClearPoint(P) && IsLand(P) && FindActivityRoute(ResidentIndex,P,Probe);};
    TArray<FVector> Route; const bool bGuard=R.ServiceRoleKey==TEXT("royal_guard"); const FVector Anchor=bGuard?GuardPatrolAnchor(*this,ResidentIndex,0,ClearPatrol):GetServiceDutyAnchor(ResidentIndex);
    if(bUseCropoutMap && !FindActivityRoute(ResidentIndex,Anchor,Route)) { CancelWage(TaskId); R.NextLifeDecision=Elapsed+120.f; return false; }
    if(!bUseCropoutMap) Route={Anchor};
    FHearthServiceDutyRecord D; D.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); D.TaskId=TaskId; D.Kind=DutyKind(R); D.Resident=ResidentIndex; D.Wage=3; D.ShiftSeconds=ServiceShiftSeconds; D.Anchor=Anchor; D.MoveSpeed=90.f; D.PatrolPoint=0; D.NextDutyAt=Elapsed;
    if(R.Coins==0 && R.Hunger>=35.f)
    {
        if(!SettleWage(ResidentIndex,TaskId)) { CancelWage(TaskId); R.NextLifeDecision=Elapsed+120.f; return false; }
        D.bWagePaid=true;
    }
    ServiceDuties.Add(D);
    R.ActiveTaskId=TaskId; R.LifeAction=R.ServiceRoleKey==TEXT("royal_guard")?61:R.ServiceRoleKey==TEXT("carter")?62:60; R.Task=EHearthTask::LifeTravel; R.Timer=0.f; R.MoveSpeed=D.MoveSpeed; R.Route=MoveTemp(Route);
    R.DecisionSource=TEXT("service_local"); R.LatestEvent=R.ServiceRoleKey==TEXT("royal_guard")?TEXT("开始沿公共道路巡逻。 "):R.ServiceRoleKey==TEXT("carter")?TEXT("检查公共马厩与货运场地。 "):TEXT("在门楼值守，等待来访者。 ");
    StartHistory(ResidentIndex,true,TEXT("service_local")); AcceptHistory(ResidentIndex,R.LatestEvent,TEXT("本地职责调度"),TEXT("service_local"));
    return true;
}

bool AHearthVillage::CancelServiceDuty(int32 ResidentIndex)
{
    if(!Residents.IsValidIndex(ResidentIndex)) return false;
    auto* Duty=ServiceDuties.FindByPredicate([ResidentIndex](const FHearthServiceDutyRecord& D){return D.Resident==ResidentIndex && D.Status==TEXT("active");});
    if(!Duty) return false;
    const bool bWasPrepaid=Duty->bWagePaid;
    // A pre-paid bootstrap wage is already a real transaction.  It is not a
    // reservation any more, so cancellation must not attempt a second refund.
    if(!bWasPrepaid && !CancelWage(Duty->TaskId)) return false;
    Duty->Status=TEXT("cancelled"); Duty->Progress=FMath::Clamp(Duty->DutySeconds/FMath::Max(1.f,Duty->ShiftSeconds),0.f,1.f); Duty->NextDutyAt=Elapsed+120.f;
    auto& R=Residents[ResidentIndex]; R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.Route.Reset(); R.Timer=0.f; R.LifeAction=0; R.MoveSpeed=240.f; R.NextLifeDecision=Elapsed+120.f;
    R.LatestEvent=bWasPrepaid?TEXT("职责取消；已结算的预付工资保留，居民可用它购买食物。 "):TEXT("职责路线取消，预留工资已退回国库。 "); CompleteHistory(ResidentIndex,R.LatestEvent); return true;
}

void AHearthVillage::AdvanceServiceResident(int32 ResidentIndex,float Dt)
{
    if(!Residents.IsValidIndex(ResidentIndex)) return;
    auto& R=Residents[ResidentIndex]; auto* Duty=ActiveDuty(ServiceDuties,ResidentIndex,R.ActiveTaskId);
    if(!Duty) return;
    // A visitor can speak to a guard while the guard remains at the duty
    // point. Freeze duty progress and leave the task/wage reference intact;
    // CloseConversation clears only ConversationId and execution resumes here.
    if(!R.ConversationId.IsEmpty()) return;
    if(R.Task==EHearthTask::LifeTravel)
    {
        if(MoveResident(ResidentIndex,Dt))
        {
            R.Task=EHearthTask::LifeActivity; R.Timer=Duty->Kind==TEXT("guard_patrol")?Duty->ShiftSeconds/3.f:Duty->ShiftSeconds;
            if(Duty->Kind==TEXT("guard_patrol")) R.LatestEvent=FString::Printf(TEXT("已到第%d个巡逻点，正在检查公共道路。 "),Duty->PatrolPoint+1);
            else R.LatestEvent=Duty->Kind==TEXT("stable_check")?TEXT("已到公共马厩，正在检查货运场地。 "):TEXT("已到门楼，正在站岗。 ");
        }
        return;
    }
    if(R.Task==EHearthTask::LifeActivity)
    {
        if(R.Hunger>=70.f || R.Energy<=35.f) { UE_LOG(LogTemp,Display,TEXT("SERVICE_CANCEL needs resident=%d hunger=%.1f energy=%.1f"),ResidentIndex,R.Hunger,R.Energy);CancelServiceDuty(ResidentIndex); return; }
        Duty->DutySeconds=FMath::Min(Duty->ShiftSeconds,Duty->DutySeconds+Dt); Duty->Progress=Duty->DutySeconds/FMath::Max(1.f,Duty->ShiftSeconds);
        const float SegmentEnd=Duty->Kind==TEXT("guard_patrol")?FMath::Min(Duty->ShiftSeconds,(Duty->PatrolPoint+1)*(Duty->ShiftSeconds/3.f)):Duty->ShiftSeconds;
        if(Duty->DutySeconds<SegmentEnd || R.Timer>0.f) return;
        if(Duty->Kind==TEXT("guard_patrol"))
        {
            if(Duty->PatrolPoint<2)
            {
                const auto ClearPatrol=[this,ResidentIndex](const FVector& P){TArray<FVector> Probe;return IsClearPoint(P) && IsLand(P) && FindActivityRoute(ResidentIndex,P,Probe);};
                ++Duty->PatrolPoint; const FVector Target=GuardPatrolAnchor(*this,ResidentIndex,Duty->PatrolPoint,ClearPatrol); TArray<FVector> Route;
                if(bUseCropoutMap && !FindActivityRoute(ResidentIndex,Target,Route)) { UE_LOG(LogTemp,Display,TEXT("SERVICE_CANCEL route resident=%d target=%s clear=%d land=%d"),ResidentIndex,*Target.ToString(),IsClearPoint(Target),IsLand(Target));CancelServiceDuty(ResidentIndex); return; }
                if(!bUseCropoutMap) Route={Target}; R.Route=MoveTemp(Route); R.Task=EHearthTask::LifeTravel; R.Timer=0.f; return;
            }
        }
        if(!Duty->bWagePaid && !SettleWage(ResidentIndex,R.ActiveTaskId)) return;
        Duty->bWagePaid=true; Duty->Progress=1.f; Duty->DutySeconds=Duty->ShiftSeconds; Duty->Status=TEXT("completed"); Duty->NextDutyAt=Elapsed+300.f;
        R.LatestEvent=TEXT("完整职责时段完成，工资已通过国库结算。 "); CompleteHistory(ResidentIndex,R.LatestEvent);
        R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.LifeAction=0; R.Timer=0.f; R.MoveSpeed=240.f; R.Route.Reset(); R.NextLifeDecision=Elapsed+300.f;
    }
}

void AHearthVillage::AdvanceServiceRuntime(float Dt)
{
    for(int32 I=10;I<Residents.Num();++I)
    {
        if(!IsSharedServiceResident(I)) continue;
        if(I==12 && FreightOrders.ContainsByPredicate([](const FHearthFreightOrder& O){return O.Status==TEXT("transporting");})) continue;
        auto& R=Residents[I];
        if(R.Task!=EHearthTask::LifeChoosing || R.ActiveTaskId.Len()>0 || Elapsed+.1f<R.NextLifeDecision) continue;
        if(R.Hunger>=35.f && R.Coins>0) { StartLifeAction(I,50,TEXT("服务居民先购买食物"),false); continue; }
        if(R.Energy<=35.f) { StartLifeAction(I,0,TEXT("服务居民轮休"),false); continue; }
        StartServiceDuty(I);
    }
    RefreshMedievalPublicVisuals(Dt);
}
