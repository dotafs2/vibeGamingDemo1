#include "HearthTavernRuntime.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#include <initializer_list>

namespace HearthTavernRuntimeDetail
{
    TMap<FString, FHearthTavernRuntimeState> LiveStates;
    TMap<FString, FHearthTavernRuntimeState*> BoundStates;

    FHearthTavernRuntimeState& StateFor(const FString& WorldId)
    {
        if(FHearthTavernRuntimeState** Bound=BoundStates.Find(WorldId)) return **Bound;
        return LiveStates.FindOrAdd(WorldId);
    }

    bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Terms)
    {
        for (const TCHAR* Term : Terms)
            if (Text.Contains(Term, ESearchCase::IgnoreCase)) return true;
        return false;
    }

    FString HostId(const FString& PlanId)
    {
        const int32 Split = PlanId.Find(TEXT(":tavern_canopy_v1"));
        return Split == INDEX_NONE ? PlanId : PlanId.Left(Split);
    }

    int32 ActionFor(int32 VenueIndex, int32 SeatIndex)
    { return HearthTavernRuntime::SeatActionBase + VenueIndex * HearthTavernRuntime::SeatActionStride + SeatIndex; }

    bool DecodeAction(int32 Action, int32& VenueIndex, int32& SeatIndex)
    {
        if (!HearthTavernRuntime::IsSeatAction(Action)) return false;
        const int32 Relative = Action - HearthTavernRuntime::SeatActionBase;
        VenueIndex = Relative / HearthTavernRuntime::SeatActionStride;
        SeatIndex = Relative % HearthTavernRuntime::SeatActionStride;
        return true;
    }

    FHearthTavernSeatState* FindSeat(FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (FHearthTavernVenueState& Venue : State.Venues)
            if (FHearthTavernSeatState* Seat = Venue.Seats.FindByPredicate([&](FHearthTavernSeatState& Candidate){ return Candidate.SeatId == SeatId; })) return Seat;
        return nullptr;
    }

    FHearthTavernVenueState* FindVenueForSeat(FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (FHearthTavernVenueState& Venue : State.Venues)
            if (Venue.Seats.ContainsByPredicate([&](const FHearthTavernSeatState& Seat){ return Seat.SeatId == SeatId; })) return &Venue;
        return nullptr;
    }

}

namespace HearthTavernRuntime
{
    bool IsCanopyNeed(const FString& Text)
    {
        const bool bVenue=HearthTavernRuntimeDetail::ContainsAny(Text,{TEXT("tavern"),TEXT("inn"),TEXT("酒馆"),TEXT("旅店"),TEXT("客栈")});
        const bool bCanopy=HearthTavernRuntimeDetail::ContainsAny(Text,{TEXT("canopy"),TEXT("awning"),TEXT("棚"),TEXT("顶棚"),TEXT("长凳"),TEXT("座位"),TEXT("seating"),TEXT("gathering"),TEXT("接待"),TEXT("聚会"),TEXT("tavern_canopy_seating")});
        return (bVenue&&bCanopy)||Text.Contains(TEXT("tavern_canopy_seating"),ESearchCase::IgnoreCase);
    }

    bool IsTavernPlan(const FHearthStructurePlan& Plan)
    {
        if(Plan.PlanId.Contains(TEXT(":tavern_canopy_v1"),ESearchCase::IgnoreCase)||Plan.Reasons.Need.Contains(TEXT("tavern_canopy"),ESearchCase::IgnoreCase)) return true;
        return Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){return C.ExtensionId.Contains(TEXT("tavern_canopy"),ESearchCase::IgnoreCase);});
    }

    FString MakeVenueId(const FString& HostSiteStableId,const FString& HostPlanId)
    { return (!HostSiteStableId.IsEmpty()?HostSiteStableId:HostPlanId)+TEXT(":tavern_venue_v1"); }

    FString MakeAttachedPlanId(const FString& HostSiteStableId,const FString& HostPlanId)
    { return (!HostSiteStableId.IsEmpty()?HostSiteStableId:HostPlanId)+TEXT(":tavern_canopy_v1"); }

    bool ProposeNeed(TArray<FHearthWorldRequest>& Requests,const FString& OwnerResidentId,const FString& HostSiteStableId,const FString& HostPlanId,FString& Error)
    {
        if(OwnerResidentId.IsEmpty()||(HostSiteStableId.IsEmpty()&&HostPlanId.IsEmpty())){Error=TEXT("tavern need requires an owner and stable host id");return false;}
        const FString Need=FString::Printf(TEXT("看见：自家酒馆已有建筑；未知：棚和座位能否安全接待；打算：在自家旁加户外棚与至少两张木长凳。 [tavern_canopy_seating host_site:%s host_plan:%s]"),*HostSiteStableId,*HostPlanId);
        return HearthWorldRequests::Submit(Requests,OwnerResidentId,Need,Error);
    }
    bool RequestTavernCanopy(FHearthResident& Resident,const FHearthSite& HostSite,TArray<FHearthWorldRequest>& Requests,FString& Error)
    {
        const bool bAccepted=ProposeNeed(Requests,Resident.StableId,HostSite.StableId,HostSite.BuildPlanId,Error);
        if(bAccepted) Resident.DesignRequest=TEXT("tavern_canopy_seating | native owner request");
        return bAccepted;
    }

    void Reconcile(FHearthTavernRuntimeState& State,const TArray<FHearthWorldRequest>& Requests,const TArray<FHearthSite>& Sites,const TArray<FHearthStructurePlan>& Plans)
    {
        TSet<FString> Seen;
        for(const FHearthStructurePlan& Plan:Plans)
        {
            if(!IsTavernPlan(Plan)) continue;
            const int32 SiteIndex=Sites.IndexOfByPredicate([&](const FHearthSite& S){return S.BuildPlanId==Plan.PlanId;});
            const FHearthSite* Site=Sites.IsValidIndex(SiteIndex)?&Sites[SiteIndex]:nullptr;
            const FString HostPlanId=HearthTavernRuntimeDetail::HostId(Plan.PlanId);
            const FString HostSiteId=Site?Site->StableId:HostPlanId;
            const FString VenueId=MakeVenueId(HostSiteId,HostPlanId); Seen.Add(VenueId);
            FHearthTavernVenueState* Venue=State.Venues.FindByPredicate([&](FHearthTavernVenueState& V){return V.VenueId==VenueId;});
            if(!Venue){FHearthTavernVenueState V;V.VenueId=VenueId;V.HostSiteStableId=HostSiteId;V.HostPlanId=HostPlanId;V.PlanId=Plan.PlanId;State.Venues.Add(MoveTemp(V));Venue=&State.Venues.Last();}
            Venue->HostSiteStableId=HostSiteId;Venue->HostPlanId=HostPlanId;Venue->PlanId=Plan.PlanId;
            for(const FHearthWorldRequest& Request:Requests) if(IsCanopyNeed(Request.Summary)&&Request.Summary.Contains(HostSiteId))
            {Venue->RequestId=Request.Id;if(Venue->OwnerResidentId.IsEmpty()&&!Request.RequesterIds.IsEmpty())Venue->OwnerResidentId=Request.RequesterIds[0];break;}
            bool bAllExtensionComponentsCompleted=Site!=nullptr;
            int32 ExtensionComponentCount=0;
            for(const FHearthStructureComponent& PlanComponent:Plan.Components)
            {
                const bool bExtension=PlanComponent.ExtensionId.Contains(TEXT("tavern_canopy"),ESearchCase::IgnoreCase)
                    || Plan.PlanId.Contains(TEXT(":tavern_canopy_v1"),ESearchCase::IgnoreCase);
                if(!bExtension) continue;
                ++ExtensionComponentCount;
                if(!Site || !Site->CottageComponents.ContainsByPredicate([&](const FHearthCottageComponent& Part)
                    { return Part.Id==PlanComponent.Id && Part.Status==TEXT("completed"); })) bAllExtensionComponentsCompleted=false;
            }
            if(ExtensionComponentCount==0) bAllExtensionComponentsCompleted=false;
            Venue->Status=Site?TEXT("building"):TEXT("planned");
            TArray<FHearthTavernSeatState> Seats;
            if(Site) for(const FHearthCottageComponent& Part:Site->CottageComponents) if(Part.AssetId==TEXT("bench_timber")&&Part.Status==TEXT("completed"))
            {
                const FHearthStructureComponent* PlanPart=Plan.Components.FindByPredicate([&](const FHearthStructureComponent& Candidate){return Candidate.Id==Part.Id;});
                if(!PlanPart) continue;
                const FVector SeatAnchor=Plan.Footprint.Origin+Plan.Footprint.Orientation.RotateVector(PlanPart->Offset);
                FHearthTavernSeatState Seat;Seat.SeatId=VenueId+TEXT(":seat:")+Part.Id;Seat.VenueId=VenueId;Seat.WorldPosition=SeatAnchor;
                if(const FHearthTavernSeatState* Previous=Venue->Seats.FindByPredicate([&](const FHearthTavernSeatState& Existing){return Existing.SeatId==Seat.SeatId;})) Seat=*Previous;
                Seat.WorldPosition=SeatAnchor;Seats.Add(MoveTemp(Seat));
            }
            Venue->Seats=MoveTemp(Seats);
            if(bAllExtensionComponentsCompleted && Venue->Seats.Num()>=2) Venue->Status=TEXT("usable");
        }
        for(FHearthTavernVenueState& Venue:State.Venues) if(!Seen.Contains(Venue.VenueId)&&Venue.Status==TEXT("usable")) Venue.Status=TEXT("planned");
    }

    int32 FindVenue(const FHearthTavernRuntimeState& State,const FString& VenueId)
    {return State.Venues.IndexOfByPredicate([&](const FHearthTavernVenueState& V){return V.VenueId==VenueId;});}
    int32 FindVenueForOwner(const FHearthTavernRuntimeState& State,const FString& OwnerResidentId)
    {return State.Venues.IndexOfByPredicate([&](const FHearthTavernVenueState& V){return V.OwnerResidentId==OwnerResidentId;});}
    bool IsSeatAction(int32 Action){return Action>=SeatActionBase&&Action<SeatActionBase+4096;}
    bool IsLegacyBuildAction(int32 Action){return Action>=LegacyBuildActionBase&&Action<LegacyBuildActionBase+HearthVillageLimits::MaxPopulation;}
    int32 MakeLegacyBuildAction(int32 ResidentIndex){return ResidentIndex>=0&&ResidentIndex<HearthVillageLimits::MaxPopulation?LegacyBuildActionBase+ResidentIndex:-1;}
    int32 LegacyBuildResident(int32 Action){return IsLegacyBuildAction(Action)?Action-LegacyBuildActionBase:-1;}

    TArray<int32> AvailableSeatActions(const FHearthTavernRuntimeState& State,const FString& ResidentId)
    {
        TArray<int32> Result;if(ResidentId.IsEmpty())return Result;
        for(int32 V=0;V<State.Venues.Num();++V) if(State.Venues[V].Status==TEXT("usable")) for(int32 S=0;S<State.Venues[V].Seats.Num()&&S<SeatActionStride;++S) if(State.Venues[V].Seats[S].Status==TEXT("free"))Result.Add(HearthTavernRuntimeDetail::ActionFor(V,S));
        return Result;
    }

    FString ActionName(const FHearthTavernRuntimeState& State,int32 Action,const FString& RequesterResidentId)
    {
        int32 V=-1,S=-1;if(!HearthTavernRuntimeDetail::DecodeAction(Action,V,S)||!State.Venues.IsValidIndex(V)||!State.Venues[V].Seats.IsValidIndex(S))return TEXT("未知酒馆座位");
        const auto& Venue=State.Venues[V];
        const TCHAR* Place=(!RequesterResidentId.IsEmpty()&&RequesterResidentId==Venue.OwnerResidentId)?TEXT("你的酒馆"):TEXT("酒馆");
        return FString::Printf(TEXT("去%s户外棚下会面/休息（位置 %d，真实走到可用点并记录使用）"),Place,S+1);
    }

    bool ReserveSeat(FHearthTavernRuntimeState& State,int32 Action,const FString& ResidentId,FVector& OutPosition,FString& OutSeatId)
    {int32 V=-1,S=-1;if(!HearthTavernRuntimeDetail::DecodeAction(Action,V,S)||!State.Venues.IsValidIndex(V))return false;auto& Venue=State.Venues[V];if(Venue.Status!=TEXT("usable")||!Venue.Seats.IsValidIndex(S))return false;auto& Seat=Venue.Seats[S];if(Seat.Status!=TEXT("free")||ResidentId.IsEmpty())return false;Seat.Status=TEXT("reserved");Seat.OccupantResidentId=ResidentId;OutPosition=Seat.WorldPosition;OutSeatId=Seat.SeatId;return true;}

    bool MarkArrived(FHearthTavernRuntimeState& State,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId)
    {auto* Seat=HearthTavernRuntimeDetail::FindSeat(State,SeatId);auto* Venue=HearthTavernRuntimeDetail::FindVenueForSeat(State,SeatId);if(!Seat||!Venue||Seat->Status!=TEXT("reserved")||Seat->OccupantResidentId!=ResidentId)return false;if(State.UseEvents.Num()>=MaxUseEvents){const int32 Oldest=State.UseEvents.IndexOfByPredicate([](const FHearthTavernUseEvent& E){return E.Status==TEXT("completed");});if(Oldest==INDEX_NONE)return false;State.UseEvents.RemoveAt(Oldest);};FHearthTavernUseEvent Event;Event.EventId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);Event.VenueId=Venue->VenueId;Event.SeatId=SeatId;Event.ResidentId=ResidentId;Event.StartedAt=SimulationTime;State.UseEvents.Add(Event);Seat->Status=TEXT("occupied");Seat->ActiveUseEventId=Event.EventId;Venue->LastUseEventId=Event.EventId;if(!Venue->UniqueVisitorIds.Contains(ResidentId)&&Venue->UniqueVisitorIds.Num()<MaxUniqueVisitors)Venue->UniqueVisitorIds.Add(ResidentId);OutEventId=Event.EventId;return true;}

    bool ReleaseSeat(FHearthTavernRuntimeState& State,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId)
    {auto* Seat=HearthTavernRuntimeDetail::FindSeat(State,SeatId);auto* Venue=HearthTavernRuntimeDetail::FindVenueForSeat(State,SeatId);if(!Seat||!Venue||Seat->OccupantResidentId!=ResidentId)return false;auto* Event=State.UseEvents.FindByPredicate([&](FHearthTavernUseEvent& E){return E.EventId==Seat->ActiveUseEventId&&E.Status==TEXT("active");});if(!Event)return false;Event->EndedAt=SimulationTime;Event->Status=TEXT("completed");OutEventId=Event->EventId;Seat->Status=TEXT("free");Seat->OccupantResidentId.Empty();Seat->ActiveUseEventId.Empty();++Venue->UseCount;return true;}
    bool CancelReservation(FHearthTavernRuntimeState& State,const FString& SeatId,const FString& ResidentId)
    {auto* Seat=HearthTavernRuntimeDetail::FindSeat(State,SeatId);if(!Seat||Seat->Status!=TEXT("reserved")||Seat->OccupantResidentId!=ResidentId)return false;Seat->Status=TEXT("free");Seat->OccupantResidentId.Empty();return true;}
    FString SeatForResident(const FHearthTavernRuntimeState& State,const FString& ResidentId)
    {for(const auto& Venue:State.Venues)for(const auto& Seat:Venue.Seats)if(Seat.OccupantResidentId==ResidentId&&(Seat.Status==TEXT("reserved")||Seat.Status==TEXT("occupied")))return Seat.SeatId;return FString();}

    FString ExportReadOnly(const FHearthTavernRuntimeState& State)
    {
        auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("version"),1);TArray<TSharedPtr<FJsonValue>> Venues,Events;
        for(const auto& V:State.Venues){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("venue_id"),V.VenueId);J->SetStringField(TEXT("owner_resident_id"),V.OwnerResidentId);J->SetStringField(TEXT("host_site_stable_id"),V.HostSiteStableId);J->SetStringField(TEXT("host_plan_id"),V.HostPlanId);J->SetStringField(TEXT("plan_id"),V.PlanId);J->SetStringField(TEXT("request_id"),V.RequestId);J->SetStringField(TEXT("status"),V.Status);J->SetStringField(TEXT("last_use_event_id"),V.LastUseEventId);J->SetNumberField(TEXT("use_count"),V.UseCount);TArray<TSharedPtr<FJsonValue>> Visitors;for(const FString& Id:V.UniqueVisitorIds)Visitors.Add(MakeShared<FJsonValueString>(Id));J->SetArrayField(TEXT("unique_visitor_ids"),Visitors);TArray<TSharedPtr<FJsonValue>> Seats;for(const auto& S:V.Seats){auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("seat_id"),S.SeatId);X->SetStringField(TEXT("status"),S.Status);X->SetStringField(TEXT("occupant_resident_id"),S.OccupantResidentId);X->SetStringField(TEXT("active_use_event_id"),S.ActiveUseEventId);X->SetStringField(TEXT("world_position"),S.WorldPosition.ToString());Seats.Add(MakeShared<FJsonValueObject>(X));}J->SetArrayField(TEXT("seats"),Seats);Venues.Add(MakeShared<FJsonValueObject>(J));}
        for(const auto& E:State.UseEvents){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("event_id"),E.EventId);J->SetStringField(TEXT("venue_id"),E.VenueId);J->SetStringField(TEXT("seat_id"),E.SeatId);J->SetStringField(TEXT("resident_id"),E.ResidentId);J->SetStringField(TEXT("kind"),E.Kind);J->SetStringField(TEXT("status"),E.Status);J->SetNumberField(TEXT("started_at"),E.StartedAt);J->SetNumberField(TEXT("ended_at"),E.EndedAt);Events.Add(MakeShared<FJsonValueObject>(J));}
        Root->SetArrayField(TEXT("venues"),Venues);Root->SetArrayField(TEXT("use_events"),Events);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));return Text;
    }

    void RefreshLive(const FString& WorldId,const TArray<FHearthWorldRequest>& Requests,const TArray<FHearthSite>& Sites,const TArray<FHearthStructurePlan>& Plans){Reconcile(HearthTavernRuntimeDetail::StateFor(WorldId),Requests,Sites,Plans);}
    TArray<int32> LiveAvailableSeatActions(const FString& WorldId,const FString& ResidentId){return AvailableSeatActions(HearthTavernRuntimeDetail::StateFor(WorldId),ResidentId);}
    FString LiveActionName(const FString& WorldId,int32 Action,const FString& ResidentId){return ActionName(HearthTavernRuntimeDetail::StateFor(WorldId),Action,ResidentId);}
    bool LiveReserveSeat(const FString& WorldId,int32 Action,const FString& ResidentId,FVector& OutPosition,FString& OutSeatId){return ReserveSeat(HearthTavernRuntimeDetail::StateFor(WorldId),Action,ResidentId,OutPosition,OutSeatId);}
    bool LiveMarkArrived(const FString& WorldId,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId){return MarkArrived(HearthTavernRuntimeDetail::StateFor(WorldId),SeatId,ResidentId,SimulationTime,OutEventId);}
    bool LiveReleaseSeat(const FString& WorldId,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId){return ReleaseSeat(HearthTavernRuntimeDetail::StateFor(WorldId),SeatId,ResidentId,SimulationTime,OutEventId);}
    bool LiveCancelReservation(const FString& WorldId,const FString& SeatId,const FString& ResidentId){return CancelReservation(HearthTavernRuntimeDetail::StateFor(WorldId),SeatId,ResidentId);}
    FString LiveSeatForResident(const FString& WorldId,const FString& ResidentId){return SeatForResident(HearthTavernRuntimeDetail::StateFor(WorldId),ResidentId);}
    FString LiveExportReadOnly(const FString& WorldId){return ExportReadOnly(HearthTavernRuntimeDetail::StateFor(WorldId));}
    void ResetLive(const FString& WorldId){if(!HearthTavernRuntimeDetail::BoundStates.Contains(WorldId))HearthTavernRuntimeDetail::LiveStates.Remove(WorldId);}
    void BindPersistentState(const FString& WorldId,FHearthTavernRuntimeState& State){HearthTavernRuntimeDetail::BoundStates.Add(WorldId,&State);HearthTavernRuntimeDetail::LiveStates.Remove(WorldId);}
    void UnbindPersistentState(const FString& WorldId){HearthTavernRuntimeDetail::BoundStates.Remove(WorldId);HearthTavernRuntimeDetail::LiveStates.Remove(WorldId);}

    bool BuildAttachedCanopyPlan(const FHearthResident& Resident,const FHearthSite& Site,int32 Budget,int32 Stone,int32 Planks,int32 Beams,int32 Tiles,FHearthResidentBuildingPlan& OutPlan)
    {
        FHearthStructureFootprint Footprint;Footprint.Origin=Site.Position;Footprint.Size=FVector2D(450.f,350.f);const FVector Road=Site.Approach-Site.Position;Footprint.Orientation=FRotator(0,Road.IsNearlyZero()?0.f:90.f+FMath::RadiansToDegrees(FMath::Atan2(Road.Y,Road.X)),0);
        FHearthStructureReasonFields Reasons;Reasons.Need=TEXT("tavern_canopy_seating");Reasons.Occupation=Resident.Role;Reasons.Budget=FString::FromInt(Budget);Reasons.RoadAccess=Site.bReachable?TEXT("reachable"):TEXT("blocked");
        FHearthResidentBuildingPlan Existing;Existing.Plan=HearthStructurePlan::MakePlan(MakeAttachedPlanId(Site.StableId,Site.BuildPlanId),Resident.StableId+TEXT("|tavern_canopy_v1"),Footprint,Reasons);Existing.Expansion.ResultingPlan=Existing.Plan;Existing.bBuildable=true;
        FHearthResidentBuildingInput Input;Input.ResidentId=Resident.StableId;Input.StableSeed=Resident.StableId+TEXT("|tavern_canopy_v1");Input.ExtensionKey=TEXT("tavern_canopy_v1");Input.Need=TEXT("tavern_canopy_seating");Input.Occupation=Resident.Role;Input.Archetype=Resident.BuildingArchetype;Input.Origin=Site.Position;Input.Budget=Budget;Input.Stone=Stone;Input.Planks=Planks;Input.Beams=Beams;Input.Tiles=Tiles;Input.bRoadAccessible=Site.bReachable;
        if(!HearthResidentBuildingPlanner::AppendCanopy(Existing,Input)){OutPlan=Existing;OutPlan.bBuildable=false;OutPlan.Reason=TEXT("构件规划器未能生成可支撑的酒馆附属棚方案。");return false;}
        // The adapter consumes Plan. AppendCanopy intentionally returns its
        // candidate in Expansion without changing the original host plan.
        Existing.Plan=Existing.Expansion.ResultingPlan;
        OutPlan=MoveTemp(Existing);return OutPlan.bBuildable;
    }
}
