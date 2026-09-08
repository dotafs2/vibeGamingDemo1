#pragma once
#include "CoreMinimal.h"
class AActor;
class UActorComponent;
namespace HearthAincradTownVisuals
{
    /** Rebuild only components owned by the Level0 preview; returns building count. */
    int32 Build(AActor* Owner,TArray<TObjectPtr<UActorComponent>>& Generated);
}
