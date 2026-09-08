#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HearthFreightVisual.h"
#include "HearthHorseCart.generated.h"

class AHearthVillage;
class UAnimSequence;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** The horse/cart is a view of the world freight record, never a second economy. */
UCLASS()
class THREEHEARTHS_API AHearthHorseCart : public AActor
{
    GENERATED_BODY()
public:
    AHearthHorseCart();
    bool Configure(AHearthVillage* Village);
    void ApplyState(const FHearthFreightVisualState& State);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TWeakObjectPtr<AHearthVillage> VillageOwner;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Horse;
    UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> HorseLayers;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StaticParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WheelLayers;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PlankLayers;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BeamLayers;
    TArray<FTransform> WheelBases;
    bool bReady=false, bWasWalking=false, bReviewCaptured=false, bCargoAuditCaptured=false;
};
