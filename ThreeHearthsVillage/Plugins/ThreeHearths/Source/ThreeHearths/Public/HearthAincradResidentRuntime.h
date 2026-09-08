#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HearthAincradResidentRuntime.generated.h"

class UAnimSequence;
class UCapsuleComponent;
class USkeletalMeshComponent;
class FJsonObject;
class IHttpResponse;
class UStaticMeshComponent;

/**
 * Independent Level0 resident visual.  This actor deliberately does not use
 * AHearthVillage or any of the retired medieval-society runtime classes.
 */
UCLASS()
class THREEHEARTHS_API AHearthAincradResidentVisual : public AActor
{
    GENERATED_BODY()

public:
    AHearthAincradResidentVisual();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCapsuleComponent> Capsule;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> Body;

    void SetWalking(bool bWalking);

private:
    UPROPERTY()
    TObjectPtr<UAnimSequence> Idle;

    UPROPERTY()
    TObjectPtr<UAnimSequence> Walk;

    bool bWalking = false;
};

/**
 * S1 native Level0 resident runtime.  The owning Level0 parent supplies the
 * persistent state and save operation; this actor owns only resident-local
 * movement, first-person observation, and the individually gated decision
 * requests.
 */
UCLASS()
class THREEHEARTHS_API AHearthAincradResidentRuntime : public AActor
{
    GENERATED_BODY()

public:
    AHearthAincradResidentRuntime();
    ~AHearthAincradResidentRuntime();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    /** Initializes from the shared Level0 state without taking ownership of it. */
    bool Initialize(TSharedPtr<FJsonObject> State, TFunction<bool()> SaveCallback);

    /** Queues one explicit resident-local action for parent/player integration. */
    UFUNCTION(BlueprintCallable)
    bool QueueAction(const FString& ResidentId, const FString& Action);

    /** Number of currently spawned independent resident visual actors. */
    UFUNCTION(BlueprintCallable)
    int32 ActorCount() const;

    /** Compact runtime status for parent/player inspection. */
    UFUNCTION(BlueprintCallable)
    FString Report() const;

    /** Player-facing, factual life status; contains no private resident thoughts. */
    FString LifeReport() const;

private:
    struct FResidentSlot;
    struct FBuildingRoute;
    struct FApiConfig;

    TSharedPtr<FJsonObject> PersistentState;
    TFunction<bool()> SaveCallback;
    TArray<TUniquePtr<FResidentSlot>> Slots;
    TArray<TObjectPtr<AHearthAincradResidentVisual>> Visuals;

    uint64 LifetimeGeneration = 1;
    double LastStateSaveAt = 0.0;
    bool bInitialized = false;
    FString CachedArtRevision;
    double ExerciseStartedAt = 0.0;
    uint8 ExerciseStage = 0;
    bool bExerciseRoutes = false;
    bool bLifeEnabled = false;
    bool bLifeSaveFailed = false;
    int32 DecisionLimit = 3;
    int32 DecisionsSent = 0;
    int32 DecisionWindowSeconds = 0;
    UPROPERTY() TObjectPtr<AActor> LifeToolVisual;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LifeBlade;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LifeHandle;
    UPROPERTY() TObjectPtr<AActor> LifeSuppliesVisual;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LifeSupplyPieces;

    void RebindLifeState();
    bool StartLifeAction(FResidentSlot& Slot, const FString& OptionId, const FString& Utterance, const FString& OperationId);
    bool TravelForLife(FResidentSlot& Slot, const FString& BuildingId, bool bMeeting);
    void AdvanceLife(FResidentSlot& Slot, float DeltaSeconds);
    bool CommitLife(FResidentSlot& Slot);
    void UpdateLifeVisual();
    bool HasLifeTrigger(const FResidentSlot& Slot) const;
    bool RestoreLifeRoute(FResidentSlot& Slot) const;

    void ClearRuntime(bool bCancelRequests);
    bool SpawnActiveResidents();
    bool BuildSlot(int32 ResidentIndex, const TSharedPtr<FJsonObject>& Resident,
        const TSharedPtr<FJsonObject>& Runtime, const FBuildingRoute& Building);
    bool ResolveBuilding(const FString& BuildingId, FBuildingRoute& OutBuilding) const;
    bool RestoreRoute(FResidentSlot& Slot) const;
    bool MoveSlot(FResidentSlot& Slot, float DeltaSeconds);
    bool BeginAction(FResidentSlot& Slot, const FString& Action, const FString& Source);
    bool CaptureObservation(FResidentSlot& Slot, const FString& TargetId, bool bManualBootstrap,
        const FString& Source = FString(), bool bLookInside = false);
    bool CaptureForDecision(FResidentSlot& Slot);
    void AdvanceSlot(FResidentSlot& Slot, float DeltaSeconds);
    void AdvanceObservationScheduling(FResidentSlot& Slot, double NowUtc);
    void AdvanceRouteExercise();
    void DispatchDecision(FResidentSlot& Slot);
    void HandleDecisionResponse(FResidentSlot& Slot, const TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>& Response, bool bTransportOk, uint64 RequestGeneration, uint64 RequestSerial);
    bool ReadApiConfig(FApiConfig& OutConfig) const;
    bool SaveState();
    void SavePosition(FResidentSlot& Slot);
    void MarkBlocked(FResidentSlot& Slot, const FString& Reason);
    void RecordEvent(FResidentSlot& Slot, const FString& Kind, const FString& Detail, const FString& Source);
    void RecordResult(FResidentSlot& Slot, const FString& Raw, const FString& Result, const FString& Source);
    void TrimMemory(FResidentSlot& Slot);
    void SetPhase(FResidentSlot& Slot, const FString& Phase);
    FString SceneRevision() const;
    FString WorldId() const;
    bool IsApiEnabled() const;
    double NowUtc() const;
};
