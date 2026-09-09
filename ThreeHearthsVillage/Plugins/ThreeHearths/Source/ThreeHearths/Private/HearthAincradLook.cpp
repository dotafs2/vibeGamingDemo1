#include "HearthAincradLook.h"

#include "Dom/JsonObject.h"

namespace HearthAincradLook
{
    namespace
    {
        constexpr double LookDeltaDegrees = 60.0;
        constexpr double MinYawDegrees = -180.0;
        constexpr double MaxYawDegrees = 180.0;

        bool ReadOptionalBool(const TSharedRef<FJsonObject>& Runtime, const TCHAR* Key, bool& OutValue)
        {
            if (!Runtime->HasField(Key))
            {
                OutValue = false;
                return true;
            }
            return Runtime->TryGetBoolField(Key, OutValue);
        }

        bool ReadOptionalNumber(const TSharedRef<FJsonObject>& Runtime, const TCHAR* Key, double& OutValue)
        {
            if (!Runtime->HasField(Key)) return false;
            return Runtime->TryGetNumberField(Key, OutValue) && FMath::IsFinite(OutValue);
        }

        bool ReadPersistedState(const TSharedRef<FJsonObject>& Runtime, bool& bTurnPending,
            bool& bFollowupCredit, bool& bFollowupPending, double& PersistedTarget)
        {
            if (!ReadOptionalBool(Runtime, TEXT("look_turn_pending"), bTurnPending)
                || !ReadOptionalBool(Runtime, TEXT("look_followup_credit"), bFollowupCredit)
                || !ReadOptionalBool(Runtime, TEXT("look_followup_pending"), bFollowupPending)) return false;

            if (Runtime->HasField(TEXT("look_target_yaw_degrees")))
            {
                if (!ReadOptionalNumber(Runtime, TEXT("look_target_yaw_degrees"), PersistedTarget)
                    || PersistedTarget < MinYawDegrees || PersistedTarget > MaxYawDegrees) return false;
            }
            else PersistedTarget = 0.0;
            if (bTurnPending || bFollowupPending)
            {
                FString Operation;
                if (!Runtime->HasField(TEXT("look_target_yaw_degrees"))
                    || !Runtime->TryGetStringField(TEXT("look_operation_id"),Operation)
                    || Operation.IsEmpty() || Operation.Len()>128) return false;
            }
            return true;
        }

        double NormalizeYaw(double Value)
        {
            double Result = FMath::Fmod(Value, 360.0);
            if (Result > MaxYawDegrees) Result -= 360.0;
            if (Result < MinYawDegrees) Result += 360.0;
            return Result;
        }
    }

    bool StartAtYaw(const TSharedRef<FJsonObject>& Runtime, double TargetYaw,
        const FString& OperationId, FString& Error)
    {
        Error.Empty();
        if (OperationId.IsEmpty() || OperationId.Len() > 128 || !FMath::IsFinite(TargetYaw))
        {
            Error = TEXT("look operation or yaw is invalid");
            return false;
        }

        bool bTurnPending = false, bFollowupCredit = false, bFollowupPending = false;
        double PersistedTarget = 0.0;
        if (!ReadPersistedState(Runtime, bTurnPending, bFollowupCredit, bFollowupPending, PersistedTarget))
        {
            Error = TEXT("persisted look state is invalid");
            return false;
        }
        if (bTurnPending)
        {
            Error = TEXT("look turn is already pending");
            return false;
        }

        FString ExistingOperation;
        if (Runtime->HasField(TEXT("look_operation_id"))
            && !Runtime->TryGetStringField(TEXT("look_operation_id"), ExistingOperation))
        {
            Error = TEXT("persisted look operation is invalid");
            return false;
        }
        if (ExistingOperation == OperationId && !ExistingOperation.IsEmpty()) return true;

        Runtime->SetStringField(TEXT("look_operation_id"), OperationId);
        Runtime->SetNumberField(TEXT("look_target_yaw_degrees"), NormalizeYaw(TargetYaw));
        Runtime->SetBoolField(TEXT("look_turn_pending"), true);
        Runtime->SetBoolField(TEXT("look_attention_only"), false);
        Runtime->RemoveField(TEXT("look_target_pitch_degrees"));
        return true;
    }

    bool StartAtAttention(const TSharedRef<FJsonObject>& Runtime, double TargetYaw,
        double TargetPitch, const FString& OperationId, FString& Error)
    {
        if (!FMath::IsFinite(TargetPitch) || TargetPitch < -89.0 || TargetPitch > 89.0)
        {
            Error = TEXT("look attention pitch is invalid");
            return false;
        }
        FString ExistingOperation;
        bool bTurnPending = false;
        if (Runtime->HasField(TEXT("look_operation_id")) && Runtime->TryGetStringField(TEXT("look_operation_id"), ExistingOperation)
            && ExistingOperation == OperationId && Runtime->TryGetBoolField(TEXT("look_turn_pending"), bTurnPending) && !bTurnPending)
            return true;
        if (!StartAtYaw(Runtime, TargetYaw, OperationId, Error)) return false;
        Runtime->SetBoolField(TEXT("look_attention_only"), true);
        Runtime->SetNumberField(TEXT("look_target_pitch_degrees"), TargetPitch);
        return true;
    }

    bool Start(const TSharedRef<FJsonObject>& Runtime, double CurrentYaw,
        const FString& OperationId, FString& Error)
    {
        Error.Empty();
        if (!FMath::IsFinite(CurrentYaw))
        {
            Error = TEXT("look operation or yaw is invalid");
            return false;
        }
        return StartAtYaw(Runtime, CurrentYaw + LookDeltaDegrees, OperationId, Error);
    }

    bool HasFollowup(const TSharedRef<FJsonObject>& Runtime)
    {
        bool bTurnPending = false, bFollowupCredit = false, bFollowupPending = false;
        double PersistedTarget = 0.0;
        if (!ReadPersistedState(Runtime, bTurnPending, bFollowupCredit, bFollowupPending, PersistedTarget)) return false;
        return !bTurnPending && bFollowupCredit && bFollowupPending;
    }

    void OnDispatch(const TSharedRef<FJsonObject>& Runtime, bool bIndependentDecision)
    {
        bool bTurnPending = false, bFollowupCredit = false, bFollowupPending = false;
        double PersistedTarget = 0.0;
        if (!ReadPersistedState(Runtime, bTurnPending, bFollowupCredit, bFollowupPending, PersistedTarget)) return;
        Runtime->SetBoolField(TEXT("look_followup_credit"), bIndependentDecision);
        Runtime->SetBoolField(TEXT("look_followup_pending"), false);
    }

    void Complete(const TSharedRef<FJsonObject>& Runtime)
    {
        bool bTurnPending = false, bFollowupCredit = false, bFollowupPending = false;
        double PersistedTarget = 0.0;
        if (!ReadPersistedState(Runtime, bTurnPending, bFollowupCredit, bFollowupPending, PersistedTarget)
            || !bTurnPending) return;
        Runtime->SetBoolField(TEXT("look_turn_pending"), false);
        Runtime->SetBoolField(TEXT("look_followup_pending"), bFollowupCredit);
    }
}
