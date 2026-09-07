#if WITH_DEV_AUTOMATION_TESTS
#include "HearthTavernRuntime.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTavernRuntimeTest,"ThreeHearths.Tavern.RequestBuildSeatsAndUse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthTavernRuntimeTest::RunTest(const FString&)
{
    TArray<FHearthWorldRequest> Requests; FString Error;
    TestTrue(TEXT("Owner can submit one idempotent canopy need"),HearthTavernRuntime::ProposeNeed(Requests,TEXT("innkeeper.A"),TEXT("site.inn"),TEXT("plan.inn"),Error));
    TestTrue(TEXT("Repeated native trigger keeps one request"),HearthTavernRuntime::ProposeNeed(Requests,TEXT("innkeeper.A"),TEXT("site.inn"),TEXT("plan.inn"),Error));
    TestEqual(TEXT("One persisted need remains"),Requests.Num(),1);
    TestEqual(TEXT("Legacy canopy request remains an asset proposal"),Requests[0].Category,FString(TEXT("asset")));
    TestEqual(TEXT("A named venue does not approve a construction request"),Requests[0].Status,FString(TEXT("proposed")));

    FHearthStructurePlan Plan; Plan.PlanId=TEXT("plan.inn"); Plan.Reasons.Need=TEXT("tavern_canopy_seating");
    struct FTestPart { const TCHAR* Key; const TCHAR* Catalog; };
    const FTestPart Parts[]={{TEXT("deck"),TEXT("floor_timber_2m")},{TEXT("post_a"),TEXT("post_timber_2_4m")},{TEXT("post_b"),TEXT("post_timber_2_4m")},{TEXT("roof"),TEXT("roof_slope_timber_2m")},{TEXT("bench_a"),TEXT("bench_timber")},{TEXT("bench_b"),TEXT("bench_timber")}};
    Plan.Footprint.Origin=FVector(50.f,100.f,8.f); Plan.Footprint.Orientation=FRotator(0.f,90.f,0.f);
    for(const FTestPart& Pair:Parts)
    { FHearthStructureComponent C; C.Id=FString::Printf(TEXT("plan.inn:%s"),Pair.Key); C.CatalogId=Pair.Catalog; C.ExtensionId=TEXT("plan.inn:tavern_canopy_v1"); C.Offset=FVector(0.f,100.f*Plan.Components.Num(),0.f); Plan.Components.Add(C); }
    FHearthSite Site; Site.StableId=TEXT("site.inn"); Site.BuildPlanId=Plan.PlanId; Site.Owner=0;
    for(const auto& C:Plan.Components){FHearthCottageComponent Part;Part.Id=C.Id;Part.AssetId=C.CatalogId;Part.Status=TEXT("completed");Part.Offset=FVector(100.f*Site.CottageComponents.Num(),0,0);Site.CottageComponents.Add(Part);}
    Site.CottageComponents[0].Status=TEXT("installing");
    FHearthTavernRuntimeState State; HearthTavernRuntime::Reconcile(State,Requests,TArray<FHearthSite>{Site},TArray<FHearthStructurePlan>{Plan});
    TestEqual(TEXT("Incomplete canopy does not open seats"),State.Venues[0].Status,FString(TEXT("building")));
    TestEqual(TEXT("Incomplete canopy exposes no action"),HearthTavernRuntime::AvailableSeatActions(State,TEXT("innkeeper.A")).Num(),0);
    Site.CottageComponents[0].Status=TEXT("completed");
    HearthTavernRuntime::Reconcile(State,Requests,TArray<FHearthSite>{Site},TArray<FHearthStructurePlan>{Plan});
    TestEqual(TEXT("All canopy parts plus two benches open venue"),State.Venues[0].Status,FString(TEXT("usable")));
    TestEqual(TEXT("Exactly two completed benches become seats"),State.Venues[0].Seats.Num(),2);
    TestFalse(TEXT("Seat anchors use distinct plan offsets"),State.Venues[0].Seats[0].WorldPosition.Equals(State.Venues[0].Seats[1].WorldPosition));
    const TArray<int32> Actions=HearthTavernRuntime::AvailableSeatActions(State,TEXT("visitor.B")); TestEqual(TEXT("Two seats are actionable"),Actions.Num(),2);
    FVector Position; FString SeatId; FString EventId;
    TestTrue(TEXT("Visitor reserves a real seat"),HearthTavernRuntime::ReserveSeat(State,Actions[0],TEXT("visitor.B"),Position,SeatId));
    TestTrue(TEXT("Visitor arrival creates active use evidence"),HearthTavernRuntime::MarkArrived(State,SeatId,TEXT("visitor.B"),10.f,EventId));
    TestEqual(TEXT("Use event kind is gather"),State.UseEvents.Last().Kind,FString(TEXT("gather")));
    TestTrue(TEXT("Visitor release closes and frees seat"),HearthTavernRuntime::ReleaseSeat(State,SeatId,TEXT("visitor.B"),22.f,EventId));
    FString OwnerAction=HearthTavernRuntime::ActionName(State,Actions[0],TEXT("innkeeper.A"));
    FString GuestAction=HearthTavernRuntime::ActionName(State,Actions[0],TEXT("visitor.B"));
    TestTrue(TEXT("Owner action names own tavern"),OwnerAction.Contains(TEXT("你的酒馆")));
    TestTrue(TEXT("Guest action names tavern"),GuestAction.Contains(TEXT("去酒馆")));
    TestTrue(TEXT("Second visitor reserves the other real seat"),HearthTavernRuntime::ReserveSeat(State,Actions[1],TEXT("visitor.C"),Position,SeatId));
    TestTrue(TEXT("Second visitor arrival creates another active event"),HearthTavernRuntime::MarkArrived(State,SeatId,TEXT("visitor.C"),30.f,EventId));
    TestTrue(TEXT("Second visitor release closes and frees the seat"),HearthTavernRuntime::ReleaseSeat(State,SeatId,TEXT("visitor.C"),42.f,EventId));
    TestEqual(TEXT("Use count records two completed uses"),State.Venues[0].UseCount,2);
    TestTrue(TEXT("Two unique visitor identities are retained"),State.Venues[0].UniqueVisitorIds.Contains(TEXT("visitor.B"))&&State.Venues[0].UniqueVisitorIds.Contains(TEXT("visitor.C"))&&State.Venues[0].UniqueVisitorIds.Num()==2);
    TestTrue(TEXT("Read-only export exposes host and use event"),HearthTavernRuntime::ExportReadOnly(State).Contains(TEXT("host_site_stable_id"))&&HearthTavernRuntime::ExportReadOnly(State).Contains(TEXT("use_events")));
    return true;
}
#endif
