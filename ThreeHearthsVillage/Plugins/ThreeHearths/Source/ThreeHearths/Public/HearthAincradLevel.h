#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "HearthAincradLevel.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class FJsonObject;
class AHearthAincradResidentRuntime;

/** Independent SAO first-floor blockout. Never loads the medieval world. */
UCLASS()
class THREEHEARTHS_API AHearthAincradLevel : public AActor
{
    GENERATED_BODY()
public:
    AHearthAincradLevel();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(CallInEditor, BlueprintCallable) void BuildPreview();
    void SetView(int32 Mode);
    void CaptureViews();
    FString WorldId() const;
    int32 ViewMode=2;
    int32 HouseCount=0;
    int32 TreeCount=0;
private:
    UPROPERTY() TArray<TObjectPtr<UActorComponent>> Generated;
    UPROPERTY() TMap<FString,TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    TSharedPtr<FJsonObject> PersistentState;
    FString StatePath;
    double StartedAt=0;
    double StartElapsed=0;
    double LastSaved=0;
    bool bCaptured=false;
    bool bWalkVerified=false;
    bool bFailed=false;
    int32 DurationSeconds=0;
    TArray<FVector> RoofVertices[2];
    TArray<int32> RoofIndices[2];
    TArray<FVector2D> RoofUVs[2];
    UInstancedStaticMeshComponent* Batch(const FString& Shape,const FString& Material);
    void Box(const FVector& Center,const FVector& Size,float Yaw,const FString& Material);
    void Shape(const FString& Mesh,const FVector& Center,const FVector& Size,float Yaw,const FString& Material);
    void House(const FVector2D& Position,float Yaw,int32 Seed);
    void Road(const TArray<FVector2D>& Points,float Width);
    void Disc(const FVector2D& Center,float Radius,float Z,const FString& Material);
    bool SaveState();
    UPROPERTY() TObjectPtr<AHearthAincradResidentRuntime> ResidentRuntime;
};

UCLASS()
class THREEHEARTHS_API AHearthAincradExplorer : public APawn
{
    GENERATED_BODY()
public:
    AHearthAincradExplorer();
    virtual void Tick(float DeltaSeconds) override;
    void SetWalking(bool bEnable);
    void WalkDisplacement(const FVector& Delta);
    bool IsWalking() const { return bWalking; }
    UCapsuleComponent* WalkingCapsule() const { return Capsule; }
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    float Speed=6000.f;
private:
    UPROPERTY() TObjectPtr<UCapsuleComponent> Capsule;
    bool bWalking=false;
    float WalkPitch=0.f;
};

UCLASS()
class THREEHEARTHS_API AHearthAincradHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

UCLASS()
class THREEHEARTHS_API AHearthAincradGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHearthAincradGameMode();
};
