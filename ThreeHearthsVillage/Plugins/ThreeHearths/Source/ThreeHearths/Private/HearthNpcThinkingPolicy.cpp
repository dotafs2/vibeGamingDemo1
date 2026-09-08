#include "HearthNpcThinkingPolicy.h"

namespace
{
    bool IsSameOrLater(const double Now, const double Then)
    {
        return FMath::IsFinite(Now) && FMath::IsFinite(Then) && Then >= 0.0 && Now >= Then;
    }
}

bool FHearthNpcThinkingClock::IsValid() const
{
    return FMath::IsFinite(MonotonicSeconds) && MonotonicSeconds >= 0.0
        && FMath::IsFinite(UtcSeconds) && UtcSeconds >= 0.0;
}

FHearthNpcThinkingPolicy::FHearthNpcThinkingPolicy() = default;

void FHearthNpcThinkingPolicy::BeginSession(const FHearthNpcThinkingClock& Now, const bool bInColdRestore)
{
    // The clock argument intentionally has no simulation-time field. It is a
    // sanity check for the host and makes the session boundary explicit.
    bColdRestore = bInColdRestore || !Now.IsValid();
}

FHearthNpcThinkingPolicy::FResidentState& FHearthNpcThinkingPolicy::StateFor(
    const FString& ResidentId, const EHearthNpcThinkingRole Role)
{
    FResidentState& State = ResidentStates.FindOrAdd(ResidentId);
    State.Persisted.ResidentId = ResidentId;
    State.Persisted.Role = Role;
    return State;
}

FString FHearthNpcThinkingPolicy::MakeRequestId(const FHearthNpcThinkingRequest& Request)
{
    return Request.ResidentId + TEXT("|") + Request.EventId;
}

FString FHearthNpcThinkingPolicy::MakeCoalesceKey(const FHearthNpcThinkingRequest& Request)
{
    return Request.ResidentId + TEXT("|") + (Request.CoalesceKey.IsEmpty() ? Request.EventId : Request.CoalesceKey);
}

bool FHearthNpcThinkingPolicy::IsApiEligible(const EHearthNpcThinkingRole Role,
    const EHearthNpcThinkingTrigger Trigger)
{
    // Gatekeepers and royal guards use local behavior for their frequent
    // routine thoughts. Civilian and carter routines may reach the paid
    // gateway, but their 30-minute spacing is enforced by MinSpacing().
    if (Trigger == EHearthNpcThinkingTrigger::Routine)
        return Role != EHearthNpcThinkingRole::Gatekeeper
            && Role != EHearthNpcThinkingRole::RoyalGuard;

    // Daydreams are an explicit occasional paid event for every role,
    // including gatekeepers; the six-hour spacing is enforced per resident.
    return Trigger == EHearthNpcThinkingTrigger::ActualInteraction
        || Trigger == EHearthNpcThinkingTrigger::ImportantEvent
        || Trigger == EHearthNpcThinkingTrigger::Daydream;
}

double FHearthNpcThinkingPolicy::MinSpacing(const EHearthNpcThinkingTrigger Trigger)
{
    switch (Trigger)
    {
    case EHearthNpcThinkingTrigger::ActualInteraction: return ActualInteractionMinSpacingSeconds;
    case EHearthNpcThinkingTrigger::ImportantEvent: return ImportantEventMinSpacingSeconds;
    case EHearthNpcThinkingTrigger::Routine: return RoutineMinSpacingSeconds;
    case EHearthNpcThinkingTrigger::Daydream: return DaydreamMinSpacingSeconds;
    default: return RoutineMinSpacingSeconds;
    }
}

int32 FHearthNpcThinkingPolicy::Priority(const FHearthNpcThinkingRequest& Request)
{
    int32 Result = Request.Trigger == EHearthNpcThinkingTrigger::ImportantEvent ? 400
        : Request.Trigger == EHearthNpcThinkingTrigger::ActualInteraction ? 300
        : Request.Trigger == EHearthNpcThinkingTrigger::Daydream ? 200 : 100;
    if (Request.bUrgent) Result += 50;
    return Result;
}

bool FHearthNpcThinkingPolicy::IsFiniteNonNegativeOrUnset(const double Value)
{
    return Value == -1.0 || (FMath::IsFinite(Value) && Value >= 0.0);
}

double FHearthNpcThinkingPolicy::SecondsSince(const double NowMonotonic, const double NowUtc,
    const double LastMonotonic, const double LastUtc) const
{
    // Monotonic time is authoritative during a live session. After a cold
    // restore (where the monotonic origin may have changed), UTC is the guard
    // that prevents an immediate burst of requests.
    if (!bColdRestore && IsSameOrLater(NowMonotonic, LastMonotonic))
        return NowMonotonic - LastMonotonic;
    if (IsSameOrLater(NowUtc, LastUtc)) return NowUtc - LastUtc;
    return 0.0;
}

double FHearthNpcThinkingPolicy::SpacingRemaining(const FResidentState& State,
    const EHearthNpcThinkingTrigger Trigger, const FHearthNpcThinkingClock& Now) const
{
    const double LastMonotonic = Trigger == EHearthNpcThinkingTrigger::Daydream
        ? State.Persisted.LastDaydreamMonotonicSeconds : State.Persisted.LastApiMonotonicSeconds;
    const double LastUtc = Trigger == EHearthNpcThinkingTrigger::Daydream
        ? State.Persisted.LastDaydreamUtcSeconds : State.Persisted.LastApiUtcSeconds;
    const double TriggerRemaining = LastMonotonic < 0.0 && LastUtc < 0.0 ? 0.0
        : FMath::Max(0.0, MinSpacing(Trigger) - SecondsSince(Now.MonotonicSeconds, Now.UtcSeconds, LastMonotonic, LastUtc));
    if (Trigger != EHearthNpcThinkingTrigger::Daydream) return TriggerRemaining;

    // Daydream has its own six-hour cadence, while the ordinary short paid
    // spacing still prevents a second request immediately after any API call.
    const double LastApiMonotonic = State.Persisted.LastApiMonotonicSeconds;
    const double LastApiUtc = State.Persisted.LastApiUtcSeconds;
    const double GlobalRemaining = LastApiMonotonic < 0.0 && LastApiUtc < 0.0 ? 0.0
        : FMath::Max(0.0, ActualInteractionMinSpacingSeconds
            - SecondsSince(Now.MonotonicSeconds, Now.UtcSeconds, LastApiMonotonic, LastApiUtc));
    return FMath::Max(TriggerRemaining, GlobalRemaining);
}

void FHearthNpcThinkingPolicy::RefreshUrgentWindow(FResidentState& State,
    const FHearthNpcThinkingClock& Now)
{
    if (State.Persisted.UrgentDispatchCount <= 0) return;
    const double Elapsed = SecondsSince(Now.MonotonicSeconds, Now.UtcSeconds,
        State.Persisted.UrgentWindowMonotonicSeconds, State.Persisted.UrgentWindowUtcSeconds);
    if (Elapsed >= UrgentWindowSeconds)
    {
        State.Persisted.UrgentDispatchCount = 0;
        State.Persisted.UrgentWindowMonotonicSeconds = -1.0;
        State.Persisted.UrgentWindowUtcSeconds = -1.0;
    }
}

bool FHearthNpcThinkingPolicy::UrgentAllowed(const FResidentState& State) const
{
    return State.Persisted.UrgentDispatchCount < MaxUrgentDispatchesPerWindow;
}

void FHearthNpcThinkingPolicy::RememberEvent(FResidentState& State, const FString& EventId)
{
    if (EventId.IsEmpty() || State.SeenEventIds.Contains(EventId)) return;
    State.SeenEventIds.Add(EventId);
    State.Persisted.SeenEventIds.Add(EventId);
    while (State.Persisted.SeenEventIds.Num() > MaxRememberedEventIdsPerResident)
    {
        const FString Oldest = State.Persisted.SeenEventIds[0];
        State.Persisted.SeenEventIds.RemoveAt(0);
        State.SeenEventIds.Remove(Oldest);
    }
}

FHearthNpcThinkingAdmissionResult FHearthNpcThinkingPolicy::Submit(
    const FHearthNpcThinkingRequest& InRequest, const FHearthNpcThinkingClock& Now)
{
    FHearthNpcThinkingAdmissionResult Result;
    FHearthNpcThinkingRequest Request = InRequest;
    Request.ResidentId.TrimStartAndEndInline();
    Request.EventId.TrimStartAndEndInline();
    Request.CoalesceKey.TrimStartAndEndInline();
    // Urgent dispatch is reserved for explicit important events. Routine
    // survival pressure must stay on the ordinary real-time cadence.
    if(Request.Trigger != EHearthNpcThinkingTrigger::ImportantEvent) Request.bUrgent=false;
    if (Request.ResidentId.IsEmpty() || Request.EventId.IsEmpty() || !Now.IsValid()
        || !IsFiniteNonNegativeOrUnset(Request.DeadlineUtcSeconds))
    {
        Result.Status = EHearthNpcThinkingAdmission::InvalidRequest;
        Result.bUseLocalBehavior = true;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("resident, idempotent event, and real clocks are required");
        return Result;
    }
    if (Request.DeadlineUtcSeconds > 0.0 && Now.UtcSeconds >= Request.DeadlineUtcSeconds)
    {
        Result.Status = EHearthNpcThinkingAdmission::ExpiredDeadline;
        Result.bUseLocalBehavior = true;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("the UTC deadline has passed");
        return Result;
    }
    FResidentState& State = StateFor(Request.ResidentId, Request.Role);
    Result.RequestId = MakeRequestId(Request);
    if (UncertainByRequestId.Contains(Result.RequestId))
    {
        Result.Status = EHearthNpcThinkingAdmission::LocalOnly;
        Result.bAccepted = true;
        Result.bUseLocalBehavior = true;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("a previous paid request was in flight at cold restore; use local fallback");
        return Result;
    }
    if (State.SeenEventIds.Contains(Request.EventId))
    {
        Result.Status = EHearthNpcThinkingAdmission::DuplicateEvent;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("the idempotent event was already queued or dispatched");
        return Result;
    }
    if (!IsApiEligible(Request.Role, Request.Trigger))
    {
        if (Request.Trigger == EHearthNpcThinkingTrigger::Routine || Request.Trigger == EHearthNpcThinkingTrigger::Daydream)
        {
            const bool bLocalDaydream = Request.Trigger == EHearthNpcThinkingTrigger::Daydream;
            const double LastMonotonic = bLocalDaydream ? State.Persisted.LastLocalDaydreamMonotonicSeconds : State.Persisted.LastLocalRoutineMonotonicSeconds;
            const double LastUtc = bLocalDaydream ? State.Persisted.LastLocalDaydreamUtcSeconds : State.Persisted.LastLocalRoutineUtcSeconds;
            const double LocalElapsed = SecondsSince(Now.MonotonicSeconds, Now.UtcSeconds,LastMonotonic,LastUtc);
            if (LastMonotonic >= 0.0 || LastUtc >= 0.0)
            {
                const double MinLocalSpacing = bLocalDaydream ? DaydreamMinSpacingSeconds : RoutineMinSpacingSeconds;
                if (FMath::Max(0.0, MinLocalSpacing-LocalElapsed) > 0.0)
                {
                    Result.Status = EHearthNpcThinkingAdmission::LocalRateLimited;
                    Result.Provenance = LocalBehaviorProvenance();
                    Result.Reason = bLocalDaydream ? TEXT("local daydream is limited to a few attempts per real day")
                        : TEXT("local routine is limited to one attempt per thirty real minutes");
                    return Result;
                }
            }
            if (bLocalDaydream)
            {
                State.Persisted.LastLocalDaydreamMonotonicSeconds = Now.MonotonicSeconds;
                State.Persisted.LastLocalDaydreamUtcSeconds = Now.UtcSeconds;
            }
            else
            {
                State.Persisted.LastLocalRoutineMonotonicSeconds = Now.MonotonicSeconds;
                State.Persisted.LastLocalRoutineUtcSeconds = Now.UtcSeconds;
            }
        }
        Result.Status = EHearthNpcThinkingAdmission::LocalOnly;
        Result.bAccepted = true;
        Result.bUseLocalBehavior = true;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("this role uses deterministic local behavior for routine thought");
        RememberEvent(State, Request.EventId);
        return Result;
    }

    const FString CoalesceKey = MakeCoalesceKey(Request);
    if (State.PendingCoalesceKeys.Contains(CoalesceKey))
    {
        RememberEvent(State, Request.EventId);
        for (FQueuedRequest& Existing : Pending)
        {
            if (MakeCoalesceKey(Existing.Request) != CoalesceKey) continue;
            Existing.Request.bUrgent |= Request.bUrgent;
            if (Priority(Request) > Priority(Existing.Request)) Existing.Request.Trigger = Request.Trigger;
            if (Existing.Request.DeadlineUtcSeconds <= 0.0 || (Request.DeadlineUtcSeconds > 0.0 && Request.DeadlineUtcSeconds < Existing.Request.DeadlineUtcSeconds))
                Existing.Request.DeadlineUtcSeconds = Request.DeadlineUtcSeconds;
            break;
        }
        Result.Status = EHearthNpcThinkingAdmission::Coalesced;
        Result.bAccepted = true;
        Result.bPaidKimiEligible = true;
        Result.Provenance = PaidKimiProvenance();
        Result.Reason = TEXT("an equivalent pending event was coalesced");
        return Result;
    }
    if (Pending.Num() + InFlightByResident.Num() + UncertainByRequestId.Num() >= MaxPendingRequests)
    {
        Result.Status = EHearthNpcThinkingAdmission::QueueFull;
        Result.bUseLocalBehavior = true;
        Result.Provenance = LocalBehaviorProvenance();
        Result.Reason = TEXT("bounded thought queue is full");
        return Result;
    }

    FQueuedRequest Queued;
    const FString NormalizedEventId = Request.EventId;
    Queued.Request = MoveTemp(Request);
    Queued.RequestId = Result.RequestId;
    Queued.Sequence = NextSequence++;
    Pending.Add(MoveTemp(Queued));
    State.PendingCoalesceKeys.Add(CoalesceKey);
    RememberEvent(State, NormalizedEventId);
    Result.Status = SuspendedUntilUtcSeconds > Now.UtcSeconds ? EHearthNpcThinkingAdmission::QueuedSuspended
        : EHearthNpcThinkingAdmission::Queued;
    Result.bAccepted = true;
    Result.bPaidKimiEligible = true;
    Result.Provenance = PaidKimiProvenance();
    Result.Reason = Result.Status == EHearthNpcThinkingAdmission::QueuedSuspended
        ? TEXT("queued until the host UTC suspension deadline") : TEXT("queued for the bounded paid gateway");
    return Result;
}

bool FHearthNpcThinkingPolicy::TryDequeueReady(const FHearthNpcThinkingClock& Now,
    FHearthNpcThinkingRequest& OutRequest, FString& OutRequestId)
{
    if (InFlightByResident.Num() >= MaxConcurrentRequests || !Now.IsValid() || SuspendedUntilUtcSeconds > Now.UtcSeconds) return false;
    int32 BestIndex = INDEX_NONE;
    int32 BestPriority = MIN_int32;
    uint64 BestSequence = ~uint64(0);
    for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
    {
        FQueuedRequest& Candidate = Pending[Index];
        if (Candidate.Request.DeadlineUtcSeconds > 0.0 && Now.UtcSeconds >= Candidate.Request.DeadlineUtcSeconds)
        {
            RemovePendingAt(Index);
            continue;
        }
        if (Candidate.Request.Trigger != EHearthNpcThinkingTrigger::ImportantEvent)
            Candidate.Request.bUrgent=false;
        FResidentState* State = ResidentStates.Find(Candidate.Request.ResidentId);
        if (!State) { RemovePendingAt(Index); continue; }
        if (InFlightByResident.Contains(Candidate.Request.ResidentId)) continue;
        RefreshUrgentWindow(*State, Now);
        if (SpacingRemaining(*State, Candidate.Request.Trigger, Now) > 0.0) continue;
        if (Candidate.Request.bUrgent && !UrgentAllowed(*State)) continue;
        const int32 CandidatePriority = Priority(Candidate.Request);
        if (CandidatePriority > BestPriority || (CandidatePriority == BestPriority && Candidate.Sequence < BestSequence))
        {
            BestIndex = Index; BestPriority = CandidatePriority; BestSequence = Candidate.Sequence;
        }
    }
    if (BestIndex == INDEX_NONE) return false;

    // RemovePendingAt must see the original request in order to release its
    // coalescing key; copying this small queue entry avoids moving it first.
    FQueuedRequest Selected = Pending[BestIndex];
    RemovePendingAt(BestIndex);
    FResidentState& State = StateFor(Selected.Request.ResidentId, Selected.Request.Role);
    State.Persisted.LastApiMonotonicSeconds = Now.MonotonicSeconds;
    State.Persisted.LastApiUtcSeconds = Now.UtcSeconds;
    if (Selected.Request.Trigger == EHearthNpcThinkingTrigger::Daydream)
    {
        State.Persisted.LastDaydreamMonotonicSeconds = Now.MonotonicSeconds;
        State.Persisted.LastDaydreamUtcSeconds = Now.UtcSeconds;
    }
    if (Selected.Request.bUrgent)
    {
        RefreshUrgentWindow(State, Now);
        if (State.Persisted.UrgentDispatchCount == 0)
        {
            State.Persisted.UrgentWindowMonotonicSeconds = Now.MonotonicSeconds;
            State.Persisted.UrgentWindowUtcSeconds = Now.UtcSeconds;
        }
        ++State.Persisted.UrgentDispatchCount;
    }
    OutRequest = Selected.Request;
    OutRequestId = Selected.RequestId;
    InFlightByResident.Add(Selected.Request.ResidentId, MoveTemp(Selected));
    return true;
}

bool FHearthNpcThinkingPolicy::Complete(const FString& RequestId,
    const FHearthNpcThinkingClock& Now, const bool)
{
    if (RequestId.IsEmpty() || !Now.IsValid()) return false;
    for (auto It=InFlightByResident.CreateIterator(); It; ++It)
    {
        if (It.Value().RequestId != RequestId) continue;
        It.RemoveCurrent();
        return true;
    }
    return false;
}

bool FHearthNpcThinkingPolicy::DiscardPending(const FString& ResidentId,
    const FString& EventIdOrCoalesceKey)
{
    FString NormalizedResidentId = ResidentId;
    FString NormalizedTarget = EventIdOrCoalesceKey;
    NormalizedResidentId.TrimStartAndEndInline();
    NormalizedTarget.TrimStartAndEndInline();
    if (NormalizedResidentId.IsEmpty() || NormalizedTarget.IsEmpty()) return false;

    for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
    {
        const FHearthNpcThinkingRequest& Request = Pending[Index].Request;
        if (Request.ResidentId != NormalizedResidentId) continue;

        // Accept the raw event id or raw coalesce key supplied by the host.
        // The fully qualified key is accepted too because it is the policy's
        // internal identity and remains unambiguous across residents.
        const bool bMatches = Request.EventId == NormalizedTarget
            || Request.CoalesceKey == NormalizedTarget
            || MakeCoalesceKey(Request) == NormalizedTarget;
        if (!bMatches) continue;

        // RemovePendingAt only releases the pending coalesce key. It does not
        // alter SeenEventIds, cooldown timestamps, or any in-flight request.
        RemovePendingAt(Index);
        return true;
    }
    return false;
}

void FHearthNpcThinkingPolicy::SuspendUntilUtc(const double UtcSeconds)
{
    SuspendedUntilUtcSeconds = FMath::IsFinite(UtcSeconds) && UtcSeconds > 0.0 ? UtcSeconds : -1.0;
}

void FHearthNpcThinkingPolicy::ClearSuspension()
{
    SuspendedUntilUtcSeconds = -1.0;
}

bool FHearthNpcThinkingPolicy::HasUncertainEvent(const FString& ResidentId, const FString& EventId) const
{
    return UncertainByRequestId.Contains(ResidentId+TEXT("|")+EventId);
}

bool FHearthNpcThinkingPolicy::AcknowledgeUncertainEvent(const FString& ResidentId, const FString& EventId)
{
    return UncertainByRequestId.Remove(ResidentId+TEXT("|")+EventId) > 0;
}

void FHearthNpcThinkingPolicy::RemovePendingAt(const int32 Index)
{
    if (!Pending.IsValidIndex(Index)) return;
    if (FResidentState* State = ResidentStates.Find(Pending[Index].Request.ResidentId))
        State->PendingCoalesceKeys.Remove(MakeCoalesceKey(Pending[Index].Request));
    Pending.RemoveAt(Index);
}

void FHearthNpcThinkingPolicy::ExportPersistence(FHearthNpcThinkingPersistence& Out) const
{
    Out = FHearthNpcThinkingPersistence();
    Out.SuspendedUntilUtcSeconds = SuspendedUntilUtcSeconds;
    for (const auto& Pair : ResidentStates) Out.Residents.Add(Pair.Value.Persisted);
    for (const FQueuedRequest& Queued : Pending) Out.Pending.Add(Queued.Request);
    for (const auto& Pair : InFlightByResident) Out.UncertainInFlight.Add(Pair.Value.Request);
    for (const auto& Pair : UncertainByRequestId) Out.UncertainInFlight.Add(Pair.Value);
}

bool FHearthNpcThinkingPolicy::ImportPersistence(const FHearthNpcThinkingPersistence& In, FString& OutError)
{
    OutError.Empty();
    ResidentStates.Reset(); Pending.Reset(); InFlightByResident.Reset(); UncertainByRequestId.Reset(); NextSequence = 1;
    if (!IsFiniteNonNegativeOrUnset(In.SuspendedUntilUtcSeconds)
        || In.Pending.Num() + In.UncertainInFlight.Num() > MaxPendingRequests
        || In.UncertainInFlight.Num() > MaxConcurrentRequests)
    { OutError = TEXT("invalid suspension or bounded queue state"); return false; }
    SuspendedUntilUtcSeconds = In.SuspendedUntilUtcSeconds;
    for (const FHearthNpcThinkingResidentPersistence& Persisted : In.Residents)
    {
        if (Persisted.ResidentId.IsEmpty() || ResidentStates.Contains(Persisted.ResidentId)
            || Persisted.UrgentDispatchCount < 0 || Persisted.UrgentDispatchCount > MaxUrgentDispatchesPerWindow)
        { OutError = TEXT("invalid or duplicate resident persistence"); return false; }
        if (!IsFiniteNonNegativeOrUnset(Persisted.LastApiMonotonicSeconds) || !IsFiniteNonNegativeOrUnset(Persisted.LastApiUtcSeconds)
            || !IsFiniteNonNegativeOrUnset(Persisted.LastDaydreamMonotonicSeconds) || !IsFiniteNonNegativeOrUnset(Persisted.LastDaydreamUtcSeconds)
            || !IsFiniteNonNegativeOrUnset(Persisted.UrgentWindowMonotonicSeconds) || !IsFiniteNonNegativeOrUnset(Persisted.UrgentWindowUtcSeconds)
            || !IsFiniteNonNegativeOrUnset(Persisted.LastLocalRoutineMonotonicSeconds) || !IsFiniteNonNegativeOrUnset(Persisted.LastLocalRoutineUtcSeconds)
            || !IsFiniteNonNegativeOrUnset(Persisted.LastLocalDaydreamMonotonicSeconds) || !IsFiniteNonNegativeOrUnset(Persisted.LastLocalDaydreamUtcSeconds))
        { OutError = TEXT("resident persistence contains an invalid timestamp"); return false; }
        if ((Persisted.LastDaydreamMonotonicSeconds >= 0.0) != (Persisted.LastDaydreamUtcSeconds >= 0.0))
        { OutError = TEXT("resident persistence contains an incomplete daydream timestamp"); return false; }
        FResidentState& State = ResidentStates.Add(Persisted.ResidentId);
        State.Persisted = Persisted;
        if (Persisted.SeenEventIds.Num() > MaxRememberedEventIdsPerResident)
        { OutError = TEXT("resident event history exceeds its bound"); return false; }
        for (const FString& EventId : Persisted.SeenEventIds)
        {
            if (EventId.IsEmpty() || State.SeenEventIds.Contains(EventId)) { OutError = TEXT("resident event history contains duplicates"); return false; }
            State.SeenEventIds.Add(EventId);
        }
    }
    TSet<FString> PendingEventIds;
    auto AddPending=[this,&OutError,&PendingEventIds](const FHearthNpcThinkingRequest& Request)
    {
        if (OutError.Len() > 0 || Request.ResidentId.IsEmpty() || Request.EventId.IsEmpty() || !IsFiniteNonNegativeOrUnset(Request.DeadlineUtcSeconds))
        { OutError = TEXT("invalid pending thought request"); return; }
        if (!IsApiEligible(Request.Role,Request.Trigger) || PendingEventIds.Contains(Request.ResidentId+TEXT("|")+Request.EventId))
        { OutError = TEXT("pending thought request is local-only or not idempotent"); return; }
        FResidentState& State = StateFor(Request.ResidentId,Request.Role);
        if (State.PendingCoalesceKeys.Contains(MakeCoalesceKey(Request)))
        { OutError = TEXT("pending thought requests are not idempotent"); return; }
        FQueuedRequest Queued; Queued.Request=Request; Queued.RequestId=MakeRequestId(Request); Queued.Sequence=NextSequence++;
        Pending.Add(MoveTemp(Queued)); State.PendingCoalesceKeys.Add(MakeCoalesceKey(Request));
        PendingEventIds.Add(Request.ResidentId+TEXT("|")+Request.EventId);
        if (!State.SeenEventIds.Contains(Request.EventId)) RememberEvent(State,Request.EventId);
    };
    for (const FHearthNpcThinkingRequest& Request : In.Pending) AddPending(Request);
    if (OutError.Len() == 0)
    {
        for (const FHearthNpcThinkingRequest& Request : In.UncertainInFlight)
        {
            if (Request.ResidentId.IsEmpty() || Request.EventId.IsEmpty() || !IsApiEligible(Request.Role,Request.Trigger))
            { OutError = TEXT("invalid uncertain in-flight request"); break; }
            const FString Id=MakeRequestId(Request);
            FResidentState& State=StateFor(Request.ResidentId,Request.Role);
            if (UncertainByRequestId.Contains(Id) || PendingEventIds.Contains(Id) || State.PendingCoalesceKeys.Contains(MakeCoalesceKey(Request)))
            { OutError = TEXT("duplicate uncertain in-flight request"); break; }
            UncertainByRequestId.Add(Id,Request);
            if (!State.SeenEventIds.Contains(Request.EventId)) RememberEvent(State,Request.EventId);
        }
    }
    if (OutError.Len() > 0) { ResidentStates.Reset(); Pending.Reset(); InFlightByResident.Reset(); UncertainByRequestId.Reset(); return false; }
    bColdRestore = true;
    return true;
}

const TCHAR* FHearthNpcThinkingPolicy::RoleName(const EHearthNpcThinkingRole Role)
{
    switch (Role)
    {
    case EHearthNpcThinkingRole::Gatekeeper: return TEXT("gatekeeper");
    case EHearthNpcThinkingRole::RoyalGuard: return TEXT("royal_guard");
    case EHearthNpcThinkingRole::Carter: return TEXT("carter");
    default: return TEXT("civilian");
    }
}

const TCHAR* FHearthNpcThinkingPolicy::TriggerName(const EHearthNpcThinkingTrigger Trigger)
{
    switch (Trigger)
    {
    case EHearthNpcThinkingTrigger::Routine: return TEXT("routine");
    case EHearthNpcThinkingTrigger::ActualInteraction: return TEXT("actual_interaction");
    case EHearthNpcThinkingTrigger::ImportantEvent: return TEXT("important_event");
    default: return TEXT("daydream");
    }
}

const TCHAR* FHearthNpcThinkingPolicy::PaidKimiProvenance()
{
    return TEXT("kimi_paid_gateway_eligible");
}

const TCHAR* FHearthNpcThinkingPolicy::LocalBehaviorProvenance()
{
    return TEXT("local_deterministic_behavior");
}
