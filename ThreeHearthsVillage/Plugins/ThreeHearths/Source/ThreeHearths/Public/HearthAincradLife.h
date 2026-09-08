#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

namespace HearthAincradLife
{
    /**
     * Installs the independent life extension on an otherwise valid world.
     * The operation is idempotent; callers should create their migration backup
     * before calling this function when the extension is absent.
     */
    THREEHEARTHS_API bool Initialize(const TSharedRef<FJsonObject>& World, bool& bAdded, FString& Error);

    /** Validates the optional life extension. An absent extension is legacy-valid. */
    THREEHEARTHS_API bool Validate(const TSharedRef<FJsonObject>& World, FString& Error);

    /** Returns model-selectable legal options for one resident. */
    THREEHEARTHS_API TArray<TSharedPtr<FJsonValue>> Options(const TSharedRef<FJsonObject>& World, const FString& ResidentId);

    /** Returns only the resident's own assets, skills, contracts and directed context. */
    THREEHEARTHS_API TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& World, const FString& ResidentId);

    /** Applies one already-runtime-validated life operation transactionally. */
    THREEHEARTHS_API bool Apply(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& OptionId, const FString& OperationId, const FString& Utterance, FString& Error);
}
