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

        // Keep the personal summary aligned with the world contract state machine:
        // completed work is still open until the owner collects it.
        bool IsPersonalActiveContractStatus(const FString& Status)
        {
            return IsActiveStatus(Status);
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

        TSharedPtr<FJsonObject> LatestOfferTerminalContract(const TSharedPtr<FJsonObject>& Life,
            const FString& WorkerId, const FString& ItemId, const FString& Part)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("contracts"), Values)) return nullptr;
            TSharedPtr<FJsonObject> Latest;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Contract = ObjectValue(Value);
                FString ExistingWorker, ExistingItem, ExistingPart, Status;
                if (Contract.IsValid() && Contract->TryGetStringField(TEXT("worker_id"), ExistingWorker)
                    && Contract->TryGetStringField(TEXT("item_id"), ExistingItem)
                    && Contract->TryGetStringField(TEXT("part"), ExistingPart)
                    && Contract->TryGetStringField(TEXT("status"), Status)
                    && ExistingWorker == WorkerId && ExistingItem == ItemId && ExistingPart == Part
                    && (Status == TEXT("rejected") || Status == TEXT("cancelled"))) Latest = Contract;
            }
            return Latest;
        }

        bool HasRepairOffer(const TSharedPtr<FJsonObject>& Life, const FString& WorkerId,
            const FString& ContractId, const FString& Part)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, Actor, ExistingContract, ExistingPart;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("actor_id"), Actor)
                    && Event->TryGetStringField(TEXT("contract_id"), ExistingContract)
                    && Event->TryGetStringField(TEXT("part"), ExistingPart)
                    && Type == TEXT("offer_repair") && Actor == WorkerId
                    && ExistingContract == ContractId && ExistingPart == Part) return true;
            }
            return false;
        }

        bool IsReplyChoice(const FString& Choice)
        {
            return Choice == TEXT("willing") || Choice == TEXT("unavailable") || Choice == TEXT("unsure");
        }

        bool IsRepairNeedChoice(const FString& Choice)
        {
            return Choice == TEXT("has_need") || Choice == TEXT("no_need") || Choice == TEXT("unsure");
        }

        bool IsInitialResident(const TMap<FString, TSharedPtr<FJsonObject>>& Residents, const FString& ResidentId)
        {
            if (ResidentId.IsEmpty()) return false;
            return ResidentByName(Residents, TEXT("艾琳")).IsValid() && ResidentByName(Residents, TEXT("艾琳"))->GetStringField(TEXT("stable_id")) == ResidentId
                || ResidentByName(Residents, TEXT("拓真")).IsValid() && ResidentByName(Residents, TEXT("拓真"))->GetStringField(TEXT("stable_id")) == ResidentId
                || ResidentByName(Residents, TEXT("柏木")).IsValid() && ResidentByName(Residents, TEXT("柏木"))->GetStringField(TEXT("stable_id")) == ResidentId;
        }

        bool IsInitialNeighbour(const TMap<FString, TSharedPtr<FJsonObject>>& Residents,
            const FString& ActorId, const FString& TargetId)
        {
            return ActorId != TargetId && IsInitialResident(Residents, ActorId) && IsInitialResident(Residents, TargetId);
        }

        bool HasHelpRequest(const TSharedPtr<FJsonObject>& Life, const FString& ActorId, const FString& TargetId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, Actor, Subject;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("actor_id"), Actor)
                    && Event->TryGetStringField(TEXT("subject_id"), Subject)
                    && Type == TEXT("ask_help") && Actor == ActorId && Subject == TargetId) return true;
            }
            return false;
        }

        TSharedPtr<FJsonObject> FindHelpRequest(const TSharedPtr<FJsonObject>& Life, const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("ask_help") && ExistingRequest == RequestId) return Event;
            }
            return nullptr;
        }

        bool HasHelpReply(const TSharedPtr<FJsonObject>& Life, const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("reply_help") && ExistingRequest == RequestId) return true;
            }
            return false;
        }

        bool HasRepairNeedQuestion(const TSharedPtr<FJsonObject>& Life, const FString& ActorId,
            const FString& TargetId, const FString& RepairSkill)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, Actor, Subject, ExistingSkill;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("actor_id"), Actor)
                    && Event->TryGetStringField(TEXT("subject_id"), Subject)
                    && Event->TryGetStringField(TEXT("repair_skill"), ExistingSkill)
                    && Type == TEXT("ask_repair_need") && Actor == ActorId && Subject == TargetId
                    && ExistingSkill == RepairSkill) return true;
            }
            return false;
        }

        TSharedPtr<FJsonObject> FindRepairNeedQuestion(const TSharedPtr<FJsonObject>& Life,
            const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("ask_repair_need") && ExistingRequest == RequestId) return Event;
            }
            return nullptr;
        }

        bool HasRepairNeedReply(const TSharedPtr<FJsonObject>& Life, const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("reply_repair_need") && ExistingRequest == RequestId) return true;
            }
            return false;
        }

        TSharedPtr<FJsonObject> FindRepairNeedReply(const TSharedPtr<FJsonObject>& Life,
            const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return nullptr;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("reply_repair_need") && ExistingRequest == RequestId) return Event;
            }
            return nullptr;
        }

        bool HasRepairNeedQuote(const TSharedPtr<FJsonObject>& Life, const FString& RequestId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("events"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, ExistingRequest;
                if (Event.IsValid() && Event->TryGetStringField(TEXT("type"), Type)
                    && Event->TryGetStringField(TEXT("request_id"), ExistingRequest)
                    && Type == TEXT("quote_repair_need") && ExistingRequest == RequestId) return true;
            }
            return false;
        }

        bool HasRepairMaterial(const TSharedPtr<FJsonObject>& Life, const FString& ResidentId,
            const FString& RepairSkill)
        {
            const TSharedPtr<FJsonObject> Account = FindAccount(Life, ResidentId);
            const TCHAR* Material = RepairSkill == TEXT("metal_repair") ? TEXT("iron")
                : RepairSkill == TEXT("wood_repair") ? TEXT("wood") : nullptr;
            int64 Quantity = 0;
            return Material && Account.IsValid()
                && IntegerField(Account, Material, Quantity, 0, 1000000) && Quantity > 0;
        }

        bool HasOwnedRepairNeed(const TSharedPtr<FJsonObject>& Life, const FString& ResidentId,
            const FString& RepairSkill)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Life.IsValid() || !Life->TryGetArrayField(TEXT("items"), Values)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject> Item = ObjectValue(Value);
                FString Owner, Custodian;
                if (!Item.IsValid() || !Item->TryGetStringField(TEXT("owner_id"), Owner)
                    || !Item->TryGetStringField(TEXT("custodian_id"), Custodian)
                    || Owner != ResidentId || Custodian != ResidentId) continue;
                const TCHAR* Part = RepairSkill == TEXT("metal_repair") ? TEXT("edge")
                    : RepairSkill == TEXT("wood_repair") ? TEXT("handle") : nullptr;
                if (Part != nullptr && Item->GetNumberField(Part) < 100.0) return true;
            }
            return false;
        }

        bool EventHasRecipient(const TSharedPtr<FJsonObject>& Event, const FString& ResidentId)
        {
            const TArray<TSharedPtr<FJsonValue>>* Recipients = nullptr;
            if (!Event.IsValid() || !Event->TryGetArrayField(TEXT("recipient_ids"), Recipients)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Recipients)
            {
                FString Recipient;
                if (Value.IsValid() && Value->TryGetString(Recipient) && Recipient == ResidentId) return true;
            }
            return false;
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

        bool AddEvent(const TSharedPtr<FJsonObject>& Life, const FString& Type, const FString& ActorId, const FString& SubjectId, const FString& ContractId, const FString& ItemId, const TArray<FString>& Recipients, const FString& Text, const FString& OperationId, int64& OutSeq, FString& Error, const FString& OfferPart = FString(), int64 OfferPrice = 0, const FString& CommunicationRequestId = FString(), const FString& CommunicationChoice = FString(), const FString& CommunicationTopic = FString(), const FString& CommunicationSkill = FString())
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
            if (Type == TEXT("offer_repair")) { Event->SetStringField(TEXT("part"), OfferPart); Event->SetNumberField(TEXT("offer_price_col"), static_cast<double>(OfferPrice)); }
            if (Type == TEXT("quote_repair_need")) Event->SetNumberField(TEXT("offer_price_col"), static_cast<double>(OfferPrice));
            if (Type == TEXT("ask_help") || Type == TEXT("reply_help") || Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need") || Type == TEXT("quote_repair_need")) { Event->SetStringField(TEXT("request_id"), CommunicationRequestId); Event->SetStringField(TEXT("topic"), CommunicationTopic.IsEmpty() ? TEXT("help_availability") : CommunicationTopic); Event->SetBoolField(TEXT("contractual"), false); if (Type == TEXT("reply_help") || Type == TEXT("reply_repair_need")) Event->SetStringField(TEXT("reply_choice"), CommunicationChoice); if (Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need") || Type == TEXT("quote_repair_need")) Event->SetStringField(TEXT("repair_skill"), CommunicationSkill); }
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
                    auto InboxEvent = MakeShared<FJsonObject>(); InboxEvent->SetNumberField(TEXT("seq"), static_cast<double>(InboxSeq + 1)); InboxEvent->SetStringField(TEXT("event_id"), EventId); InboxEvent->SetStringField(TEXT("type"), Type); InboxEvent->SetStringField(TEXT("actor_id"), ActorId); InboxEvent->SetStringField(TEXT("subject_id"), SubjectId); InboxEvent->SetStringField(TEXT("contract_id"), ContractId); InboxEvent->SetStringField(TEXT("item_id"), ItemId); InboxEvent->SetStringField(TEXT("text"), Text); if (Type == TEXT("offer_repair")) { InboxEvent->SetStringField(TEXT("part"), OfferPart); InboxEvent->SetNumberField(TEXT("offer_price_col"), static_cast<double>(OfferPrice)); } if (Type == TEXT("quote_repair_need")) InboxEvent->SetNumberField(TEXT("offer_price_col"), static_cast<double>(OfferPrice)); if (Type == TEXT("ask_help") || Type == TEXT("reply_help") || Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need") || Type == TEXT("quote_repair_need")) { InboxEvent->SetStringField(TEXT("request_id"), CommunicationRequestId); InboxEvent->SetStringField(TEXT("topic"), CommunicationTopic.IsEmpty() ? TEXT("help_availability") : CommunicationTopic); InboxEvent->SetBoolField(TEXT("contractual"), false); if (Type == TEXT("reply_help") || Type == TEXT("reply_repair_need")) InboxEvent->SetStringField(TEXT("reply_choice"), CommunicationChoice); if (Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need") || Type == TEXT("quote_repair_need")) InboxEvent->SetStringField(TEXT("repair_skill"), CommunicationSkill); }
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
            TSet<FString> EventIds, QuotedRepairNeedRequests; int64 PreviousSeq = 0;
            for (const TSharedPtr<FJsonValue>& Value : *Events)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value); FString EventId, Type, Actor, Subject, Contract, Item, Text, Operation; int64 EventSeq = 0;
                if (!RequiredString(Event, TEXT("event_id"), EventId, 128) || EventIds.Contains(EventId) || !IntegerField(Event, TEXT("seq"), EventSeq, 1, Sequence) || EventSeq <= PreviousSeq || !RequiredString(Event, TEXT("type"), Type, 64) || !RequiredString(Event, TEXT("actor_id"), Actor, 64) || !Residents.Contains(Actor) || !OptionalString(Event, TEXT("subject_id"), Subject, 64) || (!Subject.IsEmpty() && !Residents.Contains(Subject)) || !OptionalString(Event, TEXT("contract_id"), Contract, 128) || (!Contract.IsEmpty() && !ContractIds.Contains(Contract)) || !OptionalString(Event, TEXT("item_id"), Item, 128) || (!Item.IsEmpty() && !ItemIds.Contains(Item)) || !RequiredString(Event, TEXT("text"), Text, MaxTextLength) || !RequiredString(Event, TEXT("operation_id"), Operation, 128)) { Error = TEXT("life event is invalid"); return false; }
                if (Type == TEXT("offer_repair"))
                {
                    FString OfferPart; int64 OfferPrice = 0;
                    if (Contract.IsEmpty() || Item.IsEmpty() || !RequiredString(Event, TEXT("part"), OfferPart, 32)
                        || (OfferPart != TEXT("edge") && OfferPart != TEXT("handle"))
                        || !IntegerField(Event, TEXT("offer_price_col"), OfferPrice, 2, 8)
                        || (OfferPrice != 2 && OfferPrice != 5 && OfferPrice != 8))
                    { Error = TEXT("life repair offer event is invalid"); return false; }
                }
                if (Type == TEXT("ask_help") || Type == TEXT("reply_help"))
                {
                    FString RequestId, Topic, ReplyChoice; bool bContractual = true;
                    if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty()
                        || !RequiredString(Event, TEXT("request_id"), RequestId, 256)
                        || !RequiredString(Event, TEXT("topic"), Topic, 64) || Topic != TEXT("help_availability")
                        || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual
                        || (Type == TEXT("reply_help") && (!RequiredString(Event, TEXT("reply_choice"), ReplyChoice, 32) || !IsReplyChoice(ReplyChoice))))
                    { Error = TEXT("life communication event is invalid"); return false; }
                }
                if (Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need"))
                {
                    FString RequestId, Topic, RepairSkill, ReplyChoice; bool bContractual = true;
                    if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty()
                        || !RequiredString(Event, TEXT("request_id"), RequestId, 256)
                        || !RequiredString(Event, TEXT("topic"), Topic, 64) || Topic != TEXT("repair_work_availability")
                        || !RequiredString(Event, TEXT("repair_skill"), RepairSkill, 64) || !IsRepairSkill(RepairSkill)
                        || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual
                        || (Type == TEXT("reply_repair_need") && (!RequiredString(Event, TEXT("reply_choice"), ReplyChoice, 32) || !IsRepairNeedChoice(ReplyChoice))))
                    { Error = TEXT("life repair need event is invalid"); return false; }
                }
                if (Type == TEXT("quote_repair_need"))
                {
                    FString RequestId, Topic, RepairSkill; bool bContractual = true; int64 Price = 0;
                    Event->TryGetStringField(TEXT("request_id"), RequestId);
                    const TSharedPtr<FJsonObject> Question = FindRepairNeedQuestion(Life, RequestId);
                    const TSharedPtr<FJsonObject> Reply = FindRepairNeedReply(Life, RequestId);
                    FString QuestionActor, QuestionSubject, QuestionSkill, ReplyActor, ReplySubject, ReplySkill, ReplyChoice;
                    if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty()
                        || !RequiredString(Event, TEXT("request_id"), RequestId, 256) || QuotedRepairNeedRequests.Contains(RequestId)
                        || !RequiredString(Event, TEXT("topic"), Topic, 64) || Topic != TEXT("repair_work_availability")
                        || !RequiredString(Event, TEXT("repair_skill"), RepairSkill, 64) || !IsRepairSkill(RepairSkill)
                        || !IntegerField(Event, TEXT("offer_price_col"), Price, 2, 8) || (Price != 2 && Price != 5 && Price != 8)
                        || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual
                        || !Question.IsValid() || !Reply.IsValid()
                        || !Question->TryGetStringField(TEXT("actor_id"), QuestionActor) || QuestionActor != Actor
                        || !Question->TryGetStringField(TEXT("subject_id"), QuestionSubject) || QuestionSubject != Subject
                        || !Question->TryGetStringField(TEXT("repair_skill"), QuestionSkill) || QuestionSkill != RepairSkill
                        || !Reply->TryGetStringField(TEXT("actor_id"), ReplyActor) || ReplyActor != Subject
                        || !Reply->TryGetStringField(TEXT("subject_id"), ReplySubject) || ReplySubject != Actor
                        || !Reply->TryGetStringField(TEXT("repair_skill"), ReplySkill) || ReplySkill != RepairSkill
                        || !Reply->TryGetStringField(TEXT("reply_choice"), ReplyChoice) || ReplyChoice != TEXT("has_need"))
                    { Error = TEXT("life repair need quote event is invalid"); return false; }
                    QuotedRepairNeedRequests.Add(RequestId);
                }
                const TArray<TSharedPtr<FJsonValue>>* Recipients = nullptr; if (!Event->TryGetArrayField(TEXT("recipient_ids"), Recipients) || Recipients->Num() == 0) { Error = TEXT("life event recipients are invalid"); return false; }
                TSet<FString> RecipientSet;
                for (const TSharedPtr<FJsonValue>& RecipientValue : *Recipients) { FString Recipient; if (!RecipientValue.IsValid() || !RecipientValue->TryGetString(Recipient) || !Residents.Contains(Recipient) || RecipientSet.Contains(Recipient)) { Error = TEXT("life event recipient is invalid"); return false; } RecipientSet.Add(Recipient); }
                if (Type == TEXT("quote_repair_need") && (RecipientSet.Num() != 2 || !RecipientSet.Contains(Actor) || !RecipientSet.Contains(Subject))) { Error = TEXT("life repair need quote recipients are invalid"); return false; }
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
                for (const TSharedPtr<FJsonValue>& InboxValue : *InboxEvents) { const TSharedPtr<FJsonObject> InboxEvent = ObjectValue(InboxValue); FString EventId, Type, Actor, Subject, Contract, Item, Text; int64 InboxSeq = 0; if (!RequiredString(InboxEvent, TEXT("event_id"), EventId, 128) || !EventIds.Contains(EventId) || !IntegerField(InboxEvent, TEXT("seq"), InboxSeq, 1, NextSeq) || InboxSeq <= PreviousInboxSeq || !RequiredString(InboxEvent, TEXT("type"), Type, 64) || !RequiredString(InboxEvent, TEXT("actor_id"), Actor, 64) || !Residents.Contains(Actor) || !OptionalString(InboxEvent, TEXT("subject_id"), Subject, 64) || (!Subject.IsEmpty() && !Residents.Contains(Subject)) || !OptionalString(InboxEvent, TEXT("contract_id"), Contract, 128) || (!Contract.IsEmpty() && !ContractIds.Contains(Contract)) || !OptionalString(InboxEvent, TEXT("item_id"), Item, 128) || (!Item.IsEmpty() && !ItemIds.Contains(Item)) || !RequiredString(InboxEvent, TEXT("text"), Text, MaxTextLength)) { Error = TEXT("life inbox event is invalid"); return false; } if (Type == TEXT("offer_repair")) { FString OfferPart; int64 OfferPrice = 0; if (Contract.IsEmpty() || Item.IsEmpty() || !RequiredString(InboxEvent, TEXT("part"), OfferPart, 32) || (OfferPart != TEXT("edge") && OfferPart != TEXT("handle")) || !IntegerField(InboxEvent, TEXT("offer_price_col"), OfferPrice, 2, 8) || (OfferPrice != 2 && OfferPrice != 5 && OfferPrice != 8)) { Error = TEXT("life repair offer inbox event is invalid"); return false; } } if (Type == TEXT("quote_repair_need")) { FString RequestId, Topic, RepairSkill; bool bContractual = true; int64 Price = 0; if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty() || !RequiredString(InboxEvent, TEXT("request_id"), RequestId, 256) || !RequiredString(InboxEvent, TEXT("topic"), Topic, 64) || Topic != TEXT("repair_work_availability") || !RequiredString(InboxEvent, TEXT("repair_skill"), RepairSkill, 64) || !IsRepairSkill(RepairSkill) || !IntegerField(InboxEvent, TEXT("offer_price_col"), Price, 2, 8) || (Price != 2 && Price != 5 && Price != 8) || !InboxEvent->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual) { Error = TEXT("life repair need quote inbox event is invalid"); return false; } } if (Type == TEXT("ask_help") || Type == TEXT("reply_help")) { FString RequestId, Topic, ReplyChoice; bool bContractual = true; if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty() || !RequiredString(InboxEvent, TEXT("request_id"), RequestId, 256) || !RequiredString(InboxEvent, TEXT("topic"), Topic, 64) || Topic != TEXT("help_availability") || !InboxEvent->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual || (Type == TEXT("reply_help") && (!RequiredString(InboxEvent, TEXT("reply_choice"), ReplyChoice, 32) || !IsReplyChoice(ReplyChoice)))) { Error = TEXT("life communication inbox event is invalid"); return false; } } if (Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need")) { FString RequestId, Topic, RepairSkill, ReplyChoice; bool bContractual = true; if (Subject.IsEmpty() || !Contract.IsEmpty() || !Item.IsEmpty() || !RequiredString(InboxEvent, TEXT("request_id"), RequestId, 256) || !RequiredString(InboxEvent, TEXT("topic"), Topic, 64) || Topic != TEXT("repair_work_availability") || !RequiredString(InboxEvent, TEXT("repair_skill"), RepairSkill, 64) || !IsRepairSkill(RepairSkill) || !InboxEvent->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual || (Type == TEXT("reply_repair_need") && (!RequiredString(InboxEvent, TEXT("reply_choice"), ReplyChoice, 32) || !IsRepairNeedChoice(ReplyChoice)))) { Error = TEXT("life repair need inbox event is invalid"); return false; } } PreviousInboxSeq = InboxSeq; }
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

    bool HasDecisionEvent(const TSharedRef<FJsonObject>& World, const FString& ResidentId, double LastDispatchedSeq)
    {
        if (ResidentId.IsEmpty() || !FMath::IsFinite(LastDispatchedSeq)
            || LastDispatchedSeq < 0.0 || LastDispatchedSeq != FMath::FloorToDouble(LastDispatchedSeq)) return false;

        FString Error;
        TSharedPtr<FJsonObject> Life;
        if (!GetLife(World, Life, Error, false) || !Life.IsValid()) return false;

        const auto StringOrEmpty = [](const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
        {
            FString Value;
            if (Object.IsValid()) Object->TryGetStringField(Key, Value);
            return Value;
        };

        TMap<FString, TSharedPtr<FJsonObject>> Residents;
        if (!CollectResidents(World, Residents, Error) || !Residents.Contains(ResidentId)) return false;

        const TArray<TSharedPtr<FJsonValue>>* Inboxes = nullptr;
        if (!Life->TryGetArrayField(TEXT("inboxes"), Inboxes) || !Inboxes) return false;
        TSharedPtr<FJsonObject> Inbox;
        for (const TSharedPtr<FJsonValue>& Value : *Inboxes)
        {
            const TSharedPtr<FJsonObject> Candidate = ObjectValue(Value);
            if (StringOrEmpty(Candidate, TEXT("resident_id")) == ResidentId)
            {
                Inbox = Candidate;
                break;
            }
        }
        if (!Inbox.IsValid()) return false;

        double NextSeq = 0.0;
        const TArray<TSharedPtr<FJsonValue>>* InboxEvents = nullptr;
        if (!Inbox->TryGetNumberField(TEXT("next_seq"), NextSeq) || !FMath::IsFinite(NextSeq)
            || NextSeq < 0.0 || NextSeq != FMath::FloorToDouble(NextSeq)
            || !Inbox->TryGetArrayField(TEXT("events"), InboxEvents) || !InboxEvents) return false;

        const auto IsKnownType = [](const FString& Value)
        {
            return Value == TEXT("initial_condition") || Value == TEXT("repair_edge") || Value == TEXT("repair_handle")
                || Value == TEXT("accept") || Value == TEXT("reject") || Value == TEXT("cancel")
                || Value == TEXT("deliver") || Value == TEXT("work") || Value == TEXT("collect")
                || Value == TEXT("use_tool") || Value == TEXT("offer_repair")
                || Value == TEXT("ask_help") || Value == TEXT("reply_help")
                || Value == TEXT("ask_repair_need") || Value == TEXT("reply_repair_need")
                || Value == TEXT("quote_repair_need");
        };

        for (const TSharedPtr<FJsonValue>& Value : *InboxEvents)
        {
            const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
            int64 EventSeq = 0;
            FString ActorId, Type;
            if (!Event.IsValid() || !IntegerField(Event, TEXT("seq"), EventSeq, 1, TNumericLimits<int64>::Max())) return false;
            if (static_cast<double>(EventSeq) > NextSeq) return false;
            if (static_cast<double>(EventSeq) <= LastDispatchedSeq) continue;
            if (!Event->TryGetStringField(TEXT("actor_id"), ActorId)
                || !Event->TryGetStringField(TEXT("type"), Type)
                || ActorId.IsEmpty() || Type.IsEmpty()
                || !Residents.Contains(ActorId) || !IsKnownType(Type)) return false;

            if (Type == TEXT("offer_repair"))
            {
                FString OfferPart; double OfferPrice = 0.0;
                if (!Event->TryGetStringField(TEXT("part"), OfferPart)
                    || (OfferPart != TEXT("edge") && OfferPart != TEXT("handle"))
                    || !Event->TryGetNumberField(TEXT("offer_price_col"), OfferPrice)
                    || !FMath::IsFinite(OfferPrice) || FMath::FloorToDouble(OfferPrice) != OfferPrice
                    || (OfferPrice != 2.0 && OfferPrice != 5.0 && OfferPrice != 8.0)) return false;
            }
            if (Type == TEXT("quote_repair_need"))
            {
                FString RequestId, Topic, RepairSkill; double Price = 0.0; bool bContractual = true;
                if (!Event->TryGetStringField(TEXT("request_id"), RequestId) || RequestId.IsEmpty()
                    || !Event->TryGetStringField(TEXT("topic"), Topic) || Topic != TEXT("repair_work_availability")
                    || !Event->TryGetStringField(TEXT("repair_skill"), RepairSkill) || !IsRepairSkill(RepairSkill)
                    || !Event->TryGetNumberField(TEXT("offer_price_col"), Price) || !FMath::IsFinite(Price)
                    || (Price != 2.0 && Price != 5.0 && Price != 8.0)
                    || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual) return false;
            }
            if (Type == TEXT("ask_help") || Type == TEXT("reply_help"))
            {
                FString RequestId, Topic, ReplyChoice; bool bContractual = true;
                if (!Event->TryGetStringField(TEXT("request_id"), RequestId) || RequestId.IsEmpty()
                    || !Event->TryGetStringField(TEXT("topic"), Topic) || Topic != TEXT("help_availability")
                    || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual
                    || (Type == TEXT("reply_help") && (!Event->TryGetStringField(TEXT("reply_choice"), ReplyChoice) || !IsReplyChoice(ReplyChoice)))) return false;
            }
            if (Type == TEXT("ask_repair_need") || Type == TEXT("reply_repair_need"))
            {
                FString RequestId, Topic, RepairSkill, ReplyChoice; bool bContractual = true;
                if (!Event->TryGetStringField(TEXT("request_id"), RequestId) || RequestId.IsEmpty()
                    || !Event->TryGetStringField(TEXT("topic"), Topic) || Topic != TEXT("repair_work_availability")
                    || !Event->TryGetStringField(TEXT("repair_skill"), RepairSkill) || !IsRepairSkill(RepairSkill)
                    || !Event->TryGetBoolField(TEXT("contractual"), bContractual) || bContractual
                    || (Type == TEXT("reply_repair_need") && (!Event->TryGetStringField(TEXT("reply_choice"), ReplyChoice) || !IsRepairNeedChoice(ReplyChoice)))) return false;
            }

            // Initial conditions are developer-authored state, not self-talk. Allow
            // them once when an old runtime has no dispatched inbox cursor.
            if (Type == TEXT("initial_condition"))
            {
                if (ActorId != ResidentId || LastDispatchedSeq != 0.0) return false;
                const FString EventId = StringOrEmpty(Event, TEXT("event_id"));
                if (EventId.IsEmpty()) return false;
                const TArray<TSharedPtr<FJsonValue>>* WorldEvents = nullptr;
                if (!Life->TryGetArrayField(TEXT("events"), WorldEvents) || !WorldEvents) return false;
                for (const TSharedPtr<FJsonValue>& WorldValue : *WorldEvents)
                {
                    const TSharedPtr<FJsonObject> WorldEvent = ObjectValue(WorldValue);
                    if (StringOrEmpty(WorldEvent, TEXT("event_id")) == EventId
                        && StringOrEmpty(WorldEvent, TEXT("type")) == TEXT("initial_condition")
                        && StringOrEmpty(WorldEvent, TEXT("source")) == TEXT("developer_initial_condition")) return true;
                }
                return false;
            }

            // Any valid event authored by another resident is new information.
            if (ActorId != ResidentId) return true;

            // These self-authored results can expose a newly legal next action.
            if (Type == TEXT("work") || Type == TEXT("collect") || Type == TEXT("use_tool")) return true;
        }
        return false;
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

        // A worker may send one non-contractual repair suggestion for the most
        // recent cancelled/rejected contract of an item part. Iterate newest
        // first so older terminal contracts are never offered as a second list.
        TSet<FString> SeenOfferKeys;
        for (int32 ContractIndex = Contracts->Num() - 1; ContractIndex >= 0; --ContractIndex)
        {
            const TSharedPtr<FJsonObject> Contract = ObjectValue((*Contracts)[ContractIndex]);
            FString Id, Owner, Worker, Status, ItemId, Part;
            if (!Contract.IsValid() || !Contract->TryGetStringField(TEXT("id"), Id)
                || !Contract->TryGetStringField(TEXT("owner_id"), Owner)
                || !Contract->TryGetStringField(TEXT("worker_id"), Worker)
                || !Contract->TryGetStringField(TEXT("status"), Status)
                || !Contract->TryGetStringField(TEXT("item_id"), ItemId)
                || !Contract->TryGetStringField(TEXT("part"), Part)
                || ResidentId != Worker || (Status != TEXT("rejected") && Status != TEXT("cancelled"))) continue;
            const FString OfferKey = ItemId + TEXT("/") + Part;
            if (SeenOfferKeys.Contains(OfferKey)) continue;
            SeenOfferKeys.Add(OfferKey);
            const TSharedPtr<FJsonObject> Latest = LatestOfferTerminalContract(Life, ResidentId, ItemId, Part);
            if (!Latest.IsValid() || Latest->GetStringField(TEXT("id")) != Id
                || HasRepairOffer(Life, ResidentId, Id, Part)) continue;
            const TSharedPtr<FJsonObject> Item = FindItem(Life, ItemId);
            if (!Item.IsValid() || Item->GetStringField(TEXT("owner_id")) != Owner
                || Item->GetStringField(TEXT("custodian_id")) != Owner
                || Item->GetNumberField(*Part) >= 100.0 || ActiveContractForItem(Life, ItemId).IsValid()
                || !HasSkill(Life, ResidentId, SkillForPart(Part))) continue;
            const TSharedPtr<FJsonObject> Account = FindAccount(Life, ResidentId);
            const FString Material = Part == TEXT("edge") ? TEXT("iron") : TEXT("wood");
            int64 MaterialQuantity = 0;
            if (!Account.IsValid() || !IntegerField(Account, *Material, MaterialQuantity, 0, 1000000) || MaterialQuantity <= 0) continue;
            for (const int32 Price : { 2, 5, 8 })
            {
                const FString OptionId = FString::Printf(TEXT("offer_repair:%s:%d"), *Id, Price);
                const TSharedRef<FJsonObject> Option = MakeOption(OptionId,
                    FString::Printf(TEXT("向%s提供修%s建议报价%d Col（不是正式合同）"), *NameOf(Residents, Owner), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄"), Price),
                    TEXT("offer_repair"), Owner);
                Option->SetStringField(TEXT("part"), Part);
                Option->SetStringField(TEXT("terminal_contract_id"), Id);
                Option->SetNumberField(TEXT("offer_price_col"), static_cast<double>(Price));
                Result.Add(MakeShared<FJsonValueObject>(Option));
            }
        }

        for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents)
        {
            const FString TargetId = Pair.Key;
            if (!IsInitialNeighbour(Residents, ResidentId, TargetId) || HasHelpRequest(Life, ResidentId, TargetId)) continue;
            const TSharedRef<FJsonObject> Option = MakeOption(TEXT("ask_help:") + TargetId,
                FString::Printf(TEXT("询问%s近期是否愿意帮忙"), *NameOf(Residents, TargetId)), TEXT("ask_help"), TargetId);
            Option->SetStringField(TEXT("topic"), TEXT("help_availability"));
            Option->SetBoolField(TEXT("contractual"), false);
            Result.Add(MakeShared<FJsonValueObject>(Option));
        }

        const TArray<TSharedPtr<FJsonValue>>* LifeEvents = nullptr;
        if (Life->TryGetArrayField(TEXT("events"), LifeEvents))
        {
            for (const TSharedPtr<FJsonValue>& Value : *LifeEvents)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, RequestId, ActorId, TargetId;
                if (!Event.IsValid() || !Event->TryGetStringField(TEXT("type"), Type) || Type != TEXT("ask_help")
                    || !Event->TryGetStringField(TEXT("request_id"), RequestId)
                    || !Event->TryGetStringField(TEXT("actor_id"), ActorId)
                    || !Event->TryGetStringField(TEXT("subject_id"), TargetId)
                    || TargetId != ResidentId || !EventHasRecipient(Event, ResidentId) || !IsInitialNeighbour(Residents, ResidentId, ActorId)
                    || HasHelpReply(Life, RequestId)) continue;
                const TArray<TSharedPtr<FJsonValue>> Choices = {
                    MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_help:%s:willing"), *RequestId), TEXT("愿意了解具体问题"), TEXT("reply_help"), ActorId)),
                    MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_help:%s:unavailable"), *RequestId), TEXT("近期暂不方便"), TEXT("reply_help"), ActorId)),
                    MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_help:%s:unsure"), *RequestId), TEXT("目前尚不确定"), TEXT("reply_help"), ActorId))
                };
                for (const TSharedPtr<FJsonValue>& Choice : Choices)
                {
                    const TSharedPtr<FJsonObject> Option = Choice->AsObject();
                    Option->SetStringField(TEXT("request_id"), RequestId);
                    Option->SetStringField(TEXT("topic"), TEXT("help_availability"));
                    Option->SetBoolField(TEXT("contractual"), false);
                    Result.Add(Choice);
                }
            }
        }

        FString RepairSkill;
        if (HasSkill(Life, ResidentId, TEXT("metal_repair"))) RepairSkill = TEXT("metal_repair");
        else if (HasSkill(Life, ResidentId, TEXT("wood_repair"))) RepairSkill = TEXT("wood_repair");
        if (!RepairSkill.IsEmpty())
        {
            for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents)
            {
                if (!IsInitialNeighbour(Residents, ResidentId, Pair.Key) || HasRepairNeedQuestion(Life, ResidentId, Pair.Key, RepairSkill)) continue;
                const TSharedRef<FJsonObject> Option = MakeOption(TEXT("ask_repair_need:") + Pair.Key,
                    FString::Printf(TEXT("询问%s近期是否有需要%s修理的物品"), *NameOf(Residents, Pair.Key), RepairSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄")), TEXT("ask_repair_need"), Pair.Key);
                Option->SetStringField(TEXT("topic"), TEXT("repair_work_availability"));
                Option->SetStringField(TEXT("repair_skill"), RepairSkill);
                Option->SetBoolField(TEXT("contractual"), false);
                Result.Add(MakeShared<FJsonValueObject>(Option));
            }
        }

        if (Life->TryGetArrayField(TEXT("events"), LifeEvents))
        {
            for (const TSharedPtr<FJsonValue>& Value : *LifeEvents)
            {
                const TSharedPtr<FJsonObject> Event = ObjectValue(Value);
                FString Type, RequestId, ActorId, TargetId, EventSkill;
                if (!Event.IsValid() || !Event->TryGetStringField(TEXT("type"), Type) || Type != TEXT("ask_repair_need")
                    || !Event->TryGetStringField(TEXT("request_id"), RequestId)
                    || !Event->TryGetStringField(TEXT("actor_id"), ActorId)
                    || !Event->TryGetStringField(TEXT("subject_id"), TargetId)
                    || !Event->TryGetStringField(TEXT("repair_skill"), EventSkill)
                    || TargetId != ResidentId || !EventHasRecipient(Event, ResidentId)
                    || !IsInitialNeighbour(Residents, ResidentId, ActorId) || !HasSkill(Life, ActorId, EventSkill)
                    || HasRepairNeedReply(Life, RequestId)) continue;
                const bool bHasNeed = HasOwnedRepairNeed(Life, ResidentId, EventSkill);
                TArray<TSharedPtr<FJsonValue>> Choices;
                if (bHasNeed)
                {
                    Choices.Add(MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_repair_need:%s:has_need"), *RequestId), TEXT("我近期有物品需要这项修理，可以再谈"), TEXT("reply_repair_need"), ActorId)));
                }
                Choices.Add(MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_repair_need:%s:no_need"), *RequestId), TEXT("我近期没有这项修理需求"), TEXT("reply_repair_need"), ActorId)));
                Choices.Add(MakeShared<FJsonValueObject>(MakeOption(FString::Printf(TEXT("reply_repair_need:%s:unsure"), *RequestId), TEXT("我目前还不确定"), TEXT("reply_repair_need"), ActorId)));
                for (const TSharedPtr<FJsonValue>& Choice : Choices)
                {
                    const TSharedPtr<FJsonObject> Option = Choice->AsObject();
                    Option->SetStringField(TEXT("request_id"), RequestId);
                    Option->SetStringField(TEXT("topic"), TEXT("repair_work_availability"));
                    Option->SetStringField(TEXT("repair_skill"), EventSkill);
                    Option->SetBoolField(TEXT("contractual"), false);
                    Result.Add(Choice);
                }
            }
        }

        // A repairer who received a matching positive need reply may state one
        // non-contractual price. This neither reserves nor reveals the item.
        if (LifeEvents)
        {
            for (const TSharedPtr<FJsonValue>& Value : *LifeEvents)
            {
                const TSharedPtr<FJsonObject> Reply = ObjectValue(Value);
                FString Type, RequestId, OwnerId, WorkerId, ReplySkill, ReplyChoice;
                if (!Reply.IsValid() || !Reply->TryGetStringField(TEXT("type"), Type) || Type != TEXT("reply_repair_need")
                    || !Reply->TryGetStringField(TEXT("request_id"), RequestId)
                    || !Reply->TryGetStringField(TEXT("actor_id"), OwnerId)
                    || !Reply->TryGetStringField(TEXT("subject_id"), WorkerId) || WorkerId != ResidentId
                    || !Reply->TryGetStringField(TEXT("repair_skill"), ReplySkill)
                    || !Reply->TryGetStringField(TEXT("reply_choice"), ReplyChoice) || ReplyChoice != TEXT("has_need")
                    || !EventHasRecipient(Reply, ResidentId) || !IsInitialNeighbour(Residents, ResidentId, OwnerId)
                    || !HasSkill(Life, ResidentId, ReplySkill)
                    || !HasRepairMaterial(Life, ResidentId, ReplySkill) || HasRepairNeedQuote(Life, RequestId)) continue;
                const TSharedPtr<FJsonObject> Question = FindRepairNeedQuestion(Life, RequestId);
                FString QuestionActor, QuestionSubject, QuestionSkill;
                if (!Question.IsValid() || !Question->TryGetStringField(TEXT("actor_id"), QuestionActor) || QuestionActor != ResidentId
                    || !Question->TryGetStringField(TEXT("subject_id"), QuestionSubject) || QuestionSubject != OwnerId
                    || !Question->TryGetStringField(TEXT("repair_skill"), QuestionSkill) || QuestionSkill != ReplySkill) continue;
                for (const int32 Price : {2, 5, 8})
                {
                    const TSharedRef<FJsonObject> Option = MakeOption(
                        FString::Printf(TEXT("quote_repair_need:%s:%d"), *RequestId, Price),
                        FString::Printf(TEXT("向%s报出%d Col的%s参考价（不是正式合同）"), *NameOf(Residents, OwnerId), Price,
                            ReplySkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄")),
                        TEXT("quote_repair_need"), OwnerId);
                    Option->SetStringField(TEXT("request_id"), RequestId);
                    Option->SetStringField(TEXT("repair_skill"), ReplySkill);
                    Option->SetNumberField(TEXT("offer_price_col"), Price);
                    Option->SetBoolField(TEXT("contractual"), false);
                    Option->SetStringField(TEXT("effects"), TEXT("send one reference price only; creates no contract, deducts no money, and reserves or transfers no item or material"));
                    Result.Add(MakeShared<FJsonValueObject>(Option));
                }
            }
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
        int32 OwnedItemCount = 0, HeldItemCount = 0;
        Life->TryGetArrayField(TEXT("items"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Item = ObjectValue(Value); FString Owner, Custodian; Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Owner == ResidentId) ++OwnedItemCount; if (Custodian == ResidentId) ++HeldItemCount; if (Owner == ResidentId || Custodian == ResidentId) OwnItems.Add(Value); }
        Life->TryGetArrayField(TEXT("accounts"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Account = ObjectValue(Value); FString Owner; Account->TryGetStringField(TEXT("resident_id"), Owner); if (Owner == ResidentId) OwnAccounts.Add(Value); }
        int32 ActiveContractCount = 0;
        Life->TryGetArrayField(TEXT("contracts"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Contract = ObjectValue(Value); FString Owner, Worker, Status; Contract->TryGetStringField(TEXT("owner_id"), Owner); Contract->TryGetStringField(TEXT("worker_id"), Worker); Contract->TryGetStringField(TEXT("status"), Status); if (Owner == ResidentId || Worker == ResidentId) { OwnContracts.Add(Value); if (IsPersonalActiveContractStatus(Status)) ++ActiveContractCount; } }
        Life->TryGetArrayField(TEXT("relations"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Relation = ObjectValue(Value); FString From; Relation->TryGetStringField(TEXT("from_id"), From); if (From == ResidentId) OwnRelations.Add(Value); }
        int64 InboxSeq = 0; Life->TryGetArrayField(TEXT("inboxes"), Values); for (const TSharedPtr<FJsonValue>& Value : *Values) { const TSharedPtr<FJsonObject> Inbox = ObjectValue(Value); FString Owner; Inbox->TryGetStringField(TEXT("resident_id"), Owner); if (Owner == ResidentId) { IntegerField(Inbox, TEXT("next_seq"), InboxSeq, 0, TNumericLimits<int64>::Max()); const TArray<TSharedPtr<FJsonValue>>* InboxEvents = nullptr; Inbox->TryGetArrayField(TEXT("events"), InboxEvents); for (const TSharedPtr<FJsonValue>& Event : *InboxEvents) Received.Add(Event); } }
        TArray<FString> InitialNeighbourIds; for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : Residents) { FString Name; Pair.Value->TryGetStringField(TEXT("name"), Name); if (Name == TEXT("艾琳") || Name == TEXT("拓真") || Name == TEXT("柏木")) InitialNeighbourIds.Add(Pair.Key); }
        for (const FString& NeighbourId : InitialNeighbourIds) { const TSharedPtr<FJsonObject> Pair = Residents[NeighbourId]; auto Neighbour = MakeShared<FJsonObject>(); Neighbour->SetStringField(TEXT("resident_id"), NeighbourId); Neighbour->SetStringField(TEXT("name"), Pair->GetStringField(TEXT("name"))); FJsonArray PublicSkills; Life->TryGetArrayField(TEXT("skills"), Values); for (const TSharedPtr<FJsonValue>& SkillValue : *Values) { const TSharedPtr<FJsonObject> Skill = ObjectValue(SkillValue); FString Holder; Skill->TryGetStringField(TEXT("resident_id"), Holder); if (Holder == NeighbourId) { auto SkillCopy = MakeShared<FJsonObject>(); SkillCopy->SetStringField(TEXT("skill_id"), Skill->GetStringField(TEXT("skill_id"))); SkillCopy->SetStringField(TEXT("source"), Skill->GetStringField(TEXT("source"))); PublicSkills.Add(MakeShared<FJsonValueObject>(SkillCopy)); } } Neighbour->SetArrayField(TEXT("skills"), PublicSkills); Neighbours.Add(MakeShared<FJsonValueObject>(Neighbour)); }
        auto WorkStatus = MakeShared<FJsonObject>(); WorkStatus->SetNumberField(TEXT("owned_item_count"), OwnedItemCount); WorkStatus->SetNumberField(TEXT("held_item_count"), HeldItemCount); WorkStatus->SetNumberField(TEXT("active_contract_count"), ActiveContractCount); WorkStatus->SetStringField(TEXT("active_contract_statuses"), TEXT("proposed,accepted,delivered,completed; cancelled/rejected/collected excluded")); WorkStatus->SetStringField(TEXT("source"), TEXT("authoritative_personal_life_state"));
        auto ToolUse = MakeShared<FJsonObject>(); ToolUse->SetStringField(TEXT("requires"), TEXT("edge=100 and handle=100; owner_id and custodian_id are this resident; at the registered work station; at least 1 wood")); ToolUse->SetStringField(TEXT("effect"), TEXT("consume 1 wood and create 1 kindling; no wood is created")); WorkStatus->SetObjectField(TEXT("tool_use_requirements"), ToolUse);
        auto RepairRequirements = MakeShared<FJsonObject>();
        if (HasSkill(Life, ResidentId, TEXT("metal_repair"))) { auto Rule = MakeShared<FJsonObject>(); Rule->SetStringField(TEXT("requires"), TEXT("a delivered edge-repair contract, custody of its item, own registered work station, and at least 1 iron")); Rule->SetStringField(TEXT("effect"), TEXT("consume exactly 1 iron and set edge=100; consumes no wood or kindling")); RepairRequirements->SetObjectField(TEXT("metal_repair"), Rule); }
        if (HasSkill(Life, ResidentId, TEXT("wood_repair"))) { auto Rule = MakeShared<FJsonObject>(); Rule->SetStringField(TEXT("requires"), TEXT("a delivered handle-repair contract, custody of its item, own registered work station, and at least 1 wood")); Rule->SetStringField(TEXT("effect"), TEXT("consume exactly 1 wood and set handle=100; consumes no iron or kindling")); RepairRequirements->SetObjectField(TEXT("wood_repair"), Rule); }
        WorkStatus->SetObjectField(TEXT("repair_requirements"), RepairRequirements);
        Context->SetNumberField(TEXT("inbox_seq"), static_cast<double>(InboxSeq)); Context->SetArrayField(TEXT("skills"), OwnSkills); Context->SetArrayField(TEXT("items"), OwnItems); Context->SetArrayField(TEXT("accounts"), OwnAccounts); Context->SetArrayField(TEXT("contracts"), OwnContracts); Context->SetObjectField(TEXT("own_work_status"), WorkStatus); Context->SetArrayField(TEXT("received_letters"), Received); Context->SetArrayField(TEXT("known_events"), Received); Context->SetArrayField(TEXT("initial_neighbours"), Neighbours); Context->SetArrayField(TEXT("relations"), OwnRelations); Context->SetArrayField(TEXT("options"), Options(World, ResidentId)); return Context;
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
        const FString Verb = Parts[0]; int64 EventSeq = 0; FString EventText; TArray<FString> Recipients; FString SubjectId, ContractId, ItemId, OfferPart, CommunicationRequestId, CommunicationChoice, CommunicationTopic, CommunicationSkill; int64 OfferPrice = 0; bool bRelationImproved = false; FString RelationFrom, RelationTo; FString RelationReason;
        auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
        TSharedPtr<FJsonObject> ContractObject;
        if (Verb == TEXT("repair_edge") || Verb == TEXT("repair_handle"))
        {
            if (Parts.Num() != 4) return Fail(TEXT("repair proposal option is malformed")); FString ItemIdLocal = Parts[1], WorkerId = Parts[2], Part = Verb == TEXT("repair_edge") ? TEXT("edge") : TEXT("handle"); int64 Price = 0; if (!ParseInteger(Parts[3], Price) || (Price != 2 && Price != 5 && Price != 8)) return Fail(TEXT("repair price is invalid")); const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, ItemIdLocal); if (!Item.IsValid()) return Fail(TEXT("repair item is missing")); FString Owner, Custodian; Item->TryGetStringField(TEXT("owner_id"), Owner); Item->TryGetStringField(TEXT("custodian_id"), Custodian); if (Owner != ResidentId || Custodian != ResidentId || ActiveContractForItem(CandidateLife, ItemIdLocal).IsValid() || !HasSkill(CandidateLife, WorkerId, SkillForPart(Part)) || Item->GetNumberField(*Part) >= 100.0 || !Residents.Contains(WorkerId) || WorkerId == ResidentId) return Fail(TEXT("repair proposal is not currently legal")); ContractId = NewContractId(CandidateLife, CandidateLife->GetNumberField(TEXT("seq"))); auto NewContract = MakeShared<FJsonObject>(); NewContract->SetStringField(TEXT("id"), ContractId); NewContract->SetStringField(TEXT("part"), Part); NewContract->SetStringField(TEXT("item_id"), ItemIdLocal); NewContract->SetStringField(TEXT("owner_id"), ResidentId); NewContract->SetStringField(TEXT("worker_id"), WorkerId); NewContract->SetNumberField(TEXT("price_col"), static_cast<double>(Price)); NewContract->SetNumberField(TEXT("reserved_col"), 0); NewContract->SetStringField(TEXT("status"), TEXT("proposed")); const TArray<TSharedPtr<FJsonValue>>* ExistingContracts = nullptr; CandidateLife->TryGetArrayField(TEXT("contracts"), ExistingContracts); FJsonArray Contracts = *ExistingContracts; Contracts.Add(MakeShared<FJsonValueObject>(NewContract)); CandidateLife->SetArrayField(TEXT("contracts"), Contracts); ItemId = ItemIdLocal; SubjectId = WorkerId; Recipients = { ResidentId, WorkerId }; EventText = FString::Printf(TEXT("%s向%s提出修%s委托，报价%d Col。"), *NameOf(Residents, ResidentId), *NameOf(Residents, WorkerId), Part == TEXT("edge") ? TEXT("刃") : TEXT("柄"), Price);
        }
        else if (Verb == TEXT("offer_repair"))
        {
            if (Parts.Num() != 3) return Fail(TEXT("repair offer option is malformed"));
            ContractId = Parts[1];
            if (!ParseInteger(Parts[2], OfferPrice) || (OfferPrice != 2 && OfferPrice != 5 && OfferPrice != 8)) return Fail(TEXT("repair offer price is invalid"));
            ContractObject = FindContract(CandidateLife, ContractId);
            if (!ContractObject.IsValid()) return Fail(TEXT("repair offer contract is missing"));
            FString Owner, Worker, Status, ItemIdLocal;
            ContractObject->TryGetStringField(TEXT("owner_id"), Owner);
            ContractObject->TryGetStringField(TEXT("worker_id"), Worker);
            ContractObject->TryGetStringField(TEXT("status"), Status);
            ContractObject->TryGetStringField(TEXT("part"), OfferPart);
            ContractObject->TryGetStringField(TEXT("item_id"), ItemIdLocal);
            const TSharedPtr<FJsonObject> Item = FindItem(CandidateLife, ItemIdLocal);
            const TSharedPtr<FJsonObject> Account = FindAccount(CandidateLife, ResidentId);
            const FString Material = OfferPart == TEXT("edge") ? TEXT("iron") : TEXT("wood");
            int64 MaterialQuantity = 0;
            const TSharedPtr<FJsonObject> Latest = LatestOfferTerminalContract(CandidateLife, ResidentId, ItemIdLocal, OfferPart);
            if (ResidentId != Worker || Owner.IsEmpty() || Owner == Worker || !Residents.Contains(Owner)
                || (Status != TEXT("rejected") && Status != TEXT("cancelled")) || !Item.IsValid()
                || !Latest.IsValid() || Latest->GetStringField(TEXT("id")) != ContractId
                || Item->GetStringField(TEXT("owner_id")) != Owner || Item->GetStringField(TEXT("custodian_id")) != Owner
                || Item->GetNumberField(*OfferPart) >= 100.0 || ActiveContractForItem(CandidateLife, ItemIdLocal).IsValid()
                || !HasSkill(CandidateLife, ResidentId, SkillForPart(OfferPart))
                || !Account.IsValid() || !IntegerField(Account, *Material, MaterialQuantity, 0, 1000000) || MaterialQuantity <= 0
                || HasRepairOffer(CandidateLife, ResidentId, ContractId, OfferPart)) return Fail(TEXT("repair offer is not currently legal"));
            ItemId = ItemIdLocal;
            SubjectId = Owner;
            Recipients = { ResidentId, Owner };
            EventText = FString::Printf(TEXT("%s向%s提供修%s建议报价%d Col；这不是已接受的合同。"), *NameOf(Residents, ResidentId), *NameOf(Residents, Owner), OfferPart == TEXT("edge") ? TEXT("刃") : TEXT("柄"), OfferPrice);
        }
        else if (Verb == TEXT("quote_repair_need"))
        {
            if (Parts.Num() != 3) return Fail(TEXT("repair need quote option is malformed"));
            CommunicationRequestId = Parts[1];
            if (!ParseInteger(Parts[2], OfferPrice) || (OfferPrice != 2 && OfferPrice != 5 && OfferPrice != 8)) return Fail(TEXT("repair need quote price is invalid"));
            const TSharedPtr<FJsonObject> Question = FindRepairNeedQuestion(CandidateLife, CommunicationRequestId);
            const TSharedPtr<FJsonObject> Reply = FindRepairNeedReply(CandidateLife, CommunicationRequestId);
            if (!Question.IsValid() || !Reply.IsValid()) return Fail(TEXT("repair need quote request or reply is missing"));
            FString QuestionActor, OwnerId, QuestionSkill, ReplyActor, ReplySubject, ReplySkill, ReplyChoice;
            Question->TryGetStringField(TEXT("actor_id"), QuestionActor);
            Question->TryGetStringField(TEXT("subject_id"), OwnerId);
            Question->TryGetStringField(TEXT("repair_skill"), QuestionSkill);
            Reply->TryGetStringField(TEXT("actor_id"), ReplyActor);
            Reply->TryGetStringField(TEXT("subject_id"), ReplySubject);
            Reply->TryGetStringField(TEXT("repair_skill"), ReplySkill);
            Reply->TryGetStringField(TEXT("reply_choice"), ReplyChoice);
            if (QuestionActor != ResidentId || OwnerId.IsEmpty() || OwnerId == ResidentId || ReplyActor != OwnerId
                || ReplySubject != ResidentId || ReplyChoice != TEXT("has_need") || QuestionSkill != ReplySkill
                || !IsRepairSkill(QuestionSkill) || !IsInitialNeighbour(Residents, ResidentId, OwnerId)
                || !EventHasRecipient(Question, OwnerId) || !EventHasRecipient(Reply, ResidentId)
                || !HasSkill(CandidateLife, ResidentId, QuestionSkill) || !HasRepairMaterial(CandidateLife, ResidentId, QuestionSkill)
                || HasRepairNeedQuote(CandidateLife, CommunicationRequestId)) return Fail(TEXT("repair need quote is not currently legal"));
            CommunicationSkill = QuestionSkill;
            CommunicationTopic = TEXT("repair_work_availability");
            SubjectId = OwnerId;
            Recipients = {ResidentId, OwnerId};
            EventText = FString::Printf(TEXT("%s向%s报出%s参考价%d Col；这不是合同，也未扣款或预留材料。"),
                *NameOf(Residents, ResidentId), *NameOf(Residents, OwnerId),
                CommunicationSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄"), OfferPrice);
        }
        else if (Verb == TEXT("ask_help"))
        {
            if (Parts.Num() != 2) return Fail(TEXT("help question option is malformed"));
            const FString TargetId = Parts[1];
            if (!IsInitialNeighbour(Residents, ResidentId, TargetId) || HasHelpRequest(CandidateLife, ResidentId, TargetId)) return Fail(TEXT("help question is not currently legal"));
            int64 CurrentSeq = 0;
            if (!IntegerField(CandidateLife, TEXT("seq"), CurrentSeq, 0, TNumericLimits<int64>::Max()) || CurrentSeq >= TNumericLimits<int64>::Max()) return Fail(TEXT("help question sequence is invalid"));
            CommunicationRequestId = FString::Printf(TEXT("help_request_%lld"), CurrentSeq + 1);
            SubjectId = TargetId;
            Recipients = { ResidentId, TargetId };
            EventText = FString::Printf(TEXT("%s想先问问%s：近期是否愿意帮忙？"), *NameOf(Residents, ResidentId), *NameOf(Residents, TargetId));
        }
        else if (Verb == TEXT("reply_help"))
        {
            if (Parts.Num() != 3 || !IsReplyChoice(Parts[2])) return Fail(TEXT("help reply option is malformed"));
            CommunicationRequestId = Parts[1];
            const TSharedPtr<FJsonObject> Question = FindHelpRequest(CandidateLife, CommunicationRequestId);
            if (!Question.IsValid()) return Fail(TEXT("help question is missing"));
            FString QuestionActor, QuestionTarget, Topic;
            Question->TryGetStringField(TEXT("actor_id"), QuestionActor);
            Question->TryGetStringField(TEXT("subject_id"), QuestionTarget);
            Question->TryGetStringField(TEXT("topic"), Topic);
            const TArray<TSharedPtr<FJsonValue>>* QuestionRecipients = nullptr;
            bool bTargetReceived = false;
            if (Question->TryGetArrayField(TEXT("recipient_ids"), QuestionRecipients))
            {
                for (const TSharedPtr<FJsonValue>& Recipient : *QuestionRecipients)
                {
                    FString RecipientId;
                    if (Recipient.IsValid() && Recipient->TryGetString(RecipientId) && RecipientId == ResidentId) { bTargetReceived = true; break; }
                }
            }
            if (ResidentId != QuestionTarget || QuestionActor.IsEmpty() || QuestionActor == ResidentId
                || Topic != TEXT("help_availability") || !IsInitialNeighbour(Residents, ResidentId, QuestionActor)
                || !bTargetReceived || HasHelpReply(CandidateLife, CommunicationRequestId)) return Fail(TEXT("help reply is not currently legal"));
            SubjectId = QuestionActor;
            Recipients = { ResidentId, QuestionActor };
            CommunicationChoice = Parts[2];
            EventText = CommunicationChoice == TEXT("willing")
                ? FString::Printf(TEXT("%s近期愿意了解%s的具体问题。"), *NameOf(Residents, ResidentId), *NameOf(Residents, QuestionActor))
                : CommunicationChoice == TEXT("unavailable")
                    ? FString::Printf(TEXT("%s近期暂时不方便帮忙。"), *NameOf(Residents, ResidentId))
                    : FString::Printf(TEXT("%s目前尚不确定，之后再说。"), *NameOf(Residents, ResidentId));
        }
        else if (Verb == TEXT("ask_repair_need"))
        {
            if (Parts.Num() != 2) return Fail(TEXT("repair need question option is malformed"));
            const FString TargetId = Parts[1];
            if (!IsInitialNeighbour(Residents, ResidentId, TargetId)
                || (!HasSkill(CandidateLife, ResidentId, TEXT("metal_repair")) && !HasSkill(CandidateLife, ResidentId, TEXT("wood_repair")))) return Fail(TEXT("repair need question has no resident repair skill"));
            CommunicationSkill = HasSkill(CandidateLife, ResidentId, TEXT("metal_repair")) ? TEXT("metal_repair") : TEXT("wood_repair");
            if (HasRepairNeedQuestion(CandidateLife, ResidentId, TargetId, CommunicationSkill)) return Fail(TEXT("repair need question was already sent"));
            int64 CurrentSeq = 0;
            if (!IntegerField(CandidateLife, TEXT("seq"), CurrentSeq, 0, TNumericLimits<int64>::Max()) || CurrentSeq >= TNumericLimits<int64>::Max()) return Fail(TEXT("repair need question sequence is invalid"));
            CommunicationRequestId = FString::Printf(TEXT("repair_need_request_%lld"), CurrentSeq + 1);
            CommunicationTopic = TEXT("repair_work_availability");
            SubjectId = TargetId;
            Recipients = { ResidentId, TargetId };
            EventText = FString::Printf(TEXT("%s想问%s近期是否有需要%s的物品；这只是询问，不是委托。"), *NameOf(Residents, ResidentId), *NameOf(Residents, TargetId), CommunicationSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄"));
        }
        else if (Verb == TEXT("reply_repair_need"))
        {
            if (Parts.Num() != 3 || !IsRepairNeedChoice(Parts[2])) return Fail(TEXT("repair need reply option is malformed"));
            CommunicationRequestId = Parts[1];
            const TSharedPtr<FJsonObject> Question = FindRepairNeedQuestion(CandidateLife, CommunicationRequestId);
            if (!Question.IsValid()) return Fail(TEXT("repair need question is missing"));
            FString QuestionActor, QuestionTarget, Topic;
            Question->TryGetStringField(TEXT("actor_id"), QuestionActor);
            Question->TryGetStringField(TEXT("subject_id"), QuestionTarget);
            Question->TryGetStringField(TEXT("topic"), Topic);
            Question->TryGetStringField(TEXT("repair_skill"), CommunicationSkill);
            if (ResidentId != QuestionTarget || QuestionActor.IsEmpty() || QuestionActor == ResidentId
                || Topic != TEXT("repair_work_availability") || !IsRepairSkill(CommunicationSkill)
                || !IsInitialNeighbour(Residents, ResidentId, QuestionActor) || !HasSkill(CandidateLife, QuestionActor, CommunicationSkill)
                || !EventHasRecipient(Question, ResidentId) || HasRepairNeedReply(CandidateLife, CommunicationRequestId)
                || (Parts[2] == TEXT("has_need") && !HasOwnedRepairNeed(CandidateLife, ResidentId, CommunicationSkill))) return Fail(TEXT("repair need reply is not currently legal"));
            CommunicationTopic = TEXT("repair_work_availability");
            SubjectId = QuestionActor;
            CommunicationChoice = Parts[2];
            Recipients = { ResidentId, QuestionActor };
            EventText = CommunicationChoice == TEXT("has_need")
                ? FString::Printf(TEXT("%s近期确实有需要%s的物品，可以再谈；这不是已接受的委托。"), *NameOf(Residents, ResidentId), CommunicationSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄"))
                : CommunicationChoice == TEXT("no_need")
                    ? FString::Printf(TEXT("%s近期没有需要%s的物品。"), *NameOf(Residents, ResidentId), CommunicationSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄"))
                    : FString::Printf(TEXT("%s目前还不确定是否有需要%s的物品。"), *NameOf(Residents, ResidentId), CommunicationSkill == TEXT("metal_repair") ? TEXT("修刃") : TEXT("修柄"));
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
        if (!AddEvent(CandidateLife, Verb, ResidentId, SubjectId, EventContractId, ItemId, Recipients, FullText, OperationId, EventSeq, Error, OfferPart, OfferPrice, CommunicationRequestId, CommunicationChoice, CommunicationTopic, CommunicationSkill)) return false;
        if (bRelationImproved && !UpdateRelation(CandidateLife, RelationFrom, RelationTo, EventSeq, RelationReason, Error)) return false;
        if (!AddApplied(CandidateLife, OperationId, ResidentId, OptionId, Utterance, EventSeq, Error)) return false;
        if (!Validate(Candidate.ToSharedRef(), Error)) return false;
        World->Values = Candidate->Values;
        return true;
    }
}
