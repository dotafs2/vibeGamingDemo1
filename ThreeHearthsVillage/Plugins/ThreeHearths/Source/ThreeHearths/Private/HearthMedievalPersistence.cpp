#include "HearthMedievalPersistence.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr int32 MaxArrayEntries = 4096;
    constexpr int32 MaxStringLength = 256;
    constexpr double MinInt32AsDouble = -2147483648.0;
    constexpr double MaxInt32AsDouble = 2147483647.0;

    bool Fail(FString* OutError, const FString& Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
        return false;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out, FString& Error, bool bRequired = true, bool bRequireNonEmpty = true)
    {
        if (!Object.IsValid())
        {
            Error = TEXT("JSON object is null");
            return false;
        }
        if (!Object->TryGetStringField(Field, Out))
        {
            if (!bRequired && !Object->HasField(Field))
            {
                Out.Empty();
                return true;
            }
            Error = FString::Printf(TEXT("Missing string field '%s'"), Field);
            return false;
        }
        if ((bRequireNonEmpty && Out.Len() == 0) || Out.Len() > MaxStringLength)
        {
            Error = FString::Printf(TEXT("Invalid string field '%s'"), Field);
            return false;
        }
        return true;
    }

    bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out, FString& Error)
    {
        double Value = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value) || !FMath::IsFinite(Value)
            || Value < MinInt32AsDouble || Value > MaxInt32AsDouble)
        {
            Error = FString::Printf(TEXT("Invalid integer field '%s'"), Field);
            return false;
        }
        const int64 IntegerValue = static_cast<int64>(Value);
        if (static_cast<double>(IntegerValue) != Value)
        {
            Error = FString::Printf(TEXT("Non-integral field '%s'"), Field);
            return false;
        }
        Out = static_cast<int32>(IntegerValue);
        return true;
    }

    bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Out, FString& Error)
    {
        if (!Object.IsValid() || !Object->TryGetBoolField(Field, Out))
        {
            Error = FString::Printf(TEXT("Invalid boolean field '%s'"), Field);
            return false;
        }
        return true;
    }

    bool IsKnownMaterial(const FString& Id)
    {
        static const TSet<FString> Known = {
            TEXT("food"), TEXT("wood"), TEXT("raw_logs"), TEXT("stone"), TEXT("plank"),
            TEXT("planks"), TEXT("beam"), TEXT("beams"), TEXT("tiles"), TEXT("clay")
        };
        return Known.Contains(Id);
    }

    bool ValidateItems(const TMap<FString, int32>& Items, FString& Error, bool bRequirePositive)
    {
        int64 Total = 0;
        for (const TPair<FString, int32>& Pair : Items)
        {
            if (!IsKnownMaterial(Pair.Key) || Pair.Value < (bRequirePositive ? 1 : 0))
            {
                Error = FString::Printf(TEXT("Invalid material '%s'"), *Pair.Key);
                return false;
            }
            Total += Pair.Value;
            if (Total > MAX_int32)
            {
                Error = TEXT("Material quantities exceed the supported integer total");
                return false;
            }
        }
        return true;
    }

    bool ReadItems(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TMap<FString, int32>& Out, FString& Error, bool bRequirePositive)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() > MaxArrayEntries)
        {
            Error = FString::Printf(TEXT("Invalid item array '%s'"), Field);
            return false;
        }
        Out.Empty();
        int64 Total = 0;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Item.IsValid())
            {
                Error = TEXT("Item entry is not an object");
                return false;
            }
            FString Id;
            int32 Amount = 0;
            if (!ReadString(Item, TEXT("id"), Id, Error) || !ReadInt(Item, TEXT("amount"), Amount, Error)
                || !IsKnownMaterial(Id) || Amount < (bRequirePositive ? 1 : 0))
            {
                if (Error.IsEmpty())
                {
                    Error = FString::Printf(TEXT("Invalid material '%s'"), *Id);
                }
                return false;
            }
            if (Out.Contains(Id))
            {
                Error = FString::Printf(TEXT("Duplicate material '%s'"), *Id);
                return false;
            }
            Total += Amount;
            if (Total > MAX_int32)
            {
                Error = TEXT("Material quantities exceed the supported integer total");
                return false;
            }
            Out.Add(Id, Amount);
        }
        return true;
    }

    void WriteItems(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TMap<FString, int32>& Items)
    {
        TArray<FString> Keys;
        Items.GetKeys(Keys);
        Keys.Sort();
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(Keys.Num());
        for (const FString& Key : Keys)
        {
            const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("id"), Key);
            Item->SetNumberField(TEXT("amount"), Items[Key]);
            Values.Add(MakeShared<FJsonValueObject>(Item));
        }
        Object->SetArrayField(Field, Values);
    }

    const TCHAR* RoleName(EMedievalSocietyRole Role)
    {
        switch (Role)
        {
        case EMedievalSocietyRole::Gatekeeper: return TEXT("gatekeeper");
        case EMedievalSocietyRole::RoyalGuard: return TEXT("royal_guard");
        case EMedievalSocietyRole::Carter: return TEXT("carter");
        }
        return TEXT("");
    }

    bool ParseRole(const FString& Name, EMedievalSocietyRole& Out)
    {
        if (Name == TEXT("gatekeeper")) Out = EMedievalSocietyRole::Gatekeeper;
        else if (Name == TEXT("royal_guard")) Out = EMedievalSocietyRole::RoyalGuard;
        else if (Name == TEXT("carter")) Out = EMedievalSocietyRole::Carter;
        else return false;
        return true;
    }

    const TCHAR* DutyName(EMedievalDutyAction Duty)
    {
        switch (Duty)
        {
        case EMedievalDutyAction::Patrol: return TEXT("patrol");
        case EMedievalDutyAction::Stand: return TEXT("stand");
        case EMedievalDutyAction::Salute: return TEXT("salute");
        case EMedievalDutyAction::Rest: return TEXT("rest");
        }
        return TEXT("");
    }

    bool ParseDuty(const FString& Name, EMedievalDutyAction& Out)
    {
        if (Name == TEXT("patrol")) Out = EMedievalDutyAction::Patrol;
        else if (Name == TEXT("stand")) Out = EMedievalDutyAction::Stand;
        else if (Name == TEXT("salute")) Out = EMedievalDutyAction::Salute;
        else if (Name == TEXT("rest")) Out = EMedievalDutyAction::Rest;
        else return false;
        return true;
    }

    const TCHAR* StageName(EMedievalFreightStage Stage)
    {
        switch (Stage)
        {
        case EMedievalFreightStage::Reserved: return TEXT("reserved");
        case EMedievalFreightStage::PickedUp: return TEXT("picked_up");
        case EMedievalFreightStage::Delivered: return TEXT("delivered");
        case EMedievalFreightStage::Cancelled: return TEXT("cancelled");
        }
        return TEXT("");
    }

    bool ParseStage(const FString& Name, EMedievalFreightStage& Out)
    {
        if (Name == TEXT("reserved")) Out = EMedievalFreightStage::Reserved;
        else if (Name == TEXT("picked_up")) Out = EMedievalFreightStage::PickedUp;
        else if (Name == TEXT("delivered")) Out = EMedievalFreightStage::Delivered;
        else if (Name == TEXT("cancelled")) Out = EMedievalFreightStage::Cancelled;
        else return false;
        return true;
    }

    void AddString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Value)
    {
        Object->SetStringField(Field, Value);
    }

    bool ReadObjectArray(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>*& Out, FString& Error)
    {
        if (!Root->TryGetArrayField(Field, Out) || !Out || Out->Num() > MaxArrayEntries)
        {
            Error = FString::Printf(TEXT("Invalid object array '%s'"), Field);
            return false;
        }
        return true;
    }

    bool ReadObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& Out, FString& Error)
    {
        Out = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Out.IsValid())
        {
            Error = TEXT("Array entry is not an object");
            return false;
        }
        return true;
    }

    void WriteMember(const FMedievalSocietyMember& Member, TArray<TSharedPtr<FJsonValue>>& Out)
    {
        const TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        AddString(Object, TEXT("stable_id"), Member.StableId);
        AddString(Object, TEXT("display_name"), Member.DisplayName);
        AddString(Object, TEXT("role"), RoleName(Member.Role));
        Object->SetNumberField(TEXT("morale"), Member.Emotion.Morale);
        Object->SetNumberField(TEXT("fatigue"), Member.Emotion.Fatigue);
        Object->SetNumberField(TEXT("alertness"), Member.Emotion.Alertness);
        Object->SetNumberField(TEXT("social_need"), Member.Emotion.SocialNeed);
        AddString(Object, TEXT("current_duty"), DutyName(Member.CurrentDuty));
        AddString(Object, TEXT("duty_reason"), Member.DutyReason);
        Object->SetNumberField(TEXT("duty_revision"), Member.DutyRevision);
        Out.Add(MakeShared<FJsonValueObject>(Object));
    }

    bool ReadMember(const TSharedPtr<FJsonObject>& Object, FMedievalSocietyMember& Out, FString& Error)
    {
        FString Role;
        FString Duty;
        if (!ReadString(Object, TEXT("stable_id"), Out.StableId, Error)
            || !ReadString(Object, TEXT("display_name"), Out.DisplayName, Error, true, false)
            || !ReadString(Object, TEXT("role"), Role, Error)
            || !ParseRole(Role, Out.Role)
            || !ReadString(Object, TEXT("current_duty"), Duty, Error)
            || !ParseDuty(Duty, Out.CurrentDuty)
            || !ReadString(Object, TEXT("duty_reason"), Out.DutyReason, Error, false, false)
            || !ReadInt(Object, TEXT("duty_revision"), Out.DutyRevision, Error))
        {
            if (Error.IsEmpty()) Error = TEXT("Invalid member enum");
            return false;
        }
        double Morale = 0.0, Fatigue = 0.0, Alertness = 0.0, SocialNeed = 0.0;
        if (!Object->TryGetNumberField(TEXT("morale"), Morale) || !Object->TryGetNumberField(TEXT("fatigue"), Fatigue)
            || !Object->TryGetNumberField(TEXT("alertness"), Alertness) || !Object->TryGetNumberField(TEXT("social_need"), SocialNeed)
            || !FMath::IsFinite(Morale) || !FMath::IsFinite(Fatigue) || !FMath::IsFinite(Alertness) || !FMath::IsFinite(SocialNeed))
        {
            Error = TEXT("Invalid member emotion");
            return false;
        }
        Out.Emotion.Morale = static_cast<float>(Morale);
        Out.Emotion.Fatigue = static_cast<float>(Fatigue);
        Out.Emotion.Alertness = static_cast<float>(Alertness);
        Out.Emotion.SocialNeed = static_cast<float>(SocialNeed);
        return true;
    }
}

namespace HearthMedievalPersistence
{
    bool Serialize(const FMedievalSocietyState& State, FString& OutJson, FString* OutError)
    {
        OutJson.Empty();
        if (OutError)
        {
            OutError->Empty();
        }
        FString Error;
        if (!HearthMedievalSociety::ValidateState(State, &Error))
        {
            return Fail(OutError, Error);
        }
        for (const FMedievalStorage& Storage : State.Storages)
        {
            if (!ValidateItems(Storage.Inventory, Error, false) || !ValidateItems(Storage.Reserved, Error, false))
                return Fail(OutError, Error);
        }
        for (const FMedievalCart& Cart : State.Carts)
        {
            if (!ValidateItems(Cart.Cargo, Error, true)) return Fail(OutError, Error);
        }
        for (const FMedievalFreightReservation& Reservation : State.FreightReservations)
        {
            if (!ValidateItems(Reservation.Cargo, Error, true)) return Fail(OutError, Error);
            if (StageName(Reservation.Stage)[0] == TCHAR('\0')) return Fail(OutError, TEXT("Invalid freight stage"));
        }
        for (const FMedievalSocietyMember& Member : State.Members)
        {
            if (DutyName(Member.CurrentDuty)[0] == TCHAR('\0')) return Fail(OutError, TEXT("Invalid duty action"));
        }

        const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schema_version"), 1);
        Root->SetNumberField(TEXT("revision"), State.Revision);
        TArray<TSharedPtr<FJsonValue>> Members;
        for (const FMedievalSocietyMember& Member : State.Members) WriteMember(Member, Members);
        Root->SetArrayField(TEXT("members"), Members);

        TArray<TSharedPtr<FJsonValue>> Visitors;
        for (const FMedievalVisitorRecord& Visitor : State.Visitors)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            AddString(O, TEXT("stable_id"), Visitor.StableId); AddString(O, TEXT("display_name"), Visitor.DisplayName);
            O->SetBoolField(TEXT("known_resident"), Visitor.bKnownResident); O->SetBoolField(TEXT("checked_in"), Visitor.bCheckedIn);
            O->SetBoolField(TEXT("allowed"), Visitor.bAllowed); AddString(O, TEXT("last_event_id"), Visitor.LastEventId);
            AddString(O, TEXT("last_reason"), Visitor.LastReason); Visitors.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("visitors"), Visitors);

        TArray<TSharedPtr<FJsonValue>> Events;
        for (const FMedievalSocietyEvent& Event : State.Events)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            AddString(O, TEXT("event_id"), Event.EventId); AddString(O, TEXT("kind"), Event.Kind);
            AddString(O, TEXT("subject_id"), Event.SubjectId); AddString(O, TEXT("guard_id"), Event.GuardId);
            O->SetBoolField(TEXT("allowed"), Event.bAllowed); AddString(O, TEXT("reason"), Event.Reason);
            O->SetNumberField(TEXT("revision"), Event.Revision); Events.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("events"), Events);

        TArray<TSharedPtr<FJsonValue>> Storages;
        for (const FMedievalStorage& Storage : State.Storages)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>(); AddString(O, TEXT("stable_id"), Storage.StableId);
            WriteItems(O, TEXT("inventory"), Storage.Inventory); WriteItems(O, TEXT("reserved"), Storage.Reserved);
            Storages.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("storages"), Storages);

        TArray<TSharedPtr<FJsonValue>> Horses;
        for (const FMedievalHorse& Horse : State.Horses)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>(); AddString(O, TEXT("stable_id"), Horse.StableId);
            AddString(O, TEXT("harnessed_cart_id"), Horse.HarnessedCartId); Horses.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("horses"), Horses);

        TArray<TSharedPtr<FJsonValue>> Carts;
        for (const FMedievalCart& Cart : State.Carts)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>(); AddString(O, TEXT("stable_id"), Cart.StableId);
            AddString(O, TEXT("operator_stable_id"), Cart.OperatorStableId); AddString(O, TEXT("horse_stable_id"), Cart.HorseStableId);
            O->SetNumberField(TEXT("capacity_units"), Cart.CapacityUnits); WriteItems(O, TEXT("cargo"), Cart.Cargo);
            AddString(O, TEXT("active_reservation_id"), Cart.ActiveReservationId); Carts.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("carts"), Carts);

        TArray<TSharedPtr<FJsonValue>> Reservations;
        for (const FMedievalFreightReservation& Reservation : State.FreightReservations)
        {
            const TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>(); AddString(O, TEXT("reservation_id"), Reservation.ReservationId);
            AddString(O, TEXT("cart_id"), Reservation.CartId); AddString(O, TEXT("operator_stable_id"), Reservation.OperatorStableId);
            AddString(O, TEXT("source_storage_id"), Reservation.SourceStorageId); AddString(O, TEXT("destination_storage_id"), Reservation.DestinationStorageId);
            WriteItems(O, TEXT("cargo"), Reservation.Cargo); AddString(O, TEXT("stage"), StageName(Reservation.Stage));
            Reservations.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("freight_reservations"), Reservations);

        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) return Fail(OutError, TEXT("JSON serialization failed"));
        return true;
    }

    bool Deserialize(const FString& Json, FMedievalSocietyState& OutState, FString& OutError)
    {
        OutError.Empty();
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Fail(&OutError, TEXT("Invalid JSON root"));
        int32 SchemaVersion = 0;
        if (!ReadInt(Root, TEXT("schema_version"), SchemaVersion, OutError) || SchemaVersion != 1)
            return Fail(&OutError, TEXT("Unsupported schema_version"));
        FMedievalSocietyState Temp;
        if (!ReadInt(Root, TEXT("revision"), Temp.Revision, OutError)) return false;

        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!ReadObjectArray(Root, TEXT("members"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values) { TSharedPtr<FJsonObject> O; FMedievalSocietyMember M; if (!ReadObject(Value, O, OutError) || !ReadMember(O, M, OutError)) return false; Temp.Members.Add(M); }
        if (!ReadObjectArray(Root, TEXT("visitors"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            TSharedPtr<FJsonObject> O; FMedievalVisitorRecord V; if (!ReadObject(Value, O, OutError)) return false;
            if (!ReadString(O, TEXT("stable_id"), V.StableId, OutError) || !ReadString(O, TEXT("display_name"), V.DisplayName, OutError, true, false)
                || !ReadBool(O, TEXT("known_resident"), V.bKnownResident, OutError) || !ReadBool(O, TEXT("checked_in"), V.bCheckedIn, OutError)
                || !ReadBool(O, TEXT("allowed"), V.bAllowed, OutError) || !ReadString(O, TEXT("last_event_id"), V.LastEventId, OutError, false, false)
                || !ReadString(O, TEXT("last_reason"), V.LastReason, OutError, false, false)) return false;
            Temp.Visitors.Add(V);
        }
        if (!ReadObjectArray(Root, TEXT("events"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            TSharedPtr<FJsonObject> O; FMedievalSocietyEvent E; if (!ReadObject(Value, O, OutError)) return false;
            if (!ReadString(O, TEXT("event_id"), E.EventId, OutError) || !ReadString(O, TEXT("kind"), E.Kind, OutError)
                || !ReadString(O, TEXT("subject_id"), E.SubjectId, OutError) || !ReadString(O, TEXT("guard_id"), E.GuardId, OutError, false, false)
                || !ReadBool(O, TEXT("allowed"), E.bAllowed, OutError) || !ReadString(O, TEXT("reason"), E.Reason, OutError)
                || !ReadInt(O, TEXT("revision"), E.Revision, OutError)) return false;
            Temp.Events.Add(E);
        }
        if (!ReadObjectArray(Root, TEXT("storages"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values) { TSharedPtr<FJsonObject> O; FMedievalStorage S; if (!ReadObject(Value, O, OutError) || !ReadString(O, TEXT("stable_id"), S.StableId, OutError) || !ReadItems(O, TEXT("inventory"), S.Inventory, OutError, false) || !ReadItems(O, TEXT("reserved"), S.Reserved, OutError, false)) return false; Temp.Storages.Add(S); }
        if (!ReadObjectArray(Root, TEXT("horses"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values) { TSharedPtr<FJsonObject> O; FMedievalHorse H; if (!ReadObject(Value, O, OutError) || !ReadString(O, TEXT("stable_id"), H.StableId, OutError) || !ReadString(O, TEXT("harnessed_cart_id"), H.HarnessedCartId, OutError, false, false)) return false; Temp.Horses.Add(H); }
        if (!ReadObjectArray(Root, TEXT("carts"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values) { TSharedPtr<FJsonObject> O; FMedievalCart C; if (!ReadObject(Value, O, OutError) || !ReadString(O, TEXT("stable_id"), C.StableId, OutError) || !ReadString(O, TEXT("operator_stable_id"), C.OperatorStableId, OutError) || !ReadString(O, TEXT("horse_stable_id"), C.HorseStableId, OutError) || !ReadInt(O, TEXT("capacity_units"), C.CapacityUnits, OutError) || !ReadItems(O, TEXT("cargo"), C.Cargo, OutError, true) || !ReadString(O, TEXT("active_reservation_id"), C.ActiveReservationId, OutError, false, false)) return false; Temp.Carts.Add(C); }
        if (!ReadObjectArray(Root, TEXT("freight_reservations"), Values, OutError)) return false;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            TSharedPtr<FJsonObject> O; FMedievalFreightReservation R; FString Stage;
            if (!ReadObject(Value, O, OutError) || !ReadString(O, TEXT("reservation_id"), R.ReservationId, OutError) || !ReadString(O, TEXT("cart_id"), R.CartId, OutError)
                || !ReadString(O, TEXT("operator_stable_id"), R.OperatorStableId, OutError) || !ReadString(O, TEXT("source_storage_id"), R.SourceStorageId, OutError)
                || !ReadString(O, TEXT("destination_storage_id"), R.DestinationStorageId, OutError) || !ReadItems(O, TEXT("cargo"), R.Cargo, OutError, true)
                || !ReadString(O, TEXT("stage"), Stage, OutError) || !ParseStage(Stage, R.Stage)) return false;
            Temp.FreightReservations.Add(R);
        }
        if (!HearthMedievalSociety::ValidateState(Temp, &OutError)) return false;
        OutState = MoveTemp(Temp);
        return true;
    }
}
