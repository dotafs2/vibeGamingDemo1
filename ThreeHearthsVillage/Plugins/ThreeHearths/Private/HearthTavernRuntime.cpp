#include "HearthTavernRuntime.h"

#include "HearthPlannedConstructionAdapter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HearthTavernRuntimeDetail
{
    TMap<FString, FHearthTavernRuntimeState> LiveStates;

    bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Terms)
    {
        for (const TCHAR* Term : Terms) if (Text.Contains(Term, ESearchCase::IgnoreCase)) return true;
        return false;
    }

    FString PlanHostId(const FString& PlanId)
    {
        const int32 Split = PlanId.Find(TEXT(":tavern_canopy_v1"));
        return Split == INDEX_NONE ? PlanId : PlanId.Left(Split);
    }

    const FHearthWorldRequest* FindNeed(const TArray<FHearthWorldRequest>& Requests,
        const FString& OwnerId, const FString& HostSiteId, const FString& HostPlanId)
    {
        for (const FHearthWorldRequest& Request : Requests)
        {
            if (!HearthTavernRuntime::IsCanopyNeed(Request.Summary)) continue;
            if (!Request.RequesterIds.Contains(OwnerId)) continue;
            if (!HostSiteId.IsEmpty() && !Request.Summary.Contains(HostSiteId)) continue;
            if (!HostPlanId.IsEmpty() && !Request.Summary.Contains(HostPlanId)) continue;
            return &Request;
        }
        return nullptr;
    }

    FHearthTavernSeatState* FindSeat(FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (FHearthTavernVenueState& Venue : State.Venues)
            if (FHearthTavernSeatState* Seat = Venue.Seats.FindByPredicate([&](FHearthTavernSeatState& Candidate)
                { return Candidate.SeatId == SeatId; })) return Seat;
        return nullptr;
    }

    const FHearthTavernSeatState* FindSeat(const FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (const FHearthTavernVenueState& Venue : State.Venues)
            if (const FHearthTavernSeatState* Seat = Venue.Seats.FindByPredicate([&](const FHearthTavernSeatState& Candidate)
                { return Candidate.SeatId == SeatId; })) return Seat;
        return nullptr;
    }

    FHearthTavernVenueState* FindVenueForSeat(FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (FHearthTavernVenueState& Venue : State.Venues)
            if (Venue.Seats.ContainsByPredicate([&](const FHearthTavernSeatState& Seat){ return Seat.SeatId == SeatId; })) return &Venue;
        return nullptr;
    }

    const FHearthTavernVenueState* FindVenueForSeat(const FHearthTavernRuntimeState& State, const FString& SeatId)
    {
        for (const FHearthTavernVenueState& Venue : State.Venues)
            if (Venue.Seats.ContainsByPredicate([&](const FHearthTavernSeatState& Seat){ return Seat.SeatId == SeatId; })) return &Venue;
        return nullptr;
    }

    int32 ActionFor(int32 VenueIndex, int32 SeatIndex)
    {
        return HearthTavernRuntime::SeatActionBase + VenueIndex * HearthTavernRuntime::SeatActionStride + SeatIndex;
    }

    bool DecodeAction(int32 Action, int32& VenueIndex, int32& SeatIndex)
    {
        if (!HearthTavernRuntime::IsSeatAction(Action)) return false;
        const int32 Relative = Action - HearthTavernRuntime::SeatActionBase;
        VenueIndex = Relative / HearthTavernRuntime::SeatActionStride;
        SeatIndex = Relative % HearthTavernRuntime::SeatActionStride;
        return true;
    }
}

namespace HearthTavernRuntime
{
    bool IsCanopyNeed(const FString& Text)
    {
        const bool bVenue = HearthTavernRuntimeDetail::ContainsAny(Text,
            {TEXT("tavern"), TEXT("inn"), TEXT("酒馆"), TEXT("旅店"), TEXT("客栈")});
        const bool bCanopy = HearthTavernRuntimeDetail::ContainsAny(Text,
            {TEXT("canopy"), TEXT("awning"), TEXT("棚"), TEXT("顶棚"), TEXT("长凳"), TEXT("座位"),
             TEXT("seating"), TEXT("gathering"), TEXT("接待"), TEXT("聚会"), TEXT("tavern_canopy_seating")});
        return (bVenue && bCanopy) || Text.Contains(TEXT("tavern_canopy_seating"), ESearchCase::IgnoreCase);
    }

    bool IsTavernPlan(const FHearthStructurePlan& Plan)
    {
        if (Plan.PlanId.Contains(TEXT(":tavern_canopy_v1"), ESearchCase::IgnoreCase)
            || Plan.Reasons.Need.Contains(TEXT("tavern_canopy"), ESearchCase::IgnoreCase)) return true;
        return Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& Component)
            { return Component.ExtensionId.Contains(TEXT("tavern_canopy"), ESearchCase::IgnoreCase); });
    }

    FString MakeVenueId(const FString& HostSiteStableId, const FString& HostPlanId)
    {
        const FString Base = !HostSiteStableId.IsEmpty() ? HostSiteStableId : HostPlanId;
        return Base + TEXT(":tavern_venue_v1");
    }

    FString MakeAttachedPlanId(const FString& HostSiteStableId, const FString& HostPlanId)
    {
        const FString Base = !HostSiteStableId.IsEmpty() ? HostSiteStableId : HostPlanId;
        return Base + TEXT(":tavern_canopy_v1");
    }

    bool ProposeNeed(TArray<FHearthWorldRequest>& Requests, const FString& OwnerResidentId,
        const FString& HostSiteStableId, const FString& HostPlanId, FString& Error)
    {
        if (OwnerResidentId.IsEmpty() || (HostSiteStableId.IsEmpty() && HostPlanId.IsEmpty()))
        {
            Error = TEXT("tavern need requires an owner and a stable host id");
            return false;
        }
        const FString Need = FString::Printf(TEXT("看见：自家酒馆已有建筑；未知：棚和座位能否安全接待；打算：在自家旁加户外棚与至少两张木长凳。 [tavern_canopy_seating host_site:%s host_plan:%s]"),
            *HostSiteStableId, *HostPlanId);
        return HearthWorldRequests::Submit(Requests, OwnerResidentId, Need, Error);
    }

    void Reconcile(FHearthTavernRuntimeState& State, const TArray<FHearthWorldRequest>& Requests,
        const TArray<FHearthSite>& Sites, const TArray<FHearthStructurePlan>& Plans)
    {
        TSet<FString> SeenVenueIds;
        for (const FHearthStructurePlan& Plan : Plans)
        {
            if (!IsTavernPlan(Plan)) continue;
            const int32 SiteIndex = Sites.IndexOfByPredicate([&](const FHearthSite& Site){ return Site.BuildPlanId == Plan.PlanId; });
            const FHearthSite* Site = Sites.IsValidIndex(SiteIndex) ? &Sites[SiteIndex] : nullptr;
            const FString HostPlanId = HearthTavernRuntimeDetail::PlanHostId(Plan.PlanId);
            const FString HostSiteId = Site ? Site->StableId : HostPlanId;
            const FString VenueId = MakeVenueId(HostSiteId, HostPlanId);
            SeenVenueIds.Add(VenueId);
            FHearthTavernVenueState* Venue = State.Venues.FindByPredicate([&](FHearthTavernVenueState& Candidate){ return Candidate.VenueId == VenueId; });
            if (!Venue)
            {
                FHearthTavernVenueState Fresh; Fresh.VenueId=VenueId; Fresh.HostSiteStableId=HostSiteId;
                Fresh.HostPlanId=HostPlanId; Fresh.PlanId=Plan.PlanId; State.Venues.Add(MoveTemp(Fresh)); Venue=&State.Venues.Last();
            }
            Venue->HostSiteStableId=HostSiteId; Venue->HostPlanId=HostPlanId; Venue->PlanId=Plan.PlanId;
            if (const FHearthWorldRequest* Request=HearthTavernRuntimeDetail::FindNeed(Requests, Venue->OwnerResidentId, HostSiteId, HostPlanId))
                Venue->RequestId=Request->Id;
            if (Venue->OwnerResidentId.IsEmpty())
            {
                for (const FHearthWorldRequest& Request:Requests) if (IsCanopyNeed(Request.Summary) && Request.Summary.Contains(HostSiteId))
                { Venue->OwnerResidentId=Request.RequesterIds.IsEmpty()?FString():Request.RequesterIds[0]; Venue->RequestId=Request.Id; break; }
            }
            Venue->Status=Site ? TEXT("building") : TEXT("planned");
            TArray<FHearthTavernSeatState> NewSeats;
            if (Site)
            {
                for (const FHearthCottageComponent& Component : Site->CottageComponents)
                {
                    if (Component.AssetId != TEXT("bench_timber") || Component.Status != TEXT("completed")) continue;
                    FHearthTavernSeatState Seat; Seat.SeatId=VenueId+TEXT(":seat:")+Component.Id; Seat.VenueId=VenueId;
                    Seat.WorldPosition=Site->Position+Component.Offset;
                    if (const FHearthTavernSeatState* Previous=Venue->Seats.FindByPredicate([&](const FHearthTavernSeatState& Candidate){return Candidate.SeatId==Seat.SeatId;})) Seat=*Previous;
                    Seat.WorldPosition=Site->Position+Component.Offset; NewSeats.Add(MoveTemp(Seat));
                }
            }
            Venue->Seats=MoveTemp(NewSeats);
            if (Venue->Seats.Num()>=2) Venue->Status=TEXT("usable");
        }
        for (FHearthTavernVenueState& Venue : State.Venues)
            if (!SeenVenueIds.Contains(Venue.VenueId) && Venue.Status==TEXT("usable")) Venue.Status=TEXT("planned");
    }

    int32 FindVenue(const FHearthTavernRuntimeState& State, const FString& VenueId)
    { return State.Venues.IndexOfByPredicate([&](const FHearthTavernVenueState& Venue){return Venue.VenueId==VenueId;}); }

    int32 FindVenueForOwner(const FHearthTavernRuntimeState& State, const FString& OwnerResidentId)
    { return State.Venues.IndexOfByPredicate([&](const FHearthTavernVenueState& Venue){return Venue.OwnerResidentId==OwnerResidentId;}); }

    bool IsSeatAction(int32 Action)
    { return Action>=SeatActionBase && Action<SeatActionBase+4096; }

    TArray<int32> AvailableSeatActions(const FHearthTavernRuntimeState& State, const FString& ResidentId)
    {
        TArray<int32> Result;
        if (ResidentId.IsEmpty()) return Result;
        for (int32 VenueIndex=0;VenueIndex<State.Venues.Num();++VenueIndex)
        {
            const auto& Venue=State.Venues[VenueIndex]; if(Venue.Status!=TEXT("usable")) continue;
            for(int32 SeatIndex=0;SeatIndex<Venue.Seats.Num() && SeatIndex<SeatActionStride;++SeatIndex)
                if(Venue.Seats[SeatIndex].Status==TEXT("free")) Result.Add(HearthTavernRuntimeDetail::ActionFor(VenueIndex,SeatIndex));
        }
        return Result;
    }

    FString ActionName(const FHearthTavernRuntimeState& State, int32 Action)
    {
        int32 VenueIndex=-1,SeatIndex=-1; if(!HearthTavernRuntimeDetail::DecodeAction(Action,VenueIndex,SeatIndex) || !State.Venues.IsValidIndex(VenueIndex)) return TEXT("未知酒馆座位");
        const auto& Venue=State.Venues[VenueIndex]; if(!Venue.Seats.IsValidIndex(SeatIndex)) return TEXT("未知酒馆座位");
        return FString::Printf(TEXT("去自家酒馆户外棚坐下（座位 %d，真实走过去并记录聚会）"),SeatIndex+1);
    }

    bool ReserveSeat(FHearthTavernRuntimeState& State, int32 Action, const FString& ResidentId, FVector& OutPosition, FString& OutSeatId)
    {
        int32 VenueIndex=-1,SeatIndex=-1; if(!HearthTavernRuntimeDetail::DecodeAction(Action,VenueIndex,SeatIndex) || !State.Venues.IsValidIndex(VenueIndex)) return false;
        auto& Venue=State.Venues[VenueIndex]; if(Venue.Status!=TEXT("usable") || !Venue.Seats.IsValidIndex(SeatIndex)) return false;
        auto& Seat=Venue.Seats[SeatIndex]; if(Seat.Status!=TEXT("free") || ResidentId.IsEmpty()) return false;
        Seat.Status=TEXT("reserved"); Seat.OccupantResidentId=ResidentId; OutPosition=Seat.WorldPosition; OutSeatId=Seat.SeatId; return true;
    }

    bool MarkArrived(FHearthTavernRuntimeState& State, const FString& SeatId, const FString& ResidentId, float SimulationTime, FString& OutEventId)
    {
        FHearthTavernSeatState* Seat=HearthTavernRuntimeDetail::FindSeat(State,SeatId); if(!Seat || Seat->Status!=TEXT("reserved") || Seat->OccupantResidentId!=ResidentId) return false;
        FHearthTavernVenueState* Venue=HearthTavernRuntimeDetail::FindVenueForSeat(State,SeatId); if(!Venue) return false;
        FHearthTavernUseEvent Event; Event.EventId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Event.VenueId=Venue->VenueId; Event.SeatId=SeatId; Event.ResidentId=ResidentId; Event.StartedAt=SimulationTime; State.UseEvents.Add(Event);
        Seat.Status=TEXT("occupied"); Seat.ActiveUseEventId=Event.EventId; OutEventId=Event.EventId; Venue->LastUseEventId=Event.EventId; return true;
    }

    bool ReleaseSeat(FHearthTavernRuntimeState& State, const FString& SeatId, const FString& ResidentId, float SimulationTime, FString& OutEventId)
    {
        FHearthTavernSeatState* Seat=HearthTavernRuntimeDetail::FindSeat(State,SeatId); if(!Seat || Seat->OccupantResidentId!=ResidentId) return false;
        FHearthTavernVenueState* Venue=HearthTavernRuntimeDetail::FindVenueForSeat(State,SeatId); if(!Venue) return false;
        FHearthTavernUseEvent* Event=State.UseEvents.FindByPredicate([&](FHearthTavernUseEvent& Candidate){return Candidate.EventId==Seat->ActiveUseEventId && Candidate.Status==TEXT("active");});
        if(!Event) return false;
        Event->EndedAt=SimulationTime; Event->Status=TEXT("completed"); OutEventId=Event->EventId; Seat->Status=TEXT("free"); Seat->OccupantResidentId.Empty(); Seat->ActiveUseEventId.Empty(); ++Venue->UseCount; return true;
    }

    FString ExportReadOnly(const FHearthTavernRuntimeState& State)
    {
        auto Root=MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("version"),1); TArray<TSharedPtr<FJsonValue>> Venues,Events;
        for(const auto& Venue:State.Venues){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("venue_id"),Venue.VenueId);J->SetStringField(TEXT("owner_resident_id"),Venue.OwnerResidentId);J->SetStringField(TEXT("host_site_stable_id"),Venue.HostSiteStableId);J->SetStringField(TEXT("host_plan_id"),Venue.HostPlanId);J->SetStringField(TEXT("plan_id"),Venue.PlanId);J->SetStringField(TEXT("request_id"),Venue.RequestId);J->SetStringField(TEXT("status"),Venue.Status);J->SetStringField(TEXT("last_use_event_id"),Venue.LastUseEventId);J->SetNumberField(TEXT("use_count"),Venue.UseCount);TArray<TSharedPtr<FJsonValue>> Seats;for(const auto& Seat:Venue.Seats){auto S=MakeShared<FJsonObject>();S->SetStringField(TEXT("seat_id"),Seat.SeatId);S->SetStringField(TEXT("status"),Seat.Status);S->SetStringField(TEXT("occupant_resident_id"),Seat.OccupantResidentId);S->SetStringField(TEXT("active_use_event_id"),Seat.ActiveUseEventId);S->SetStringField(TEXT("world_position"),Seat.WorldPosition.ToString());Seats.Add(MakeShared<FJsonValueObject>(S));}J->SetArrayField(TEXT("seats"),Seats);Venues.Add(MakeShared<FJsonValueObject>(J));}
        for(const auto& Event:State.UseEvents){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("event_id"),Event.EventId);J->SetStringField(TEXT("venue_id"),Event.VenueId);J->SetStringField(TEXT("seat_id"),Event.SeatId);J->SetStringField(TEXT("resident_id"),Event.ResidentId);J->SetStringField(TEXT("kind"),Event.Kind);J->SetStringField(TEXT("status"),Event.Status);J->SetNumberField(TEXT("started_at"),Event.StartedAt);J->SetNumberField(TEXT("ended_at"),Event.EndedAt);Events.Add(MakeShared<FJsonValueObject>(J));}
        Root->SetArrayField(TEXT("venues"),Venues);Root->SetArrayField(TEXT("use_events"),Events);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));return Text;
    }

    void RefreshLive(const FString& WorldId,const TArray<FHearthWorldRequest>& Requests,const TArray<FHearthSite>& Sites,const TArray<FHearthStructurePlan>& Plans)
    { Reconcile(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId),Requests,Sites,Plans); }
    TArray<int32> LiveAvailableSeatActions(const FString& WorldId,const FString& ResidentId)
    { return AvailableSeatActions(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId),ResidentId); }
    bool LiveReserveSeat(const FString& WorldId,int32 Action,const FString& ResidentId,FVector& OutPosition,FString& OutSeatId)
    { return ReserveSeat(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId),Action,ResidentId,OutPosition,OutSeatId); }
    bool LiveMarkArrived(const FString& WorldId,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId)
    { return MarkArrived(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId),SeatId,ResidentId,SimulationTime,OutEventId); }
    bool LiveReleaseSeat(const FString& WorldId,const FString& SeatId,const FString& ResidentId,float SimulationTime,FString& OutEventId)
    { return ReleaseSeat(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId),SeatId,ResidentId,SimulationTime,OutEventId); }
    FString LiveExportReadOnly(const FString& WorldId)
    { return ExportReadOnly(HearthTavernRuntimeDetail::LiveStates.FindOrAdd(WorldId)); }
    void ResetLive(const FString& WorldId) { HearthTavernRuntimeDetail::LiveStates.Remove(WorldId); }

    bool BuildAttachedCanopyPlan(const FHearthResident& Resident,const FHearthSite& Site,int32 Budget,int32 Stone,int32 Planks,int32 Beams,int32 Tiles,FHearthResidentBuildingPlan& OutPlan)
    {
        FHearthStructureFootprint Footprint; Footprint.Origin=Site.Position; Footprint.Size=FVector2D(450.f,350.f); const FVector Road=Site.Approach-Site.Position; Footprint.Orientation=FRotator(0,Road.IsNearlyZero()?0.f:90.f+FMath::RadiansToDegrees(FMath::Atan2(Road.Y,Road.X)),0);
        FHearthStructureReasonFields Reasons; Reasons.Need=TEXT("tavern_canopy_seating"); Reasons.Occupation=Resident.Role; Reasons.Budget=FString::FromInt(Budget); Reasons.RoadAccess=Site.bReachable?TEXT("reachable"):TEXT("blocked");
        FHearthResidentBuildingPlan Existing; Existing.Plan=HearthStructurePlan::MakePlan(MakeAttachedPlanId(Site.StableId,Site.BuildPlanId),Resident.StableId+TEXT("|tavern_canopy_v1"),Footprint,Reasons); Existing.Expansion.ResultingPlan=Existing.Plan; Existing.bBuildable=true;
        FHearthResidentBuildingInput Input; Input.ResidentId=Resident.StableId; Input.StableSeed=Resident.StableId+TEXT("|tavern_canopy_v1"); Input.ExtensionKey=TEXT("tavern_canopy_v1"); Input.Need=TEXT("tavern_canopy_seating"); Input.Occupation=Resident.Role; Input.Archetype=Resident.BuildingArchetype; Input.Origin=Site.Position; Input.Budget=Budget; Input.Stone=Stone; Input.Planks=Planks; Input.Beams=Beams; Input.Tiles=Tiles; Input.bRoadAccessible=Site.bReachable;
        if(!HearthResidentBuildingPlanner::AppendCanopy(Existing,Input)){OutPlan=Existing;OutPlan.bBuildable=false;OutPlan.Reason=TEXT("构件规划器未能生成可支撑的酒馆附属棚方案。");return false;} OutPlan=MoveTemp(Existing); return OutPlan.bBuildable;
    }
}
