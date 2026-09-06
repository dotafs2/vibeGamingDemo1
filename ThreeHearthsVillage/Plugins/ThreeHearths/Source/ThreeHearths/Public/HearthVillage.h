#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "HearthVillage.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UAnimSequence;
class UCameraComponent;
class IHttpRequest;
class FJsonObject;
class IFileHandle;

UENUM(BlueprintType)
enum class EHearthTask : uint8 { Choosing, ToWood, Chopping, ToHome, Delivering, Building, Settled, LifeChoosing, LifeTravel, LifeActivity, ProductionTravel, ProductionWork, ProductionDeliver, ProductionDeposit };

// Stable operation IDs: each site offers only the operations its current state permits.
enum class EHearthSiteKind : uint8 { Empty, Land, Corn, Wheat, Lettuce, Pumpkin, House, Tree, Shrub, Stone };
struct FHearthSite
{
    FString StableId;
    EHearthSiteKind Kind=EHearthSiteKind::Empty;
    FVector Position=FVector::ZeroVector, Approach=FVector::ZeroVector;
    float Radius=270.f, Growth=0.f, GrowDuration=120.f, Progress=0.f;
    int32 Stage=0, Units=0, Capacity=0, ReservedBy=-1, Owner=-1, VisualStage=-99;
    bool bReachable=true, bExpansion=false;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Meshes;
    TWeakObjectPtr<UStaticMeshComponent> Soil;
};

struct FHearthDecisionRecord
{
    FString Run, Timestamp, Kind, Context, Choice, Reason, Result, Source, Model;
    FString Status = TEXT("thinking");
    int32 Resident = -1, Tokens = 0;
    float At = 0.f;
    double Latency = 0;
    bool bHasUsage = false;
};

// Each resident owns one request slot; replies never share mutable decision data.
struct FHearthPendingDecision
{
    FString OperationId, ConversationId;
    TSharedPtr<IHttpRequest,ESPMode::ThreadSafe> Request;
    uint64 Serial=0;
    bool bActive=false, bReturned=false, bLife=false, bSocial=false, bHasUsage=false;
    int32 Choice=-1, Tokens=0, HistoryIndex=-1;
    double StartedAt=0, Latency=0;
    FString Reason, Error;
    TArray<int32> AllowedActions;
};

struct FHearthBond
{
    float Affinity=0, Trust=50;
    int32 Meetings=0;
    FString Memory;
};
struct FHearthDialogueLine
{
    int32 Speaker=-1, Intent=0;
    float At=0;
    FString Text, Source;
};
struct FHearthConversation
{
    FString Id, FirstId, SecondId, Outcome;
    int32 First=-1, Second=-1, Speaker=-1;
    int32 Offer=-1, Proposer=-1, OfferAction=-1;
    bool bMet=false, bClosed=false, bAccepted=false;
    float TravelTime=0, TurnDelay=0;
    TArray<FHearthDialogueLine> Lines;
};
struct FHearthCommitment
{
    FString Id, ConversationId, TaskId, Status=TEXT("promised"), Result;
    int32 Worker=-1, Beneficiary=-1, Action=-1;
};

UCLASS()
class THREEHEARTHS_API AHearthVillager : public AActor
{
    GENERATED_BODY()
public:
    AHearthVillager();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Hat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SelectionDisc;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bundle;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> AppearanceParts;
    UFUNCTION(BlueprintCallable) void ConfigureAppearance(int32 Profile);
    void SetMotion(EHearthTask Task, float Rate, int32 WorkKind=-1);
    int32 ResidentIndex = -1;
private:
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TObjectPtr<UAnimSequence> Chop;
    UPROPERTY() TObjectPtr<UAnimSequence> Build;
    UPROPERTY() TObjectPtr<UAnimSequence> Farm;
    UPROPERTY() TObjectPtr<UAnimSequence> Mine;
    UPROPERTY() TObjectPtr<UAnimSequence> Gather;
    int32 LastWorkKind=-99;
    EHearthTask LastMotion = EHearthTask::Settled;
};

USTRUCT(BlueprintType)
struct FHearthResident
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString StableId;
    UPROPERTY(BlueprintReadOnly) FString ActiveTaskId;
    UPROPERTY(BlueprintReadOnly) FString Role;
    UPROPERTY(BlueprintReadOnly) float Hunger=15.f;
    UPROPERTY(BlueprintReadOnly) float Mood=65.f;
    UPROPERTY(BlueprintReadOnly) int32 Age=30;
    UPROPERTY(BlueprintReadOnly) bool bKing=false;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Personality;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) FString LatestEvent;
    UPROPERTY(BlueprintReadOnly) FString DecisionSource = TEXT("pending");
    UPROPERTY(BlueprintReadOnly) FString DecisionNote;
    UPROPERTY(BlueprintReadOnly) EHearthTask Task = EHearthTask::Choosing;
    UPROPERTY(BlueprintReadOnly) int32 Plot = -1;
    UPROPERTY(BlueprintReadOnly) int32 CarriedWood = 0;
    UPROPERTY(BlueprintReadOnly) int32 DeliveredWood = 0;
    UPROPERTY(BlueprintReadOnly) float BuildProgress = 0.f;
    UPROPERTY(BlueprintReadOnly) float Energy = 75.f;
    UPROPERTY(BlueprintReadOnly) float SocialNeed = 30.f;
    UPROPERTY() TObjectPtr<AHearthVillager> Actor;
    int32 Source = -1;
    int32 Trips = 0;
    float Timer = 0.f;
    float MoveSpeed = 240.f;
    float MoveRetry = 0.f;
    bool bMovementBlocked = false;
    double NextLifeDecision = 0;
    TArray<FVector> Route;
    int32 HistoryIndex = -1;
    int32 LifeAction = -1;
    int32 ProductionSite = -1, ProductionOp = -1;
    int32 CargoType = -1, CargoAmount = 0;
    float WorkDuration = 0.f;
    FString ConversationId, Speech;
    float SpeechRemaining=0;
    TMap<FString,FHearthBond> Bonds;
};

UCLASS()
class THREEHEARTHS_API AHearthVillage : public AActor
{
    GENERATED_BODY()
public:
    AHearthVillage();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) void RestartVillage();
    UFUNCTION(BlueprintCallable) bool SaveWorld();
    UFUNCTION(BlueprintCallable) bool LoadWorld();
    UFUNCTION(BlueprintCallable) FString ExportWorldState() const;
    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FString WorldSaveStatus;
    UFUNCTION(BlueprintCallable) void TogglePause();
    UFUNCTION(BlueprintCallable) void CycleSpeed();
    UFUNCTION(BlueprintCallable) void SetSimulationSpeed(float Speed);
    UFUNCTION(BlueprintCallable) void SelectResident(int32 Index);
    UFUNCTION(BlueprintCallable) FString GetSnapshot() const;
    UFUNCTION(BlueprintCallable) FString GetDecisionHistory(int32 Index = -1) const;
    UFUNCTION(BlueprintCallable) FString GetSocialState(int32 Index = -1) const;
    TArray<FHearthConversation> Conversations;
    TArray<FHearthCommitment> Commitments;
    bool bSocialOpen=false;
    int32 SocialRevision=0;
    FString RelationshipSummary(int32 Index) const;
    bool IsSociallyAvailable(int32 Index) const;
    UFUNCTION(BlueprintCallable) void ToggleAutonomy();
    UPROPERTY(BlueprintReadOnly) bool bAutonomousLifeEnabled = true;
    bool bHistoryOpen = false;
    TArray<FHearthDecisionRecord> DecisionHistory;
    int32 HistoryRevision = 0;
    FString HistorySaveStatus;
    FString CurrentRun;
    int32 HistoryCount(int32 Index) const;
    FString LifeSummary() const;
    UFUNCTION(BlueprintCallable) FString GetAvailableActivities(int32 Index) const;
    UFUNCTION(BlueprintCallable) bool AssignActivity(int32 Index, int32 Action);
    UFUNCTION(BlueprintCallable) FString GetProductionState() const;
    FString ProductionSummary() const;
    FString CargoSummary(int32 Index) const;
    bool CanAssignActivity(int32 Index) const;
    TArray<int32> AvailableLifeActions(int32 Index) const;
    FString LifeActionName(int32 Index, int32 Action) const;
    UPROPERTY(BlueprintReadOnly) int32 FoodStock=30;
    UPROPERTY(BlueprintReadOnly) int32 StoneStock=0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Village") bool bUseCropoutMap = false;
    UPROPERTY(BlueprintReadOnly) TArray<FHearthResident> Residents;
    UPROPERTY(BlueprintReadOnly) int32 SelectedResident = 0;
    UPROPERTY(BlueprintReadOnly) bool bSimulationPaused = false;
    UPROPERTY(BlueprintReadOnly) float SimulationSpeed = 1.f;
    UPROPERTY(BlueprintReadOnly) float Elapsed = 0.f;
    UPROPERTY(BlueprintReadOnly) FString VillageEvent;
    UPROPERTY(BlueprintReadOnly) FString ApiStatus;
    UPROPERTY(BlueprintReadOnly) int32 ApiRequests = 0;
    UPROPERTY(BlueprintReadOnly) int32 ApiSuccesses = 0;
    UPROPERTY(BlueprintReadOnly) int32 ApiTokens = 0;
    FString ApiSummary() const;
    FString DecisionLabel(int32 Index) const;
    int32 CompletedHomes() const;
    int32 AvailableWood() const;
    int32 CostFor(int32 Resident) const;
    FString PlotNameFor(int32 Resident) const;
    FString StatusFor(int32 Resident) const;
    FLinearColor ResidentColor(int32 Index) const;
    int32 HousingPlotCount() const { return bUseCropoutMap?10:3; }
    FString PlotLabel(int32 Plot) const;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HouseMeshes;
    friend class FHearthMovementIntegrationTest;
    friend class FHearthParallelCapacityTest;
    friend class FHearthWorldPersistenceTest;
    friend class FHearthWorldRecoveryTest;
    friend class FHearthSocietyPopulationTest;
    friend class FHearthSocialIntegrationTest;
    FString WorldPath;
    FString PlotIds[10];
    TSharedPtr<IFileHandle> WorldLease;
    int64 WorldRevision=0;
    float WorldSaveTimer=0;
    bool bWorldPersistenceEnabled=false, bWorldWriteBlocked=false;
    void InitializeWorldPersistence();
    void ResetVillageState();
    bool ApplyWorldState(const FString& Text, FString& Error);
    void InitializeResidentIdentity(int32 Index,FHearthResident& Resident) const;
    void AdvanceNeeds(float Dt);
    bool MigrateWorldPopulation(struct FHearthWorldImage& Image,FString& Error) const;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StockMeshes;
    UPROPERTY() TObjectPtr<UMaterialInterface> TintMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ProductionMeshes;
    TArray<FHearthSite> ProductionSites;
    TSet<FIntPoint> LandGrid;
    TArray<FVector> FixedObstacles;
    TMap<FString,int32> ProductionTotals;
    int32 Produced[3]={0,0,0}, Spent[3]={0,0,0}; // Food, Wood, Stone.
    FString ProductionStatus;
    void InitializeProduction();
    void AdvanceProductionWorld(float Dt);
    void AdvanceProduction(int32 Index,float Dt);
    void RefreshProductionVisuals();
    void UpdateSiteVisual(int32 Site);
    TArray<int32> AvailableProductionActions(int32 Index) const;
    bool IsProductionAllowed(int32 Index,int32 Action) const;
    FString ProductionActionName(int32 Action) const;
    int32 ChooseProductionLocally(int32 Index) const;
    bool StartProduction(int32 Index,int32 Action,const FString& Reason,bool bFromApi);
    void FinishProduction(int32 Index,const FString& Result);
    void AppendProductionContext(const TSharedRef<FJsonObject>& Context) const;
    bool IsLand(const FVector& Position) const;
    bool IsClearPoint(const FVector& Position) const;
    bool IsClearSegment(const FVector& A,const FVector& B) const;
    bool FindProductionPath(const FVector& Start,const FVector& End,TArray<FVector>& Out) const;
    void BuildLandGrid();
    bool ChooseSiteApproach(int32 Site);
    bool FindActivityRoute(int32 Index,const FVector& Target,TArray<FVector>& Route) const;
    FVector PlotPositions[10];
    FVector WoodPositions[3];
    int32 PlotOwners[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    int32 WoodStock[3] = {12,12,12};
    int32 PlotCosts[10] = {12,9,6,6,6,6,6,6,6,6};
    float SnapshotTimer = 0.f;
    double SimulationRemainder = 0;
    void AdvanceSimulation(float Dt);
    bool bReportedComplete = false;
    bool bApiReady = false;
    bool bApiConfigured = false;
    bool bApiDisabledThisRun = false;
    bool bHasApiUsage = false;
    bool bApiBudgeted = false;
    FString ApiBudgetLedger;
    double ApiBudgetSpent = 0, ApiBudgetReserved = 0, ApiBudgetRemaining = 0;
    float LifeDecisionInterval = 6.f;
    int32 LastLifeResident = -1;
    FString HistoryPath;
    FString ApiBackend;
    FString ApiEndpoint;
    FString ApiKey;
    FString ApiModel;
    FString ApiTokenField = TEXT("max_completion_tokens");
    FString ApiFormat = TEXT("json_object");
    FString ApiThinkingMode;
    float ApiTimeout = 30.f;
    int32 ApiMaxTokens = 256;
    int32 ApiMaxRequests = 600;
    uint64 DecisionGeneration = 0;
    uint64 DecisionSerial = 0;
    TArray<FHearthPendingDecision> PendingDecisions;
    bool IsDecisionPending(int32 Index) const;
    int32 PendingDecisionCount() const;
    int32 DecisionConcurrencyLimit() const;
    bool HasDecisionCapacity(int32 Index) const;
    FProcHandle BridgeProcess;
    void LoadApiConfig();
    void StopDecisionRequests();
    void RequestDecision(int32 Index);
    void SendDecisionRequest(int32 Index, const TSharedRef<FJsonObject>& Context, const FString& Prompt, bool bLife, bool bSocial=false);
    void ConsumeDecision();
    void DecideLocally(int32 Index, const FString& Failure = FString());
    bool ReservePlot(int32 Index, int32 Plot, const FString& Reason, bool bFromApi);
    void LoadHistory();
    void SaveHistory();
    void CloseHistoryRun(const FString& Result);
    void StartHistory(int32 Index, bool bLife, const FString& Source);
    void AcceptHistory(int32 Index, const FString& Choice, const FString& Reason, const FString& Source);
    void CompleteHistory(int32 Index, const FString& Result);
    void UpdateLifeDecisions();
    void RequestLifeDecision(int32 Index);
    void DecideLifeLocally(int32 Index, const FString& Failure = FString());
    bool StartLifeAction(int32 Index, int32 Action, const FString& Reason, bool bFromApi);
    void AdvanceLife(int32 Index, float Dt);
    bool BeginConversation(int32 Index,int32 Other,const FString& Reason,bool bFromApi);
    void AdvanceSocial(float RealDt);
    TArray<int32> AvailableSocialIntents(int32 Index) const;
    bool ResolveSocialTurn(int32 Index,int32 Intent,const FString& Words,const FString& Source);
    void DecideSocialLocally(int32 Index,const FString& Failure=FString());
    void RequestSocialDecision(int32 Index);
    void CloseConversation(FHearthConversation& Conversation,const FString& Outcome);
    void CompleteCommitments(int32 Worker,bool bSuccess,const FString& Result);
    int32 FindHelpActivity(int32 Worker) const;
    void BuildEnvironment();
    void BuildIslandVillage();
    void Decide(int32 Index);
    void SeekWood(int32 Index);
    void SetRoute(int32 Index, const FVector& Target);
    bool MoveResident(int32 Index, float Dt);
    bool TryYieldFor(int32 Walker);
    void SetHouseStage(int32 Plot, int32 Stage);
    void WriteSnapshot() const;
    UStaticMeshComponent* AddMesh(const FString& MeshPath, const FVector& Position, const FVector& Scale, const FLinearColor* Color = nullptr);
};

UCLASS()
class THREEHEARTHS_API AHearthPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AHearthPlayerController();
    UFUNCTION(BlueprintCallable) void ShowIsland();
    UFUNCTION(BlueprintCallable) void FocusResident();
    UFUNCTION(BlueprintCallable) void PanCamera(float Right, float Forward);
    UFUNCTION(BlueprintCallable) void ZoomCamera(float Factor);
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<AHearthVillage> Village;
    TSharedPtr<class SWidget> VillageUI;
    float CameraBaseFov = 52.f;
    FVector CameraCenter = FVector::ZeroVector;
    FVector CameraOffset = FVector(-2300,-2800,3300);
    float CameraZoom = 1.f;
    bool bIslandCamera = false;
    void UpdateCamera();
};

UCLASS()
class THREEHEARTHS_API AHearthGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHearthGameMode();
};
