#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
#include "HearthNpcThinkingPolicy.h"
#include "Misc/AutomationTest.h"

namespace
{
    FHearthNpcThinkingClock Clock(const double Monotonic, const double Utc)
    {
        FHearthNpcThinkingClock Result; Result.MonotonicSeconds=Monotonic; Result.UtcSeconds=Utc; return Result;
    }

    FHearthNpcThinkingRequest Request(const TCHAR* Resident, const TCHAR* Event,
        EHearthNpcThinkingRole Role, EHearthNpcThinkingTrigger Trigger,
        const TCHAR* Coalesce=nullptr, bool bUrgent=false)
    {
        FHearthNpcThinkingRequest Result; Result.ResidentId=Resident; Result.EventId=Event; Result.Role=Role;
        Result.Trigger=Trigger; Result.bUrgent=bUrgent; if(Coalesce) Result.CoalesceKey=Coalesce; return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingRolePolicyTest,
    "ThreeHearths.NpcThinking.RolePacingAndLocalGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingRolePolicyTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Policy; Policy.BeginSession(Clock(100.0,1000.0),false);
    auto GateRoutine=Policy.Submit(Request(TEXT("gate"),TEXT("guard-shift"),EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Routine),Clock(100.0,1000.0));
    TestEqual(TEXT("Gatekeeper routine stays local"),GateRoutine.Status,EHearthNpcThinkingAdmission::LocalOnly);
    TestTrue(TEXT("Gatekeeper local provenance is explicit"),GateRoutine.bUseLocalBehavior && GateRoutine.Provenance==FHearthNpcThinkingPolicy::LocalBehaviorProvenance());
    auto GateDaydream=Policy.Submit(Request(TEXT("gate"),TEXT("daydream-1"),EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),Clock(100.0,1000.0));
    TestTrue(TEXT("Gatekeeper daydream can use the paid gateway"),GateDaydream.bAccepted && GateDaydream.bPaidKimiEligible);
    FHearthNpcThinkingRequest Out; FString RequestId;
    TestTrue(TEXT("Gatekeeper daydream dispatches"),Policy.TryDequeueReady(Clock(100.0,1000.0),Out,RequestId));
    TestTrue(TEXT("Gatekeeper daydream completion clears in-flight"),Policy.Complete(RequestId,Clock(101.0,1001.0),true));
    auto GateDaydreamSoon=Policy.Submit(Request(TEXT("gate"),TEXT("daydream-2"),EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),Clock(101.0,1001.0));
    TestTrue(TEXT("Gatekeeper daydream remains queued during its six-hour spacing"),GateDaydreamSoon.bAccepted
        && !Policy.TryDequeueReady(Clock(100.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0,
            1000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("Gatekeeper daydream reopens after six real hours"),Policy.TryDequeueReady(
        Clock(100.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds,
            1000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds),Out,RequestId));
    Policy.Complete(RequestId,Clock(100.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0,
        1000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0),true);

    auto GuardRoutine=Policy.Submit(Request(TEXT("guard"),TEXT("review-1"),EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::Routine),Clock(100.0,1000.0));
    TestEqual(TEXT("Royal guard routine uses local behavior"),GuardRoutine.Status,EHearthNpcThinkingAdmission::LocalOnly);
    auto TooSoonLocal=Policy.Submit(Request(TEXT("guard"),TEXT("review-2"),EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::Routine),Clock(101.0,1001.0));
    TestEqual(TEXT("Guard routine is capped at thirty real minutes"),TooSoonLocal.Status,EHearthNpcThinkingAdmission::LocalRateLimited);
    auto LaterLocal=Policy.Submit(Request(TEXT("guard"),TEXT("review-3"),EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::Routine),Clock(100.0+FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds+1.0,1000.0+FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds+1.0));
    TestEqual(TEXT("Guard local routine reopens after thirty real minutes"),LaterLocal.Status,EHearthNpcThinkingAdmission::LocalOnly);

    auto GuardInteraction=Policy.Submit(Request(TEXT("guard"),TEXT("interaction-1"),EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::ActualInteraction),Clock(100.0,1000.0));
    TestTrue(TEXT("Actual guard interaction is eligible for the paid gateway"),GuardInteraction.bAccepted && GuardInteraction.bPaidKimiEligible);
    TestTrue(TEXT("First real interaction can dispatch"),Policy.TryDequeueReady(Clock(100.0,1000.0),Out,RequestId));
    TestTrue(TEXT("Single resident request has the same idempotent event"),Out.EventId==TEXT("interaction-1") && !RequestId.IsEmpty() && Policy.HasInFlight());
    TestTrue(TEXT("Gateway completion clears in-flight"),Policy.Complete(RequestId,Clock(101.0,1001.0),true) && !Policy.HasInFlight());

    FHearthNpcThinkingPolicy CivilianRoutine;
    auto CarterRoutine = CivilianRoutine.Submit(Request(TEXT("carter"),TEXT("route-1"),
        EHearthNpcThinkingRole::Carter,EHearthNpcThinkingTrigger::Routine),Clock(0.0,2000.0));
    TestTrue(TEXT("Carter routine can use the paid gateway"),CarterRoutine.bAccepted && CarterRoutine.bPaidKimiEligible);
    TestTrue(TEXT("Carter routine dispatches"),CivilianRoutine.TryDequeueReady(Clock(0.0,2000.0),Out,RequestId));
    CivilianRoutine.Complete(RequestId,Clock(1.0,2001.0),true);
    auto CarterRoutineSoon = CivilianRoutine.Submit(Request(TEXT("carter"),TEXT("route-2"),
        EHearthNpcThinkingRole::Carter,EHearthNpcThinkingTrigger::Routine),Clock(1.0,2001.0));
    TestTrue(TEXT("Carter routine retains a request during the thirty-minute spacing"),CarterRoutineSoon.bAccepted
        && !CivilianRoutine.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds-1.0,
            2000.0+FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds-1.0),Out,RequestId));

    FHearthNpcThinkingPolicy CivilianRoutineRole;
    auto CivilianRoutineResult = CivilianRoutineRole.Submit(Request(TEXT("civilian-routine"),TEXT("work-1"),
        EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Routine),Clock(0.0,3000.0));
    TestTrue(TEXT("Civilian routine can use the paid gateway"),CivilianRoutineResult.bAccepted && CivilianRoutineResult.bPaidKimiEligible);
    TestTrue(TEXT("Civilian routine dispatches"),CivilianRoutineRole.TryDequeueReady(Clock(0.0,3000.0),Out,RequestId));
    CivilianRoutineRole.Complete(RequestId,Clock(1.0,3001.0),true);
    auto CivilianRoutineSoon = CivilianRoutineRole.Submit(Request(TEXT("civilian-routine"),TEXT("work-2"),
        EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Routine),Clock(1.0,3001.0));
    TestTrue(TEXT("Civilian routine retains a request during the thirty-minute spacing"),CivilianRoutineSoon.bAccepted
        && !CivilianRoutineRole.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds-1.0,
            3000.0+FHearthNpcThinkingPolicy::RoutineMinSpacingSeconds-1.0),Out,RequestId));

    FHearthNpcThinkingPolicy Daydream; Daydream.BeginSession(Clock(0.0,5000.0),false);
    auto Dream1=Daydream.Submit(Request(TEXT("civilian"),TEXT("dream-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Daydream),Clock(0.0,5000.0));
    TestTrue(TEXT("Civilian daydream is paid eligible but queued"),Dream1.bPaidKimiEligible && Daydream.TryDequeueReady(Clock(0.0,5000.0),Out,RequestId));
    Daydream.Complete(RequestId,Clock(1.0,5001.0),true);
    auto Dream2=Daydream.Submit(Request(TEXT("civilian"),TEXT("dream-2"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Daydream),Clock(1.0,5001.0));
    TestTrue(TEXT("Daydream repeats are retained"),Dream2.bAccepted && !Daydream.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0,5000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("Daydream reopens after six real hours"),Daydream.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0,5000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0),Out,RequestId));
    Daydream.Complete(RequestId,Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+2.0,5000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+2.0),true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingEventSafetyTest,
    "ThreeHearths.NpcThinking.EventCoalescingAndUrgentCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingEventSafetyTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Policy; Policy.BeginSession(Clock(0.0,1000.0),false);
    auto First=Policy.Submit(Request(TEXT("citizen"),TEXT("talk-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction,TEXT("market")),Clock(0.0,1000.0));
    auto Duplicate=Policy.Submit(Request(TEXT("citizen"),TEXT("talk-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction,TEXT("market")),Clock(0.0,1000.0));
    auto Coalesced=Policy.Submit(Request(TEXT("citizen"),TEXT("talk-2"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction,TEXT("market")),Clock(0.0,1000.0));
    TestTrue(TEXT("First event is queued"),First.bAccepted && First.Status==EHearthNpcThinkingAdmission::Queued);
    TestEqual(TEXT("Same event id is idempotent"),Duplicate.Status,EHearthNpcThinkingAdmission::DuplicateEvent);
    TestEqual(TEXT("Same resident event key coalesces"),Coalesced.Status,EHearthNpcThinkingAdmission::Coalesced);
    TestEqual(TEXT("Coalescing keeps one queue item"),Policy.PendingCount(),1);

    FHearthNpcThinkingRequest Out; FString RequestId;
    TestTrue(TEXT("Coalesced interaction dispatches once"),Policy.TryDequeueReady(Clock(0.0,1000.0),Out,RequestId));
    Policy.Complete(RequestId,Clock(1.0,1001.0),true);
    for(int32 I=0;I<FHearthNpcThinkingPolicy::MaxUrgentDispatchesPerWindow;++I)
    {
        const double T=2000.0+I*FHearthNpcThinkingPolicy::ImportantEventMinSpacingSeconds+1.0;
        auto Urgent=Policy.Submit(Request(TEXT("urgent"),*FString::Printf(TEXT("event-%d"),I),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ImportantEvent,nullptr,true),Clock(T,3000.0+T));
        TestTrue(TEXT("Urgent event is accepted within the window"),Urgent.bAccepted);
        TestTrue(TEXT("Urgent event dispatches"),Policy.TryDequeueReady(Clock(T,3000.0+T),Out,RequestId));
        Policy.Complete(RequestId,Clock(T+1.0,3001.0+T),true);
    }
    auto Capped=Policy.Submit(Request(TEXT("urgent"),TEXT("event-cap"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ImportantEvent,nullptr,true),Clock(2200.0,5200.0));
    TestTrue(TEXT("Fourth urgent event remains queued under rate cap"),Capped.bAccepted && !Policy.TryDequeueReady(Clock(2200.0,5200.0),Out,RequestId));
    TestTrue(TEXT("Urgent queue reopens after the real window"),Policy.TryDequeueReady(Clock(2000.0+FHearthNpcThinkingPolicy::UrgentWindowSeconds+1.0,4000.0+FHearthNpcThinkingPolicy::UrgentWindowSeconds+1.0),Out,RequestId));
    Policy.Complete(RequestId,Clock(3000.0,6000.0),true);

    FHearthNpcThinkingPolicy Concurrent;
    for(int32 I=0;I<FHearthNpcThinkingPolicy::MaxConcurrentRequests;++I)
    {
        const FString Resident=FString::Printf(TEXT("resident-%d"),I);
        const FString Event=FString::Printf(TEXT("interaction-%d"),I);
        auto Result=Concurrent.Submit(Request(*Resident,*Event,EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(0.0,9000.0));
        TestTrue(TEXT("Different residents can queue independently"),Result.bAccepted);
    }
    for(int32 I=0;I<FHearthNpcThinkingPolicy::MaxConcurrentRequests;++I)
    {
        TestTrue(TEXT("Each resident may have one in-flight request"),Concurrent.TryDequeueReady(Clock(0.0,9000.0),Out,RequestId));
    }
    TestEqual(TEXT("Global concurrency is capped at ten"),Concurrent.InFlightCount(),FHearthNpcThinkingPolicy::MaxConcurrentRequests);
    auto SameResident=Concurrent.Submit(Request(TEXT("resident-0"),TEXT("interaction-followup"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(0.0,9000.0));
    TestTrue(TEXT("A second request for one resident waits in the queue"),SameResident.bAccepted && Concurrent.PendingCount()==1);
    TestTrue(TEXT("One other resident can complete independently"),Concurrent.Complete(TEXT("resident-9|interaction-9"),Clock(1.0,9001.0),true));
    TestFalse(TEXT("The per-resident single-flight gate blocks its follow-up"),Concurrent.TryDequeueReady(Clock(0.0,9000.0),Out,RequestId));
    TestTrue(TEXT("The follow-up dispatches after that resident completes and real spacing passes"),Concurrent.Complete(TEXT("resident-0|interaction-0"),Clock(2.0,9002.0),true)
        && Concurrent.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds+1.0,9000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds+1.0),Out,RequestId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingDaydreamClockTest,
    "ThreeHearths.NpcThinking.DaydreamIndependentRealClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingDaydreamClockTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Policy;
    Policy.BeginSession(Clock(0.0,10000.0),false);
    FHearthNpcThinkingRequest Out; FString RequestId;

    auto DispatchDaydream=[this,&Policy,&Out,&RequestId](int32 Number,double At)->bool
    {
        const FString Event=FString::Printf(TEXT("daydream-%d"),Number);
        const auto Submitted=Policy.Submit(Request(TEXT("gatekeeper"),*Event,
            EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),Clock(At,10000.0+At));
        const bool Dispatched=Submitted.bAccepted && Policy.TryDequeueReady(
            Clock(At,10000.0+At),Out,RequestId);
        if (!Dispatched) return false;
        return Policy.Complete(RequestId,Clock(At+1.0,10001.0+At),true);
    };

    TestTrue(TEXT("first guard daydream dispatches"),DispatchDaydream(0,0.0));
    TestTrue(TEXT("second daydream dispatches after six real hours"),DispatchDaydream(
        1,FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds));
    TestTrue(TEXT("third daydream dispatches after another six real hours"),DispatchDaydream(
        2,2.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds));
    TestTrue(TEXT("fourth daydream dispatches within one real day"),DispatchDaydream(
        3,3.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds));

    const double BeforeDay=4.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0;
    const auto Fifth=Policy.Submit(Request(TEXT("gatekeeper"),TEXT("daydream-4"),
        EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),Clock(BeforeDay,10000.0+BeforeDay));
    TestTrue(TEXT("the fifth daydream is retained but not dispatched before six hours"),
        Fifth.bAccepted && !Policy.TryDequeueReady(Clock(BeforeDay,10000.0+BeforeDay),Out,RequestId));
    TestTrue(TEXT("the fifth daydream opens at the six-hour boundary"),Policy.TryDequeueReady(
        Clock(4.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds,
            10000.0+4.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds),Out,RequestId));
    Policy.Complete(RequestId,Clock(4.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0,
        10001.0+4.0*FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds),true);

    FHearthNpcThinkingPolicy Interaction;
    Interaction.BeginSession(Clock(0.0,20000.0),false);
    const auto Dream=Interaction.Submit(Request(TEXT("guard"),TEXT("dream-before-visitor"),
        EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::Daydream),Clock(0.0,20000.0));
    TestTrue(TEXT("guard daydream is paid eligible"),Dream.bPaidKimiEligible
        && Interaction.TryDequeueReady(Clock(0.0,20000.0),Out,RequestId));
    Interaction.Complete(RequestId,Clock(1.0,20001.0),true);
    const auto Visitor=Interaction.Submit(Request(TEXT("guard"),TEXT("visitor"),
        EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::ActualInteraction),Clock(120.0,20120.0));
    TestTrue(TEXT("real visitor interaction can dispatch after the short global spacing"),Visitor.bAccepted
        && Interaction.TryDequeueReady(Clock(120.0,20120.0),Out,RequestId));
    Interaction.Complete(RequestId,Clock(121.0,20121.0),true);
    const auto DreamAfterVisitor=Interaction.Submit(Request(TEXT("guard"),TEXT("dream-after-visitor"),
        EHearthNpcThinkingRole::RoyalGuard,EHearthNpcThinkingTrigger::Daydream),Clock(121.0,20121.0));
    TestTrue(TEXT("visitor interaction does not restart the six-hour daydream clock"),DreamAfterVisitor.bAccepted
        && !Interaction.TryDequeueReady(Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0,
            20000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("daydream dispatches at its own persisted cadence"),Interaction.TryDequeueReady(
        Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds,20000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds),Out,RequestId));
    Interaction.Complete(RequestId,Clock(FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0,
        20001.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds),true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingDaydreamPersistenceTest,
    "ThreeHearths.NpcThinking.DaydreamColdRestoreAndInvalidClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingDaydreamPersistenceTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Active;
    Active.BeginSession(Clock(10.0,30000.0),false);
    FHearthNpcThinkingRequest Out; FString RequestId;
    const auto Submitted=Active.Submit(Request(TEXT("restore-guard"),TEXT("dream-1"),
        EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),Clock(10.0,30000.0));
    TestTrue(TEXT("cold-restore fixture dispatches a daydream"),Submitted.bAccepted
        && Active.TryDequeueReady(Clock(10.0,30000.0),Out,RequestId));
    TestTrue(TEXT("cold-restore fixture completes"),Active.Complete(RequestId,Clock(11.0,30001.0),true));
    FHearthNpcThinkingPersistence Saved; Active.ExportPersistence(Saved);
    TestTrue(TEXT("paid daydream timestamps are persisted"),Saved.Residents.Num()==1
        && Saved.Residents[0].LastDaydreamMonotonicSeconds==10.0
        && Saved.Residents[0].LastDaydreamUtcSeconds==30000.0);

    FHearthNpcThinkingPolicy Restored; FString Error;
    TestTrue(TEXT("cold restore imports daydream timestamps"),Restored.ImportPersistence(Saved,Error));
    Restored.BeginSession(Clock(0.0,30000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0),true);
    const auto Next=Restored.Submit(Request(TEXT("restore-guard"),TEXT("dream-2"),
        EHearthNpcThinkingRole::Gatekeeper,EHearthNpcThinkingTrigger::Daydream),
        Clock(1.0,30000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0));
    TestTrue(TEXT("cold restore keeps daydream blocked by UTC before six hours"),Next.bAccepted
        && !Restored.TryDequeueReady(Clock(1.0,30000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("cold restore opens daydream at the UTC boundary"),Restored.TryDequeueReady(
        Clock(2.0,30000.0+FHearthNpcThinkingPolicy::DaydreamMinSpacingSeconds+1.0),Out,RequestId));

    FHearthNpcThinkingPersistence OneMissing;
    FHearthNpcThinkingResidentPersistence MissingResident;
    MissingResident.ResidentId=TEXT("bad-one-missing");
    MissingResident.LastDaydreamMonotonicSeconds=20.0;
    OneMissing.Residents.Add(MissingResident);
    FHearthNpcThinkingPolicy RejectMissing; Error.Empty();
    TestFalse(TEXT("one missing daydream timestamp is rejected"),RejectMissing.ImportPersistence(OneMissing,Error));

    FHearthNpcThinkingPersistence NonFinite;
    FHearthNpcThinkingResidentPersistence NonFiniteResident;
    NonFiniteResident.ResidentId=TEXT("bad-nonfinite");
    NonFiniteResident.LastDaydreamMonotonicSeconds=std::numeric_limits<double>::quiet_NaN();
    NonFinite.Residents.Add(NonFiniteResident);
    FHearthNpcThinkingPolicy RejectNonFinite; Error.Empty();
    TestFalse(TEXT("non-finite daydream timestamp is rejected"),RejectNonFinite.ImportPersistence(NonFinite,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingDiscardPendingTest,
    "ThreeHearths.NpcThinking.DiscardPendingPreservesInFlightAndState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingDiscardPendingTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Policy; Policy.BeginSession(Clock(0.0,1000.0),false);
    auto B = Policy.Submit(Request(TEXT("resident-b"),TEXT("event-b"),EHearthNpcThinkingRole::Civilian,
        EHearthNpcThinkingTrigger::ActualInteraction),Clock(0.0,1000.0));
    auto A = Policy.Submit(Request(TEXT("resident-a"),TEXT("event-a"),EHearthNpcThinkingRole::Civilian,
        EHearthNpcThinkingTrigger::ActualInteraction,TEXT("coalesce-a")),Clock(0.0,1000.0));
    FHearthNpcThinkingRequest Out; FString RequestId;
    TestTrue(TEXT("A and B are accepted"),B.bAccepted && A.bAccepted && Policy.PendingCount()==2);
    TestTrue(TEXT("B enters the real in-flight set"),Policy.TryDequeueReady(Clock(0.0,1000.0),Out,RequestId)
        && Out.ResidentId==TEXT("resident-b"));

    TestTrue(TEXT("DiscardPending removes A by its raw coalesce key"),Policy.DiscardPending(TEXT("resident-a"),TEXT("coalesce-a")));
    TestEqual(TEXT("Only B remains in-flight after discarding A"),Policy.InFlightCount(),1);
    TestEqual(TEXT("A is removed from the unsent queue"),Policy.PendingCount(),0);
    TestTrue(TEXT("A event identity remains remembered after discard"),Policy.Submit(
        Request(TEXT("resident-a"),TEXT("event-a"),EHearthNpcThinkingRole::Civilian,
            EHearthNpcThinkingTrigger::ActualInteraction,TEXT("coalesce-a")),Clock(1.0,1001.0)).Status
        == EHearthNpcThinkingAdmission::DuplicateEvent);

    TestTrue(TEXT("B completion still works after discarding A"),Policy.Complete(RequestId,Clock(1.0,1001.0),true));
    auto BFollowup = Policy.Submit(Request(TEXT("resident-b"),TEXT("event-b-followup"),
        EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(1.0,1001.0));
    TestTrue(TEXT("B follow-up is accepted without resetting its cooldown"),BFollowup.bAccepted);
    TestFalse(TEXT("B cooldown still blocks an immediate follow-up"),Policy.TryDequeueReady(
        Clock(FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds-1.0,
            1000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("DiscardPending also removes by event id"),Policy.DiscardPending(TEXT("resident-b"),TEXT("event-b-followup")));
    TestEqual(TEXT("Discarding the follow-up leaves no pending work"),Policy.PendingCount(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNpcThinkingPersistenceTest,
    "ThreeHearths.NpcThinking.BoundedQueueColdRestoreAndDeadlines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthNpcThinkingPersistenceTest::RunTest(const FString&)
{
    FHearthNpcThinkingPolicy Policy; Policy.BeginSession(Clock(10.0,10000.0),false);
    for(int32 I=0;I<FHearthNpcThinkingPolicy::MaxPendingRequests;++I)
    {
        auto Result=Policy.Submit(Request(TEXT("queue"),*FString::Printf(TEXT("q-%d"),I),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Daydream,*FString::Printf(TEXT("q-%d"),I)),Clock(10.0,10000.0));
        TestTrue(TEXT("Bounded queue accepts entries up to its cap"),Result.bAccepted);
    }
    auto Full=Policy.Submit(Request(TEXT("queue"),TEXT("q-overflow"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::Daydream,TEXT("q-overflow")),Clock(10.0,10000.0));
    TestEqual(TEXT("Bounded queue rejects the overflow"),Full.Status,EHearthNpcThinkingAdmission::QueueFull);

    FHearthNpcThinkingPolicy Active; Active.BeginSession(Clock(20.0,20000.0),false);
    auto Start=Active.Submit(Request(TEXT("restore"),TEXT("interaction-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(20.0,20000.0));
    FHearthNpcThinkingRequest Out; FString RequestId;
    TestTrue(TEXT("Restore fixture starts one request"),Start.bAccepted && Active.TryDequeueReady(Clock(20.0,20000.0),Out,RequestId));
    FHearthNpcThinkingPersistence Saved; Active.ExportPersistence(Saved);
    TestEqual(TEXT("Saved in-flight request is marked uncertain"),Saved.UncertainInFlight.Num(),1);
    FHearthNpcThinkingPolicy Restored; FString Error;
    TestTrue(TEXT("Cold restore accepts uncertain in-flight state"),Restored.ImportPersistence(Saved,Error));
    TestTrue(TEXT("Cold restore exposes the unknown request for local fallback"),Restored.HasUncertainEvent(TEXT("restore"),TEXT("interaction-1")));
    auto LocalFallback=Restored.Submit(Request(TEXT("restore"),TEXT("interaction-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(0.0,20000.0));
    TestTrue(TEXT("Unknown paid outcome forces local fallback"),LocalFallback.bUseLocalBehavior && LocalFallback.Status==EHearthNpcThinkingAdmission::LocalOnly);
    TestTrue(TEXT("Host can acknowledge the uncertain event"),Restored.AcknowledgeUncertainEvent(TEXT("restore"),TEXT("interaction-1")));

    FHearthNpcThinkingPolicy Completed; Completed.BeginSession(Clock(20.0,20000.0),false);
    auto CompletedStart=Completed.Submit(Request(TEXT("wall"),TEXT("interaction-1"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(20.0,20000.0));
    TestTrue(TEXT("Completed request dispatches before cold save"),CompletedStart.bAccepted && Completed.TryDequeueReady(Clock(20.0,20000.0),Out,RequestId));
    TestTrue(TEXT("Completed request leaves no unknown result"),Completed.Complete(RequestId,Clock(21.0,20001.0),true));
    FHearthNpcThinkingPersistence CompletedSaved; Completed.ExportPersistence(CompletedSaved);
    FHearthNpcThinkingPolicy WallRestored; TestTrue(TEXT("Wall-time restore imports accepted timestamps"),WallRestored.ImportPersistence(CompletedSaved,Error));
    WallRestored.BeginSession(Clock(0.0,20000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds-1.0),true);
    auto WallNext=WallRestored.Submit(Request(TEXT("wall"),TEXT("interaction-2"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ActualInteraction),Clock(1.0,20000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds-1.0));
    TestTrue(TEXT("Wall-time restore queues the next event"),WallNext.bAccepted);
    TestFalse(TEXT("Cold restore uses persisted UTC spacing, not a reset monotonic clock"),WallRestored.TryDequeueReady(Clock(1.0,20000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds-1.0),Out,RequestId));
    TestTrue(TEXT("Cold restore dispatches after the wall interval"),WallRestored.TryDequeueReady(Clock(2.0,20000.0+FHearthNpcThinkingPolicy::ActualInteractionMinSpacingSeconds+1.0),Out,RequestId));
    WallRestored.Complete(RequestId,Clock(3.0,20002.0),true);

    FHearthNpcThinkingPolicy Suspended; Suspended.SuspendUntilUtc(50000.0);
    auto Pending=Suspended.Submit(Request(TEXT("deadline"),TEXT("event"),EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ImportantEvent),Clock(0.0,49000.0));
    TestEqual(TEXT("UTC suspension is visible to the caller"),Pending.Status,EHearthNpcThinkingAdmission::QueuedSuspended);
    TestFalse(TEXT("Suspended policy does not dequeue before the root deadline"),Suspended.TryDequeueReady(Clock(1.0,49999.0),Out,RequestId));
    TestTrue(TEXT("Suspended policy resumes after the root deadline"),Suspended.TryDequeueReady(Clock(2.0,50001.0),Out,RequestId));

    FHearthNpcThinkingPolicy Deadline;
    auto SubmitAtExactDeadlineRequest = Request(TEXT("deadline-submit"),TEXT("exact-submit"),
        EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ImportantEvent);
    SubmitAtExactDeadlineRequest.DeadlineUtcSeconds = 6000.0;
    auto SubmitAtExactDeadline = Deadline.Submit(SubmitAtExactDeadlineRequest,Clock(0.0,6000.0));
    TestEqual(TEXT("A request is expired at its exact UTC deadline"),SubmitAtExactDeadline.Status,
        EHearthNpcThinkingAdmission::ExpiredDeadline);

    auto DequeueAtExactDeadlineRequest = Request(TEXT("deadline-dequeue"),TEXT("exact-dequeue"),
        EHearthNpcThinkingRole::Civilian,EHearthNpcThinkingTrigger::ImportantEvent);
    DequeueAtExactDeadlineRequest.DeadlineUtcSeconds = 7000.0;
    auto DequeueRequest = Deadline.Submit(DequeueAtExactDeadlineRequest,Clock(0.0,6000.0));
    TestTrue(TEXT("A future-deadline request is initially accepted"),DequeueRequest.bAccepted);
    TestFalse(TEXT("A queued request is removed at its exact UTC deadline"),Deadline.TryDequeueReady(
        Clock(1.0,7000.0),Out,RequestId));
    TestEqual(TEXT("The exact-deadline request leaves no pending work"),Deadline.PendingCount(),0);
    return true;
}
#endif
