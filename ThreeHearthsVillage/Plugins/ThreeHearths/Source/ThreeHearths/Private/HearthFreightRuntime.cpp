#include "HearthWorldState.h"
#include "HearthFreightVisual.h"
#include "HearthFreightNavigation.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    constexpr int32 Carter=12;
    constexpr float Speed=60.f, StopDuration=5.f;
    FVector Handle(const FHearthFreightOrder& O)
    {
        return O.VehiclePosition+FRotator(0,O.VehicleYaw,0).RotateVector(FVector(330,115,0));
    }
    int32 BeamNeed(const AHearthVillage& V)
    {
        int32 Need=0;
        for(const auto& P:V.PublicProject.Parts) if(P.Status!=TEXT("completed"))
            Need+=FMath::Max(0,P.Required[2]-P.Reserved[2]-P.Delivered[2]);
        for(const auto& R:V.Residents) if((R.Task==EHearthTask::PublicTravel || R.Task==EHearthTask::PublicWork) && R.CargoType==4) Need-=R.CargoAmount;
        for(const auto& O:V.FreightOrders) if(O.Status==TEXT("transporting")) Need-=O.CargoQuantity;
        return FMath::Max(0,Need-V.PublicProject.Stock[2]);
    }
}

bool AHearthVillage::IsFreightPoseSafe(const FVector& P,float Yaw,bool bIncludePeople) const
{
    const auto Reject=[&](const TCHAR* Reason)
    {
        static int32 TraceCount=0;
        if(TraceCount<12 && !FreightOrders.IsEmpty() && FreightOrders.Last().bLoaded
            && FParse::Param(FCommandLine::Get(),TEXT("HearthFreightTrace")))
        { ++TraceCount;UE_LOG(LogTemp,Display,TEXT("FREIGHT_POSE_BLOCKED reason=%s point=%s yaw=%.3f"),Reason,*P.ToString(),Yaw); }
        return false;
    };
    if(P.ContainsNaN() || !FMath::IsFinite(Yaw)) return false;
    const FVector F=FRotator(0,Yaw,0).Vector(),S(-F.Y,F.X,0);
    // Use the authored compound footprint: a wide cart, a narrower horse,
    // and the driver beside it. A cart-width box extending to the horse's
    // nose falsely blocks open space beside troughs and hitching posts.
    struct FPartBox { float X,Y,HalfX,HalfY; };
    const FPartBox Boxes[]={{5,0,125,115},{305,0,175,50},{330,115,50,40}};
    // Planning leaves extra clearance between its 50cm samples. Runtime
    // retains a 20cm physical margin while interpolating between them.
    const float Margin=bIncludePeople?20.f:40.f;
    // BuildFreightVehicleRoute validates every 50cm primitive sample. Add a
    // bounded swept allowance to the per-pose mesh test so a wall cannot fit
    // between samples while the cart turns (the largest compound-box corner
    // moves by roughly 25cm over one planning sample). Runtime ticks move far
    // less than this and use the same conservative envelope.
    const float SweepAllowance=32.f;
    const float H=GroundHeightAt(P);
    for(float Along:{-170.f,530.f})
        if(FMath::Abs(GroundHeightAt(P+F*Along)-H)>FMath::Abs(Along)*.21f+3.f) return Reject(TEXT("longitudinal_slope"));
    if(FMath::Abs(GroundHeightAt(P+S*165.f)-GroundHeightAt(P-S*165.f))>330.f*.17f) return Reject(TEXT("cross_slope"));
    // These authored facilities are presentation components and therefore do
    // not appear in the old point navigation obstacle list.
    TArray<UStaticMeshComponent*> Parts;GetComponents(Parts);
    for(const auto& Box:Boxes)
    {
        const FVector Center=P+F*Box.X+S*Box.Y;
        const float HX=Box.HalfX+Margin,HY=Box.HalfY+Margin;
        for(float Along:{-HX,0.f,HX}) for(float Across:{-HY,0.f,HY})
            if(!IsClearPoint(Center+F*Along+S*Across)) return Reject(TEXT("point"));
        for(float Across:{-HY,0.f,HY})
            if(!IsClearSegment(Center-F*HX+S*Across,Center+F*HX+S*Across)) return Reject(TEXT("segment"));
        // OrganicBlocksPoint intentionally uses the authored walkable floor
        // cells. Those cells are a useful pedestrian obstacle model, but the
        // rendered walls, skirts, stairs, and roof attachments can extend
        // beyond a cell by more than a wagon's clearance. Use the actual
        // currently rendered components for freight only. Each layer is
        // tested separately, preserving courtyards and other gaps in a
        // compound home instead of wrapping the whole recipe in one box.
        for(int32 Plot=0; Plot<HousingPlotCount(); ++Plot)
        {
            for(const TWeakObjectPtr<UStaticMeshComponent>& WeakMesh:StarterArchitectureMeshes[Plot])
            {
                UStaticMeshComponent* Mesh=WeakMesh.Get();
                // The array contains only installed home layers. Keep hidden
                // state out of this safety test: a hidden collision/render
                // toggle must not make a physical building disappear from
                // freight planning until its component is actually removed.
                if(!IsValid(Mesh) || !Mesh->IsRegistered()) continue;
                const FVector MeshCenter=Mesh->Bounds.Origin;
                const FVector MeshExtent=Mesh->Bounds.BoxExtent;
                if(!FMath::IsFinite(MeshCenter.X) || !FMath::IsFinite(MeshCenter.Y)
                    || !FMath::IsFinite(MeshExtent.X) || !FMath::IsFinite(MeshExtent.Y)) continue;
                // Bounds are world axis aligned. Project them onto the
                // vehicle's local forward/side axes and perform a 2D SAT
                // overlap against this footprint box.
                const FVector Delta=MeshCenter-Center;
                const float MeshAlong=MeshExtent.X*FMath::Abs(F.X)+MeshExtent.Y*FMath::Abs(F.Y);
                const float MeshAcross=MeshExtent.X*FMath::Abs(S.X)+MeshExtent.Y*FMath::Abs(S.Y);
                const float MeshHX=HX+SweepAllowance,MeshHY=HY+SweepAllowance;
                if(FMath::Abs(Delta.X)<=MeshHX*FMath::Abs(F.X)+MeshHY*FMath::Abs(S.X)+MeshExtent.X
                    && FMath::Abs(Delta.Y)<=MeshHX*FMath::Abs(F.Y)+MeshHY*FMath::Abs(S.Y)+MeshExtent.Y
                    && FMath::Abs(FVector::DotProduct(Delta,F))<=MeshHX+MeshAlong
                    && FMath::Abs(FVector::DotProduct(Delta,S))<=MeshHY+MeshAcross)
                {
                    static int32 MeshTraceCount=0;
                    if(MeshTraceCount<6 && FParse::Param(FCommandLine::Get(),TEXT("HearthFreightTrace")))
                    {
                        ++MeshTraceCount;
                        UE_LOG(LogTemp,Display,TEXT("FREIGHT_BLOCKING_MESH asset=%s center=%s extent=%s vehicle=%s part_x=%.0f ground=%.2f"),
                            *GetPathNameSafe(Mesh->GetStaticMesh()),*MeshCenter.ToString(),*MeshExtent.ToString(),*P.ToString(),Box.X,H);
                    }
                    return Reject(TEXT("organic_mesh"));
                }
            }
        }
        for(auto* Part:Parts)
        {
            if(!IsValid(Part) || !Part->ComponentHasTag(TEXT("HearthMedievalVisuals"))) continue;
            const FVector C=Part->Bounds.Origin,E=Part->Bounds.BoxExtent,D=C-Center;
            const bool bOverlap=FMath::Abs(D.X)<=HX*FMath::Abs(F.X)+HY*FMath::Abs(S.X)+E.X
                && FMath::Abs(D.Y)<=HX*FMath::Abs(F.Y)+HY*FMath::Abs(S.Y)+E.Y
                && FMath::Abs(FVector::DotProduct(D,F))<=HX+E.X*FMath::Abs(F.X)+E.Y*FMath::Abs(F.Y)
                && FMath::Abs(FVector::DotProduct(D,S))<=HY+E.X*FMath::Abs(S.X)+E.Y*FMath::Abs(S.Y);
            if(bOverlap) return Reject(TEXT("public_facility"));
        }
        if(bIncludePeople) for(int32 I=0;I<Residents.Num();++I)
        {
            if(I==Carter || !IsValid(Residents[I].Actor)) continue;
            const FVector D=Residents[I].Actor->GetActorLocation()-Center;
            if(FMath::Abs(FVector::DotProduct(D,F))<HX+35.f && FMath::Abs(FVector::DotProduct(D,S))<HY+35.f) return Reject(*Residents[I].Name);
        }
    }
    return true;
}

bool AHearthVillage::BuildFreightVehicleRoute(const FVector& Start,float Yaw,const FVector& Goal,TArray<FVector>& Points,TArray<float>& Yaws) const
{
    Points.Reset();Yaws.Reset();
    TArray<HearthFreightNavigation::FPose> Poses;
    if(!HearthFreightNavigation::Plan({Start,Yaw},Goal,
        [this](const HearthFreightNavigation::FPose& P){return IsFreightPoseSafe(P.Position,P.Yaw,false);},Poses,6000)) return false;
    if(Poses.Num()>2049) return false;
    for(int32 I=1;I<Poses.Num();++I)
    {
        FVector P=Poses[I].Position;P.Z=GroundHeightAt(P)+5.2f;
        Points.Add(P);Yaws.Add(Poses[I].Yaw);
    }
    return true;
}

bool AHearthVillage::StartFreightOrder()
{
    if(!IsOrganicVillage() || !Residents.IsValidIndex(Carter) || !IsValid(Residents[Carter].Actor)) return false;
    auto& R=Residents[Carter];
    if(R.ServiceRoleKey!=TEXT("carter") || R.Task!=EHearthTask::LifeChoosing || !R.ActiveTaskId.IsEmpty()
        || Elapsed<R.NextLifeDecision || PublicProject.Status!=TEXT("building")
        || !ProductionSites.IsValidIndex(PublicProject.Site) || R.Hunger>=60.f || R.Energy<=35.f) return false;
    if(FreightOrders.ContainsByPredicate([](const auto& O){return O.Status==TEXT("transporting");})) return false;
    const int32 Quantity=FMath::Min(6,FMath::Min(BeamStock,BeamNeed(*this)));
    if(Quantity<=0) return false;
    // Pick a loading bay adjacent to the real central warehouse. The fenced
    // stable is a separate facility; it never becomes an inventory source.
    FVector Source;float InitialYaw=0;bool bSource=false;
    for(const FVector Offset:{FVector(0,-400,0),FVector(-450,0,0),FVector(0,400,0),FVector(450,0,0),FVector(-450,450,0),FVector(-600,-100,0)})
    {
        for(float Angle:{0.f,90.f,-90.f,180.f})
        {
            FVector P=FVector(-1650,-1050,0)+Offset;P.Z=GroundHeightAt(P)+5.2f;
            if(IsFreightPoseSafe(P,Angle,false) && IsFreightPoseSafe(P,Angle,true)) { Source=P;InitialYaw=Angle;bSource=true;break; }
        }
        if(bSource) break;
    }
    if(!bSource) { R.NextLifeDecision=Elapsed+120;return false; }
    FVector Position=Source;float Yaw=InitialYaw;float Distance=0;
    for(int32 I=FreightOrders.Num()-1;I>=0;--I) if(FreightOrders[I].bVehicleKnown)
    { Position=FreightOrders[I].VehiclePosition;Yaw=FreightOrders[I].VehicleYaw;Distance=FreightOrders[I].WheelDistanceCm;break; }
    const auto& Site=ProductionSites[PublicProject.Site];
    // The first public keep reserves a large square. Its historical pedestrian
    // approach can lie at the far end of an entire wall; choose the nearest
    // exterior loading edge instead of forcing a wagon across the island.
    const FVector Edge(FMath::Clamp(Source.X,Site.Position.X-Site.Radius,Site.Position.X+Site.Radius),
        FMath::Clamp(Source.Y,Site.Position.Y-Site.Radius,Site.Position.Y+Site.Radius),0);
    FVector Away=(Source-Edge).GetSafeNormal2D();if(Away.IsNearlyZero()) Away=FVector(-1,0,0);
    FVector Destination;bool bDestination=false;
    // Stop outside the construction footprint. Delivery is to its loading
    // edge, never a route through the keep's interior or narrow gate.
    for(float Extension:{350.f,650.f,950.f})
    {
        const FVector Candidate=Edge+Away*Extension;
        for(float Angle:{0.f,90.f,-90.f,180.f}) if(IsFreightPoseSafe(Candidate,Angle,false))
        { Destination=Candidate;Destination.Z=GroundHeightAt(Candidate)+5.2f;bDestination=true;break; }
        if(bDestination) break;
    }
    if(!bDestination) { R.NextLifeDecision=Elapsed+120;return false; }
    FHearthFreightOrder O;O.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);O.ProjectId=PublicProject.Id;
    O.Carter=Carter;O.CargoType=4;O.CargoQuantity=Quantity;O.Source=Source;O.Destination=Destination;
    O.VehiclePosition=Position;O.VehicleYaw=Yaw;O.WheelDistanceCm=Distance;O.bVehicleKnown=true;O.CreatedAt=Elapsed;
    TArray<FVector> Walk;
    if(!FindActivityRoute(Carter,Handle(O),Walk)) { R.NextLifeDecision=Elapsed+120;return false; }
    if(!ReserveWage(Carter,O.Id,3,GeneralFunds()<3 && TaxProjectCoins>=3,-1)) { R.NextLifeDecision=Elapsed+120;return false; }
    if(R.Coins==0 && R.Hunger>=35.f)
    {
        if(!SettleWage(Carter,O.Id)) { CancelWage(O.Id);return false; } O.bWagePaid=true;
    }
    BeamStock-=Quantity;PublicProject.Grants[2]+=Quantity;FreightOrders.Add(O);
    R.ActiveTaskId=O.Id;R.Task=EHearthTask::LifeTravel;R.LifeAction=62;R.MoveSpeed=90;R.Route=MoveTemp(Walk);
    R.Timer=0;R.DecisionSource=TEXT("freight_local");R.LatestEvent=TEXT("先走到停着的马车，再把已预留的房梁运到公共工程。");
    StartHistory(Carter,true,TEXT("freight_local"));AcceptHistory(Carter,R.LatestEvent,TEXT("公共工程真实房梁缺口"),TEXT("freight_local"));
    UE_LOG(LogTemp,Display,TEXT("FREIGHT_RESERVED id=%s beams=%d source=%s destination=%s"),*O.Id,Quantity,*Source.ToString(),*Destination.ToString());
    return true;
}

bool AHearthVillage::CancelFreightOrder(int32 Index)
{
    if(!Residents.IsValidIndex(Index)) return false;
    auto* O=FreightOrders.FindByPredicate([Index](const auto& V){return V.Carter==Index && V.Status==TEXT("transporting");});
    if(!O || O->bLoaded) return false;
    if(!O->bWagePaid && !CancelWage(O->Id)) return false;
    BeamStock+=O->CargoQuantity;PublicProject.Grants[2]-=O->CargoQuantity;
    O->Status=TEXT("cancelled");O->Phase=0;O->StopRemaining=0;O->bCarterAttached=false;O->bPausedForNeeds=false;O->VehicleRoute.Reset();O->VehicleRouteYaws.Reset();
    auto& R=Residents[Index];R.Task=EHearthTask::LifeChoosing;R.ActiveTaskId.Empty();R.Route.Reset();R.Timer=0;R.LifeAction=0;R.MoveSpeed=90;R.NextLifeDecision=Elapsed+120;
    R.LatestEvent=TEXT("取消尚未装车的房梁预留，马车留在原地。");CompleteHistory(Index,R.LatestEvent);return true;
}

void AHearthVillage::AdvanceFreightResident(int32 Index,float Dt)
{
    if(Index!=Carter || !Residents.IsValidIndex(Index) || !IsValid(Residents[Index].Actor)) return;
    auto& R=Residents[Index];
    auto* O=FreightOrders.FindByPredicate([&R](const auto& V){return V.Id==R.ActiveTaskId && V.Status==TEXT("transporting");});
    if(!O) return;
    if(R.Hunger>=65.f || R.Energy<=35.f)
    {
        if(!O->bLoaded) { CancelFreightOrder(Index);return; }
        // Keep the loaded vehicle parked; the same worker buys real food or
        // rests, then walks back. No return of distant cargo to the depot.
        if(R.Coins==0 && !O->bWagePaid && SettleWage(Index,O->Id)) O->bWagePaid=true;
        O->bPausedForNeeds=true;O->bCarterAttached=false;
        R.ActiveTaskId.Empty();R.Task=EHearthTask::LifeChoosing;R.Route.Reset();R.Timer=0;R.NextLifeDecision=Elapsed;return;
    }
    if(!O->bCarterAttached)
    {
        R.MoveSpeed=90.f;
        if(!MoveResident(Index,Dt)) return;
        FVector Target=Handle(*O);Target.Z=GroundHeightAt(Target)+5.2f;
        if(FVector::Dist2D(R.Actor->GetActorLocation(),Target)>35.f)
        {
            if(IsClearSegment(R.Actor->GetActorLocation(),Target)) R.Route={Target};
            else if(Elapsed>=O->NextRouteAttempt) { FindActivityRoute(Index,Target,R.Route);O->NextRouteAttempt=Elapsed+15; }
            return;
        }
        // Only a short final walking connector is allowed; attaching never
        // changes the vehicle's parked position or odometer.
        R.Actor->SetActorLocation(Target);O->bCarterAttached=true;R.MoveSpeed=Speed;R.Route.Reset();
    }
    if(O->Phase==1 || O->Phase==3)
    {
        R.Task=EHearthTask::LifeActivity;O->StopRemaining=FMath::Max(0.f,O->StopRemaining-Dt);
        if(O->StopRemaining>0) return;
        if(O->Phase==1)
        {
            O->bLoaded=true;O->Phase=2;O->NextRouteAttempt=0;
            R.LatestEvent=FString::Printf(TEXT("已逐根装好%d根房梁，准备驶往公共工程。"),O->CargoQuantity);
            UE_LOG(LogTemp,Display,TEXT("FREIGHT_LOADED id=%s beams=%d"),*O->Id,O->CargoQuantity);
        }
        else
        {
            if(!O->bWagePaid && !SettleWage(Index,O->Id)) return;
            O->bWagePaid=true;PublicProject.Stock[2]+=O->CargoQuantity;O->Status=TEXT("completed");O->bLoaded=false;O->bCarterAttached=false;
            O->VehicleRoute.Reset();O->VehicleRouteYaws.Reset();
            R.LatestEvent=FString::Printf(TEXT("已卸下%d根房梁，公共工程入库与工资各结算一次。"),O->CargoQuantity);CompleteHistory(Index,R.LatestEvent);
            UE_LOG(LogTemp,Display,TEXT("FREIGHT_DELIVERED id=%s beams=%d distance_cm=%.1f"),*O->Id,O->CargoQuantity,O->WheelDistanceCm);
            R.ActiveTaskId.Empty();R.Task=EHearthTask::LifeChoosing;R.LifeAction=0;R.Route.Reset();R.MoveSpeed=90;R.NextLifeDecision=Elapsed+180;return;
        }
    }
    const FVector Goal=O->Phase==0?O->Source:O->Destination;
    if(O->VehicleRoute.IsEmpty())
    {
        if(FVector::Dist2D(O->VehiclePosition,Goal)<=150.f)
        {
            O->Phase=O->Phase==0?1:3;O->StopRemaining=StopDuration;R.Task=EHearthTask::LifeActivity;return;
        }
        R.Task=EHearthTask::LifeActivity;
        if(Elapsed<O->NextRouteAttempt) return;
        O->NextRouteAttempt=Elapsed+30;
        if(!BuildFreightVehicleRoute(O->VehiclePosition,O->VehicleYaw,Goal,O->VehicleRoute,O->VehicleRouteYaws))
        {
            R.LatestEvent=TEXT("前方没有适合马车的通路，保留货物并停靠等待。");
            UE_LOG(LogTemp,Display,TEXT("FREIGHT_ROUTE_WAIT id=%s phase=%d from=%s goal=%s"),*O->Id,O->Phase,*O->VehiclePosition.ToString(),*Goal.ToString());return;
        }
        UE_LOG(LogTemp,Display,TEXT("FREIGHT_ROUTE_READY id=%s phase=%d points=%d"),*O->Id,O->Phase,O->VehicleRoute.Num());
    }
    if(O->VehicleRoute.Num()!=O->VehicleRouteYaws.Num()) { O->VehicleRoute.Reset();O->VehicleRouteYaws.Reset();return; }
    if(O->VehicleRoute.IsEmpty()) return;
    const FVector Before=O->VehiclePosition,Target=O->VehicleRoute[0];
    const float Remaining=FVector::Dist2D(Before,Target),Travel=FMath::Min(Speed*FMath::Clamp(Dt,0.f,.25f),Remaining);
    const float T=Remaining>.001f?Travel/Remaining:1.f;
    FVector Next=FMath::Lerp(Before,Target,T);Next.Z=GroundHeightAt(Next)+5.2f;
    const float NextYaw=FRotator::NormalizeAxis(O->VehicleYaw+FMath::FindDeltaAngleDegrees(O->VehicleYaw,O->VehicleRouteYaws[0])*T);
    if(!IsFreightPoseSafe(Next,NextYaw,true))
    {
        R.Task=EHearthTask::LifeActivity;
        if(Elapsed>=O->NextRouteAttempt) { O->VehicleRoute.Reset();O->VehicleRouteYaws.Reset();O->NextRouteAttempt=Elapsed+30; }
        return;
    }
    O->VehiclePosition=Next;O->VehicleYaw=NextYaw;O->WheelDistanceCm+=Travel;O->LastVehicleMoveAt=Elapsed;
    FVector Person=Handle(*O);Person.Z=GroundHeightAt(Person)+5.2f;
    R.Actor->SetActorLocationAndRotation(Person,FRotator(0,NextYaw,0));R.Task=EHearthTask::LifeTravel;R.MoveSpeed=Speed;R.bMovementBlocked=false;
    if(Remaining<=Travel+.01f) { O->VehicleRoute.RemoveAt(0);O->VehicleRouteYaws.RemoveAt(0); }
}

void AHearthVillage::AdvanceFreightRuntime(float Dt)
{
    if(!IsOrganicVillage() || !Residents.IsValidIndex(Carter)) return;
    auto& R=Residents[Carter];
    auto* O=FreightOrders.FindByPredicate([](const auto& V){return V.Status==TEXT("transporting");});
    if(O)
    {
        if(!O->bPausedForNeeds || R.Task!=EHearthTask::LifeChoosing || !R.ActiveTaskId.IsEmpty() || Elapsed<R.NextLifeDecision) return;
        if(R.Hunger>=35.f && R.Coins>0 && FoodStock>0) { StartLifeAction(Carter,50,TEXT("把马车停好，先购买食物"),false);return; }
        if(R.Energy<=45.f) { StartLifeAction(Carter,0,TEXT("把马车停好，休息后继续运输"),false);return; }
        if(R.Hunger>=65.f) { R.NextLifeDecision=Elapsed+30;return; }
        TArray<FVector> Route;
        if(!FindActivityRoute(Carter,Handle(*O),Route)) { R.NextLifeDecision=Elapsed+30;return; }
        O->bPausedForNeeds=false;O->bCarterAttached=false;
        R.ActiveTaskId=O->Id;R.LifeAction=62;R.Task=EHearthTask::LifeTravel;R.Timer=0;R.MoveSpeed=90;R.Route=MoveTemp(Route);return;
    }
    if(!bAutonomousLifeEnabled || R.Task!=EHearthTask::LifeChoosing || !R.ActiveTaskId.IsEmpty() || Elapsed<R.NextLifeDecision) return;
    if(R.Hunger>=35.f && R.Coins>0 && FoodStock>0) { StartLifeAction(Carter,50,TEXT("装运前购买食物"),false);return; }
    if(R.Energy<=35.f) { StartLifeAction(Carter,0,TEXT("运输前休息"),false);return; }
    StartFreightOrder();
}

FHearthFreightVisualState AHearthVillage::GetFreightVisualState() const
{
    FHearthFreightVisualState Out;Out.ElapsedSeconds=Elapsed;
    const FHearthFreightOrder* Order=nullptr;
    for(int32 I=FreightOrders.Num()-1;I>=0;--I) if(FreightOrders[I].bVehicleKnown) { Order=&FreightOrders[I];break; }
    if(!Order) return Out;
    Out.bEnabled=true;Out.bMoving=Order->Status==TEXT("transporting") && Order->bCarterAttached && !Order->bPausedForNeeds
        && (Order->Phase==0 || Order->Phase==2) && Elapsed-Order->LastVehicleMoveAt<.11f && !bSimulationPaused;
    Out.CartPosition=Order->VehiclePosition;Out.CartYaw=Order->VehicleYaw;Out.WheelDistanceCm=Order->WheelDistanceCm;
    Out.CargoType=Order->bLoaded?4:-1;Out.CargoQuantity=Order->bLoaded?Order->CargoQuantity:0;
    Out.SimulationRate=bSimulationPaused?0:SimulationSpeed;return Out;
}
