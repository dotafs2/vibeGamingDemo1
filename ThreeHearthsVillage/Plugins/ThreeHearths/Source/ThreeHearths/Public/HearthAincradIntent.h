#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/** Small, resident-owned intent journal. It records self reports only; it never
 * turns a thought into an authoritative world fact or installs a capability. */
namespace HearthAincradIntent
{
    THREEHEARTHS_API bool RecordDecision(const TSharedRef<FJsonObject>& Runtime,
        const FString& OperationId, const FString& Goal, const FString& Need,
        bool bRequestChange, double NowUtc, FString& Error);

    /** Returns bounded, private-to-the-resident intent data. Read-only. */
    THREEHEARTHS_API TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& Runtime);
}
