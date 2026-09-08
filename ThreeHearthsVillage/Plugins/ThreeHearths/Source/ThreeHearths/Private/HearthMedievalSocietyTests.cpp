#if WITH_DEV_AUTOMATION_TESTS

#include "HearthMedievalSociety.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMedievalSocietyTest,
    "ThreeHearths.MedievalSociety.RosterGuardsAndExactlyOnceFreight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthMedievalSocietyTest::RunTest(const FString&)
{
    using namespace HearthMedievalSociety;
    FMedievalSocietyState State;
    const auto Storage=[&](const FString& Id)->const FMedievalStorage&
    { return *State.Storages.FindByPredicate([&](const FMedievalStorage& Candidate){return Candidate.StableId==Id;}); };
    FMedievalSocietyMember Gate; Gate.StableId=TEXT("gatekeeper-01"); Gate.DisplayName=TEXT("Gatekeeper"); Gate.Role=EMedievalSocietyRole::Gatekeeper;
    FMedievalSocietyMember Guard; Guard.StableId=TEXT("royal-guard-01"); Guard.DisplayName=TEXT("Royal Guard"); Guard.Role=EMedievalSocietyRole::RoyalGuard;
    FMedievalSocietyMember Carter; Carter.StableId=TEXT("carter-01"); Carter.DisplayName=TEXT("Carter"); Carter.Role=EMedievalSocietyRole::Carter;
    FString Error;
    TestTrue(TEXT("gatekeeper joins roster"),AddMember(State,Gate,&Error));
    TestTrue(TEXT("royal guard joins roster"),AddMember(State,Guard,&Error));
    TestTrue(TEXT("carter joins roster"),AddMember(State,Carter,&Error));
    TestFalse(TEXT("duplicate stable id is rejected"),AddMember(State,Gate,&Error));

    Guard.Emotion.Fatigue=90.f;
    TestEqual(TEXT("fatigued guard chooses rest"),DecideLocalDuty(Guard,{}).Action,EMedievalDutyAction::Rest);
    Guard.Emotion.Fatigue=10.f;
    FMedievalDutyContext Threat; Threat.bThreatDetected=true;
    TestEqual(TEXT("threat chooses patrol"),DecideLocalDuty(Guard,Threat).Action,EMedievalDutyAction::Patrol);
    FMedievalDutyContext VisitorWaiting; VisitorWaiting.bVisitorWaiting=true;
    TestEqual(TEXT("waiting visitor chooses salute"),DecideLocalDuty(Gate,VisitorWaiting).Action,EMedievalDutyAction::Salute);
    TestEqual(TEXT("idle carter chooses stand"),DecideLocalDuty(Carter,{}).Action,EMedievalDutyAction::Stand);

    FMedievalVisitorResult VisitorResult;
    TestTrue(TEXT("pass-bearing visitor is allowed"),CheckInVisitor(State,TEXT("visitor-01"),TEXT("Merchant"),false,true,Gate.StableId,TEXT("visit-event-01"),VisitorResult));
    TestTrue(TEXT("allowed decision is recorded"),VisitorResult.bAllowed && State.Events.Num()==1);
    const int32 EventCount=State.Events.Num();
    TestTrue(TEXT("duplicate visitor event is idempotent"),CheckInVisitor(State,TEXT("visitor-01"),TEXT("Merchant"),false,true,Gate.StableId,TEXT("visit-event-01"),VisitorResult));
    TestTrue(TEXT("duplicate visitor event does not duplicate log"),VisitorResult.bAlreadyApplied && State.Events.Num()==EventCount);
    TestTrue(TEXT("visitor without credentials is handled as a denial"),CheckInVisitor(State,TEXT("visitor-02"),TEXT("Stranger"),false,false,Gate.StableId,TEXT("visit-event-02"),VisitorResult));
    TestFalse(TEXT("visitor denial is explicit"),VisitorResult.bAllowed);
    TestFalse(TEXT("carter cannot operate the gate"),CheckInVisitor(State,TEXT("visitor-03"),TEXT("Intruder"),false,false,Carter.StableId,TEXT("visit-event-03"),VisitorResult));
    TestTrue(TEXT("invalid gate event leaves no log entry"),State.Events.Num()==2);

    FMedievalHorse Horse; Horse.StableId=TEXT("horse-01");
    TestTrue(TEXT("horse enters stable"),AddHorse(State,Horse,&Error));
    FMedievalCart Cart; Cart.StableId=TEXT("cart-01"); Cart.OperatorStableId=Carter.StableId; Cart.HorseStableId=Horse.StableId; Cart.CapacityUnits=4;
    TestTrue(TEXT("cart claims one horse and one carter"),AddCart(State,Cart,&Error));
    FMedievalStorage Source; Source.StableId=TEXT("store-source"); Source.Inventory.Add(TEXT("wood"),5); Source.Inventory.Add(TEXT("stone"),2);
    FMedievalStorage Destination; Destination.StableId=TEXT("store-destination");
    TestTrue(TEXT("source storage is registered"),AddStorage(State,Source,&Error));
    TestTrue(TEXT("destination storage is registered"),AddStorage(State,Destination,&Error));

    TMap<FString,int32> Cargo; Cargo.Add(TEXT("wood"),3); Cargo.Add(TEXT("stone"),1);
    FMedievalFreightResult Freight;
    TMap<FString,int32> TooMuch; TooMuch.Add(TEXT("wood"),5);
    TestFalse(TEXT("finite cart capacity rejects oversized freight"),ReservePickup(State,TEXT("freight-over"),Cart.StableId,Carter.StableId,Source.StableId,Destination.StableId,TooMuch,Freight));
    TestTrue(TEXT("freight reservation succeeds within capacity"),ReservePickup(State,TEXT("freight-01"),Cart.StableId,Carter.StableId,Source.StableId,Destination.StableId,Cargo,Freight));
    TestEqual(TEXT("reservation does not transfer goods early"),InventoryCount(Storage(Source.StableId),TEXT("wood")),5);
    TestTrue(TEXT("repeating reservation is idempotent"),ReservePickup(State,TEXT("freight-01"),Cart.StableId,Carter.StableId,Source.StableId,Destination.StableId,Cargo,Freight) && Freight.bAlreadyApplied);
    TestFalse(TEXT("replaying a reservation under another owner is rejected"),ReservePickup(State,TEXT("freight-01"),Cart.StableId,Gate.StableId,Source.StableId,Destination.StableId,Cargo,Freight));
    TestFalse(TEXT("wrong carter cannot reserve another cart's freight"),ReservePickup(State,TEXT("freight-bad"),Cart.StableId,Gate.StableId,Source.StableId,Destination.StableId,Cargo,Freight));
    TestTrue(TEXT("pickup transfers reserved goods once"),CompletePickup(State,TEXT("freight-01"),Freight));
    TestEqual(TEXT("pickup removes exactly three wood"),InventoryCount(Storage(Source.StableId),TEXT("wood")),2);
    TestTrue(TEXT("repeating pickup is idempotent"),CompletePickup(State,TEXT("freight-01"),Freight) && Freight.bAlreadyApplied);
    TestTrue(TEXT("delivery transfers cart goods once"),CompleteDelivery(State,TEXT("freight-01"),Freight));
    TestEqual(TEXT("delivery creates exactly three destination wood"),InventoryCount(Storage(Destination.StableId),TEXT("wood")),3);
    TestTrue(TEXT("repeating delivery is idempotent"),CompleteDelivery(State,TEXT("freight-01"),Freight) && Freight.bAlreadyApplied);
    TestEqual(TEXT("source plus destination wood is conserved"),InventoryCount(Storage(Source.StableId),TEXT("wood"))+InventoryCount(Storage(Destination.StableId),TEXT("wood")),5);

    TMap<FString,int32> CancelCargo; CancelCargo.Add(TEXT("wood"),1);
    TestTrue(TEXT("second reservation succeeds after delivery frees cart"),ReservePickup(State,TEXT("freight-02"),Cart.StableId,Carter.StableId,Source.StableId,Destination.StableId,CancelCargo,Freight));
    const int32 BeforeCancel=InventoryCount(Storage(Source.StableId),TEXT("wood"));
    TestTrue(TEXT("unpicked reservation cancels"),CancelFreight(State,TEXT("freight-02"),Freight));
    TestEqual(TEXT("cancel releases escrow without changing inventory"),InventoryCount(Storage(Source.StableId),TEXT("wood")),BeforeCancel);
    TestTrue(TEXT("repeating cancellation is idempotent"),CancelFreight(State,TEXT("freight-02"),Freight) && Freight.bAlreadyApplied);

    TestTrue(TEXT("valid society state survives a reload-shaped copy"),ValidateState(State,&Error));
    FMedievalSocietyState Reloaded=State;
    Reloaded.Events[0].GuardId=TEXT("missing-guard");
    TestFalse(TEXT("reload rejects an event with an invalid guard reference"),ValidateState(Reloaded,&Error));

    FMedievalSocietyState EscrowMismatch=State;
    EscrowMismatch.Storages[0].Reserved.Add(TEXT("wood"),1);
    TestFalse(TEXT("orphaned storage escrow is rejected"),ValidateState(EscrowMismatch,&Error));

    FMedievalSocietyState OrphanedCargo=State;
    OrphanedCargo.Carts[0].Cargo.Add(TEXT("wood"),1);
    TestFalse(TEXT("cart cargo without an active pickup is rejected"),ValidateState(OrphanedCargo,&Error));

    FMedievalSocietyState PickedManifestMismatch=State;
    FMedievalFreightReservation* PickedReservation=PickedManifestMismatch.FreightReservations.FindByPredicate([](FMedievalFreightReservation& R){return R.ReservationId==TEXT("freight-01");});
    PickedManifestMismatch.Carts[0].ActiveReservationId=TEXT("freight-01");
    PickedManifestMismatch.Carts[0].Cargo.Add(TEXT("wood"),1);
    if(PickedReservation) PickedReservation->Stage=EMedievalFreightStage::PickedUp;
    TestFalse(TEXT("picked-up manifest must match active cart cargo"),ValidateState(PickedManifestMismatch,&Error));

    FMedievalSocietyState DuplicateActive=State;
    FMedievalFreightReservation* ActiveReservation=DuplicateActive.FreightReservations.FindByPredicate([](FMedievalFreightReservation& R){return R.ReservationId==TEXT("freight-01");});
    if(ActiveReservation)
    {
        ActiveReservation->Stage=EMedievalFreightStage::PickedUp;
        FMedievalFreightReservation Duplicate=*ActiveReservation;
        Duplicate.ReservationId=TEXT("freight-03");
        DuplicateActive.Carts[0].ActiveReservationId=ActiveReservation->ReservationId;
        DuplicateActive.Carts[0].Cargo=ActiveReservation->Cargo;
        // TArray::Add may reallocate; finish reading the source pointer first.
        DuplicateActive.FreightReservations.Add(Duplicate);
    }
    TestFalse(TEXT("one cart cannot own two active reservations"),ValidateState(DuplicateActive,&Error));

    FMedievalSocietyState BrokenHarness=State;
    BrokenHarness.Horses[0].HarnessedCartId.Empty();
    TestFalse(TEXT("one-way horse harness ownership is rejected"),ValidateState(BrokenHarness,&Error));

    FMedievalSocietyState InvalidDuty=State;
    InvalidDuty.Members[0].CurrentDuty=static_cast<EMedievalDutyAction>(255);
    TestFalse(TEXT("unknown duty enum is rejected"),ValidateState(InvalidDuty,&Error));

    FMedievalSocietyState FutureEvent=State;
    FutureEvent.Events[0].Revision=FutureEvent.Revision+1;
    TestFalse(TEXT("event revision beyond state revision is rejected"),ValidateState(FutureEvent,&Error));

    FMedievalSocietyState SameEndpoint=State;
    SameEndpoint.FreightReservations[0].DestinationStorageId=SameEndpoint.FreightReservations[0].SourceStorageId;
    TestFalse(TEXT("same-source-destination freight is rejected"),ValidateState(SameEndpoint,&Error));
    return true;
}

#endif
