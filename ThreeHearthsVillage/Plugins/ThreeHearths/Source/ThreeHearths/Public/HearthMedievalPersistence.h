#pragma once

#include "CoreMinimal.h"
#include "HearthMedievalSociety.h"

namespace HearthMedievalPersistence
{
    /** Encode a validated medieval society state as schema_version=1 JSON. */
    THREEHEARTHS_API bool Serialize(const FMedievalSocietyState& State, FString& OutJson, FString* OutError = nullptr);

    /** Parse into a temporary state and replace OutState only after every check succeeds. */
    THREEHEARTHS_API bool Deserialize(const FString& Json, FMedievalSocietyState& OutState, FString& OutError);
}
