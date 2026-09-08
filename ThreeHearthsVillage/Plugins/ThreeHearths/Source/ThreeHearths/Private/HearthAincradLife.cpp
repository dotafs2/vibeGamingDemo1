#include "HearthAincradLife.h"
#include "HearthAincradTownLayout.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

namespace HearthAincradLife
{
    namespace
    {
        using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

        constexpr int32 LifeVersion = 1;
        constexpr int32 MaxEvents = 4096;
        constexpr int32 MaxContracts = 256;
        constexpr int32 MaxItems = 64;
        constexpr int32 MaxSkills = 64;
        constexpr int32 MaxInboxEvents = 1024;
        constexpr int32 MaxApplied = 4096;
        constexpr int32 MaxRelations = 256;
        constexpr int32 MaxTextLength = 2000;
        constexpr double InteractionDistanceCm = 250.0;
        constexpr double ResidentHandoffDistanceCm = 220.0;

        bool RequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Out, int32 MaxLength = 256)
        {
            return Object.IsValid() && Object->TryGetStringField(Key, Out) && !Out.IsEmpty() && Out.Len() <= MaxLength;
        }

        bool OptionalString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Out, int32 MaxLength = 256)
        {
            if (!Object.IsValid() || !Object->TryGetStringField(Key, Out)) return false;
            return Out.Len() <= MaxLength;
        }

        bool IntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Out, int64 Minimum, int64 Maximum)
        {
            double Value = 0.0;
            if (!Object.IsValid() || !Object->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value != FMath::FloorToDouble(Value) || Value < static_cast<double>(Minimum) || Value > static_cast<double>(Maximum)) return false;
            Out = static_cast<int64>(Value);
            return true;
        }

        bool NumberArray3(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double& X, double& Y, double& Z)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || Values->Num() != 3) return false;
            return (*Values)[0].IsValid() && (*Values)[0]->TryGetNumber(X) && (*Values)[1].IsValid() && (*Values)[1]->TryGetNumber(Y) && (*Values)[2].IsValid() && (*Values)[2]->TryGetNumber(Z)
                && FMath::IsFinite(X) && FMath::IsFinite(Y) && FMath::IsFinite(Z);
        }

        FJsonArray StringArray(const TArray<FString>& Values)
        {
            FJsonArray Result;
            for (const FString& Value : Values) Result.Add(MakeShared<FJsonValueString>(Value));
            return Result;
        }

        TSharedPtr<FJsonObject> ObjectValue(const TSharedPtr<FJsonValue>& Value)
        {
            return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        }

        bool GetArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const TArray<TSharedPtr<FJsonValue>>*& Out, int32 Maximum, FString& Error)
        {
            if (!Object.IsValid() || !Object->TryGetArrayField(Key, Out) || Out->Num() > Maximum)
            {
                Error = FString::Printf(TEXT("life.%s is missing or too large"), Key);
                return false;
            }
            return true;
        }

        bool CollectResidents(const TSharedRef<FJsonObject>& World, TMap<FString, TSharedPtr<FJsonObject>>& Out, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!World->TryGetArrayField(TEXT("residents"), Values) || Values->Num() != 13)
            {
                Error = TEXT("life requires the 13-resident world profile");
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Resident = ObjectValue(Value);
                FString Id;
                if (!RequiredString(Resident, TEXT("stable_id"), Id, 64) || Out.Contains(Id))
                {
                    Error = TEXT("life resident stable_id is missing or duplicated");
                    return false;
                }
                Out.Add(Id, Resident);
            }
            return true;
        }

        TSharedPtr<FJsonObject> ResidentById(const TMap<FString, TSharedPtr<FJsonObject>>& Residents, const FString& ResidentId)
        {
            const TSharedPtr<FJsonObject>* Found = Residents.Find(ResidentId);
            return Found ? *Found : nullptr;
        }

        TSharedPtr<FJsonObject> ResidentByName(const TMap<FString, TSharedPtr<FJsonObject>>& Residents, const FString& Name)
        {
            for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents)
            {
                FString Candidate;
                if (Pair.Value.IsValid() && Pair.Value->TryGetStringField(TEXT("name"), Candidate) && Candidate == Name) return Pair.Value;
            }
            return nullptr;
        }

        bool GetLife(const TSharedRef<FJsonObject>& World, TSharedPtr<FJsonObject>& Out, FString& Error, bool bAllowAbsent)
        {
            if (!World->HasField(TEXT("life")))
            {
                if (bAllowAbsent) { Out.Reset(); return true; }
                Error = TEXT("life extension is absent");
                return false;
            }
            if (!World->HasTypedField<EJson::Object>(TEXT("life")))
            {
                Error = TEXT("life extension is not an object");
                return false;
            }
            Out = World->GetObjectField(TEXT("life"));
            if (!Out.IsValid()) { Error = TEXT("life extension is null"); return false; }
            return true;
        }

        bool IsRepairSkill(const FString& SkillId)
        {
            return SkillId == TEXT("metal_repair") || SkillId == TEXT("wood_repair");
        }

        FString SkillForPart(const FString& Part)
        {
            return Part == TEXT("edge") ? TEXT("metal_repair") : TEXT("wood_repair");
        }

        bool HasSkill(const TSharedPtr<FJsonObject>& Life, const FString& ResidentId, const FString& SkillId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("skills"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Skill = ObjectValue(Value);
                FString Holder, Id;
                if (RequiredString(Skill, TEXT("resident_id"), Holder, 64) && RequiredString(Skill, TEXT("skill_id"), Id, 64) && Holder == ResidentId && Id == SkillId) return true;
            }
            return false;
        }

        TSharedPtr<FJsonObject> FindItem(const TSharedPtr<FJsonObject>& Life, const FString& ItemId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("items"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Item = ObjectValue(Value);
                FString Id;
                if (RequiredString(Item, TEXT("id"), Id, 128) && Id == ItemId) return Item;
            }
            return nullptr;
        }

        TSharedPtr<FJsonObject> FindContract(const TSharedPtr<FJsonObject>& Life, const FString& ContractId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("contracts"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Contract = ObjectValue(Value);
                FString Id;
                if (RequiredString(Contract, TEXT("id"), Id, 128) && Id == ContractId) return Contract;
            }
            return nullptr;
        }

        TSharedPtr<FJsonObject> FindAccount(const TSharedPtr<FJsonObject>& Life, const FString& ResidentId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("accounts"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Account = ObjectValue(Value);
                FString Id;
                if (RequiredString(Account, TEXT("resident_id"), Id, 64) && Id == ResidentId) return Account;
            }
            return nullptr;
        }

        bool IsActiveStatus(const FString& Status)
        {
            return Status == TEXT("proposed") || Status == TEXT("accepted") || Status == TEXT("delivered") || Status == TEXT("completed");
        }

        bool IsTerminalStatus(const FString& Status)
        {
            return Status == TEXT("rejected") || Status == TEXT("cancelled") || Status == TEXT("collected");
        }

        TSharedPtr<FJsonObject> ActiveContractForItem(const TSharedPtr<FJsonObject>& Life, const FString& ItemId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("contracts"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Contract = ObjectValue(Value);
                FString ExistingItem, Status;
                if (RequiredString(Contract, TEXT("item_id"), ExistingItem, 128) && Contract->TryGetStringField(TEXT("status"), Status) && ExistingItem == ItemId && IsActiveStatus(Status)) return Contract;
            }
            return nullptr;
        }

        FString NameOf(const TMap<FString, TSharedPtr<FJsonObject>>& Residents, const FString& Id)
        {
            const TSharedPtr<FJsonObject> Resident = ResidentById(Residents, Id);
            FString Name;
            return Resident.IsValid() && Resident->TryGetStringField(TEXT("name"), Name) ? Name : Id;
        }

        bool BuildingForOwner(const TSharedRef<FJsonObject>& World, const FString& ResidentId, FString& OutBuilding)
        {
            const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
            if (!World->TryGetArrayField(TEXT("buildings"), Buildings)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Buildings)
            {
                const TSharedPtr<FJsonObject> Building = ObjectValue(Value);
                FString Owner, Id;
                if (RequiredString(Building, TEXT("owner_id"), Owner, 64) && RequiredString(Building, TEXT("building_id"), Id, 128) && Owner == ResidentId)
                {
                    OutBuilding = Id;
                    return true;
                }
            }
            return false;
        }

        bool BuildingPosition(const TSharedRef<FJsonObject>& World, const FString& BuildingId, double& X, double& Y, double& Z)
        {
            const HearthAincradTownLayout::FPlan Plan = HearthAincradTownLayout::Build();
            const HearthAincradTownLayout::FBuilding* Building = HearthAincradTownLayout::Find(Plan, BuildingId);
            if (Building == nullptr) return false;
            X = Building->WorkCm.X; Y = Building->WorkCm.Y; Z = Building->WorkCm.Z;
            return FMath::IsFinite(X) && FMath::IsFinite(Y) && FMath::IsFinite(Z);
        }

        bool RuntimePosition(const TSharedPtr<FJsonObject>& Resident, double& X, double& Y, double& Z)
        {
            if (!Resident.IsValid() || !Resident->HasTypedField<EJson::Object>(TEXT("runtime"))) return false;
            return NumberArray3(Resident->GetObjectField(TEXT("runtime")), TEXT("position_cm"), X, Y, Z);
        }

        bool AtBuilding(const TSharedRef<FJsonObject>& World, const TSharedPtr<FJsonObject>& Resident, const FString& BuildingId)
        {
            double RX = 0.0, RY = 0.0, RZ = 0.0, BX = 0.0, BY = 0.0, BZ = 0.0;
            if (!RuntimePosition(Resident, RX, RY, RZ) || !BuildingPosition(World, BuildingId, BX, BY, BZ)) return false;
            return FMath::Square(RX - BX) + FMath::Square(RY - BY) + FMath::Square(RZ - BZ) <= FMath::Square(InteractionDistanceCm);
        }

        bool ResidentsClose(const TSharedPtr<FJsonObject>& First, const TSharedPtr<FJsonObject>& Second)
        {
            double AX = 0.0, AY = 0.0, AZ = 0.0, BX = 0.0, BY = 0.0, BZ = 0.0;
            if (!RuntimePosition(First, AX, AY, AZ) || !RuntimePosition(Second, BX, BY, BZ)) return false;
            return FMath::Square(AX - BX) + FMath::Square(AY - BY) + FMath::Square(AZ - BZ) <= FMath::Square(ResidentHandoffDistanceCm);
        }

        bool ParseInteger(const FString& Text, int64& Out)
        {
            if (Text.IsEmpty()) return false;
            for (const TCHAR Character : Text) if (Character < TEXT('0') || Character > TEXT('9')) return false;
            Out = FCString::Atoi64(*Text);
            return true;
        }

        bool CloneWorld(const TSharedRef<FJsonObject>& Source, TSharedPtr<FJsonObject>& Out, FString& Error)
        {
            FString Text;
            if (!FJsonSerializer::Serialize(Source, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text))) { Error = TEXT("life transaction serialization failed"); return false; }
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text), Out) || !Out.IsValid()) { Error = TEXT("life transaction clone failed"); return false; }
            return true;
        }

        TSharedRef<FJsonObject> MakeOption(const FString& Id, const FString& Label, const FString& Verb, const FString& Counterparty = FString(), const FString& TargetBuilding = FString(), int32 DurationSeconds = 0)
        {
            auto Option = MakeShared<FJsonObject>();
            Option->SetStringField(TEXT("id"), Id); Option->SetStringField(TEXT("label"), Label); Option->SetStringField(TEXT("verb"), Verb);
            if (!Counterparty.IsEmpty()) Option->SetStringField(TEXT("counterparty_id"), Counterparty);
            if (!TargetBuilding.IsEmpty()) Option->SetStringField(TEXT("target_building_id"), TargetBuilding);
            if (DurationSeconds > 0) Option->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
            return Option;
        }

        bool AddEvent(const TSharedPtr<FJsonObject>& Life, const FString& Type, const FString& ActorId, const FString& SubjectId, const FString& ContractId, const FString& ItemId, const TArray<FString>& Recipients, const FString& Text, const FString& OperationId, int64& OutSeq, FString& Error)
        {
            int64 CurrentSeq = 0;
            if (!IntegerField(Life, TEXT("seq"), CurrentSeq, 0, TNumericLimits<int64>::Max())) { Error = TEXT("life seq is invalid"); return false; }
            const TArray<TSharedPtr<FJsonValue>>* ExistingEvents = nullptr;
            if (!Life->TryGetArrayField(TEXT("events"), ExistingEvents) || ExistingEvents->Num() >= MaxEvents) { Error = TEXT("life event log is full or invalid"); return false; }
            OutSeq = CurrentSeq + 1;
            Life->SetNumberField(TEXT("seq"), static_cast<double>(OutSeq));
            const FString EventId = FString::Printf(TEXT("life_event_%lld"), OutSeq);
            auto Event = MakeShared<FJsonObject>();
            Event->SetStringField(TEXT("event_id"), EventId); Event->SetNumberField(TEXT("seq"), static_cast<double>(OutSeq)); Event->SetStringField(TEXT("type"), Type);
            Event->SetStringField(TEXT("actor_id"), ActorId); Event->SetStringField(TEXT("subject_id"), SubjectId); Event->SetStringField(TEXT("contract_id"), ContractId); Event->SetStringField(TEXT("item_id"), ItemId);
            Event->SetArrayField(TEXT("recipient_ids"), StringArray(Recipients)); Event->SetStringField(TEXT("text"), Text); Event->SetStringField(TEXT("operation_id"), OperationId); if (Type == TEXT("initial_condition")) Event->SetStringField(TEXT("source"), TEXT("developer_initial_condition"));
            FJsonArray Events = *ExistingEvents; Events.Add(MakeShared<FJsonValueObject>(Event)); Life->SetArrayField(TEXT("events"), Events);

            const TArray<TSharedPtr<FJsonValue>>* Inboxes = nullptr;
            if (!Life->TryGetArrayField(TEXT("inboxes"), Inboxes)) { Error = TEXT("life inboxes are missing"); return false; }
            for (const FString& RecipientId : Recipients)
            {
                bool Found = false;
                FJsonArray NewInboxes = *Inboxes;
                for (const TSharedPtr<FJsonValue>& InboxValue : NewInboxes)
                {
                    const TSharedPtr<FJsonObject> Inbox = ObjectValue(InboxValue);
                    FString InboxResident;
                    if (!RequiredString(Inbox, TEXT("resident_id"), InboxResident, 64) || InboxResident != RecipientId) continue;
                    int64 InboxSeq = 0;
                    if (!IntegerField(Inbox, TEXT("next_seq"), InboxSeq, 0, TNumericLimits<int64>::Max())) { Error = TEXT("life inbox sequence is invalid"); return false; }
                    const TArray<TSharedPtr<FJsonValue>>* InboxEvents = nullptr;
                    if (!Inbox->TryGetArrayField(TEXT("events"), InboxEvents) || InboxEvents->Num() >= MaxInboxEvents) { Error = TEXT("life inbox is full or invalid"); return false; }
                    auto InboxEvent = MakeShared<FJsonObject>(); InboxEvent->SetNumberField(TEXT("seq"), static_cast<double>(InboxSeq + 1)); InboxEvent->SetStringField(TEXT("event_id"), EventId); InboxEvent->SetStringField(TEXT("type"), Type); InboxEvent->SetStringField(TEXT("actor_id"), ActorId); InboxEvent->SetStringField(TEXT("subject_id"), SubjectId); InboxEvent->SetStringField(TEXT("contract_id"), ContractId); InboxEvent->SetStringField(TEXT("item_id"), ItemId); InboxEvent->SetStringField(TEXT("text"), Text);
                    FJsonArray NewInboxEvents = *InboxEvents; NewInboxEvents.Add(MakeShared<FJsonValueObject>(InboxEvent)); Inbox->SetArrayField(TEXT("events"), NewInboxEvents); Inbox->SetNumberField(TEXT("next_seq"), static_cast<double>(InboxSeq + 1)); Found = true; break;
                }
                if (!Found) { Error = TEXT("life event recipient has no inbox"); return false; }
                Inboxes = nullptr;
                if (!Life->TryGetArrayField(TEXT("inboxes"), Inboxes)) { Error = TEXT("life inboxes became invalid"); return false; }
            }
            return true;
        }

        bool AddApplied(const TSharedPtr<FJsonObject>& Life, const FString& OperationId, const FString& ActorId, const FString& OptionId, const FString& Utterance, int64 EventSeq, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
            if (!Life->TryGetArrayField(TEXT("applied"), Existing) || Existing->Num() >= MaxApplied) { Error = TEXT("life applied operation log is full or invalid"); return false; }
            auto Applied = MakeShared<FJsonObject>(); Applied->SetStringField(TEXT("operation_id"), OperationId); Applied->SetStringField(TEXT("actor_id"), ActorId); Applied->SetStringField(TEXT("option_id"), OptionId); Applied->SetStringField(TEXT("utterance"), Utterance); Applied->SetNumberField(TEXT("event_seq"), static_cast<double>(EventSeq));
            FJsonArray Values = *Existing; Values.Add(MakeShared<FJsonValueObject>(Applied)); Life->SetArrayField(TEXT("applied"), Values); return true;
        }

        bool UpdateRelation(const TSharedPtr<FJsonObject>& Life, const FString& FirstId, const FString& SecondId, int64 EventSeq, const FString& Reason, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
            if (!Life->TryGetArrayField(TEXT("relations"), Existing)) { Error = TEXT("life relations are missing"); return false; }
            FJsonArray Values = *Existing;
            for (const TSharedPtr<FJsonValue>& Value : Values)
            {
                const TSharedPtr<FJsonObject> Relation = ObjectValue(Value); FString From, To;
                if (RequiredString(Relation, TEXT("from_id"), From, 64) && RequiredString(Relation, TEXT("to_id"), To, 64) && From == FirstId && To == SecondId)
                {
                    int64 Trust = 0; if (!IntegerField(Relation, TEXT("trust"), Trust, -100, 100)) { Error = TEXT("life relation trust is invalid"); return false; }
                    Relation->SetNumberField(TEXT("trust"), FMath::Clamp<int64>(Trust + 1, -100, 100)); Relation->SetNumberField(TEXT("last_event_seq"), static_cast<double>(EventSeq)); Relation->SetStringField(TEXT("last_reason"), Reason); return true;
                }
            }
            auto Relation = MakeShared<FJsonObject>(); Relation->SetStringField(TEXT("from_id"), FirstId); Relation->SetStringField(TEXT("to_id"), SecondId); Relation->SetNumberField(TEXT("trust"), 1); Relation->SetNumberField(TEXT("last_event_seq"), static_cast<double>(EventSeq)); Relation->SetStringField(TEXT("last_reason"), Reason); Values.Add(MakeShared<FJsonValueObject>(Relation)); Life->SetArrayField(TEXT("relations"), Values); return true;
        }

        bool ValidateLifeObject(const TSharedRef<FJsonObject>& World, const TSharedPtr<FJsonObject>& Life, FString& Error)
        {
            TMap<FString, TSharedPtr<FJsonObject>> Residents;
            if (!CollectResidents(World, Residents, Error)) return false;
            int64 Version = 0, Sequence = 0;
            FString Source;
            if (!IntegerField(Life, TEXT("version"), Version, LifeVersion, LifeVersion) || !IntegerField(Life, TEXT("seq"), Sequence, 0, TNumericLimits<int64>::Max()) || !RequiredString(Life, TEXT("initialization_source"), Source, 128) || Source != TEXT("developer_initial_condition")) { Error = TEXT("life version, seq or initialization source is invalid"); return false; }

            const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
            if (!GetArray(Life, TEXT("initial_conditions"), Conditions, 64, Error)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Conditions)
            {
                const TSharedPtr<FJsonObject> Condition = ObjectValue(Value); FString ConditionSource, ResidentId, Fact;
                if (!RequiredString(Condition, TEXT("source"), ConditionSource, 128) || ConditionSource != Source || !RequiredString(Condition, TEXT("resident_id"), ResidentId, 64) || !Residents.Contains(ResidentId) || !RequiredString(Condition, TEXT("fact"), Fact, MaxTextLength)) { Error = TEXT("life initial condition is invalid"); return false; }
            }

            const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
            if (!GetArray(Life, TEXT("items"), Items, MaxItems, Error)) return false;
            TSet<FString> ItemIds;
            TMap<FString, TSharedPtr<FJsonObject>> ItemById;
            for (const TSharedPtr<FJsonValue>& Value : *Items)
            {
                const TSharedPtr<FJsonObject> Item = ObjectValue(Value); FString Id, Kind, Owner, Custodian, ItemSource; int64 Edge = 0, Handle = 0;
                if (!RequiredString(Item, TEXT("id"), Id, 128) || ItemIds.Contains(Id) || !RequiredString(Item, TEXT("kind"), Kind, 64) || Kind != TEXT("axe") || !RequiredString(Item, TEXT("owner_id"), Owner, 64) || !Residents.Contains(Owner) || !RequiredString(Item, TEXT("custodian_id"), Custodian, 64) || !Residents.Contains(Custodian) || !IntegerField(Item, TEXT("edge"), Edge, 0, 100) || !IntegerField(Item, TEXT("handle"), Handle, 0, 100) || !RequiredString(Item, TEXT("source"), ItemSource, 128)) { Error = TEXT("life item is invalid"); return false; }
                ItemIds.Add(Id); ItemById.Add(Id, Item);
            }

            const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
            if (!GetArray(Life, TEXT("skills"), Skills, MaxSkills, Error)) return false;
            TSet<FString> SkillKeys;
            for (const TSharedPtr<FJsonValue>& Value : *Skills)
            {
                const TSharedPtr<FJsonObject> Skill = ObjectValue(Value); FString ResidentId, SkillId, SkillSource;
                if (!RequiredString(Skill, TEXT("resident_id"), ResidentId, 64) || !Residents.Contains(ResidentId) || !RequiredString(Skill, TEXT("skill_id"), SkillId, 64) || !IsRepairSkill(SkillId) || !RequiredString(Skill, TEXT("source"), SkillSource, 128)) { Error = TEXT("life skill is invalid"); return false; }
                const FString SkillKey = ResidentId + TEXT("/") + SkillId;
                if (SkillKeys.Contains(SkillKey)) { Error = TEXT("life skill is duplicated"); return false; }
                SkillKeys.Add(SkillKey);
            }

            const TArray<TSharedPtr<FJsonValue>>* Accounts = nullptr;
            if (!Life->TryGetArrayField(TEXT("accounts"), Accounts) || Accounts->Num() != Residents.Num()) { Error = TEXT("life accounts must contain one entry per resident"); return false; }
            TSet<FString> AccountIds;
            TMap<FString, TSharedPtr<FJsonObject>> AccountById;
            for (const TSharedPtr<FJsonValue>& Value : *Accounts)
            {
                const TSharedPtr<FJsonObject> Account = ObjectValue(Value); FString ResidentId, AccountSource; int64 Wood = 0, Iron = 0, Kindling = 0, Reserved = 0;
                if (!RequiredString(Account, TEXT("resident_id"), ResidentId, 64) || !Residents.Contains(ResidentId) || AccountIds.Contains(ResidentId) || !IntegerField(Account, TEXT("wood"), Wood, 0, 1000000) || !IntegerField(Account, TEXT("iron"), Iron, 0, 1000000) || !IntegerField(Account, TEXT("kindling"), Kindling, 0, 1000000) || !IntegerField(Account, TEXT("reserved_col"), Reserved, 0, 1000000) || !RequiredString(Account, TEXT("source"), AccountSource, 128)) { Error = TEXT("life material account is invalid"); return false; }
                AccountIds.Add(ResidentId); AccountById.Add(ResidentId, Account);
            }

            const TArray<TSharedPtr<FJsonValue>>* Contracts = nullptr;
            if (!GetArray(Life, TEXT("contracts"), Contracts, MaxContracts, Error)) return false;
            TSet<FString> ContractIds;
            TMap<FString, int64> ReservedByOwner;
            TMap<FString, int32> ActiveItems;
            for (const TSharedPtr<FJsonValue>& Value : *Contracts)
            {
                const TSharedPtr<FJsonObject> Contract = ObjectValue(Value); FString Id, Part, ItemId, Owner, Worker, Status; int64 Price = 0, Reserved = 0;
                if (!RequiredString(Contract, TEXT("id"), Id, 128) || ContractIds.Contains(Id) || !RequiredString(Contract, TEXT("part"), Part, 32) || !(Part == TEXT("edge") || Part == TEXT("handle")) || !RequiredString(Contract, TEXT("item_id"), ItemId, 128) || !ItemById.Contains(ItemId) || !RequiredString(Contract, TEXT("owner_id"), Owner, 64) || !Residents.Contains(Owner) || !RequiredString(Contract, TEXT("worker_id"), Worker, 64) || !Residents.Contains(Worker) || Owner == Worker || !RequiredString(Contract, TEXT("status"), Status, 32) || !(Status == TEXT("proposed") || Status == TEXT("accepted") || Status == TEXT("delivered") || Status == TEXT("completed") || IsTerminalStatus(Status)) || !IntegerField(Contract, TEXT("price_col"), Price, 0, 1000000) || (Price != 2 && Price != 5 && Price != 8) || !IntegerField(Contract, TEXT("reserved_col"), Reserved, 0, 1000000) || !HasSkill(Life, Worker, SkillForPart(Part))) { Error = TEXT("life contract header is invalid"); return false; }
                const TSharedPtr<FJsonObject> Item = ItemById[ItemId]; FString ItemOwner, Custodian; Item->TryGetStringField(TEXT("owner_id"), ItemOwner); Item->TryGetStringField(TEXT("custodian_id"), Custodian);
                if (ItemOwner != Owner || (Status == TEXT("proposed") && (Reserved != 0 || Custodian != Owner)) || (Status == TEXT("accepted") && (Reserved != Price || Custodian != Owner)) || ((Status == TEXT("delivered") || Status == TEXT("completed")) && (Reserved != Price || Custodian != Worker)) || (IsTerminalStatus(Status) && Reserved != 0)) { Error = TEXT("life contract status or reserve is invalid"); return false; }
                if (Status == TEXT("completed") && Item->GetNumberField(*Part) != 100.0) { Error = TEXT("completed life repair does not have a repaired component"); return false; }
                if (IsActiveStatus(Status)) { ActiveItems.FindOrAdd(ItemId)++; if (ActiveItems[ItemId] > 1) { Error = TEXT("one tool has more than one active life contract"); return false; } }
                if (Reserved > 0) ReservedByOwner.FindOrAdd(Owner) += Reserved;
                ContractIds.Add(Id);
            }
            for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : AccountById)
            {
                int64 Reserved = 0; if (!IntegerField(Pair.Value, TEXT("reserved_col"), Reserved, 0, 1000000) || Reserved != ReservedByOwner.FindRef(Pair.Key)) { Error = TEXT("life account reserve does not match contracts"); return false; }
            }

            const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
            if (!GetArray(Life, TEXT("events"), Events, MaxEvents, Error)) return false;
            TSet<FString> EventIds; int64 PreviousSeq = 0;
            for (const TSharedPtr<FJsonValue>& Value : *Events)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value); FString EventId, Type, Actor, Subject, Contract, Item, Text, Operation; int64 EventSeq = 0;
                if (!RequiredString(Event, TEXT("event_id"), EventId, 128) || EventIds.Contains(EventId) || !IntegerField(Event, TEXT("seq"), EventSeq, 1, Sequence) || EventSeq <= PreviousSeq || !RequiredString(Event, TEXT("type"), Type, 64) || !RequiredString(Event, TEXT("actor_id"), Actor, 64) || !Residents.Contains(Actor) || !OptionalString(Event, TEXT("subject_id"), Subject, 64) || (!Subject.IsEmpty() && !Residents.Contains(Subject)) || !OptionalString(Event, TEXT("contract_id"), Contract, 128) || (!Contract.IsEmpty() && !ContractIds.Contains(Contract)) || !OptionalString(Event, TEXT("item_id"), Item, 128) || (!Item.IsEmpty() && !ItemIds.Contains(Item)) || !RequiredString(Event, TEXT("text"), Text, MaxTextLength) || !RequiredString(Event, TEXT("operation_id"), Operation, 128)) { Error = TEXT("life event is invalid"); return false; }
                const TArray<TSharedPtr<FJsonValue>>* Recipients = nullptr; if (!Event->TryGetArrayField(TEXT("recipient_ids"), Recipients) || Recipients->Num() == 0) { Error = TEXT("life event recipients are invalid"); return false; }
                TSet<FString> RecipientSet;
                for (const TSharedPtr<FJsonValue>& RecipientValue : *Recipients) { FString Recipient; if (!RecipientValue.IsValid() || !RecipientValue->TryGetString(Recipient) || !Residents.Contains(Recipient) || RecipientSet.Contains(Recipient)) { Error = TEXT("life event recipient is invalid"); return false; } RecipientSet.Add(Recipient); }
                EventIds.Add(EventId); PreviousSeq = EventSeq;
            }
            if ((Events->Num() == 0 && Sequence != 0) || (Events->Num() > 0 && PreviousSeq != Sequence)) { Error = TEXT("life seq does not match event log"); return false; }

            const TArray<TSharedPtr<FJsonValue>>* Applied = nullptr;
            if (!GetArray(Life, TEXT("applied"), Applied, MaxApplied, Error)) return false;
            TSet<FString> OperationIds;
            for (const TSharedPtr<FJsonValue>& Value : *Applied)
            {
                const TSharedPtr<FJsonObject> Record = ObjectValue(Value); FString OperationId, Actor, Option, Utterance; int64 EventSeq = 0;
                if (!RequiredString(Record, TEXT("operation_id"), OperationId, 128) || OperationIds.Contains(OperationId) || !RequiredString(Record, TEXT("actor_id"), Actor, 64) || !Residents.Contains(Actor) || !RequiredString(Record, TEXT("option_id"), Option, 256) || !OptionalString(Record, TEXT("utterance"), Utterance, MaxTextLength) || !IntegerField(Record, TEXT("event_seq"), EventSeq, 1, Sequence)) { Error = TEXT("life applied operation is invalid"); return false; }
                OperationIds.Add(OperationId);
            }

            const TArray<TSharedPtr<FJsonValue>>* Inboxes = nullptr;
            if (!Life->TryGetArrayField(TEXT("inboxes"), Inboxes) || Inboxes->Num() != Residents.Num()) { Error = TEXT("life inboxes must contain one entry per resident"); return false; }
            TSet<FString> InboxIds;
            for (const TSharedPtr<FJsonValue>& Value : *Inboxes)
            {
                const TSharedPtr<FJsonObject> Inbox = ObjectValue(Value); FString ResidentId; int64 NextSeq = 0;
                if (!RequiredString(Inbox, TEXT("resident_id"), ResidentId, 64) || !Residents.Contains(ResidentId) || InboxIds.Contains(ResidentId) || !IntegerField(Inbox, TEXT("next_seq"), NextSeq, 0, TNumericLimits<int64>::Max())) { Error = TEXT("life inbox header is invalid"); return false; }
                InboxIds.Add(ResidentId);
                const TArray<TSharedPtr<FJsonValue>>* InboxEvents = nullptr; if (!Inbox->TryGetArrayField(TEXT("events"), InboxEvents) || InboxEvents->Num() > MaxInboxEvents) { Error = TEXT("life inbox events are invalid"); return false; }
                int64 PreviousInboxSeq = 0;
                for (const TSharedPtr<FJsonValue>& InboxValue : *InboxEvents) { const TSharedPtr<FJsonObject> InboxEvent = ObjectValue(InboxValue); FString EventId, Type, Actor, Subject, Contract, Item, Text; int64 InboxSeq = 0; if (!RequiredString(InboxEvent, TEXT("event_id"), EventId, 128) || !EventIds.Contains(EventId) || !IntegerField(InboxEvent, TEXT("seq"), InboxSeq, 1, NextSeq) || InboxSeq <= PreviousInboxSeq || !RequiredString(InboxEvent, TEXT("type"), Type, 64) || !RequiredString(InboxEvent, TEXT("actor_id"), Actor, 64) || !Residents.Contains(Actor) || !OptionalString(InboxEvent, TEXT("subject_id"), Subject, 64) || (!Subject.IsEmpty() && !Residents.Contains(Subject)) || !OptionalString(InboxEvent, TEXT("contract_id"), Contract, 128) || (!Contract.IsEmpty() && !ContractIds.Contains(Contract)) || !OptionalString(InboxEvent, TEXT("item_id"), Item, 128) || (!Item.IsEmpty() && !ItemIds.Contains(Item)) || !RequiredString(InboxEvent, TEXT("text"), Text, MaxTextLength)) { Error = TEXT("life inbox event is invalid"); return false; } PreviousInboxSeq = InboxSeq; }
                if ((InboxEvents->Num() == 0 && NextSeq != 0) || (InboxEvents->Num() > 0 && PreviousInboxSeq != NextSeq)) { Error = TEXT("life inbox sequence is invalid"); return false; }
            }

            const TArray<TSharedPtr<FJsonValue>>* Relations = nullptr;
            if (!GetArray(Life, TEXT("relations"), Relations, MaxRelations, Error)) return false;
            TSet<FString> RelationKeys;
            for (const TSharedPtr<FJsonValue>& Value : *Relations) { const TSharedPtr<FJsonObject> Relation = ObjectValue(Value); FString From, To, Reason; int64 Trust = 0, LastSeq = 0; if (!RequiredString(Relation, TEXT("from_id"), From, 64) || !Residents.Contains(From) || !RequiredString(Relation, TEXT("to_id"), To, 64) || !Residents.Contains(To) || From == To || !IntegerField(Relation, TEXT("trust"), Trust, -100, 100) || !IntegerField(Relation, TEXT("last_event_seq"), LastSeq, 1, Sequence) || !RequiredString(Relation, TEXT("last_reason"), Reason, MaxTextLength)) { Error = TEXT("life relation is invalid"); return false; } const FString RelationKey = From + TEXT("/") + To; if (RelationKeys.Contains(RelationKey)) { Error = TEXT("life relation is duplicated"); return false; } RelationKeys.Add(RelationKey); }

            for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : ItemById)
            {
                const TSharedPtr<FJsonObject> Item = Pair.Value; FString Custodian; Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Custodian != Item->GetStringField(TEXT("owner_id")) && !ActiveContractForItem(Life, Pair.Key).IsValid()) { Error = TEXT("life item custody has no active contract"); return false; }
            }
            return true;
        }

        FString NewContractId(const TSharedPtr<FJsonObject>& Life, int64 Sequence)
        {
            FString Candidate; int32 Suffix = 0;
            do { Candidate = FString::Printf(TEXT("repair_contract_%lld_%d"), Sequence + 1, Suffix++); } while (FindContract(Life, Candidate).IsValid());
            return Candidate;
        }

        void AddCommonOptionFields(const TSharedRef<FJsonObject>& Option, const FString& Counterparty, const FString& TargetBuilding, int32 Duration)
        {
            if (!Counterparty.IsEmpty()) Option->SetStringField(TEXT("counterparty_id"), Counterparty);
            if (!TargetBuilding.IsEmpty()) Option->SetStringField(TEXT("target_building_id"), TargetBuilding);
            if (Duration > 0) Option->SetNumberField(TEXT("duration_seconds"), Duration);
        }
    }

    bool Validate(const TSharedRef<FJsonObject>& World, FString& Error)
    {
        Error.Empty();
        TSharedPtr<FJsonObject> Life;
        if (!GetLife(World, Life, Error, true)) return false;
        return !Life.IsValid() || ValidateLifeObject(World, Life, Error);
    }

    bool Initialize(const TSharedRef<FJsonObject>& World, bool& bAdded, FString& Error)
    {
        bAdded = false; Error.Empty();
        TSharedPtr<FJsonObject> Existing;
        if (!GetLife(World, Existing, Error, true)) return false;
        if (Existing.IsValid()) return ValidateLifeObject(World, Existing.ToSharedRef(), Error);

        TMap<FString, TSharedPtr<FJsonObject>> Residents;
        if (!CollectResidents(World, Residents, Error)) return false;
        const TSharedPtr<FJsonObject> Erin = ResidentByName(Residents, TEXT("艾琳"));
        const TSharedPtr<FJsonObject> Takuya = ResidentByName(Residents, TEXT("拓真"));
        const TSharedPtr<FJsonObject> Kashiwagi = ResidentByName(Residents, TEXT("柏木"));
        if (!Erin.IsValid() || !Takuya.IsValid() || !Kashiwagi.IsValid()) { Error = TEXT("life initial residents 艾琳/拓真/柏木 are missing"); return false; }
        FString ErinId, TakuyaId, KashiwagiId; Erin->TryGetStringField(TEXT("stable_id"), ErinId); Takuya->TryGetStringField(TEXT("stable_id"), TakuyaId); Kashiwagi->TryGetStringField(TEXT("stable_id"), KashiwagiId);

        auto Life = MakeShared<FJsonObject>(); Life->SetNumberField(TEXT("version"), LifeVersion); Life->SetNumberField(TEXT("seq"), 0); Life->SetStringField(TEXT("initialization_source"), TEXT("developer_initial_condition"));
        Life->SetArrayField(TEXT("events"), {}); Life->SetArrayField(TEXT("applied"), {}); Life->SetArrayField(TEXT("contracts"), {}); Life->SetArrayField(TEXT("relations"), {});
        auto Axe = MakeShared<FJsonObject>(); Axe->SetStringField(TEXT("id"), TEXT("aincrad_axe_erin")); Axe->SetStringField(TEXT("kind"), TEXT("axe")); Axe->SetStringField(TEXT("owner_id"), ErinId); Axe->SetStringField(TEXT("custodian_id"), ErinId); Axe->SetNumberField(TEXT("edge"), 20); Axe->SetNumberField(TEXT("handle"), 20); Axe->SetStringField(TEXT("source"), TEXT("developer_initial_condition")); Life->SetArrayField(TEXT("items"), { MakeShared<FJsonValueObject>(Axe) });
        auto Metal = MakeShared<FJsonObject>(); Metal->SetStringField(TEXT("resident_id"), TakuyaId); Metal->SetStringField(TEXT("skill_id"), TEXT("metal_repair")); Metal->SetStringField(TEXT("source"), TEXT("developer_initial_condition"));
        auto Wood = MakeShared<FJsonObject>(); Wood->SetStringField(TEXT("resident_id"), KashiwagiId); Wood->SetStringField(TEXT("skill_id"), TEXT("wood_repair")); Wood->SetStringField(TEXT("source"), TEXT("developer_initial_condition")); Life->SetArrayField(TEXT("skills"), { MakeShared<FJsonValueObject>(Metal), MakeShared<FJsonValueObject>(Wood) });

        FJsonArray Accounts, Conditions, Inboxes;
        for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents)
        {
            auto Account = MakeShared<FJsonObject>(); Account->SetStringField(TEXT("resident_id"), Pair.Key); Account->SetNumberField(TEXT("wood"), 0); Account->SetNumberField(TEXT("iron"), 0); Account->SetNumberField(TEXT("kindling"), 0); Account->SetNumberField(TEXT("reserved_col"), 0); Account->SetStringField(TEXT("source"), TEXT("developer_initial_condition"));
            if (Pair.Key == ErinId) Account->SetNumberField(TEXT("wood"), 2); else if (Pair.Key == TakuyaId) Account->SetNumberField(TEXT("iron"), 1); else if (Pair.Key == KashiwagiId) Account->SetNumberField(TEXT("wood"), 1); Accounts.Add(MakeShared<FJsonValueObject>(Account));
            auto Inbox = MakeShared<FJsonObject>(); Inbox->SetStringField(TEXT("resident_id"), Pair.Key); Inbox->SetNumberField(TEXT("next_seq"), 0); Inbox->SetArrayField(TEXT("events"), {}); Inboxes.Add(MakeShared<FJsonValueObject>(Inbox));
        }
        auto Condition = [&Conditions](const FString& ResidentId, const FString& Fact)
        { auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("source"), TEXT("developer_initial_condition")); Entry->SetStringField(TEXT("resident_id"), ResidentId); Entry->SetStringField(TEXT("fact"), Fact); Conditions.Add(MakeShared<FJsonValueObject>(Entry)); };
        Condition(ErinId, TEXT("拥有一把需要修刃和修柄的柴斧，edge=20，handle=20")); Condition(ErinId, TEXT("拥有2木料")); Condition(TakuyaId, TEXT("拥有metal_repair技能")); Condition(TakuyaId, TEXT("拥有1铁料")); Condition(KashiwagiId, TEXT("拥有wood_repair技能")); Condition(KashiwagiId, TEXT("拥有1木料"));
        Life->SetArrayField(TEXT("accounts"), Accounts); Life->SetArrayField(TEXT("initial_conditions"), Conditions); Life->SetArrayField(TEXT("inboxes"), Inboxes);
        World->SetObjectField(TEXT("life"), Life);
        int64 InitialEventSeq = 0;
        if (!AddEvent(Life, TEXT("initial_condition"), ErinId, ErinId, FString(), TEXT("aincrad_axe_erin"), { ErinId }, TEXT("开发者明确写入：艾琳拥有待修的柴斧和2木料；这不是居民自行发现。"), TEXT("initial_condition:erin"), InitialEventSeq, Error)
            || !AddEvent(Life, TEXT("initial_condition"), TakuyaId, TakuyaId, FString(), FString(), { TakuyaId }, TEXT("开发者明确写入：拓真拥有metal_repair技能和1铁料；这不是居民自行发现。"), TEXT("initial_condition:takuya"), InitialEventSeq, Error)
            || !AddEvent(Life, TEXT("initial_condition"), KashiwagiId, KashiwagiId, FString(), FString(), { KashiwagiId }, TEXT("开发者明确写入：柏木拥有wood_repair技能和1木料；这不是居民自行发现。"), TEXT("initial_condition:kashiwagi"), InitialEventSeq, Error))
        {
            World->RemoveField(TEXT("life"));
            return false;
        }
        if (!ValidateLifeObject(World, Life, Error)) { World->RemoveField(TEXT("life")); return false; }
        bAdded = true;
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Options(const TSharedRef<FJsonObject>& World, const FString& ResidentId)
    {
        TArray<TSharedPtr<FJsonValue>> Result; FString Error; if (!Validate(World, Error)) return Result;
        TSharedPtr<FJsonObject> Life; if (!GetLife(World, Life, Error, false)) return Result;
        TMap<FString, TSharedPtr<FJsonObject>> Residents; if (!CollectResidents(World, Residents, Error) || !Residents.Contains(ResidentId)) return Result;
        const TSharedPtr<FJsonObject> Resident = Residents[ResidentId];
        const TArray<TSharedPtr<FJsonValue>>* Contracts = nullptr; Life->TryGetArrayField(TEXT("contracts"), Contracts);
        for (const TSharedPtr<FJsonValue>& Value : *Contracts)
        {
            const TSharedPtr<FJsonObject> Contract = ObjectValue(Value); FString Id, Owner, Worker, Status, ItemId; Contract->TryGetStringField(TEXT("id"), Id); Contract->TryGetStringField(TEXT("owner_id"), Owner); Contract->TryGetStringField(TEXT("worker_id"), Worker); Contract->TryGetStringField(TEXT("status"), Status); Contract->TryGetStringField(TEXT("item_id"), ItemId);
            FString Part; Contract->TryGetStringField(TEXT("part"), Part); const TSharedPtr<FJsonObject> Item = FindItem(Life, ItemId); FString WorkerBuilding; BuildingForOwner(World, Worker, WorkerBuilding);
            if (ResidentId == Worker && Status == TEXT("proposed")) { Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("accept:") + Id, FString::Printf(TEXT("接受%s修%s报价"), *NameOf(Residents, Owner), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄")), TEXT("accept"), Owner, WorkerBuilding))); Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("reject:") + Id, FString::Printf(TEXT("拒绝%s修%s报价"), *NameOf(Residents, Owner), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄")), TEXT("reject"), Owner))); }
            if (ResidentId == Owner && Status == TEXT("accepted")) Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("deliver:") + Id, TEXT("把柴斧送到修理岗位"), TEXT("deliver"), Worker, WorkerBuilding)));
            if (ResidentId == Worker && Status == TEXT("delivered")) Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("work:") + Id, FString::Printf(TEXT("在岗位修理%s"), Part == TEXT("edge") ? TEXT("斧刃") : TEXT("斧柄")), TEXT("work"), Owner, WorkerBuilding, 60)));
            if (ResidentId == Owner && Status == TEXT("completed")) Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("collect:") + Id, TEXT("取回修好的柴斧并结算"), TEXT("collect"), Worker, WorkerBuilding)));
            if (ResidentId == Owner && (Status == TEXT("proposed") || Status == TEXT("accepted"))) Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("cancel:") + Id, TEXT("取消修理委托"), TEXT("cancel"), Worker)));
        }

        const TArray<TSharedPtr<FJsonValue>>* Items = nullptr; Life->TryGetArrayField(TEXT("items"), Items);
        double Coins = Resident->GetNumberField(TEXT("coins_col"));
        for (const TSharedPtr<FJsonValue>& Value : *Items)
        {
            const TSharedPtr<FJsonObject> Item = ObjectValue(Value); FString ItemId, Owner, Custodian; Item->TryGetStringField(TEXT("id"), ItemId); Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Owner != ResidentId || Custodian != ResidentId || ActiveContractForItem(Life, ItemId).IsValid()) continue;
            int64 Edge = 0, Handle = 0; IntegerField(Item, TEXT("edge"), Edge, 0, 100); IntegerField(Item, TEXT("handle"), Handle, 0, 100);
            const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr; Life->TryGetArrayField(TEXT("skills"), Skills);
            for (const FString Part : { FString(TEXT("edge")), FString(TEXT("handle")) })
            {
                const int64 Condition = Part == TEXT("edge") ? Edge : Handle; if (Condition >= 100) continue; const FString NeededSkill = SkillForPart(Part);
                for (const TSharedPtr<FJsonValue>& SkillValue : *Skills)
                {
                    const TSharedPtr<FJsonObject> Skill = ObjectValue(SkillValue); FString Worker, SkillId; Skill->TryGetStringField(TEXT("resident_id"), Worker); Skill->TryGetStringField(TEXT("skill_id"), SkillId); if (SkillId != NeededSkill || Worker == ResidentId) continue;
                    for (const int32 Price : { 2, 5, 8 }) if (Coins >= Price) { const FString OptionId = FString::Printf(TEXT("repair_%s:%s:%s:%d"), *Part, *ItemId, *Worker, Price); const FString Label = FString::Printf(TEXT("请%s修%s，报价%d Col"), *NameOf(Residents, Worker), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄"), Price); Result.Add(MakeShared<FJsonValueObject>(MakeOption(OptionId, Label, TEXT("propose"), Worker))); }
                }
            }
            if (Edge == 100 && Handle == 100)
            {
                const TSharedPtr<FJsonObject> Account = FindAccount(Life, ResidentId); int64 Wood = 0; if (Account.IsValid() && IntegerField(Account, TEXT("wood"), Wood, 0, 1000000) && Wood > 0) { FString OwnerBuilding; if (BuildingForOwner(World, ResidentId, OwnerBuilding)) Result.Add(MakeShared<FJsonValueObject>(MakeOption(TEXT("use_tool:") + ItemId, TEXT("用修好的柴斧把1木料变成柴火"), TEXT("use_tool"), FString(), OwnerBuilding, 6))); }
            }
        }
        return Result;
    }

    TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& World, const FString& ResidentId)
    {
        auto Context = MakeShared<FJsonObject>(); Context->SetStringField(TEXT("resident_id"), ResidentId); FString Error; TSharedPtr<FJsonObject> Life;
        if (!Validate(World, Error) || !GetLife(World, Life, Error, false)) { Context->SetBoolField(TEXT("life_present"), false); Context->SetNumberField(TEXT("inbox_seq"), 0); Context->SetArrayField(TEXT("options"), {}); Context->SetStringField(TEXT("error"), Error); return Context; }
        TMap<FString, TSharedPtr<FJsonObject>> Residents; if (!CollectResidents(World, Residents, Error) || !Residents.Contains(ResidentId)) { Context->SetBoolField(TEXT("life_present"), true); Context->SetNumberField(TEXT("inbox_seq"), 0); Context->SetArrayField(TEXT("options"), {}); Context->SetStringField(TEXT("error"), TEXT("unknown resident")); return Context; }
        const TSharedPtr<FJsonObject> Resident = Residents[ResidentId]; Context->SetBoolField(TEXT("life_present"), true); Context->SetNumberField(TEXT("life_version"), LifeVersion); Context->SetStringField(TEXT("name"), Resident->GetStringField(TEXT("name"))); Context->SetNumberField(TEXT("coins_col"), Resident->GetNumberField(TEXT("coins_col")));
        FJsonArray OwnSkills, OwnItems, OwnAccounts, OwnContracts, Neighbours, OwnRelations, Received;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr; Life->TryGetArrayField(TEXT("skills"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Skill = ObjectValue(Value); FString Holder; Skill->TryGetStringField(TEXT("resident_id"), Holder); if (Holder == ResidentId) OwnSkills.Add(Value); }
        Life->TryGetArrayField(TEXT("items"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Item = ObjectValue(Value); FString Owner, Custodian; Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Owner == ResidentId || Custodian == ResidentId) OwnItems.Add(Value); }
        Life->TryGetArrayField(TEXT("accounts"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Account = ObjectValue(Value); FString Owner; Account->TryGetStringField(TEXT("resident_id"), Owner); if (Owner == ResidentId) OwnAccounts.Add(Value); }
        Life->TryGetArrayField(TEXT("contracts"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Contract = ObjectValue(Value); FString Owner, Worker; Contract->TryGetStringField(TEXT("owner_id"), Owner); Contract->TryGetStringField(TEXT("worker_id"), Worker); if (Owner == ResidentId || Worker == ResidentId) OwnContracts.Add(Value); }
        Life->TryGetArrayField(TEXT("relations"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Relation = ObjectValue(Value); FString From; Relation->TryGetStringField(TEXT("from_id"), From); if (From == ResidentId) OwnRelations.Add(Value); }
        int64 InboxSeq = 0; Life->TryGetArrayField(TEXT("inboxes"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Inbox = ObjectValue(Value); FString Owner; Inbox->TryGetStringField(TEXT("resident_id"), Owner); if (Owner == ResidentId) { IntegerField(Inbox, TEXT("next_seq"), InboxSeq, 0, TNumericLimits<int64>::Max()); const TArray<TSharedPtr<FJsonValue>>* InboxEvents = nullptr; Inbox->TryGetArrayField(TEXT("events"), InboxEvents); for (const TSharedPtr<FJsonValue>& Event : *InboxEvents) Received.Add(Event); } }
        TArray<FString> InitialNeighbourIds; for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents) { FString Name; Pair.Value->TryGetStringField(TEXT("name"), Name); if (Name == TEXT("艾琳") || Name == TEXT("拓真") || Name == TEXT("柏木")) InitialNeighbourIds.Add(Pair.Key); }
        for (const FString& NeighbourId : InitialNeighbourIds) { const TSharedPtr<FJsonObject> Pair = Residents[NeighbourId]; auto Neighbour = MakeShared<FJsonObject>(); Neighbour->SetStringField(TEXT("resident_id"), NeighbourId); Neighbour->SetStringField(TEXT("name"), Pair->GetStringField(TEXT("name"))); FJsonArray PublicSkills; Life->TryGetArrayField(TEXT("skills"), Values); for (const TSharedPtr<FJsonValue>& SkillValue : *Values) { const TSharedPtr<FJsonObject> Skill = ObjectValue(SkillValue); FString Holder; Skill->TryGetStringField(TEXT("resident_id"), Holder); if (Holder == NeighbourId) { auto SkillCopy = MakeShared<FJsonObject>(); SkillCopy->SetStringField(TEXT("skill_id"), Skill->GetStringField(TEXT("skill_id"))); SkillCopy->SetStringField(TEXT("source"), Skill->GetStringField(TEXT("source"))); PublicSkills.Add(MakeShared<FJsonValueObject>(SkillCopy)); } } Neighbour->SetArrayField(TEXT("skills"), PublicSkills); Neighbours.Add(MakeShared<FJsonValueObject>(Neighbour)); }
        Context->SetNumberField(TEXT("inbox_seq"), static_cast<double>(InboxSeq)); Context->SetArrayField(TEXT("skills"), OwnSkills); Context->SetArrayField(TEXT("items"), OwnItems); Context->SetArrayField(TEXT("accounts"), OwnAccounts); Context->SetArrayField(TEXT("contracts"), OwnContracts); Context->SetArrayField(TEXT("received_letters"), Received); Context->SetArrayField(TEXT("known_events"), Received); Context->SetArrayField(TEXT("initial_neighbours"), Neighbours); Context->SetArrayField(TEXT("relations"), OwnRelations); Context->SetArrayField(TEXT("options"), Options(World, ResidentId)); return Context;
    }

    bool Apply(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& OptionId, const FString& OperationId, const FString& Utterance, FString& Error)
    {
        Error.Empty(); if (ResidentId.IsEmpty() || OptionId.IsEmpty() || OperationId.IsEmpty() || OperationId.Len() > 128 || OptionId.Len() > 256 || Utterance.Len() > MaxTextLength) { Error = TEXT("life operation payload is invalid"); return false; }
        if (!Validate(World, Error)) return false;
        TSharedPtr<FJsonObject> Life; if (!GetLife(World, Life, Error, false)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Applied = nullptr; Life->TryGetArrayField(TEXT("applied"), Applied); for (const TSharedPtr<FJsonValue>& Value : *Applied) { const TSharedPtr<FJsonObject> Record = ObjectValue(Value); FString ExistingOperation, ExistingActor, ExistingOption, ExistingUtterance; Record->TryGetStringField(TEXT("operation_id"), ExistingOperation); if (ExistingOperation == OperationId) { Record->TryGetStringField(TEXT("actor_id"), ExistingActor); Record->TryGetStringField(TEXT("option_id"), ExistingOption); Record->TryGetStringField(TEXT("utterance"), ExistingUtterance); if (ExistingActor == ResidentId && ExistingOption == OptionId && ExistingUtterance == Utterance) return true; Error = TEXT("operation_id was already used with a different actor or payload"); return false; } }
        TSharedPtr<FJsonObject> Candidate; if (!CloneWorld(World, Candidate, Error)) return false; TSharedPtr<FJsonObject> CandidateLife; if (!GetLife(Candidate.ToSharedRef(), CandidateLife, Error, false)) return false;
        TMap<FString, TSharedPtr<FJsonObject>> Residents; if (!CollectResidents(Candidate.ToSharedRef(), Residents, Error) || !Residents.Contains(ResidentId)) { Error = TEXT("life operation actor is unknown"); return false; }
        TArray<FString> Parts; OptionId.ParseIntoArray(Parts, TEXT(":"), false); if (Parts.Num() < 2) { Error = TEXT("life option id is malformed"); return false; }
        const FString Verb = Parts[0]; int64 EventSeq = 0; FString EventText; TArray<FString> Recipients; FString SubjectId, ContractId, ItemId; bool bRelationImproved = false; FString RelationFrom, RelationTo; FString RelationReason;
        auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
        TSharedPtr<FJsonObject> ContractObject;
        if (Verb == TEXT("repair_edge") || Verb == TEXT("repair_handle"))
        {
            if (Parts.Num() != 4) return Fail(TEXT("repair proposal option is malformed")); FString ItemIdLocal = Parts[1], WorkerId = Parts[2], Part = Verb == TEXT("repair_edge") ? TEXT("edge") : TEXT("handle"); int64 Price = 0; if (!ParseInteger(Parts[3], Price) || (Price != 2 && Price != 5 && Price != 8)) return Fail(TEXT("repair price is invalid")); const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, ItemIdLocal); if (!Item.IsValid()) return Fail(TEXT("repair item is missing")); FString Owner, Custodian; Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Owner != ResidentId || Custodian != ResidentId || ActiveContractForItem(CandidateLife, ItemIdLocal).IsValid() || !HasSkill(CandidateLife, WorkerId, SkillForPart(Part)) || Item->GetNumberField(*Part) >= 100.0 || !Residents.Contains(WorkerId) || WorkerId == ResidentId) return Fail(TEXT("repair proposal is not currently legal")); ContractId = NewContractId(CandidateLife, CandidateLife->GetNumberField(TEXT("seq"))); auto NewContract = MakeShared<FJsonObject>(); NewContract->SetStringField(TEXT("id"), ContractId); NewContract->SetStringField(TEXT("part"), Part); NewContract->SetStringField(TEXT("item_id"), ItemIdLocal); NewContract->SetStringField(TEXT("owner_id"), ResidentId); NewContract->SetStringField(TEXT("worker_id"), WorkerId); NewContract->SetNumberField(TEXT("price_col"), static_cast<double>(Price)); NewContract->SetNumberField(TEXT("reserved_col"), 0); NewContract->SetStringField(TEXT("status"), TEXT("proposed")); const TArray<TSharedPtr<FJsonValue>>* ExistingContracts = nullptr; CandidateLife->TryGetArrayField(TEXT("contracts"), ExistingContracts); FJsonArray Contracts = *ExistingContracts; Contracts.Add(MakeShared<FJsonValueObject>(NewContract)); CandidateLife->SetArrayField(TEXT("contracts"), Contracts); ItemId = ItemIdLocal; SubjectId = WorkerId; Recipients = { ResidentId, WorkerId }; EventText = FString::Printf(TEXT("%s向%s提出修%s委托，报价%d Col。"), *NameOf(Residents, ResidentId), *NameOf(Residents, WorkerId), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄"), Price);
        }
        else if (Verb == TEXT("accept") || Verb == TEXT("reject") || Verb == TEXT("cancel"))
        {
            if (Parts.Num() != 2) return Fail(TEXT("contract response option is malformed")); ContractObject = FindContract(CandidateLife, Parts[1]); if (!ContractObject.IsValid()) return Fail(TEXT("contract is missing")); FString Owner, Worker, Status, Part, ItemIdLocal; ContractObject->TryGetStringField(TEXT("owner_id"), Owner); ContractObject->TryGetStringField(TEXT("worker_id"), Worker); ContractObject->TryGetStringField(TEXT("status"), Status); ContractObject->TryGetStringField(TEXT("part"), Part); ContractObject->TryGetStringField(TEXT("item_id"), ItemIdLocal); int64 Price = static_cast<int64>(ContractObject->GetNumberField(TEXT("price_col")));
            if (Verb == TEXT("accept")) { if (ResidentId != Worker || Status != TEXT("proposed")) return Fail(TEXT("only the worker can accept a pending proposal")); auto WorkerMaterials = FindAccount(CandidateLife, Worker); const FString NeededMaterial = Part == TEXT("edge") ? TEXT("iron") : TEXT("wood"); int64 AvailableMaterial = 0; if (!HasSkill(CandidateLife, Worker, SkillForPart(Part)) || !IntegerField(WorkerMaterials, *NeededMaterial, AvailableMaterial, 1, 1000000)) return Fail(TEXT("worker cannot accept without skill and material")); FString WorkerBuilding; if (!BuildingForOwner(Candidate.ToSharedRef(), Worker, WorkerBuilding) || !AtBuilding(Candidate.ToSharedRef(), Residents[Worker], WorkerBuilding)) return Fail(TEXT("accept requires the worker at the worker station")); const TSharedPtr<FJsonObject> OwnerResident = Residents[Owner]; if (OwnerResident->GetNumberField(TEXT("coins_col")) < Price) return Fail(TEXT("owner balance is insufficient to reserve the fee")); auto OwnerAccount = FindAccount(CandidateLife, Owner); if (!OwnerAccount.IsValid()) return Fail(TEXT("owner account is missing")); OwnerResident->SetNumberField(TEXT("coins_col"), OwnerResident->GetNumberField(TEXT("coins_col")) - Price); OwnerAccount->SetNumberField(TEXT("reserved_col"), OwnerAccount->GetNumberField(TEXT("reserved_col")) + Price); ContractObject->SetNumberField(TEXT("reserved_col"), Price); ContractObject->SetStringField(TEXT("status"), TEXT("accepted")); EventText = FString::Printf(TEXT("%s接受%s的修%s委托，已明确预留%d Col。"), *NameOf(Residents, Worker), *NameOf(Residents, Owner), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄"), Price); SubjectId = Owner; }
            else if (Verb == TEXT("reject")) { if (ResidentId != Worker || Status != TEXT("proposed")) return Fail(TEXT("only the worker can reject a pending proposal")); ContractObject->SetStringField(TEXT("status"), TEXT("rejected")); SubjectId = Owner; EventText = FString::Printf(TEXT("%s拒绝了%s的修%s委托。"), *NameOf(Residents, Worker), *NameOf(Residents, Owner), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄")); }
            else { if (ResidentId != Owner || !(Status == TEXT("proposed") || Status == TEXT("accepted"))) return Fail(TEXT("only the owner can cancel an un-delivered contract")); if (Status == TEXT("accepted")) { auto OwnerResident = Residents[Owner]; auto OwnerAccount = FindAccount(CandidateLife, Owner); OwnerResident->SetNumberField(TEXT("coins_col"), OwnerResident->GetNumberField(TEXT("coins_col")) + Price); OwnerAccount->SetNumberField(TEXT("reserved_col"), OwnerAccount->GetNumberField(TEXT("reserved_col")) - Price); } ContractObject->SetNumberField(TEXT("reserved_col"), 0); ContractObject->SetStringField(TEXT("status"), TEXT("cancelled")); SubjectId = Worker; EventText = FString::Printf(TEXT("%s取消了给%s的修%s委托。"), *NameOf(Residents, Owner), *NameOf(Residents, Worker), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄")); }
            const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, ItemIdLocal); ItemId = ItemIdLocal; Recipients = { ResidentId, SubjectId };
        }
        else if (Verb == TEXT("deliver") || Verb == TEXT("work") || Verb == TEXT("collect"))
        {
            if (Parts.Num() != 2) return Fail(TEXT("contract work option is malformed")); ContractObject = FindContract(CandidateLife, Parts[1]); if (!ContractObject.IsValid()) return Fail(TEXT("contract is missing")); FString Owner, Worker, Status, Part, ItemIdLocal; ContractObject->TryGetStringField(TEXT("owner_id"), Owner); ContractObject->TryGetStringField(TEXT("worker_id"), Worker); ContractObject->TryGetStringField(TEXT("status"), Status); ContractObject->TryGetStringField(TEXT("part"), Part); ContractObject->TryGetStringField(TEXT("item_id"), ItemIdLocal); const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, ItemIdLocal); if (!Item.IsValid()) return Fail(TEXT("contract item is missing")); FString Custodian; Item->TryGetStringField(TEXT("custodian_id"), Custodian); FString WorkerBuilding; if (!BuildingForOwner(Candidate.ToSharedRef(), Worker, WorkerBuilding)) return Fail(TEXT("worker building is missing")); const TSharedPtr<FJsonObject> OwnerResident = Residents[Owner]; const TSharedPtr<FJsonObject> WorkerResident = Residents[Worker];
            if (Verb == TEXT("deliver")) { if (ResidentId != Owner || Status != TEXT("accepted") || Custodian != Owner || !AtBuilding(Candidate.ToSharedRef(), OwnerResident, WorkerBuilding) || !AtBuilding(Candidate.ToSharedRef(), WorkerResident, WorkerBuilding) || !ResidentsClose(OwnerResident, WorkerResident)) return Fail(TEXT("delivery requires both residents at the worker station")); Item->SetStringField(TEXT("custodian_id"), Worker); ContractObject->SetStringField(TEXT("status"), TEXT("delivered")); SubjectId = Worker; EventText = FString::Printf(TEXT("%s已把%s交到%s的岗位，等待实际修理。"), *NameOf(Residents, Owner), *NameOf(Residents, ItemIdLocal), *NameOf(Residents, Worker)); }
            else if (Verb == TEXT("work")) { if (ResidentId != Worker || Status != TEXT("delivered") || Custodian != Worker || !AtBuilding(Candidate.ToSharedRef(), WorkerResident, WorkerBuilding) || !HasSkill(CandidateLife, Worker, SkillForPart(Part))) return Fail(TEXT("worker must perform the repair at the worker station")); auto Account = FindAccount(CandidateLife, Worker); if (!Account.IsValid()) return Fail(TEXT("worker account is missing")); const FString Material = Part == TEXT("edge") ? TEXT("iron") : TEXT("wood"); int64 Quantity = 0; if (!IntegerField(Account, *Material, Quantity, 0, 1000000) || Quantity <= 0) return Fail(TEXT("worker lacks the required repair material")); Account->SetNumberField(*Material, Quantity - 1); Item->SetNumberField(*Part, 100); ContractObject->SetStringField(TEXT("status"), TEXT("completed")); SubjectId = Owner; EventText = FString::Printf(TEXT("%s完成了%s的修%s，修理完成使双方对这次合作的信任改善。"), *NameOf(Residents, Worker), *NameOf(Residents, ItemIdLocal), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄")); bRelationImproved = true; RelationFrom = Owner; RelationTo = Worker; RelationReason = TEXT("修理完成，合作履约使信任改善"); }
            else { if (ResidentId != Owner || Status != TEXT("completed") || Custodian != Worker || !AtBuilding(Candidate.ToSharedRef(), OwnerResident, WorkerBuilding) || !AtBuilding(Candidate.ToSharedRef(), WorkerResident, WorkerBuilding) || !ResidentsClose(OwnerResident, WorkerResident)) return Fail(TEXT("collection requires both residents at the worker station")); auto OwnerAccount = FindAccount(CandidateLife, Owner); auto WorkerResidentMutable = Residents[Worker]; if (!OwnerAccount.IsValid()) return Fail(TEXT("owner account is missing")); const int64 Price = static_cast<int64>(ContractObject->GetNumberField(TEXT("price_col"))); OwnerAccount->SetNumberField(TEXT("reserved_col"), OwnerAccount->GetNumberField(TEXT("reserved_col")) - Price); WorkerResidentMutable->SetNumberField(TEXT("coins_col"), WorkerResidentMutable->GetNumberField(TEXT("coins_col")) + Price); Item->SetStringField(TEXT("custodian_id"), Owner); ContractObject->SetNumberField(TEXT("reserved_col"), 0); ContractObject->SetStringField(TEXT("status"), TEXT("collected")); SubjectId = Worker; EventText = FString::Printf(TEXT("%s取回%s，向%s结算%d Col。"), *NameOf(Residents, Owner), *NameOf(Residents, ItemIdLocal), *NameOf(Residents, Worker), Price); }
            ItemId = ItemIdLocal; Recipients = { Owner, Worker };
        }
        else if (Verb == TEXT("use_tool"))
        {
            if (Parts.Num() != 2) return Fail(TEXT("use_tool option is malformed")); const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, Parts[1]); if (!Item.IsValid()) return Fail(TEXT("tool is missing")); FString Owner, Custodian; Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); auto Account = FindAccount(CandidateLife, ResidentId); FString OwnerBuilding; int64 WoodQuantity = 0; if (Owner != ResidentId || Custodian != ResidentId || Item->GetNumberField(TEXT("edge")) != 100.0 || Item->GetNumberField(TEXT("handle")) != 100.0 || !IntegerField(Account, TEXT("wood"), WoodQuantity, 0, 1000000) || WoodQuantity <= 0 || !BuildingForOwner(Candidate.ToSharedRef(), ResidentId, OwnerBuilding) || !AtBuilding(Candidate.ToSharedRef(), Residents[ResidentId], OwnerBuilding)) return Fail(TEXT("tool use requires a fully repaired axe, one wood and the owner's station")); Account->SetNumberField(TEXT("wood"), WoodQuantity - 1); Account->SetNumberField(TEXT("kindling"), Account->GetNumberField(TEXT("kindling")) + 1); ItemId = Parts[1]; SubjectId = ResidentId; Recipients = { ResidentId }; EventText = FString::Printf(TEXT("%s在自己的岗位用修好的柴斧把1木料变成了柴火。"), *NameOf(Residents, ResidentId));
        }
        else return Fail(TEXT("unknown life option verb"));

        if (Recipients.Num() == 0) return Fail(TEXT("life event has no recipients"));
        const FString FullText = Utterance.IsEmpty() ? EventText : EventText + TEXT(" 原话：") + Utterance;
        FString EventContractId = ContractId;
        if (EventContractId.IsEmpty() && (Verb == TEXT("accept") || Verb == TEXT("reject") || Verb == TEXT("cancel") || Verb == TEXT("deliver") || Verb == TEXT("work") || Verb == TEXT("collect"))) EventContractId = Parts[1];
        if (!AddEvent(CandidateLife, Verb, ResidentId, SubjectId, EventContractId, ItemId, Recipients, FullText, OperationId, EventSeq, Error)) return false;
        if (bRelationImproved && !UpdateRelation(CandidateLife, RelationFrom, RelationTo, EventSeq, RelationReason, Error)) return false;
        if (!AddApplied(CandidateLife, OperationId, ResidentId, OptionId, Utterance, EventSeq, Error)) return false;
        if (!Validate(Candidate.ToSharedRef(), Error)) return false;
        World->Values = Candidate->Values;
        return true;
    }
}
