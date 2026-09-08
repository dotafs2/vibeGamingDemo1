#pragma once

#include "CoreMinimal.h"

/** Pure policy roles. The policy never performs an API call. */
enum class EHearthNpcThinkingRole : uint8
{
    Gatekeeper,
    RoyalGuard,
    Carter,
    Civilian
};

enum class EHearthNpcThinkingTrigger : uint8
{
    Routine,
    ActualInteraction,
    ImportantEvent,
    Daydream
};

enum class EHearthNpcThinkingAdmission : uint8
{
    Queued,
    QueuedSuspended,
    Coalesced,
    LocalOnly,
    DuplicateEvent,
    InvalidRequest,
    QueueFull,
    ExpiredDeadline,
    LocalRateLimited
};

/** Real clocks supplied by the host. Simulation time must never be passed here. */
struct THREEHEARTHS_API FHearthNpcThinkingClock
{
    double MonotonicSeconds = -1.0;
    double UtcSeconds = -1.0;

    bool IsValid() const;
};

struct THREEHEARTHS_API FHearthNpcThinkingRequest
{
    FString ResidentId;
    EHearthNpcThinkingRole Role = EHearthNpcThinkingRole::Civilian;
    EHearthNpcThinkingTrigger Trigger = EHearthNpcThinkingTrigger::ActualInteraction;
    FString EventId;
    /** Same-key requests for one resident are coalesced while pending. */
    FString CoalesceKey;
    FString Context;
    bool bUrgent = false;
    /** Optional UTC expiry; the request expires when current UTC is >= this value. */
    double DeadlineUtcSeconds = -1.0;
};

struct THREEHEARTHS_API FHearthNpcThinkingResidentPersistence
{
    FString ResidentId;
    EHearthNpcThinkingRole Role = EHearthNpcThinkingRole::Civilian;
    double LastApiMonotonicSeconds = -1.0;
    double LastApiUtcSeconds = -1.0;
    /** Paid daydream cadence is independent of ordinary interactions. */
    double LastDaydreamMonotonicSeconds = -1.0;
    double LastDaydreamUtcSeconds = -1.0;
    double UrgentWindowMonotonicSeconds = -1.0;
    double UrgentWindowUtcSeconds = -1.0;
    int32 UrgentDispatchCount = 0;
    double LastLocalRoutineMonotonicSeconds = -1.0;
    double LastLocalRoutineUtcSeconds = -1.0;
    double LastLocalDaydreamMonotonicSeconds = -1.0;
    double LastLocalDaydreamUtcSeconds = -1.0;
    TArray<FString> SeenEventIds;
};

/** Host-owned save payload. In-flight work is marked uncertain on cold restore. */
struct THREEHEARTHS_API FHearthNpcThinkingPersistence
{
    double SuspendedUntilUtcSeconds = -1.0;
    TArray<FHearthNpcThinkingResidentPersistence> Residents;
    TArray<FHearthNpcThinkingRequest> Pending;
    /** Requests sent before a save whose paid outcome is unknown. */
    TArray<FHearthNpcThinkingRequest> UncertainInFlight;
};

struct THREEHEARTHS_API FHearthNpcThinkingAdmissionResult
{
    EHearthNpcThinkingAdmission Status = EHearthNpcThinkingAdmission::InvalidRequest;
    bool bAccepted = false;
    bool bUseLocalBehavior = false;
    bool bPaidKimiEligible = false;
    FString Provenance;
    FString RequestId;
    FString Reason;
};

/**
 * Admission and pacing policy for resident thought requests.
 *
 * It only admits, coalesces, paces, and returns work to a caller. The caller
 * owns local behavior and the actual paid gateway invocation.
 */
class THREEHEARTHS_API FHearthNpcThinkingPolicy
{
public:
    static constexpr int32 MaxPendingRequests = 16;
    static constexpr int32 MaxConcurrentRequests = 10;
    static constexpr int32 MaxRememberedEventIdsPerResident = 64;
    static constexpr int32 MaxUrgentDispatchesPerWindow = 3;
    static constexpr double UrgentWindowSeconds = 900.0;
    static constexpr double ActualInteractionMinSpacingSeconds = 120.0;
    static constexpr double ImportantEventMinSpacingSeconds = 60.0;
    static constexpr double RoutineMinSpacingSeconds = 1800.0;
    static constexpr double DaydreamMinSpacingSeconds = 21600.0;

    FHearthNpcThinkingPolicy();

    /** Preserve persisted wall timestamps across a new process/session. */
    void BeginSession(const FHearthNpcThinkingClock& Now, bool bColdRestore);

    FHearthNpcThinkingAdmissionResult Submit(const FHearthNpcThinkingRequest& Request,
        const FHearthNpcThinkingClock& Now);

    /** Returns one ready request; call repeatedly up to MaxConcurrentRequests. */
    bool TryDequeueReady(const FHearthNpcThinkingClock& Now, FHearthNpcThinkingRequest& OutRequest,
        FString& OutRequestId);

    bool Complete(const FString& RequestId, const FHearthNpcThinkingClock& Now, bool bAccepted);

    /** Remove one unsent request by resident plus event id or raw coalesce key. */
    bool DiscardPending(const FString& ResidentId, const FString& EventIdOrCoalesceKey);

    /** Hold queued work until the host's UTC deadline passes. */
    void SuspendUntilUtc(double UtcSeconds);
    void ClearSuspension();
    double SuspendedUntilUtc() const { return SuspendedUntilUtcSeconds; }

    bool HasInFlight() const { return InFlightByResident.Num() > 0; }
    int32 InFlightCount() const { return InFlightByResident.Num(); }
    int32 PendingCount() const { return Pending.Num(); }

    bool HasUncertainEvent(const FString& ResidentId, const FString& EventId) const;
    bool AcknowledgeUncertainEvent(const FString& ResidentId, const FString& EventId);

    void ExportPersistence(FHearthNpcThinkingPersistence& Out) const;
    bool ImportPersistence(const FHearthNpcThinkingPersistence& In, FString& OutError);

    static const TCHAR* RoleName(EHearthNpcThinkingRole Role);
    static const TCHAR* TriggerName(EHearthNpcThinkingTrigger Trigger);
    static const TCHAR* PaidKimiProvenance();
    static const TCHAR* LocalBehaviorProvenance();

private:
    struct FResidentState
    {
        FHearthNpcThinkingResidentPersistence Persisted;
        TSet<FString> SeenEventIds;
        TSet<FString> PendingCoalesceKeys;
    };

    struct FQueuedRequest
    {
        FHearthNpcThinkingRequest Request;
        FString RequestId;
        uint64 Sequence = 0;
    };

    TMap<FString, FResidentState> ResidentStates;
    TArray<FQueuedRequest> Pending;
    TMap<FString, FQueuedRequest> InFlightByResident;
    TMap<FString, FHearthNpcThinkingRequest> UncertainByRequestId;
    uint64 NextSequence = 1;
    double SuspendedUntilUtcSeconds = -1.0;
    bool bColdRestore = false;

    FResidentState& StateFor(const FString& ResidentId, EHearthNpcThinkingRole Role);
    static FString MakeRequestId(const FHearthNpcThinkingRequest& Request);
    static FString MakeCoalesceKey(const FHearthNpcThinkingRequest& Request);
    static bool IsApiEligible(EHearthNpcThinkingRole Role, EHearthNpcThinkingTrigger Trigger);
    static double MinSpacing(EHearthNpcThinkingTrigger Trigger);
    static int32 Priority(const FHearthNpcThinkingRequest& Request);
    static bool IsFiniteNonNegativeOrUnset(double Value);
    double SecondsSince(double NowMonotonic, double NowUtc, double LastMonotonic,
        double LastUtc) const;
    double SpacingRemaining(const FResidentState& State, EHearthNpcThinkingTrigger Trigger,
        const FHearthNpcThinkingClock& Now) const;
    void RefreshUrgentWindow(FResidentState& State, const FHearthNpcThinkingClock& Now);
    bool UrgentAllowed(const FResidentState& State) const;
    void RememberEvent(FResidentState& State, const FString& EventId);
    void RemovePendingAt(int32 Index);
};
