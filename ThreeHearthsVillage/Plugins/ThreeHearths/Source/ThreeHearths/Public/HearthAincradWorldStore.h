#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace HearthAincradWorldStore
{
    /**
     * Loads a validated SAO Aincrad Floor 1 state, or creates the initial
     * 13-resident state when Path does not exist. Existing malformed or
     * wrong-profile files are errors and are never replaced with defaults.
     */
    THREEHEARTHS_API bool LoadOrCreate(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error);

    /** Explicitly migrates an existing validated v1 state to v2. A v2 state is an idempotent no-op. */
    THREEHEARTHS_API bool MigrateV1ToV2(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error);

    /** Validates and atomically saves a v2 SAO Aincrad Floor 1 state as UTF-8 JSON. */
    THREEHEARTHS_API bool Save(const FString& Path, const TSharedRef<FJsonObject>& State, FString& Error);
}
