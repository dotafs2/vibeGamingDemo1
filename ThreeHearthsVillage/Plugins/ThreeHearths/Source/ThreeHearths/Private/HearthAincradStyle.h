#pragma once
#include "CoreMinimal.h"
class AActor;
class AHearthVillage;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
namespace HearthAincradStyle
{
    void ApplyToMesh(UStaticMeshComponent* Mesh);
    void ConfigureWorld(AHearthVillage& Village,AActor& TerrainOwner);
    void ConfigureCapture(USceneCaptureComponent2D* Capture);
    void ExportViews(AHearthVillage& Village);
}
