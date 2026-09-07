#pragma once
#include "CoreMinimal.h"
#include "HearthWorldRequests.h"
#include "Dom/JsonValue.h"

namespace HearthWorldRequestJson
{
    TArray<TSharedPtr<FJsonValue>> Encode(const TArray<FHearthWorldRequest>& Requests);
    bool Decode(const TArray<TSharedPtr<FJsonValue>>& Values,TArray<FHearthWorldRequest>& Out,FString& Error);
}
