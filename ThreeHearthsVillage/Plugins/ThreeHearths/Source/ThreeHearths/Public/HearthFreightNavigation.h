#pragma once

#include "CoreMinimal.h"

/**
 * Small, deterministic, forward-only planner for a wide freight vehicle.
 *
 * The planner deliberately knows nothing about the world or vehicle footprint.
 * The callback owns those decisions: it is called for the start pose and for
 * every <=50 cm sample of every candidate primitive.  A caller can therefore
 * reject a pose using the actual organic ground, static buildings, dynamic
 * residents, and the vehicle's complete swept footprint.
 */
namespace HearthFreightNavigation
{
    struct FPose
    {
        FVector Position = FVector::ZeroVector;
        float Yaw = 0.f;
    };

    /**
     * Plan a continuous forward-only route.  The first output pose is Start;
     * the final pose is within 150 cm of Goal.  Yaw is in UE degrees.
     *
     * The search is bounded by MaxNodes expansions.  It never falls back to a
     * straight line, in-place turn, lateral motion, reverse motion, or a
     * teleport when no valid route exists.  The implied travel speed is 60
     * cm/s; this is exposed as a contract for callers, not used as a cost.
     */
    THREEHEARTHS_API bool Plan(
        FPose Start,
        FVector Goal,
        TFunctionRef<bool(const FPose&)> IsPoseClear,
        TArray<FPose>& Out,
        int32 MaxNodes = 6000);
}
