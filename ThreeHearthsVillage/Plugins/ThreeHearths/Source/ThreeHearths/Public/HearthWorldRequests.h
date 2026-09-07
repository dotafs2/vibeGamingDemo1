#pragma once

#include "CoreMinimal.h"
#include "HearthWorldRequests.generated.h"

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthResidentAssetContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ResidentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Personality;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString InnerStory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DesignGoal;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthAssetNeedContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Purpose;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHearthResidentAssetContext> ResidentContexts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetPositionCm=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasTargetPosition=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ObservationId;
};

USTRUCT(BlueprintType)
struct THREEHEARTHS_API FHearthWorldRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Category;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Status;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Summary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Resolution;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> RequesterIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasAssetContext=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FHearthAssetNeedContext AssetContext;
};

namespace HearthWorldRequests
{
    constexpr int32 MaxRequests = 64;

    THREEHEARTHS_API bool Submit(TArray<FHearthWorldRequest>& Requests,
        const FString& ResidentId, const FString& Need, FString& Error);
    THREEHEARTHS_API bool SubmitAsset(TArray<FHearthWorldRequest>& Requests,
        const FString& ResidentId, const FHearthAssetNeedContext& Context, FString& Error);
    THREEHEARTHS_API bool Validate(const TArray<FHearthWorldRequest>& Requests, FString& Error);
    THREEHEARTHS_API void RefreshUnresolved(TArray<FHearthWorldRequest>& Requests);
}
