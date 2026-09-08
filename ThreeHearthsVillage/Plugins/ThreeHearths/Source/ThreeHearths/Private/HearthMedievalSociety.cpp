#include "HearthMedievalSociety.h"

namespace
{
    bool Fail(FString* OutError, const TCHAR* Message)
    {
        if(OutError) *OutError=Message;
        return false;
    }

    bool Fail(FMedievalVisitorResult& Out, const TCHAR* Message)
    {
        Out=FMedievalVisitorResult(); Out.Reason=Message; return false;
    }

    bool Fail(FMedievalFreightResult& Out, const TCHAR* Message)
    {
        Out=FMedievalFreightResult(); Out.Reason=Message; return false;
    }

    bool IsGuard(EMedievalSocietyRole Role)
    { return Role==EMedievalSocietyRole::Gatekeeper || Role==EMedievalSocietyRole::RoyalGuard; }

    bool IsValidRole(EMedievalSocietyRole Role)
    { return Role==EMedievalSocietyRole::Gatekeeper || Role==EMedievalSocietyRole::RoyalGuard || Role==EMedievalSocietyRole::Carter; }

    bool IsValidDuty(EMedievalDutyAction Duty)
    { return Duty==EMedievalDutyAction::Patrol || Duty==EMedievalDutyAction::Stand || Duty==EMedievalDutyAction::Salute || Duty==EMedievalDutyAction::Rest; }

    int32 Units(const TMap<FString,int32>& Items)
    {
        int64 Total=0;
        for(const auto& Pair:Items) Total+=Pair.Value;
        return Total>MAX_int32?MAX_int32:static_cast<int32>(Total);
    }

    bool ValidManifest(const TMap<FString,int32>& Items, FString* OutError=nullptr)
    {
        if(Items.IsEmpty()) return Fail(OutError,TEXT("cargo manifest is empty"));
        for(const auto& Pair:Items)
            if(Pair.Key.IsEmpty() || Pair.Value<=0) return Fail(OutError,TEXT("cargo item ids and quantities must be positive"));
        return true;
    }

    bool SameManifest(const TMap<FString,int32>& A, const TMap<FString,int32>& B)
    {
        if(A.Num()!=B.Num()) return false;
        for(const auto& Pair:A) if(B.FindRef(Pair.Key)!=Pair.Value) return false;
        return true;
    }

    const FMedievalStorage* FindStorage(const FMedievalSocietyState& State, const FString& Id)
    { return State.Storages.FindByPredicate([&](const FMedievalStorage& S){return S.StableId==Id;}); }

    FMedievalStorage* FindStorage(FMedievalSocietyState& State, const FString& Id)
    { return State.Storages.FindByPredicate([&](FMedievalStorage& S){return S.StableId==Id;}); }

    const FMedievalCart* FindCart(const FMedievalSocietyState& State, const FString& Id)
    { return State.Carts.FindByPredicate([&](const FMedievalCart& C){return C.StableId==Id;}); }

    FMedievalCart* FindCart(FMedievalSocietyState& State, const FString& Id)
    { return State.Carts.FindByPredicate([&](FMedievalCart& C){return C.StableId==Id;}); }

    FMedievalFreightReservation* FindReservation(FMedievalSocietyState& State, const FString& Id)
    { return State.FreightReservations.FindByPredicate([&](FMedievalFreightReservation& R){return R.ReservationId==Id;}); }

    const FMedievalFreightReservation* FindReservation(const FMedievalSocietyState& State, const FString& Id)
    { return State.FreightReservations.FindByPredicate([&](const FMedievalFreightReservation& R){return R.ReservationId==Id;}); }

    bool AddCount(TMap<FString,int32>& Items, const FString& Key, int32 Amount)
    {
        const int32 Existing=Items.FindRef(Key);
        if(Amount<0 || Existing>MAX_int32-Amount) return false;
        Items.Add(Key,Existing+Amount); return true;
    }

    bool RemoveCount(TMap<FString,int32>& Items, const FString& Key, int32 Amount)
    {
        const int32 Existing=Items.FindRef(Key);
        if(Amount<0 || Existing<Amount) return false;
        if(Existing==Amount) Items.Remove(Key); else Items.Add(Key,Existing-Amount);
        return true;
    }

    void AppendEvent(FMedievalSocietyState& State, const FString& EventId, const TCHAR* Kind,
        const FString& SubjectId, const FString& GuardId, bool bAllowed, const FString& Reason)
    {
        if(EventId.IsEmpty() || State.Events.ContainsByPredicate([&](const FMedievalSocietyEvent& E){return E.EventId==EventId;})) return;
        FMedievalSocietyEvent Event; Event.EventId=EventId; Event.Kind=Kind; Event.SubjectId=SubjectId; Event.GuardId=GuardId;
        Event.bAllowed=bAllowed; Event.Reason=Reason; Event.Revision=++State.Revision; State.Events.Add(MoveTemp(Event));
    }
}

namespace HearthMedievalSociety
{
    bool ValidateMember(const FMedievalSocietyMember& Member, FString* OutError)
    {
        if(Member.StableId.IsEmpty()) return Fail(OutError,TEXT("society member stable id is empty"));
        if(!IsValidRole(Member.Role)) return Fail(OutError,TEXT("society member role is invalid"));
        if(!IsValidDuty(Member.CurrentDuty)) return Fail(OutError,TEXT("society member duty is invalid"));
        if(!FMath::IsFinite(Member.Emotion.Morale) || !FMath::IsFinite(Member.Emotion.Fatigue)
            || !FMath::IsFinite(Member.Emotion.Alertness) || !FMath::IsFinite(Member.Emotion.SocialNeed))
            return Fail(OutError,TEXT("society member emotion contains a non-finite value"));
        if(Member.Emotion.Morale<0 || Member.Emotion.Morale>100 || Member.Emotion.Fatigue<0 || Member.Emotion.Fatigue>100
            || Member.Emotion.Alertness<0 || Member.Emotion.Alertness>100 || Member.Emotion.SocialNeed<0 || Member.Emotion.SocialNeed>100)
            return Fail(OutError,TEXT("society member emotion is outside 0..100"));
        if(Member.DutyRevision<0) return Fail(OutError,TEXT("society member duty revision is negative"));
        return true;
    }

    const FMedievalSocietyMember* FindMember(const FMedievalSocietyState& State, const FString& StableId)
    { return State.Members.FindByPredicate([&](const FMedievalSocietyMember& M){return M.StableId==StableId;}); }

    FMedievalSocietyMember* FindMember(FMedievalSocietyState& State, const FString& StableId)
    { return State.Members.FindByPredicate([&](FMedievalSocietyMember& M){return M.StableId==StableId;}); }

    bool ValidateState(const FMedievalSocietyState& State, FString* OutError)
    {
        if(State.Revision<0) return Fail(OutError,TEXT("society revision is negative"));
        TSet<FString> MemberIds;
        for(const auto& Member:State.Members)
        {
            if(!ValidateMember(Member,OutError) || MemberIds.Contains(Member.StableId)) return Fail(OutError,TEXT("society member ids must be unique"));
            MemberIds.Add(Member.StableId);
        }
        TSet<FString> VisitorIds;
        for(const auto& Visitor:State.Visitors)
        {
            if(Visitor.StableId.IsEmpty() || VisitorIds.Contains(Visitor.StableId)) return Fail(OutError,TEXT("visitor ids must be unique and non-empty"));
            VisitorIds.Add(Visitor.StableId);
            if(!Visitor.LastEventId.IsEmpty() && !State.Events.ContainsByPredicate([&](const auto& E){return E.EventId==Visitor.LastEventId;}))
                return Fail(OutError,TEXT("visitor references a missing event"));
        }
        TSet<FString> EventIds;
        for(const auto& Event:State.Events)
        {
            if(Event.EventId.IsEmpty() || EventIds.Contains(Event.EventId) || Event.Revision<=0 || Event.Revision>State.Revision) return Fail(OutError,TEXT("society events are invalid or duplicated"));
            EventIds.Add(Event.EventId);
            if(!Event.GuardId.IsEmpty())
            {
                const auto* Guard=FindMember(State,Event.GuardId);
                if(!Guard || !IsGuard(Guard->Role)) return Fail(OutError,TEXT("event references a missing or non-guard member"));
            }
        }
        TSet<FString> StorageIds;
        for(const auto& Storage:State.Storages)
        {
            if(Storage.StableId.IsEmpty() || StorageIds.Contains(Storage.StableId)) return Fail(OutError,TEXT("storage ids must be unique and non-empty"));
            StorageIds.Add(Storage.StableId);
            for(const auto& Pair:Storage.Inventory) if(Pair.Key.IsEmpty() || Pair.Value<0) return Fail(OutError,TEXT("storage inventory is invalid"));
            for(const auto& Pair:Storage.Reserved) if(Pair.Key.IsEmpty() || Pair.Value<0 || Pair.Value>Storage.Inventory.FindRef(Pair.Key)) return Fail(OutError,TEXT("storage reservations exceed inventory"));
        }
        TSet<FString> HorseIds;
        for(const auto& Horse:State.Horses)
        {
            if(Horse.StableId.IsEmpty() || HorseIds.Contains(Horse.StableId)) return Fail(OutError,TEXT("horse ids must be unique and non-empty"));
            HorseIds.Add(Horse.StableId);
            if(!Horse.HarnessedCartId.IsEmpty() && !State.Carts.ContainsByPredicate([&](const auto& C){return C.StableId==Horse.HarnessedCartId;}))
                return Fail(OutError,TEXT("horse references a missing cart"));
        }
        TSet<FString> CartIds;
        for(const auto& Cart:State.Carts)
        {
            if(Cart.StableId.IsEmpty() || CartIds.Contains(Cart.StableId) || Cart.CapacityUnits<=0) return Fail(OutError,TEXT("cart identity or capacity is invalid"));
            CartIds.Add(Cart.StableId);
            const auto* Horse=State.Horses.FindByPredicate([&](const FMedievalHorse& Candidate){return Candidate.StableId==Cart.HorseStableId;});
            if(!Horse || Horse->HarnessedCartId!=Cart.StableId) return Fail(OutError,TEXT("cart references an unassigned or differently harnessed horse"));
            const auto* Operator=FindMember(State,Cart.OperatorStableId);
            if(!Operator || Operator->Role!=EMedievalSocietyRole::Carter) return Fail(OutError,TEXT("cart operator is missing or is not a carter"));
            if(!ValidManifest(Cart.Cargo,OutError) && !Cart.Cargo.IsEmpty()) return false;
            if(Units(Cart.Cargo)>Cart.CapacityUnits) return Fail(OutError,TEXT("cart cargo exceeds capacity"));
        }
        for(const auto& Horse:State.Horses)
        {
            if(!Horse.HarnessedCartId.IsEmpty())
            {
                const auto* Cart=State.Carts.FindByPredicate([&](const FMedievalCart& Candidate){return Candidate.StableId==Horse.HarnessedCartId;});
                if(!Cart || Cart->HorseStableId!=Horse.StableId) return Fail(OutError,TEXT("horse and cart harness references are not reciprocal"));
            }
        }
        TSet<FString> ReservationIds;
        TSet<FString> ActiveCartIds;
        TMap<FString,TMap<FString,int32>> ExpectedReserved;
        for(const auto& Reservation:State.FreightReservations)
        {
            if(Reservation.ReservationId.IsEmpty() || ReservationIds.Contains(Reservation.ReservationId) || !ValidManifest(Reservation.Cargo,OutError)) return false;
            ReservationIds.Add(Reservation.ReservationId);
            if(Reservation.SourceStorageId==Reservation.DestinationStorageId) return Fail(OutError,TEXT("freight source and destination must differ"));
            const bool bKnownStage=Reservation.Stage==EMedievalFreightStage::Reserved || Reservation.Stage==EMedievalFreightStage::PickedUp
                || Reservation.Stage==EMedievalFreightStage::Delivered || Reservation.Stage==EMedievalFreightStage::Cancelled;
            if(!bKnownStage) return Fail(OutError,TEXT("freight stage is invalid"));
            if(!FindCart(State,Reservation.CartId) || !FindStorage(State,Reservation.SourceStorageId) || !FindStorage(State,Reservation.DestinationStorageId))
                return Fail(OutError,TEXT("freight reservation references missing storage or cart"));
            const auto* ReservationCart=FindCart(State,Reservation.CartId);
            if(Reservation.OperatorStableId.IsEmpty() || !ReservationCart || ReservationCart->OperatorStableId!=Reservation.OperatorStableId)
                return Fail(OutError,TEXT("freight reservation operator does not own its cart"));
            if(Reservation.Stage==EMedievalFreightStage::Reserved || Reservation.Stage==EMedievalFreightStage::PickedUp)
            {
                if(ActiveCartIds.Contains(Reservation.CartId)) return Fail(OutError,TEXT("a cart has more than one active reservation"));
                ActiveCartIds.Add(Reservation.CartId);
            }
            if(Reservation.Stage==EMedievalFreightStage::Reserved)
            {
                TMap<FString,int32>& Expected=ExpectedReserved.FindOrAdd(Reservation.SourceStorageId);
                for(const auto& Pair:Reservation.Cargo)
                {
                    const int32 Existing=Expected.FindRef(Pair.Key);
                    if(Existing>MAX_int32-Pair.Value) return Fail(OutError,TEXT("reserved freight manifests overflow their aggregate quantity"));
                    Expected.Add(Pair.Key,Existing+Pair.Value);
                }
                const auto* Source=FindStorage(State,Reservation.SourceStorageId);
                for(const auto& Pair:Reservation.Cargo) if(Source->Reserved.FindRef(Pair.Key)<Pair.Value) return Fail(OutError,TEXT("reserved freight is absent from source escrow"));
            }
            if(Reservation.Stage==EMedievalFreightStage::PickedUp && !SameManifest(ReservationCart->Cargo,Reservation.Cargo))
                return Fail(OutError,TEXT("picked-up reservation does not match cart cargo"));
        }
        for(const auto& Storage:State.Storages)
        {
            const TMap<FString,int32>* Expected=ExpectedReserved.Find(Storage.StableId);
            if((Expected && Storage.Reserved.Num()!=Expected->Num()) || (!Expected && !Storage.Reserved.IsEmpty()))
                return Fail(OutError,TEXT("storage escrow does not equal reserved freight manifests"));
            if(Expected) for(const auto& Pair:*Expected) if(Storage.Reserved.FindRef(Pair.Key)!=Pair.Value) return Fail(OutError,TEXT("storage escrow does not equal reserved freight manifests"));
        }
        for(const auto& Cart:State.Carts)
        {
            if(Cart.ActiveReservationId.IsEmpty())
            {
                if(!Cart.Cargo.IsEmpty()) return Fail(OutError,TEXT("cart cargo has no active picked-up reservation"));
                continue;
            }
            const auto* Reservation=FindReservation(State,Cart.ActiveReservationId);
            if(!Reservation || Reservation->CartId!=Cart.StableId || (Reservation->Stage!=EMedievalFreightStage::Reserved && Reservation->Stage!=EMedievalFreightStage::PickedUp))
                return Fail(OutError,TEXT("cart active reservation is inconsistent"));
            if(Reservation->Stage==EMedievalFreightStage::Reserved && !Cart.Cargo.IsEmpty()) return Fail(OutError,TEXT("reserved cart cannot contain picked-up cargo"));
            if(Reservation->Stage==EMedievalFreightStage::PickedUp && !SameManifest(Cart.Cargo,Reservation->Cargo)) return Fail(OutError,TEXT("cart cargo does not match active reservation"));
        }
        return true;
    }

    bool AddMember(FMedievalSocietyState& State, const FMedievalSocietyMember& Member, FString* OutError)
    {
        if(!ValidateMember(Member,OutError) || FindMember(State,Member.StableId)) return Fail(OutError,TEXT("member is invalid or already owned by the roster"));
        State.Members.Add(Member); ++State.Revision; return true;
    }

    bool AddStorage(FMedievalSocietyState& State, const FMedievalStorage& Storage, FString* OutError)
    {
        if(Storage.StableId.IsEmpty() || FindStorage(State,Storage.StableId)) return Fail(OutError,TEXT("storage is invalid or already owned by the ledger"));
        for(const auto& Pair:Storage.Inventory) if(Pair.Key.IsEmpty() || Pair.Value<0) return Fail(OutError,TEXT("storage inventory is invalid"));
        if(!Storage.Reserved.IsEmpty()) return Fail(OutError,TEXT("new storage cannot start with reserved inventory"));
        State.Storages.Add(Storage); ++State.Revision; return true;
    }

    bool AddHorse(FMedievalSocietyState& State, const FMedievalHorse& Horse, FString* OutError)
    {
        if(Horse.StableId.IsEmpty() || !Horse.HarnessedCartId.IsEmpty() || State.Horses.ContainsByPredicate([&](const auto& Existing){return Existing.StableId==Horse.StableId;}))
            return Fail(OutError,TEXT("horse is invalid or already owned by the stable"));
        State.Horses.Add(Horse); ++State.Revision; return true;
    }

    bool AddCart(FMedievalSocietyState& State, const FMedievalCart& Cart, FString* OutError)
    {
        if(Cart.StableId.IsEmpty() || FindCart(State,Cart.StableId) || Cart.CapacityUnits<=0 || !Cart.ActiveReservationId.IsEmpty())
            return Fail(OutError,TEXT("cart is invalid or already owned by the ledger"));
        FMedievalHorse* Horse=State.Horses.FindByPredicate([&](FMedievalHorse& Candidate){return Candidate.StableId==Cart.HorseStableId;});
        if(!Horse || !Horse->HarnessedCartId.IsEmpty()) return Fail(OutError,TEXT("cart requires one free horse"));
        const auto* Operator=FindMember(State,Cart.OperatorStableId);
        if(!Operator || Operator->Role!=EMedievalSocietyRole::Carter) return Fail(OutError,TEXT("cart requires a carter operator"));
        if(!Cart.Cargo.IsEmpty()) return Fail(OutError,TEXT("new cart cannot start with untracked cargo"));
        State.Carts.Add(Cart); Horse->HarnessedCartId=Cart.StableId; ++State.Revision; return true;
    }

    FMedievalDutyDecision DecideLocalDuty(const FMedievalSocietyMember& Member, const FMedievalDutyContext& Context)
    {
        FMedievalDutyDecision Decision;
        const bool bGuard=IsGuard(Member.Role);
        if(Member.Emotion.Fatigue>=80.f || Member.Emotion.SocialNeed>=90.f)
        {
            Decision.Action=EMedievalDutyAction::Rest; Decision.Description=TEXT("Rest in the guard room and recover before taking another watch.");
            Decision.Reason=TEXT("Fatigue or social need is too high for a safe watch."); return Decision;
        }
        if(Context.bThreatDetected && bGuard)
        {
            Decision.Action=EMedievalDutyAction::Patrol; Decision.Description=TEXT("Patrol the gate road and inspect the perimeter.");
            Decision.Reason=TEXT("A threat signal requires an alert guard patrol."); return Decision;
        }
        if(Context.bVisitorWaiting && Context.bAtGate && bGuard)
        {
            Decision.Action=EMedievalDutyAction::Salute; Decision.Description=TEXT("Salute the visitor, check their pass, and announce the gate decision.");
            Decision.Reason=TEXT("A visitor is waiting at the gate."); return Decision;
        }
        if(Member.Role==EMedievalSocietyRole::RoyalGuard && Context.MinutesOnDuty>=90.f)
        {
            Decision.Action=EMedievalDutyAction::Rest; Decision.Description=TEXT("Stand down for a short rest after a long royal watch.");
            Decision.Reason=TEXT("The royal watch has reached its local rotation limit."); return Decision;
        }
        Decision.Action=EMedievalDutyAction::Stand;
        Decision.Description=Member.Role==EMedievalSocietyRole::Carter
            ?TEXT("Stand by the stable with the cart ready for the next paid delivery.")
            :TEXT("Stand the gate post and observe the road.");
        Decision.Reason=TEXT("No urgent visitor, threat, or recovery signal is present."); return Decision;
    }

    bool CheckInVisitor(FMedievalSocietyState& State, const FString& VisitorId, const FString& DisplayName,
        bool bKnownResident, bool bHasPass, const FString& GuardId, const FString& EventId, FMedievalVisitorResult& Out)
    {
        Out=FMedievalVisitorResult();
        if(const auto* ExistingEvent=State.Events.FindByPredicate([&](const auto& E){return E.EventId==EventId;}))
        {
            if(ExistingEvent->Kind!=TEXT("visitor_checkin")) return Fail(Out,TEXT("event id is already owned by another event kind"));
            Out.bSuccess=true; Out.bAlreadyApplied=true; Out.bAllowed=ExistingEvent->bAllowed; Out.Reason=ExistingEvent->Reason; return true;
        }
        if(VisitorId.IsEmpty() || EventId.IsEmpty()) return Fail(Out,TEXT("visitor and event ids are required"));
        const auto* Guard=FindMember(State,GuardId);
        if(!Guard || !IsGuard(Guard->Role)) return Fail(Out,TEXT("check-in requires a gatekeeper or royal guard"));
        const bool bAllowed=bKnownResident || bHasPass;
        const FString Reason=bAllowed?TEXT("Visitor checked in by an authorized gate guard."):TEXT("Visitor denied: resident identity or a valid pass was not supplied.");
        FMedievalVisitorRecord* Visitor=State.Visitors.FindByPredicate([&](FMedievalVisitorRecord& V){return V.StableId==VisitorId;});
        if(!Visitor) { FMedievalVisitorRecord NewVisitor; NewVisitor.StableId=VisitorId; State.Visitors.Add(MoveTemp(NewVisitor)); Visitor=&State.Visitors.Last(); }
        Visitor->DisplayName=DisplayName; Visitor->bKnownResident=bKnownResident; Visitor->bCheckedIn=bAllowed; Visitor->bAllowed=bAllowed; Visitor->LastEventId=EventId; Visitor->LastReason=Reason;
        AppendEvent(State,EventId,TEXT("visitor_checkin"),VisitorId,GuardId,bAllowed,Reason);
        Out.bSuccess=true; Out.bAllowed=bAllowed; Out.Reason=Reason; return true;
    }

    int32 InventoryCount(const FMedievalStorage& Storage, const FString& ItemId)
    { return Storage.Inventory.FindRef(ItemId); }

    bool ReservePickup(FMedievalSocietyState& State, const FString& ReservationId, const FString& CartId,
        const FString& OperatorStableId, const FString& SourceStorageId, const FString& DestinationStorageId,
        const TMap<FString, int32>& Cargo, FMedievalFreightResult& Out)
    {
        Out=FMedievalFreightResult();
        if(const auto* Existing=FindReservation(State,ReservationId))
        {
            const auto* ExistingCart=FindCart(State,Existing->CartId);
            const bool bSame=Existing->CartId==CartId && Existing->OperatorStableId==OperatorStableId && Existing->SourceStorageId==SourceStorageId && Existing->DestinationStorageId==DestinationStorageId && SameManifest(Existing->Cargo,Cargo);
            if(!bSame) return Fail(Out,TEXT("reservation id is already owned by another freight request"));
            if(!ExistingCart || ExistingCart->OperatorStableId!=OperatorStableId) return Fail(Out,TEXT("reservation ownership is inconsistent"));
            Out.bSuccess=true; Out.bAlreadyApplied=true; Out.Reason=TEXT("Freight reservation was already applied."); return true;
        }
        if(ReservationId.IsEmpty() || CartId.IsEmpty() || SourceStorageId.IsEmpty() || DestinationStorageId.IsEmpty() || SourceStorageId==DestinationStorageId)
            return Fail(Out,TEXT("freight ids are incomplete or source equals destination"));
        FString ManifestError; if(!ValidManifest(Cargo,&ManifestError)) return Fail(Out,*ManifestError);
        FMedievalCart* Cart=FindCart(State,CartId); FMedievalStorage* Source=FindStorage(State,SourceStorageId);
        if(!Cart || !Source || !FindStorage(State,DestinationStorageId)) return Fail(Out,TEXT("freight references a missing cart or storage"));
        if(Cart->OperatorStableId!=OperatorStableId) return Fail(Out,TEXT("only the cart's assigned carter may reserve its freight"));
        if(!Cart->ActiveReservationId.IsEmpty() || !Cart->Cargo.IsEmpty()) return Fail(Out,TEXT("cart is already occupied"));
        if(Units(Cargo)>Cart->CapacityUnits) return Fail(Out,TEXT("cargo exceeds cart capacity"));
        for(const auto& Pair:Cargo) if(Source->Inventory.FindRef(Pair.Key)-Source->Reserved.FindRef(Pair.Key)<Pair.Value) return Fail(Out,TEXT("source inventory is insufficient after existing reservations"));
        for(const auto& Pair:Cargo) Source->Reserved.Add(Pair.Key,Source->Reserved.FindRef(Pair.Key)+Pair.Value);
        FMedievalFreightReservation Reservation; Reservation.ReservationId=ReservationId; Reservation.CartId=CartId; Reservation.OperatorStableId=OperatorStableId; Reservation.SourceStorageId=SourceStorageId; Reservation.DestinationStorageId=DestinationStorageId; Reservation.Cargo=Cargo; Reservation.Stage=EMedievalFreightStage::Reserved;
        State.FreightReservations.Add(MoveTemp(Reservation)); Cart->ActiveReservationId=ReservationId;
        AppendEvent(State,ReservationId+TEXT(":reserve"),TEXT("freight_reserved"),ReservationId,FString(),true,TEXT("Freight was reserved against source inventory."));
        Out.bSuccess=true; Out.MovedUnits=0; Out.Reason=TEXT("Freight reservation created without transferring goods."); return true;
    }

    bool CompletePickup(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out)
    {
        Out=FMedievalFreightResult(); FMedievalFreightReservation* Reservation=FindReservation(State,ReservationId);
        if(!Reservation) return Fail(Out,TEXT("freight reservation is missing"));
        if(Reservation->Stage==EMedievalFreightStage::PickedUp || Reservation->Stage==EMedievalFreightStage::Delivered)
        { Out.bSuccess=true; Out.bAlreadyApplied=true; Out.Reason=TEXT("Freight pickup was already applied."); return true; }
        if(Reservation->Stage!=EMedievalFreightStage::Reserved) return Fail(Out,TEXT("freight reservation is not awaiting pickup"));
        FMedievalStorage* Source=FindStorage(State,Reservation->SourceStorageId); FMedievalCart* Cart=FindCart(State,Reservation->CartId);
        if(!Source || !Cart || Cart->ActiveReservationId!=ReservationId) return Fail(Out,TEXT("pickup references inconsistent freight ownership"));
        for(const auto& Pair:Reservation->Cargo) if(Source->Inventory.FindRef(Pair.Key)<Pair.Value || Source->Reserved.FindRef(Pair.Key)<Pair.Value) return Fail(Out,TEXT("reserved goods are no longer present at pickup"));
        for(const auto& Pair:Reservation->Cargo) { if(!RemoveCount(Source->Inventory,Pair.Key,Pair.Value) || !RemoveCount(Source->Reserved,Pair.Key,Pair.Value)) return Fail(Out,TEXT("pickup transfer failed atomically")); }
        for(const auto& Pair:Reservation->Cargo) if(!AddCount(Cart->Cargo,Pair.Key,Pair.Value)) return Fail(Out,TEXT("pickup would overflow cart inventory"));
        Reservation->Stage=EMedievalFreightStage::PickedUp;
        AppendEvent(State,ReservationId+TEXT(":pickup"),TEXT("freight_picked_up"),ReservationId,FString(),true,TEXT("Reserved goods moved from source storage into the cart."));
        Out.bSuccess=true; Out.MovedUnits=Units(Reservation->Cargo); Out.Reason=TEXT("Freight picked up exactly once."); return true;
    }

    bool CompleteDelivery(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out)
    {
        Out=FMedievalFreightResult(); FMedievalFreightReservation* Reservation=FindReservation(State,ReservationId);
        if(!Reservation) return Fail(Out,TEXT("freight reservation is missing"));
        if(Reservation->Stage==EMedievalFreightStage::Delivered)
        { Out.bSuccess=true; Out.bAlreadyApplied=true; Out.Reason=TEXT("Freight delivery was already applied."); return true; }
        if(Reservation->Stage!=EMedievalFreightStage::PickedUp) return Fail(Out,TEXT("freight must be picked up before delivery"));
        FMedievalStorage* Destination=FindStorage(State,Reservation->DestinationStorageId); FMedievalCart* Cart=FindCart(State,Reservation->CartId);
        if(!Destination || !Cart || Cart->ActiveReservationId!=ReservationId) return Fail(Out,TEXT("delivery references inconsistent freight ownership"));
        for(const auto& Pair:Reservation->Cargo) if(Cart->Cargo.FindRef(Pair.Key)<Pair.Value) return Fail(Out,TEXT("cart no longer contains the reserved goods"));
        for(const auto& Pair:Reservation->Cargo) if(Destination->Inventory.FindRef(Pair.Key)>MAX_int32-Pair.Value) return Fail(Out,TEXT("delivery would overflow destination inventory"));
        for(const auto& Pair:Reservation->Cargo) { if(!RemoveCount(Cart->Cargo,Pair.Key,Pair.Value) || !AddCount(Destination->Inventory,Pair.Key,Pair.Value)) return Fail(Out,TEXT("delivery transfer failed atomically")); }
        Reservation->Stage=EMedievalFreightStage::Delivered; Cart->ActiveReservationId.Empty();
        AppendEvent(State,ReservationId+TEXT(":delivery"),TEXT("freight_delivered"),ReservationId,FString(),true,TEXT("Cart goods moved into destination storage."));
        Out.bSuccess=true; Out.MovedUnits=Units(Reservation->Cargo); Out.Reason=TEXT("Freight delivered exactly once."); return true;
    }

    bool CancelFreight(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out)
    {
        Out=FMedievalFreightResult(); FMedievalFreightReservation* Reservation=FindReservation(State,ReservationId);
        if(!Reservation) return Fail(Out,TEXT("freight reservation is missing"));
        if(Reservation->Stage==EMedievalFreightStage::Cancelled)
        { Out.bSuccess=true; Out.bAlreadyApplied=true; Out.Reason=TEXT("Freight cancellation was already applied."); return true; }
        if(Reservation->Stage==EMedievalFreightStage::Delivered) return Fail(Out,TEXT("delivered freight cannot be cancelled"));
        FMedievalStorage* Source=FindStorage(State,Reservation->SourceStorageId); FMedievalCart* Cart=FindCart(State,Reservation->CartId);
        if(!Source || !Cart || Cart->ActiveReservationId!=ReservationId) return Fail(Out,TEXT("cancellation references inconsistent freight ownership"));
        if(Reservation->Stage==EMedievalFreightStage::Reserved)
        {
            for(const auto& Pair:Reservation->Cargo) if(Source->Reserved.FindRef(Pair.Key)<Pair.Value) return Fail(Out,TEXT("source reservation is missing"));
            for(const auto& Pair:Reservation->Cargo) if(!RemoveCount(Source->Reserved,Pair.Key,Pair.Value)) return Fail(Out,TEXT("reservation release failed atomically"));
        }
        else
        {
            for(const auto& Pair:Reservation->Cargo) if(Cart->Cargo.FindRef(Pair.Key)<Pair.Value || Source->Inventory.FindRef(Pair.Key)>MAX_int32-Pair.Value) return Fail(Out,TEXT("picked-up goods cannot be returned safely"));
            for(const auto& Pair:Reservation->Cargo) { if(!RemoveCount(Cart->Cargo,Pair.Key,Pair.Value) || !AddCount(Source->Inventory,Pair.Key,Pair.Value)) return Fail(Out,TEXT("cargo return failed atomically")); }
        }
        Reservation->Stage=EMedievalFreightStage::Cancelled; Cart->ActiveReservationId.Empty();
        AppendEvent(State,ReservationId+TEXT(":cancel"),TEXT("freight_cancelled"),ReservationId,FString(),false,TEXT("Freight reservation was cancelled and goods were returned or released."));
        Out.bSuccess=true; Out.MovedUnits=0; Out.Reason=TEXT("Freight cancellation applied exactly once."); return true;
    }
}
