#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace HearthAincradLook
{
    /** Starts one persisted, local look turn. No request or world mutation occurs. */
    THREEHEARTHS_API bool Start(const TSharedRef<FJsonObject>& Runtime, double CurrentYaw,
        const FString& OperationId, FString& Error);

    /** Starts a persisted look turn at an explicit target yaw in degrees. */
    THREEHEARTHS_API bool StartAtYaw(const TSharedRef<FJsonObject>& Runtime, double TargetYaw,
        const FString& OperationId, FString& Error);

    /** Starts a persisted attention-only turn: the body remains still while the camera aims at a target. */
    THREEHEARTHS_API bool StartAtAttention(const TSharedRef<FJsonObject>& Runtime, double TargetYaw,
        double TargetPitch, const FString& OperationId, FString& Error);

    /** Returns true only when a completed turn has one eligible follow-up. */
    THREEHEARTHS_API bool HasFollowup(const TSharedRef<FJsonObject>& Runtime);

    /** Records which kind of decision is being dispatched and consumes old follow-up state. */
    THREEHEARTHS_API void OnDispatch(const TSharedRef<FJsonObject>& Runtime, bool bIndependentDecision);

    /** Completes the pending turn without granting a new credit. */
    THREEHEARTHS_API void Complete(const TSharedRef<FJsonObject>& Runtime);
}
