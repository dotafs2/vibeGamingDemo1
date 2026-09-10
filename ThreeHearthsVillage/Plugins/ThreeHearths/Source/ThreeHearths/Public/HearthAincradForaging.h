#pragma once
#include "CoreMinimal.h"
class FJsonObject;
class FJsonValue;
namespace HearthAincradForaging
{
    THREEHEARTHS_API FVector WorkPoint();
    THREEHEARTHS_API FVector VisiblePoint();
    THREEHEARTHS_API bool Initialize(const TSharedRef<FJsonObject>& World, bool& bAdded, FString& Error);
    THREEHEARTHS_API bool Validate(const TSharedRef<FJsonObject>& World, FString& Error);
    THREEHEARTHS_API bool Tick(const TSharedRef<FJsonObject>& World, double DeltaSeconds, FString& Error);
    THREEHEARTHS_API TArray<TSharedPtr<FJsonValue>> Options(const TSharedRef<FJsonObject>& World, const FString& ResidentId);
    THREEHEARTHS_API bool Apply(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& OptionId, FString& Error);
    THREEHEARTHS_API TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& World, const FString& ResidentId);
}
