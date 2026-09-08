#pragma once

#include "CoreMinimal.h"

/** Read-only presentation of the persisted cart; visuals never mutate stock. */
struct THREEHEARTHS_API FHearthFreightVisualState
{
    bool bEnabled=false, bMoving=false;
    FVector CartPosition=FVector::ZeroVector;
    float CartYaw=0.f, SimulationRate=1.f;
    double WheelDistanceCm=0.0, ElapsedSeconds=0.0;
    int32 CargoType=-1, CargoQuantity=0;
};
