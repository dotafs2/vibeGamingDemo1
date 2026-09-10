#include "HearthAincradIntent.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

namespace HearthAincradIntent
{
    namespace
    {
        constexpr int32 MaxOperation = 128;
        constexpr int32 MaxText = 500;
        constexpr int32 MaxHistory = 8;
        constexpr int32 MaxSeenOperations = 128;
        constexpr int32 MaxRequests = 16;
        constexpr int32 MaxPresentedRequests = 4;
        constexpr int32 Schema = 1;

        FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
        {
            FString V; if (O.IsValid()) O->TryGetStringField(Key, V); return V;
        }

        bool ValidText(const FString& V) { return V.IsEmpty() || V.Len() <= MaxText; }

        TArray<TSharedPtr<FJsonValue>> Array(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            return O.IsValid() && O->TryGetArrayField(Key, Values) && Values ? *Values : TArray<TSharedPtr<FJsonValue>>();
        }

        bool SameOperation(const TSharedPtr<FJsonObject>& Entry, const FString& OperationId)
        { return Str(Entry, TEXT("operation_id")) == OperationId; }
    }

    static bool RecordDecisionInPlace(const TSharedRef<FJsonObject>& Runtime, const FString& OperationId,
        const FString& Goal, const FString& Need, bool bRequestChange, double NowUtc, FString& Error)
    {
        Error.Empty();
        if (OperationId.IsEmpty() || OperationId.Len() > MaxOperation || !ValidText(Goal)
            || !ValidText(Need) || !FMath::IsFinite(NowUtc) || NowUtc < 0.0)
        { Error = TEXT("intent payload is invalid"); return false; }

        TArray<TSharedPtr<FJsonValue>> History = Array(Runtime, TEXT("intent_history"));
        for (const TSharedPtr<FJsonValue>& Value : History)
        {
            const TSharedPtr<FJsonObject> Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            if (SameOperation(Entry, OperationId))
            {
                bool ExistingRequest = false;
                if (!Entry->TryGetBoolField(TEXT("request_change"), ExistingRequest)) { Error = TEXT("saved intent request flag is invalid"); return false; }
                if (Str(Entry, TEXT("goal")) == Goal && Str(Entry, TEXT("need")) == Need && ExistingRequest == bRequestChange) return true;
                Error = TEXT("operation_id conflicts with an existing intent payload"); return false;
            }
        }
        TArray<TSharedPtr<FJsonValue>> Seen = Array(Runtime, TEXT("intent_seen_operations"));
        for (const TSharedPtr<FJsonValue>& Value : Seen)
            if (Value.IsValid() && Value->Type == EJson::String && Value->AsString() == OperationId) { Error = TEXT("operation_id was already used with a different intent"); return false; }
        if (Seen.Num() >= MaxSeenOperations) { Error = TEXT("intent operation ledger is full"); return false; }
        if (History.Num() >= MaxHistory) History.RemoveAt(0);

        const FString PreviousGoal = Str(Runtime, TEXT("self_goal"));
        if (!Goal.IsEmpty() && !PreviousGoal.IsEmpty() && Goal != PreviousGoal)
        {
            for (const TSharedPtr<FJsonValue>& Value : History)
            {
                const TSharedPtr<FJsonObject> Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
                if (Entry.IsValid() && Str(Entry, TEXT("kind")) == TEXT("goal") && Str(Entry, TEXT("status")) == TEXT("active"))
                    Entry->SetStringField(TEXT("status"), TEXT("superseded"));
            }
        }

        auto Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("operation_id"), OperationId);
        Entry->SetStringField(TEXT("goal"), Goal);
        Entry->SetStringField(TEXT("need"), Need);
        // A request_change may carry both a new self goal and a capability
        // request. Keep it in the goal lineage so a later goal can supersede it.
        Entry->SetStringField(TEXT("kind"), Goal.IsEmpty() ? (bRequestChange ? TEXT("request_change") : TEXT("self_report")) : TEXT("goal"));
        Entry->SetStringField(TEXT("status"), TEXT("active"));
        Entry->SetStringField(TEXT("source"), TEXT("kimi_self_report"));
        Entry->SetNumberField(TEXT("utc"), NowUtc);
        Entry->SetBoolField(TEXT("request_change"), bRequestChange);
        History.Add(MakeShared<FJsonValueObject>(Entry));
        Seen.Add(MakeShared<FJsonValueString>(OperationId));
        Runtime->SetNumberField(TEXT("intent_schema_version"), Schema);
        if (!Goal.IsEmpty()) Runtime->SetStringField(TEXT("self_goal"), Goal);
        if (!Need.IsEmpty()) Runtime->SetStringField(TEXT("self_need"), Need);
        Runtime->SetNumberField(TEXT("self_intent_updated_utc"), NowUtc);
        Runtime->SetArrayField(TEXT("intent_history"), History);
        Runtime->SetArrayField(TEXT("intent_seen_operations"), Seen);

        if (bRequestChange && !Need.IsEmpty())
        {
            TArray<TSharedPtr<FJsonValue>> Requests = Array(Runtime, TEXT("capability_requests"));
            bool bExisting = false;
            for (const TSharedPtr<FJsonValue>& Value : Requests)
            {
                const TSharedPtr<FJsonObject> Request = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
                if (Request.IsValid() && Str(Request, TEXT("need")) == Need && Str(Request, TEXT("status")) == TEXT("proposed"))
                {
                    Request->SetNumberField(TEXT("last_seen_utc"), NowUtc);
                    bExisting = true; break;
                }
            }
            if (!bExisting)
            {
                if (Requests.Num() >= MaxRequests) { Error = TEXT("capability request capacity is full"); return false; }
                auto Request = MakeShared<FJsonObject>();
                Request->SetStringField(TEXT("operation_id"), OperationId);
                Request->SetStringField(TEXT("need"), Need);
                Request->SetStringField(TEXT("status"), TEXT("proposed"));
                Request->SetStringField(TEXT("source"), TEXT("kimi_self_report"));
                Request->SetNumberField(TEXT("utc"), NowUtc);
                Request->SetNumberField(TEXT("last_seen_utc"), NowUtc);
                Requests.Add(MakeShared<FJsonValueObject>(Request));
                Runtime->SetArrayField(TEXT("capability_requests"), Requests);
            }
        }
        return true;
    }

    bool RecordDecision(const TSharedRef<FJsonObject>& Runtime, const FString& OperationId,
        const FString& Goal, const FString& Need, bool bRequestChange, double NowUtc, FString& Error)
    {
        FString Text; if (!FJsonSerializer::Serialize(Runtime, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text)))
        { Error = TEXT("intent transaction clone failed"); return false; }
        TSharedPtr<FJsonObject> Candidate;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text), Candidate) || !Candidate.IsValid())
        { Error = TEXT("intent transaction clone failed"); return false; }
        if (!RecordDecisionInPlace(Candidate.ToSharedRef(), OperationId, Goal, Need, bRequestChange, NowUtc, Error)) return false;
        Runtime->Values = Candidate->Values;
        return true;
    }

    TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& Runtime)
    {
        auto Out = MakeShared<FJsonObject>();
        Out->SetNumberField(TEXT("intent_schema_version"), Schema);
        FString Goal = Str(Runtime, TEXT("self_goal"));
        FString Need = Str(Runtime, TEXT("self_need"));
        FString Source = TEXT("kimi_self_report");
        if (Goal.IsEmpty()) Goal = Str(Runtime, TEXT("last_goal"));
        if (Need.IsEmpty()) Need = Str(Runtime, TEXT("last_need"));
        if (Str(Runtime, TEXT("self_goal")).IsEmpty() && Str(Runtime, TEXT("self_need")).IsEmpty())
            Source = TEXT("legacy_runtime_self_report");
        if (Goal.Len() > MaxText) Goal.Empty();
        if (Need.Len() > MaxText) Need.Empty();
        Out->SetStringField(TEXT("goal"), Goal);
        Out->SetStringField(TEXT("need"), Need);
        Out->SetStringField(TEXT("source"), Source);
        Out->SetStringField(TEXT("interpretation"), TEXT("resident intent; not an authoritative world fact"));
        TArray<TSharedPtr<FJsonValue>> Pending;
        int32 Presented = 0;
        for (const TSharedPtr<FJsonValue>& Value : Array(Runtime, TEXT("capability_requests")))
        {
            const TSharedPtr<FJsonObject> Request = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
            const FString RequestId = Str(Request, TEXT("operation_id"));
            const FString RequestNeed = Str(Request, TEXT("need"));
            if (Request.IsValid() && RequestId.Len() <= MaxOperation && RequestNeed.Len() <= MaxText
                && Str(Request, TEXT("status")) == TEXT("proposed") && Presented < MaxPresentedRequests)
            {
                auto Copy = MakeShared<FJsonObject>();
                Copy->SetStringField(TEXT("operation_id"), Str(Request, TEXT("operation_id")));
                Copy->SetStringField(TEXT("need"), Str(Request, TEXT("need")));
                Copy->SetStringField(TEXT("status"), Str(Request, TEXT("status")));
                Copy->SetStringField(TEXT("source"), TEXT("kimi_self_report"));
                double Utc = 0.0; if (Request->TryGetNumberField(TEXT("utc"), Utc)) Copy->SetNumberField(TEXT("utc"), Utc);
                double LastSeen = 0.0; if (Request->TryGetNumberField(TEXT("last_seen_utc"), LastSeen)) Copy->SetNumberField(TEXT("last_seen_utc"), LastSeen);
                Pending.Add(MakeShared<FJsonValueObject>(Copy)); ++Presented;
            }
        }
        Out->SetArrayField(TEXT("unresolved_capability_requests"), Pending);
        return Out;
    }
}

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    TSharedPtr<FJsonObject> RoundTrip(const TSharedRef<FJsonObject>& Source)
    {
        FString Text; FJsonSerializer::Serialize(Source, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        TSharedPtr<FJsonObject> Out; FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text), Out); return Out;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradIntentTest, "ThreeHearths.AincradIntent.IdempotentPrivateBounded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradIntentTest::RunTest(const FString&)
{
    auto Runtime = MakeShared<FJsonObject>(); FString Error;
    TestTrue(TEXT("first self report records"), HearthAincradIntent::RecordDecision(Runtime, TEXT("op-1"), TEXT("repair home"), TEXT("need timber"), true, 10.0, Error));
    TestTrue(TEXT("duplicate operation is idempotent"), HearthAincradIntent::RecordDecision(Runtime, TEXT("op-1"), TEXT("repair home"), TEXT("need timber"), true, 11.0, Error));
    TestEqual(TEXT("duplicate does not grow history"), Runtime->GetArrayField(TEXT("intent_history")).Num(), 1);
    auto Reloaded = RoundTrip(Runtime); TestTrue(TEXT("serialize reload succeeds"), Reloaded.IsValid());
    auto Context = HearthAincradIntent::PersonalContext(Reloaded.ToSharedRef()); TestEqual(TEXT("goal survives reload"), Context->GetStringField(TEXT("goal")), FString(TEXT("repair home"))); TestEqual(TEXT("one pending proposal"), Context->GetArrayField(TEXT("unresolved_capability_requests")).Num(), 1);
    TestTrue(TEXT("empty goal does not erase"), HearthAincradIntent::RecordDecision(Reloaded.ToSharedRef(), TEXT("op-2"), FString(), TEXT("need timber"), false, 12.0, Error)); TestEqual(TEXT("goal remains"), Reloaded->GetStringField(TEXT("self_goal")), FString(TEXT("repair home")));
    FString Long; Long.Reserve(501); for (int32 I=0; I<501; ++I) Long.AppendChar(TEXT('x')); TestFalse(TEXT("overlong self report rejected"), HearthAincradIntent::RecordDecision(Reloaded.ToSharedRef(), TEXT("op-3"), Long, FString(), false, 13.0, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradIntentIsolationTest, "ThreeHearths.AincradIntent.GoalSupersedesAndContextIsPrivate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradIntentIsolationTest::RunTest(const FString&)
{
    auto A = MakeShared<FJsonObject>(); auto B = MakeShared<FJsonObject>(); FString Error;
    HearthAincradIntent::RecordDecision(A, TEXT("a-1"), TEXT("old goal"), TEXT("need A"), true, 1.0, Error);
    HearthAincradIntent::RecordDecision(A, TEXT("a-2"), TEXT("new goal"), TEXT("need B"), false, 2.0, Error);
    auto C = HearthAincradIntent::PersonalContext(B); TestTrue(TEXT("other resident context has no leakage"), C->GetStringField(TEXT("goal")).IsEmpty() && C->GetStringField(TEXT("need")).IsEmpty());
    TestEqual(TEXT("latest goal is visible"), A->GetStringField(TEXT("self_goal")), FString(TEXT("new goal"))); TestEqual(TEXT("history retains both reports"), A->GetArrayField(TEXT("intent_history")).Num(), 2);
    TestEqual(TEXT("old goal is superseded"), A->GetArrayField(TEXT("intent_history"))[0]->AsObject()->GetStringField(TEXT("status")), FString(TEXT("superseded")));
    auto Legacy = MakeShared<FJsonObject>(); Legacy->SetStringField(TEXT("last_goal"), TEXT("legacy goal")); Legacy->SetStringField(TEXT("last_need"), TEXT("legacy need"));
    auto LegacyContext = HearthAincradIntent::PersonalContext(Legacy); TestEqual(TEXT("legacy goal fallback is read-only"), LegacyContext->GetStringField(TEXT("goal")), FString(TEXT("legacy goal"))); TestEqual(TEXT("legacy source is explicit"), LegacyContext->GetStringField(TEXT("source")), FString(TEXT("legacy_runtime_self_report"))); TestFalse(TEXT("fallback does not write schema"), Legacy->HasField(TEXT("intent_schema_version")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradIntentConflictTest, "ThreeHearths.AincradIntent.ConflictCapacityAndCopy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradIntentConflictTest::RunTest(const FString&)
{
    auto Runtime = MakeShared<FJsonObject>(); FString Error;
    TestTrue(TEXT("request one"), HearthAincradIntent::RecordDecision(Runtime, TEXT("same"), TEXT("g"), TEXT("need"), true, 1.0, Error));
    const int32 HistoryBefore = Runtime->GetArrayField(TEXT("intent_history")).Num();
    TestFalse(TEXT("same operation conflict is rejected"), HearthAincradIntent::RecordDecision(Runtime, TEXT("same"), TEXT("different"), TEXT("need"), true, 2.0, Error));
    TestEqual(TEXT("conflict leaves state unchanged"), Runtime->GetArrayField(TEXT("intent_history")).Num(), HistoryBefore);
    TestTrue(TEXT("same need new operation is retained without duplicate request"), HearthAincradIntent::RecordDecision(Runtime, TEXT("same-2"), TEXT("g"), TEXT("need"), true, 3.0, Error));
    TestEqual(TEXT("same need has one request"), Runtime->GetArrayField(TEXT("capability_requests")).Num(), 1);
    TestEqual(TEXT("same need updates last seen"), Runtime->GetArrayField(TEXT("capability_requests"))[0]->AsObject()->GetNumberField(TEXT("last_seen_utc")), 3.0);
    auto Context = HearthAincradIntent::PersonalContext(Runtime);
    Context->GetArrayField(TEXT("unresolved_capability_requests"))[0]->AsObject()->SetStringField(TEXT("need"), TEXT("mutated copy"));
    TestEqual(TEXT("context request is copied"), Runtime->GetArrayField(TEXT("capability_requests"))[0]->AsObject()->GetStringField(TEXT("need")), FString(TEXT("need")));
    for (int32 I = 0; I < 126; ++I)
    {
        const FString Id = FString::Printf(TEXT("op-%d"), I);
        TestTrue(TEXT("bounded seen operation accepted"), HearthAincradIntent::RecordDecision(Runtime, Id, FString(), FString(), false, 10.0 + I, Error));
    }
    TestFalse(TEXT("seen operation capacity rejects without forgetting history"), HearthAincradIntent::RecordDecision(Runtime, TEXT("op-overflow"), FString(), FString(), false, 999.0, Error));
    return true;
}
#endif
