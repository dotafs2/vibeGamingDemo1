#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradLook.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

#include <limits>

namespace
{
    TSharedRef<FJsonObject> LookRuntime()
    {
        return MakeShared<FJsonObject>();
    }

    bool LookBool(const TSharedRef<FJsonObject>& Runtime, const TCHAR* Key)
    {
        bool Value = false;
        return Runtime->TryGetBoolField(Key, Value) && Value;
    }

    double LookNumber(const TSharedRef<FJsonObject>& Runtime, const TCHAR* Key)
    {
        double Value = 0.0;
        Runtime->TryGetNumberField(Key, Value);
        return Value;
    }

    TSharedRef<FJsonObject> LookReload(const TSharedRef<FJsonObject>& Runtime)
    {
        FString Text;
        FJsonSerializer::Serialize(Runtime, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        TSharedPtr<FJsonObject> Reloaded;
        FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text), Reloaded);
        return Reloaded.ToSharedRef();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLookStateTest, "ThreeHearths.AincradLook.StateAndIdempotency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLookStateTest::RunTest(const FString&)
{
    const TSharedRef<FJsonObject> Runtime = LookRuntime();
    FString Error;
    TestTrue(TEXT("look starts with wrapped yaw"), HearthAincradLook::Start(Runtime, 170.0, TEXT("look-1"), Error));
    TestTrue(TEXT("turn is persisted"), LookBool(Runtime, TEXT("look_turn_pending")));
    TestEqual(TEXT("170 plus 60 wraps to -130"), LookNumber(Runtime, TEXT("look_target_yaw_degrees")), -130.0);
    const FString BeforeDuplicate = Runtime->GetStringField(TEXT("look_operation_id"));
    const TSharedRef<FJsonObject> PendingReload = LookReload(Runtime);
    TestTrue(TEXT("pending turn marker survives JSON reload"), LookBool(PendingReload, TEXT("look_turn_pending")));
    HearthAincradLook::Complete(PendingReload);
    TestTrue(TEXT("same operation retry is an idempotent success"), HearthAincradLook::Start(PendingReload, 170.0, TEXT("look-1"), Error));
    TestFalse(TEXT("same operation retry does not start another turn"), LookBool(PendingReload, TEXT("look_turn_pending")));
    TestEqual(TEXT("same operation remains recorded"), PendingReload->GetStringField(TEXT("look_operation_id")), BeforeDuplicate);
    TestTrue(TEXT("a new operation can start after completion"), HearthAincradLook::Start(PendingReload, -170.0, TEXT("look-2"), Error));
    TestEqual(TEXT("negative yaw wraps to -110"), LookNumber(PendingReload, TEXT("look_target_yaw_degrees")), -110.0);
    TestFalse(TEXT("a pending turn rejects another operation"), HearthAincradLook::Start(PendingReload, 0.0, TEXT("look-3"), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLookInvalidInputTest, "ThreeHearths.AincradLook.InvalidInputFailsClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLookInvalidInputTest::RunTest(const FString&)
{
    FString Error;
    const TSharedRef<FJsonObject> Empty = LookRuntime();
    TestFalse(TEXT("empty operation is rejected"), HearthAincradLook::Start(Empty, 0.0, FString(), Error));
    TestFalse(TEXT("non-finite yaw is rejected"), HearthAincradLook::Start(Empty, std::numeric_limits<double>::quiet_NaN(), TEXT("look-nan"), Error));

    const TSharedRef<FJsonObject> BadPending = LookRuntime();
    BadPending->SetStringField(TEXT("look_turn_pending"), TEXT("yes"));
    TestFalse(TEXT("malformed pending flag is rejected"), HearthAincradLook::Start(BadPending, 0.0, TEXT("look-bad"), Error));

    const TSharedRef<FJsonObject> BadTarget = LookRuntime();
    BadTarget->SetNumberField(TEXT("look_target_yaw_degrees"), 181.0);
    TestFalse(TEXT("out of range persisted target is rejected"), HearthAincradLook::Start(BadTarget, 0.0, TEXT("look-bad-target"), Error));
    TestFalse(TEXT("malformed state has no follow-up"), HearthAincradLook::HasFollowup(BadTarget));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLookExplicitTargetTest, "ThreeHearths.AincradLook.ExplicitTargetAndRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLookExplicitTargetTest::RunTest(const FString&)
{
    FString Error;
    const TSharedRef<FJsonObject> Runtime = LookRuntime();
    TestTrue(TEXT("explicit target starts a look"), HearthAincradLook::StartAtYaw(Runtime, 725.0, TEXT("look-target-1"), Error));
    TestEqual(TEXT("explicit target is normalized"), LookNumber(Runtime, TEXT("look_target_yaw_degrees")), 5.0);
    HearthAincradLook::Complete(Runtime);

    HearthAincradLook::OnDispatch(Runtime, true);
    TestTrue(TEXT("cross-boundary target starts a look"), HearthAincradLook::StartAtYaw(Runtime, 181.0, TEXT("look-target-2"), Error));
    TestEqual(TEXT("181 degrees normalizes across positive boundary"), LookNumber(Runtime, TEXT("look_target_yaw_degrees")), -179.0);
    HearthAincradLook::Complete(Runtime);
    TestTrue(TEXT("completed credited look has one follow-up"), HearthAincradLook::HasFollowup(Runtime));

    const TSharedRef<FJsonObject> Reloaded = LookReload(Runtime);
    const double BeforeRetryTarget = LookNumber(Reloaded, TEXT("look_target_yaw_degrees"));
    TestTrue(TEXT("same operation retry after cold restore is idempotent"), HearthAincradLook::StartAtYaw(Reloaded, 42.0, TEXT("look-target-2"), Error));
    TestFalse(TEXT("idempotent retry does not create a pending turn"), LookBool(Reloaded, TEXT("look_turn_pending")));
    TestEqual(TEXT("idempotent retry preserves completed target"), LookNumber(Reloaded, TEXT("look_target_yaw_degrees")), BeforeRetryTarget);
    TestTrue(TEXT("idempotent retry does not increase or consume follow-up credit"), HearthAincradLook::HasFollowup(Reloaded));

    const TSharedRef<FJsonObject> Invalid = LookRuntime();
    Invalid->SetBoolField(TEXT("look_followup_credit"), true);
    const FString InvalidBefore = Invalid->GetBoolField(TEXT("look_followup_credit")) ? TEXT("true") : TEXT("false");
    TestFalse(TEXT("non-finite explicit target is rejected"), HearthAincradLook::StartAtYaw(Invalid, std::numeric_limits<double>::quiet_NaN(), TEXT("look-target-invalid"), Error));
    TestFalse(TEXT("invalid target has no pending side effect"), Invalid->HasField(TEXT("look_turn_pending")));
    TestEqual(TEXT("invalid target preserves unrelated credit"), Invalid->GetBoolField(TEXT("look_followup_credit")) ? TEXT("true") : TEXT("false"), InvalidBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLookFollowupTest, "ThreeHearths.AincradLook.OneFollowupAndColdRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLookFollowupTest::RunTest(const FString&)
{
    FString Error;
    const TSharedRef<FJsonObject> Runtime = LookRuntime();
    HearthAincradLook::OnDispatch(Runtime, true);
    TestTrue(TEXT("independent dispatch grants look credit"), LookBool(Runtime, TEXT("look_followup_credit")));
    TestTrue(TEXT("look turn starts"), HearthAincradLook::Start(Runtime, 120.0, TEXT("look-followup-1"), Error));
    HearthAincradLook::Complete(Runtime);
    TestTrue(TEXT("completed look exposes one follow-up"), HearthAincradLook::HasFollowup(Runtime));

    const TSharedRef<FJsonObject> Reloaded = LookReload(Runtime);
    TestTrue(TEXT("follow-up state survives JSON reload"), HearthAincradLook::HasFollowup(Reloaded));
    HearthAincradLook::OnDispatch(Reloaded, false);
    TestFalse(TEXT("pure look follow-up consumes credit"), HearthAincradLook::HasFollowup(Reloaded));
    TestFalse(TEXT("follow-up marker is cleared by dispatch"), LookBool(Reloaded, TEXT("look_followup_pending")));

    TestTrue(TEXT("follow-up can turn again"), HearthAincradLook::Start(Reloaded, 0.0, TEXT("look-followup-2"), Error));
    HearthAincradLook::Complete(Reloaded);
    TestFalse(TEXT("second look has no immediate paid follow-up"), HearthAincradLook::HasFollowup(Reloaded));

    HearthAincradLook::OnDispatch(Reloaded, true);
    TestTrue(TEXT("an external or independent event can grant one new credit"), LookBool(Reloaded, TEXT("look_followup_credit")));
    TestTrue(TEXT("new credited look starts"), HearthAincradLook::Start(Reloaded, -179.0, TEXT("look-followup-3"), Error));
    HearthAincradLook::Complete(Reloaded);
    TestTrue(TEXT("new credit creates exactly one new follow-up"), HearthAincradLook::HasFollowup(Reloaded));
    HearthAincradLook::Complete(Reloaded);
    TestTrue(TEXT("completion without a pending turn does not revoke follow-up"), HearthAincradLook::HasFollowup(Reloaded));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLookHeldToolAttentionTest, "ThreeHearths.AincradLook.HeldToolAttentionState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLookHeldToolAttentionTest::RunTest(const FString&)
{
    FString Error;
    const TSharedRef<FJsonObject> Runtime = LookRuntime();
    HearthAincradLook::OnDispatch(Runtime, true);
    TestTrue(TEXT("held-tool attention starts"), HearthAincradLook::StartAtAttention(Runtime, -45.0, -18.5, TEXT("held-tool-look-1"), Error));
    TestTrue(TEXT("held-tool attention is persisted"), LookBool(Runtime, TEXT("look_attention_only")));
    TestEqual(TEXT("held-tool target yaw is normalized"), LookNumber(Runtime, TEXT("look_target_yaw_degrees")), -45.0);
    TestEqual(TEXT("held-tool target pitch is persisted"), LookNumber(Runtime, TEXT("look_target_pitch_degrees")), -18.5);
    const TSharedRef<FJsonObject> Reloaded = LookReload(Runtime);
    TestTrue(TEXT("held-tool attention survives reload"), LookBool(Reloaded, TEXT("look_attention_only")));
    HearthAincradLook::Complete(Reloaded);
    TestFalse(TEXT("held-tool attention completes pending turn"), LookBool(Reloaded, TEXT("look_turn_pending")));
    TestTrue(TEXT("completed attention grants one follow-up"), HearthAincradLook::HasFollowup(Reloaded));
    const double CompletedPitch = LookNumber(Reloaded, TEXT("look_target_pitch_degrees"));
    const TSharedRef<FJsonObject> FollowupReload = LookReload(Reloaded);
    HearthAincradLook::OnDispatch(FollowupReload, false);
    TestFalse(TEXT("follow-up dispatch consumes exactly one credit"), HearthAincradLook::HasFollowup(FollowupReload));
    TestTrue(TEXT("same completed operation is idempotent"), HearthAincradLook::StartAtAttention(FollowupReload, 120.0, 45.0, TEXT("held-tool-look-1"), Error));
    TestFalse(TEXT("same completed operation does not restore pending turn"), LookBool(FollowupReload, TEXT("look_turn_pending")));
    TestEqual(TEXT("same completed operation preserves attention pitch"), LookNumber(FollowupReload, TEXT("look_target_pitch_degrees")), CompletedPitch);
    const TSharedRef<FJsonObject> Invalid = LookRuntime();
    TestFalse(TEXT("held-tool attention rejects excessive pitch"), HearthAincradLook::StartAtAttention(Invalid, 0.0, 90.1, TEXT("held-tool-look-bad"), Error));
    TestFalse(TEXT("invalid attention leaves no pending turn"), Invalid->HasField(TEXT("look_turn_pending")));
    return true;
}

#endif
