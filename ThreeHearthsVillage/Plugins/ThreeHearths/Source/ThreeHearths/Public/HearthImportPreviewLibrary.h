#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HearthImportPreviewLibrary.generated.h"

class AStaticMeshActor;
class UStaticMesh;
class UStaticMeshComponent;

/** A narrow Python bridge: preview visuals may only be spawned inside PIE. */
UCLASS()
class THREEHEARTHS_API UHearthImportPreviewLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Three Hearths|Import Preview")
    static AStaticMeshActor* SpawnPreview(UStaticMeshComponent* Original, UStaticMesh* Mesh);
};
