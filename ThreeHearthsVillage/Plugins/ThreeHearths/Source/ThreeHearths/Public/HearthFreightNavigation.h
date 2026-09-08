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

    struct FFootprintBox { float X, Y, HalfX, HalfY; };
    THREEHEARTHS_API TConstArrayView<FFootprintBox> Footprint();

    /** Static occupied rectangle, including logical floor/site proxies. */
    struct FRecoveryObstacle
    {
        FVector Center = FVector::ZeroVector;
        FVector2D HalfSize = FVector2D::ZeroVector;
        float Yaw = 0.f;
        float ExtraMargin = 0.f;
    };

    /** Fixed-heading reverse only. Checks the entire swept compound footprint:
     * no newly occupied space inside any obstacle, and strictly less total
     * overlap if the start overlaps. Terrain/people remain the caller's veto.
     */
    THREEHEARTHS_API bool CanReverseStep(const FPose& From, const FPose& To,
        TConstArrayView<FRecoveryObstacle> Obstacles);

    /** <=900 cm retreat, <=10 cm samples, at most six forward connection
     * attempts. Out includes Start, the reverse prefix and the forward tail.
     * Failure leaves Out empty; Plan's forward-only contract is unchanged.
     */
    THREEHEARTHS_API bool PlanRecovery(FPose Start,
        TFunctionRef<bool(const FPose&, const FPose&)> CanReverse,
        TFunctionRef<bool(const FPose&, TArray<FPose>&)> ConnectForward,
        TArray<FPose>& Out, float MaxDistance = 900.f);

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
