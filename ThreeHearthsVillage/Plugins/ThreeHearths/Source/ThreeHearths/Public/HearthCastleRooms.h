#pragma once

#include "CoreMinimal.h"

// A lightweight room index used by the castle manifest. It carries no world
// pointers or construction state; actual work is still represented by parts.
struct THREEHEARTHS_API FHearthCastleRoom
{
    FString Id;
    FString Label;
    FString Purpose;
    FVector Center = FVector::ZeroVector;
    FVector Entry = FVector::ZeroVector;
    FVector2D Size = FVector2D::ZeroVector;
    int32 Floor = 0;
    bool bCourtyard = false;
    bool bWalkable = true;
};

namespace HearthCastleRooms
{
    THREEHEARTHS_API TArray<FHearthCastleRoom> BuildV2();
}
