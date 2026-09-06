#include "HearthMovement.h"
#include "HearthVillage.h"

bool HearthMovement::SegmentHitsBox(const FVector& Start,const FVector& End,const FVector& Center,double Radius)
{
    double Enter=0,Leave=1;
    for(int32 Axis=0;Axis<2;++Axis)
    {
        const double A=Start[Axis]-Center[Axis], Delta=End[Axis]-Start[Axis];
        if(FMath::Abs(Delta)<1e-10) { if(FMath::Abs(A)>=Radius) return false; continue; }
        const double T1=(-Radius-A)/Delta,T2=(Radius-A)/Delta;
        Enter=FMath::Max(Enter,FMath::Min(T1,T2)); Leave=FMath::Min(Leave,FMath::Max(T1,T2));
        if(Leave<=Enter) return false;
    }
    return Leave>Enter;
}

bool HearthMovement::GridSegmentClear(const FVector& Start,const FVector& End,double CellSize,TFunctionRef<bool(FIntPoint)> CellAllowed)
{
    auto Cell=[CellSize](const FVector& P) { return FIntPoint(FMath::RoundToInt(P.X/CellSize),FMath::RoundToInt(P.Y/CellSize)); };
    if(!CellAllowed(Cell(Start)) || !CellAllowed(Cell(End))) return false;
    TArray<double,TInlineAllocator<32>> Cuts={0,1};
    for(int32 Axis=0;Axis<2;++Axis)
    {
        const double Delta=End[Axis]-Start[Axis]; if(FMath::Abs(Delta)<1e-10) continue;
        const int32 First=FMath::FloorToInt(FMath::Min(Start[Axis],End[Axis])/CellSize-.5)+1;
        const int32 Last=FMath::FloorToInt(FMath::Max(Start[Axis],End[Axis])/CellSize-.5);
        for(int32 I=First;I<=Last;++I) { const double T=((I+.5)*CellSize-Start[Axis])/Delta; if(T>0 && T<1) Cuts.Add(T); }
    }
    Cuts.Sort();
    for(int32 I=1;I<Cuts.Num();++I) if(!CellAllowed(Cell(FMath::Lerp(Start,End,(Cuts[I-1]+Cuts[I])*.5)))) return false;
    return true;
}

bool HearthMovement::SegmentClear(const FVector& Start,const FVector& End,TConstArrayView<FVector> People)
{
    const FVector Delta(End.X-Start.X,End.Y-Start.Y,0);
    const double LengthSquared=Delta.SizeSquared();
    for(const FVector& Person:People)
    {
        const FVector Offset(Person.X-Start.X,Person.Y-Start.Y,0);
        const double Alpha=LengthSquared>UE_SMALL_NUMBER?FMath::Clamp(FVector::DotProduct(Offset,Delta)/LengthSquared,0.,1.):0.;
        const double ClosestSquared=(Offset-Alpha*Delta).SizeSquared();
        if(Offset.SizeSquared()<FMath::Square(Separation)-0.01)
        {
            // Existing overlaps may only move apart; never move through the other person.
            if(ClosestSquared+0.01<Offset.SizeSquared() || FVector::DistSquared2D(End,Person)<=Offset.SizeSquared()+0.01) return false;
        }
        else if(ClosestSquared<FMath::Square(Separation)-0.01) return false;
    }
    return true;
}

bool HearthMovement::FindDetour(const FVector& Start,const FVector& Goal,TConstArrayView<FVector> People,
    TFunctionRef<bool(const FVector&,const FVector&)> ClearGround,TArray<FVector>& Out)
{
    Out.Reset();
    if(!SegmentClear(Goal,Goal,People)) return false;
    TArray<FVector> Nodes={Start,Goal};
    // A bounded visibility graph around nearby villagers. The island grid remains unchanged.
    for(const FVector& Person:People)
    {
        if(FVector::DistSquared2D(Start,Person)>FMath::Square(800.f)) continue;
        // Several distances provide a way around adjacent scenery, not just the
        // tight circle around a person that can itself be cut off by a doorstep.
        for(double Radius:{130.,240.,360.}) for(int32 Side=0;Side<24;++Side)
        {
            if(Nodes.Num()>=128) break;
            const double Angle=Side*UE_DOUBLE_PI/12.;
            const FVector Candidate=FVector(Person.X+Radius*FMath::Cos(Angle),Person.Y+Radius*FMath::Sin(Angle),Start.Z);
            if(SegmentClear(Candidate,Candidate,People) && ClearGround(Candidate,Candidate)) Nodes.Add(Candidate);
        }
        if(Nodes.Num()>=128) break;
    }
    TArray<double> Cost; Cost.Init(DBL_MAX,Nodes.Num()); Cost[0]=0;
    TArray<int32> Parent; Parent.Init(INDEX_NONE,Nodes.Num());
    TArray<bool> Closed; Closed.Init(false,Nodes.Num());
    for(int32 Iteration=0;Iteration<Nodes.Num();++Iteration)
    {
        int32 Best=INDEX_NONE;
        for(int32 I=0;I<Nodes.Num();++I) if(!Closed[I] && Cost[I]<DBL_MAX && (Best==INDEX_NONE || Cost[I]<Cost[Best])) Best=I;
        if(Best==INDEX_NONE) break;
        if(Best==1)
        {
            for(int32 At=1;At!=0;At=Parent[At]) Out.Insert(Nodes[At],0);
            return true;
        }
        Closed[Best]=true;
        for(int32 I=1;I<Nodes.Num();++I) if(!Closed[I])
        {
            const double CandidateCost=Cost[Best]+FVector::Dist2D(Nodes[Best],Nodes[I]);
            if(CandidateCost>=Cost[I] || !SegmentClear(Nodes[Best],Nodes[I],People) || !ClearGround(Nodes[Best],Nodes[I])) continue;
            Cost[I]=CandidateCost; Parent[I]=Best;
        }
    }
    return false;
}

bool AHearthVillage::TryYieldFor(int32 Walker)
{
    const auto& Walking=Residents[Walker]; if(Walking.Route.IsEmpty()) return false;
    for(int32 Other=0;Other<Residents.Num();++Other)
    {
        auto& Idle=Residents[Other];
        if(Other==Walker || Idle.Task!=EHearthTask::LifeChoosing || !Idle.Route.IsEmpty() || IsDecisionPending(Other)
            || !Idle.ConversationId.IsEmpty() || FVector::Dist2D(Idle.Actor->GetActorLocation(),Walking.Actor->GetActorLocation())>220) continue;
        TArray<FVector> People,Destinations;
        for(int32 I=0;I<Residents.Num();++I) if(I!=Other)
        { People.Add(Residents[I].Actor->GetActorLocation()); if(!Residents[I].Route.IsEmpty()) Destinations.Add(Residents[I].Route.Last()); }
        const FVector Start=Idle.Actor->GetActorLocation();
        for(float Radius:{120.f,240.f}) for(int32 Side=0;Side<16;++Side)
        {
            const double Angle=Side*UE_DOUBLE_PI/8.;
            const FVector Target=Start+FVector(Radius*FMath::Cos(Angle),Radius*FMath::Sin(Angle),0);
            if(!HearthMovement::SegmentClear(Start,Target,People) || !HearthMovement::SegmentClear(Target,Target,Destinations)
                || !HearthMovement::SegmentClear(Walking.Actor->GetActorLocation(),Walking.Route[0],TArray<FVector>{Target})) continue;
            if(bUseCropoutMap && !IsClearSegment(Start,Target)) continue;
            bool Land=true; const int32 Steps=FMath::CeilToInt(Radius/40.f);
            for(int32 Step=0;Step<=Steps;++Step) if(!IsLand(FMath::Lerp(Start,Target,static_cast<float>(Step)/Steps))) Land=false;
            if(!Land) continue;
            // A short avoidance step keeps the idle resident's identity and plan;
            // it cannot interrupt a conversation, a job or a paid decision.
            Idle.Route={Target}; Idle.MoveRetry=0; Idle.bMovementBlocked=false;
            Idle.LatestEvent=TEXT("给")+Walking.Name+TEXT("让出通道，再继续自己的安排。");
            return true;
        }
    }
    return false;
}

bool AHearthVillage::MoveResident(int32 Index,float Dt)
{
    auto& R=Residents[Index];
    R.bMovementBlocked=false;
    if(R.Route.IsEmpty()) return true;
    R.MoveRetry=FMath::Max(0.f,R.MoveRetry-Dt);
    if(R.MoveRetry>0) { R.bMovementBlocked=true; return false; }
    TArray<FVector,TInlineAllocator<8>> People;
    for(int32 I=0;I<Residents.Num();++I) if(I!=Index && IsValid(Residents[I].Actor)) People.Add(Residents[I].Actor->GetActorLocation());
    float Budget=Dt*R.MoveSpeed;
    for(int32 Step=0;Step<64 && !R.Route.IsEmpty() && Budget>0;++Step)
    {
        const FVector Start=R.Actor->GetActorLocation(), Delta=R.Route[0]-Start;
        const double Distance=Delta.Size();
        if(Distance<0.01) { R.Route.RemoveAt(0); continue; }
        const FVector Next=Start+Delta*(FMath::Min<double>(Distance,Budget)/Distance);
        if(bUseCropoutMap && !ProductionSites.IsEmpty() && !IsClearSegment(Start,Next))
        {
            TArray<FVector> NewRoute;
            if(FindActivityRoute(Index,R.Route.Last(),NewRoute)) R.Route=MoveTemp(NewRoute);
            R.MoveRetry=.35f+Index*.05f; R.bMovementBlocked=true; return false;
        }
        if(!HearthMovement::SegmentClear(Start,Next,People))
        {
            TArray<FVector> Detour;
            auto ClearGround=[this](const FVector& A,const FVector& B)
            {
                if(bUseCropoutMap)
                {
                    if(!IsClearSegment(A,B)) return false;
                    // Imported scene props can occupy only part of a coarse land cell.
                    // Reject their edges inside the search so it can choose the other
                    // side, rather than repeatedly rejecting the same shortest detour.
                    const int32 Probes=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/40.f));
                    for(int32 Probe=0;Probe<=Probes;++Probe)
                        if(!IsLand(FMath::Lerp(A,B,static_cast<float>(Probe)/Probes))) return false;
                    return true;
                }
                return IsLand(A) && IsLand(B);
            };
            // Skip an occupied intermediate waypoint when the following route node is reachable.
            for(int32 Join=0;Join<FMath::Min(3,R.Route.Num());++Join)
            {
                if(HearthMovement::FindDetour(Start,R.Route[Join],People,ClearGround,Detour))
                {
                    bool OnLand=true;
                    FVector Previous=Start;
                    for(const FVector& Waypoint:Detour)
                    {
                        const int32 Probes=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Previous,Waypoint)/40.f));
                        for(int32 Probe=1;Probe<=Probes;++Probe) if(!IsLand(FMath::Lerp(Previous,Waypoint,static_cast<float>(Probe)/Probes))) OnLand=false;
                        Previous=Waypoint;
                    }
                    if(!OnLand) { Detour.Reset(); continue; }
                    R.Route.RemoveAt(0,Join+1);
                    R.Route.Insert(Detour,0);
                    break;
                }
            }
            if(Detour.IsEmpty())
            {
                TryYieldFor(Index);
                // A finished worker may still be standing at a newly assigned work point.
                TArray<FVector> NewRoute;
                if(bUseCropoutMap && !HearthMovement::SegmentClear(R.Route.Last(),R.Route.Last(),People)
                    && FindActivityRoute(Index,R.Route.Last(),NewRoute)) R.Route=MoveTemp(NewRoute);
                R.MoveRetry=0.35f+Index*0.05f;
                R.bMovementBlocked=true;
                return false;
            }
            continue;
        }
        R.Actor->SetActorRotation(FRotator(0,Delta.Rotation().Yaw,0));
        R.Actor->SetActorLocation(Next);
        Budget-=FMath::Min<double>(Distance,Budget);
        if(Next.Equals(R.Route[0],0.01)) R.Route.RemoveAt(0);
    }
    return R.Route.IsEmpty();
}
