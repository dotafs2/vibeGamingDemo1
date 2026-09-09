#pragma once

#include "CoreMinimal.h"

class AHearthAincradLevel;

namespace HearthAincradCharacterReview
{
    /** Writes isolated local-verification character frames once when every safety flag is present. */
    bool TryCapture(AHearthAincradLevel* Owner, const FString& VerificationWorldPath);
}
