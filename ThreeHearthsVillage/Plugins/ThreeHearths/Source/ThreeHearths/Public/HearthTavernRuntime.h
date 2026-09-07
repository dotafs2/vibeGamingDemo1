#pragma once

#include "CoreMinimal.h"
#include "HearthVillage.h"
#include "HearthResidentBuildingPlanner.h"

// Compatibility declaration until the planner colleague's shared header
// change lands. An identical declaration may also live in that header.
namespace HearthResidentBuildingPlanner
{
    THREEHEARTHS_API bool AppendCanopy(FHearthResidentBuildingPlan& Existing,
        const FHearthResidentBuildingInput& Input);
}

// Tavern state is deliberately kept as plain data here. HearthWorldState owns
// the persisted arrays; this module owns the invariants and live seat actions.
struct THREEHEARTHS_API FHearthTavernSeatState
{
    FString SeatId;
    FString VenueId;
    FString Status = TEXT("free"); // free, reserved, occupied
    FString OccupantResidentId;
    FVector WorldPosition = FVector::ZeroVector;
    FString ActiveUseEventId;
};

struct THREEHEARTHS_API FHearthTavernUseEvent
{
    FString EventId;
    FString VenueId;
    FString SeatId;
    FString ResidentId;
    FString Kind = TEXT("gather"); // gather or rest; no sit animation is assumed
    FString Status = TEXT("active"); // active or completed
    float StartedAt = 0.f;
    float EndedAt = 0.f;
};

struct THREEHEARTHS_API FHearthTavernVenueState
{
    FString VenueId;
    FString OwnerResidentId;
    FString HostSiteStableId;
    FString HostPlanId;
    FString PlanId;
    FString RequestId;
    FString Status = TEXT("requested"); // requested, planned, building, usable
    TArray<FHearthTavernSeatState> Seats;
    FString LastUseEventId;
    int32 UseCount = 0;
    TArray<FString> UniqueVisitorIds;
};

struct THREEHEARTHS_API FHearthTavernRuntimeState
{
    TArray<FHearthTavernVenueState> Venues;
    TArray<FHearthTavernUseEvent> UseEvents;
};

namespace HearthTavernRuntime
{
    constexpr int32 SeatActionBase = 7000;
    constexpr int32 SeatActionStride = 16;
    constexpr int32 LegacyBuildActionBase = 18000;
    constexpr int32 MaxUseEvents = 256;
    constexpr int32 MaxUniqueVisitors = 64;

    THREEHEARTHS_API bool IsCanopyNeed(const FString& Text);
    THREEHEARTHS_API bool IsTavernPlan(const FHearthStructurePlan& Plan);
    THREEHEARTHS_API FString MakeVenueId(const FString& HostSiteStableId, const FString& HostPlanId);
    THREEHEARTHS_API FString MakeAttachedPlanId(const FString& HostSiteStableId, const FString& HostPlanId);

    // Adds one idempotent, evidence-separated need to the existing request board.
    // It records an intention only; it never creates a plan or consumes stock.
    THREEHEARTHS_API bool ProposeNeed(TArray<FHearthWorldRequest>& Requests,
        const FString& OwnerResidentId, const FString& HostSiteStableId,
        const FString& HostPlanId, FString& Error);
    // Parent wrapper can expose this as RequestTavernCanopy(Index). It only
    // records the owner's request and never bypasses construction checks.
    THREEHEARTHS_API bool RequestTavernCanopy(FHearthResident& Resident,
        const FHearthSite& HostSite, TArray<FHearthWorldRequest>& Requests, FString& Error);

    // Rebuilds derived venue/seat facts from persisted requests, plans and sites.
    // Existing occupancy and use events are retained by stable SeatId/EventId.
    THREEHEARTHS_API void Reconcile(FHearthTavernRuntimeState& State,
        const TArray<FHearthWorldRequest>& Requests,
        const TArray<FHearthSite>& Sites,
        const TArray<FHearthStructurePlan>& Plans);

    THREEHEARTHS_API int32 FindVenue(const FHearthTavernRuntimeState& State, const FString& VenueId);
    THREEHEARTHS_API int32 FindVenueForOwner(const FHearthTavernRuntimeState& State, const FString& OwnerResidentId);
    THREEHEARTHS_API bool IsSeatAction(int32 Action);
    // A stable action for a completed starter inn that has an exterior host
    // but no native production plan yet. It resolves to a real site only when
    // construction starts; it never marks a component or grants material.
    THREEHEARTHS_API bool IsLegacyBuildAction(int32 Action);
    THREEHEARTHS_API int32 MakeLegacyBuildAction(int32 ResidentIndex);
    THREEHEARTHS_API int32 LegacyBuildResident(int32 Action);
    THREEHEARTHS_API TArray<int32> AvailableSeatActions(const FHearthTavernRuntimeState& State,
        const FString& ResidentId);
    THREEHEARTHS_API FString ActionName(const FHearthTavernRuntimeState& State, int32 Action,
        const FString& RequesterResidentId = FString());

    // These transitions are the only operations that can create a tavern use
    // event. They do not grant materials, wages, construction, food or coins.
    THREEHEARTHS_API bool ReserveSeat(FHearthTavernRuntimeState& State, int32 Action,
        const FString& ResidentId, FVector& OutPosition, FString& OutSeatId);
    THREEHEARTHS_API bool MarkArrived(FHearthTavernRuntimeState& State, const FString& SeatId,
        const FString& ResidentId, float SimulationTime, FString& OutEventId);
    THREEHEARTHS_API bool ReleaseSeat(FHearthTavernRuntimeState& State, const FString& SeatId,
        const FString& ResidentId, float SimulationTime, FString& OutEventId);
    THREEHEARTHS_API bool CancelReservation(FHearthTavernRuntimeState& State, const FString& SeatId,
        const FString& ResidentId);
    THREEHEARTHS_API FString SeatForResident(const FHearthTavernRuntimeState& State,
        const FString& ResidentId);

    THREEHEARTHS_API FString ExportReadOnly(const FHearthTavernRuntimeState& State);

    // Live convenience layer for AHearthVillage, whose owning/persisted arrays
    // are added by the world task. The parent can replace this session map by
    // binding the same calls to its persistent FHearthTavernRuntimeState.
    THREEHEARTHS_API void RefreshLive(const FString& WorldId,
        const TArray<FHearthWorldRequest>& Requests,
        const TArray<FHearthSite>& Sites,
        const TArray<FHearthStructurePlan>& Plans);
    THREEHEARTHS_API TArray<int32> LiveAvailableSeatActions(const FString& WorldId,
        const FString& ResidentId);
    THREEHEARTHS_API FString LiveActionName(const FString& WorldId, int32 Action,
        const FString& ResidentId);
    THREEHEARTHS_API bool LiveReserveSeat(const FString& WorldId, int32 Action,
        const FString& ResidentId, FVector& OutPosition, FString& OutSeatId);
    THREEHEARTHS_API bool LiveMarkArrived(const FString& WorldId, const FString& SeatId,
        const FString& ResidentId, float SimulationTime, FString& OutEventId);
    THREEHEARTHS_API bool LiveReleaseSeat(const FString& WorldId, const FString& SeatId,
        const FString& ResidentId, float SimulationTime, FString& OutEventId);
    THREEHEARTHS_API bool LiveCancelReservation(const FString& WorldId, const FString& SeatId,
        const FString& ResidentId);
    THREEHEARTHS_API FString LiveSeatForResident(const FString& WorldId, const FString& ResidentId);
    THREEHEARTHS_API FString LiveExportReadOnly(const FString& WorldId);
    THREEHEARTHS_API void ResetLive(const FString& WorldId);
    // Euclid binds the persistent WorldState-owned object before simulation
    // ticks. All Live* calls then operate on that object and Encode/Decode can
    // serialize the same arrays without a second source of truth.
    THREEHEARTHS_API void BindPersistentState(const FString& WorldId, FHearthTavernRuntimeState& State);
    THREEHEARTHS_API void UnbindPersistentState(const FString& WorldId);

    // Builds a canopy-only attached plan for a legacy house with no native
    // structure plan. The colleague-owned AppendCanopy hook supplies geometry.
    THREEHEARTHS_API bool BuildAttachedCanopyPlan(const FHearthResident& Resident,
        const FHearthSite& Site, int32 Budget, int32 Stone, int32 Planks, int32 Beams,
        int32 Tiles, FHearthResidentBuildingPlan& OutPlan);
}
