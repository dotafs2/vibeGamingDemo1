#include "HearthVillage.h"
#include "HearthHorseCart.h"
#include "EngineUtils.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace HearthMedievalVisuals
{
    namespace
    {
        constexpr TCHAR VisualTag[] = TEXT("HearthMedievalVisuals");
        constexpr TCHAR CatalogRelativePath[] = TEXT("ThreeHearths/Data/MedievalLifeCatalog.json");
        struct FLayerMesh
        {
            FString Layer;
            FString Path;
        };

        struct FAssemblyPiece
        {
            FString Key;
            FString ParentKey;
            FString Module;
            FVector PositionM = FVector::ZeroVector;
            FRotator Rotation = FRotator::ZeroRotator;
            FVector Scale = FVector::OneVector;
        };

        struct FCatalog
        {
            TMap<FString, TArray<FLayerMesh>> ModuleMeshes;
            TMap<FString, TArray<FAssemblyPiece>> Assemblies;
        };

        static bool ReadCatalog(FCatalog& Out)
        {
            FString Text;
            const FString Path = FPaths::ProjectContentDir() / CatalogRelativePath;
            if (!FFileHelper::LoadFileToString(Text, *Path) || Text.Len() > 4 * 1024 * 1024)
                return false;
            TSharedPtr<FJsonObject> Root;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
                return false;

            const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
            if (!Root->TryGetArrayField(TEXT("assets"), Assets)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                if (!Value.IsValid() || Value->Type != EJson::Object) continue;
                const TSharedPtr<FJsonObject> Object = Value->AsObject();
                FString Id, Layer, Mesh;
                if (!Object->TryGetStringField(TEXT("id"), Id) || !Object->TryGetStringField(TEXT("layer"), Layer)
                    || !Object->TryGetStringField(TEXT("mesh"), Mesh) || Id.IsEmpty() || Layer.IsEmpty() || Mesh.IsEmpty()) continue;
                const int32 Slash = Id.Find(TEXT("/"));
                if (Slash <= 0) continue;
                FLayerMesh Entry; Entry.Layer = Layer; Entry.Path = Mesh;
                Out.ModuleMeshes.FindOrAdd(Id.Left(Slash)).Add(MoveTemp(Entry));
            }

            const TArray<TSharedPtr<FJsonValue>>* Assemblies = nullptr;
            if (!Root->TryGetArrayField(TEXT("assemblies"), Assemblies)) return false;
            for (const TSharedPtr<FJsonValue>& Value : *Assemblies)
            {
                if (!Value.IsValid() || Value->Type != EJson::Object) continue;
                const TSharedPtr<FJsonObject> Object = Value->AsObject();
                FString Id;
                const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
                if (!Object->TryGetStringField(TEXT("id"), Id) || !Object->TryGetArrayField(TEXT("pieces"), Pieces)) continue;
                TArray<FAssemblyPiece>& Parsed = Out.Assemblies.FindOrAdd(Id);
                for (const TSharedPtr<FJsonValue>& PieceValue : *Pieces)
                {
                    if (!PieceValue.IsValid() || PieceValue->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Piece = PieceValue->AsObject();
                    FAssemblyPiece Item;
                    const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
                    if (!Piece->TryGetStringField(TEXT("key"), Item.Key)
                        || !Piece->TryGetStringField(TEXT("module"), Item.Module)
                        || !Piece->TryGetArrayField(TEXT("position_m"), Position) || !Position || Position->Num() != 3)
                        continue;
                    Piece->TryGetStringField(TEXT("parent_key"), Item.ParentKey);
                    double X = 0., Y = 0., Z = 0., Yaw = 0.;
                    if (!(*Position)[0].IsValid() || !(*Position)[1].IsValid() || !(*Position)[2].IsValid()
                        || !(*Position)[0]->TryGetNumber(X) || !(*Position)[1]->TryGetNumber(Y) || !(*Position)[2]->TryGetNumber(Z)
                        || !Piece->TryGetNumberField(TEXT("yaw_degrees"), Yaw)
                        || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z) || !FMath::IsFinite(Yaw)) continue;
                    Item.PositionM = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
                    // Source catalog explicitly declares yaw_ue = -yaw_blender.
                    Item.Rotation = FRotator(0.f, -static_cast<float>(Yaw), 0.f);
                    const TArray<TSharedPtr<FJsonValue>>* Scale = nullptr;
                    if (Piece->TryGetArrayField(TEXT("scale"), Scale) && Scale && Scale->Num() == 3)
                    {
                        double SX = 1., SY = 1., SZ = 1.;
                        if ((*Scale)[0].IsValid() && (*Scale)[1].IsValid() && (*Scale)[2].IsValid()
                            && (*Scale)[0]->TryGetNumber(SX) && (*Scale)[1]->TryGetNumber(SY) && (*Scale)[2]->TryGetNumber(SZ)
                            && FMath::IsFinite(SX) && FMath::IsFinite(SY) && FMath::IsFinite(SZ)
                            && FMath::Abs(SX) > 0.0001 && FMath::Abs(SY) > 0.0001 && FMath::Abs(SZ) > 0.0001)
                            Item.Scale = FVector(static_cast<float>(SX), static_cast<float>(SY), static_cast<float>(SZ));
                    }
                    Parsed.Add(MoveTemp(Item));
                }
            }
            return Out.ModuleMeshes.Num() > 0 && Out.Assemblies.Contains(TEXT("royal_gate_watch"))
                && Out.Assemblies.Contains(TEXT("paddock_corner"));
        }

        static bool IsExcludedModule(const FString& Module)
        {
            // These are deliberately omitted: the real service residents and
            // future horse actor own the live duties, while inventory owns any
            // cargo.  No static character or loaded stock is fabricated here.
            return Module == TEXT("royal_guard_body") || Module == TEXT("guard_helmet")
                || Module == TEXT("guard_shield") || Module == TEXT("guard_spear")
                || Module == TEXT("horse_bay") || Module == TEXT("horse_grey")
                || Module == TEXT("carter_body") || Module == TEXT("cargo_logs")
                || Module == TEXT("cargo_sacks");
        }

        static void RemoveExisting(AHearthVillage& Village)
        {
            TArray<UStaticMeshComponent*> Existing;
            Village.GetComponents<UStaticMeshComponent>(Existing);
            for (UStaticMeshComponent* Component : Existing)
                if (IsValid(Component) && Component->ComponentTags.Contains(FName(VisualTag))) Component->DestroyComponent();
        }

        static const FAssemblyPiece* FindPiece(const TArray<FAssemblyPiece>& Pieces, const FString& Key)
        {
            return Pieces.FindByPredicate([&Key](const FAssemblyPiece& Piece){ return Piece.Key == Key; });
        }

        static bool ResolvePieceTransform(const TArray<FAssemblyPiece>& Pieces, const FAssemblyPiece& Piece,
            TMap<FString, FTransform>& Cache, TSet<FString>& Visiting, FTransform& Out)
        {
            if (const FTransform* Existing = Cache.Find(Piece.Key)) { Out = *Existing; return true; }
            if (Visiting.Contains(Piece.Key)) return false;
            Visiting.Add(Piece.Key);
            const FVector LocalPosition(Piece.PositionM.X * 100.f, -Piece.PositionM.Y * 100.f, Piece.PositionM.Z * 100.f);
            FRotator LocalRotation = Piece.Rotation;
            FVector LocalScale = Piece.Scale;
            if (Piece.Module == TEXT("gate_leaf"))
            {
                // Catalog source yaw is the authored half-open pose (+/-34
                // degrees after reflection).  The runtime baseline keeps the
                // entrance visibly open at +/-85 degrees and mirrors the right
                // leaf on local X as required by the module note.
                const bool bRightLeaf = Piece.Scale.X < 0.f;
                LocalRotation.Yaw = bRightLeaf ? -85.f : 85.f;
            }
            const FTransform Local(LocalRotation, LocalPosition, LocalScale);
            if (Piece.ParentKey.IsEmpty()) Out = Local;
            else
            {
                const FAssemblyPiece* Parent = FindPiece(Pieces, Piece.ParentKey);
                FTransform ParentTransform;
                if (!Parent || !ResolvePieceTransform(Pieces, *Parent, Cache, Visiting, ParentTransform))
                { Visiting.Remove(Piece.Key); return false; }
                // UE's A*B applies A first and then B.  Attachments therefore
                // use child-local first, followed by their parent transform.
                Out = Local * ParentTransform;
            }
            Visiting.Remove(Piece.Key); Cache.Add(Piece.Key, Out); return true;
        }

        static int32 AddAssembly(AHearthVillage& Village, const FCatalog& Catalog, const FString& AssemblyId,
            const FVector& AssemblyBase, float AssemblyYaw)
        {
            const TArray<FAssemblyPiece>* Pieces = Catalog.Assemblies.Find(AssemblyId);
            if (!Pieces) return 0;
            TMap<FString, FTransform> Transforms;
            TSet<FString> Visiting;
            const FTransform AssemblyTransform(FRotator(0.f, AssemblyYaw, 0.f), AssemblyBase, FVector::OneVector);
            int32 Added = 0;
            for (const FAssemblyPiece& Piece : *Pieces)
            {
                if (IsExcludedModule(Piece.Module)) continue;
                FTransform Local;
                if (!ResolvePieceTransform(*Pieces, Piece, Transforms, Visiting, Local)) continue;
                const TArray<FLayerMesh>* Layers = Catalog.ModuleMeshes.Find(Piece.Module);
                if (!Layers) continue;
                for (const FLayerMesh& Layer : *Layers)
                {
                    UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *Layer.Path);
                    if (!Asset) continue;
                    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(&Village);
                    if (!IsValid(Mesh)) continue;
                    Mesh->SetStaticMesh(Asset);
                    Mesh->SetMobility(EComponentMobility::Static);
                    // The first baseline keeps the gate leaves open and avoids
                    // inventing navigation/interaction blockers for scenery.
                    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Mesh->ComponentTags.Add(FName(VisualTag));
                    Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("HearthAssembly=%s"), *AssemblyId)));
                    Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("HearthLayer=%s"), *Layer.Layer)));
                    Mesh->ComponentTags.Add(FName(*FString::Printf(TEXT("HearthSourceModule=%s"), *Piece.Module)));
                    Mesh->SetWorldTransform(Local * AssemblyTransform);
                    Village.AddInstanceComponent(Mesh);
                    Mesh->RegisterComponent();
                    ++Added;
                }
            }
            return Added;
        }
    }
}

void AHearthVillage::BuildMedievalPublicVisuals()
{
    if (!IsOrganicVillage() || !GetWorld()) return;
    HearthMedievalVisuals::RemoveExisting(*this);

    HearthMedievalVisuals::FCatalog Catalog;
    if (!HearthMedievalVisuals::ReadCatalog(Catalog))
    {
        UE_LOG(LogTemp, Warning, TEXT("MEDIEVAL_VISUALS_CATALOG_UNAVAILABLE path=%s"),
            *(FPaths::ProjectContentDir() / HearthMedievalVisuals::CatalogRelativePath));
        return;
    }
    if (!Residents.IsValidIndex(10) || !Residents.IsValidIndex(12))
    {
        UE_LOG(LogTemp, Warning, TEXT("MEDIEVAL_PUBLIC_VISUALS_WAITING_FOR_SERVICE_RESIDENTS"));
        return;
    }

    // Duty anchors are supplied by the real service runtime.  Project the
    // assembly bases once; all catalog layers and parented pieces retain their
    // relative authored transforms and therefore cannot scatter on a slope.
    const FTransform GateFrame=GetServiceGateFrame();
    FVector GateBase = GateFrame.GetLocation();
    // The stable sits behind its open approach, apart from the shared meal
    // and rest point. Residents must not spawn inside the rail enclosure.
    FVector PaddockBase = GetServiceDutyAnchor(12)+FVector(0,-250,0);
    GateBase.Z = GroundHeightAt(GateBase);
    PaddockBase.Z = GroundHeightAt(PaddockBase);
    const int32 GateCount = HearthMedievalVisuals::AddAssembly(*this, Catalog, TEXT("royal_gate_watch"), GateBase, GateFrame.Rotator().Yaw);
    const int32 PaddockCount = HearthMedievalVisuals::AddAssembly(*this, Catalog, TEXT("paddock_corner"), PaddockBase, 0.f);
    if (GateCount <= 0 || PaddockCount <= 0)
    {
        HearthMedievalVisuals::RemoveExisting(*this);
        UE_LOG(LogTemp, Warning, TEXT("MEDIEVAL_PUBLIC_VISUALS_ASSETS_UNAVAILABLE gate=%d paddock=%d"), GateCount, PaddockCount);
        return;
    }
    FString FreightText;TSharedPtr<FJsonObject> FreightCatalog;
    if(FFileHelper::LoadFileToString(FreightText,*(FPaths::ProjectContentDir()/TEXT("ThreeHearths/Data/MedievalFreightKitCatalog.json")))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FreightText),FreightCatalog) && FreightCatalog.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* Assets=nullptr;
        FVector RackBase(-2350,-1100,0);RackBase.Z=GroundHeightAt(RackBase)+1.f;
        if(FreightCatalog->TryGetArrayField(TEXT("assets"),Assets)) for(const auto& Value:*Assets)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) continue;
            FString Id,Path;
            if(!Value->AsObject()->TryGetStringField(TEXT("id"),Id) || !Id.StartsWith(TEXT("freight_depot_rack/"))
                || !Value->AsObject()->TryGetStringField(TEXT("mesh"),Path)) continue;
            auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path);if(!Asset) continue;
            auto* Part=NewObject<UStaticMeshComponent>(this);Part->SetStaticMesh(Asset);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetMobility(EComponentMobility::Static);
            Part->ComponentTags.Add(TEXT("HearthMedievalVisuals"));Part->ComponentTags.Add(TEXT("HearthSourceModule=freight_depot_rack"));
            Part->SetWorldTransform(FTransform(FRotator(0,-90,0),RackBase));AddInstanceComponent(Part);Part->RegisterComponent();
        }
    }
    UE_LOG(LogTemp, Display, TEXT("MEDIEVAL_PUBLIC_VISUALS_BUILT gate_anchor=%s carter_anchor=%s gate_layers=%d paddock_layers=%d"),
        *GateBase.ToString(), *PaddockBase.ToString(), GateCount, PaddockCount);
    AHearthHorseCart* Cart=nullptr;
    for(TActorIterator<AHearthHorseCart> It(GetWorld());It;++It)
        if(It->GetOwner()==this) { if(!Cart) Cart=*It;else It->Destroy(); }
    if(!Cart) Cart=GetWorld()->SpawnActor<AHearthHorseCart>();
    if(!Cart || !Cart->Configure(this)) UE_LOG(LogTemp,Warning,TEXT("MEDIEVAL_HORSE_CART_ASSETS_UNAVAILABLE"));
}

void AHearthVillage::RefreshMedievalPublicVisuals(float DeltaSeconds)
{
    // Public facilities are rebuilt only by the explicit Build hook.  Keeping
    // this refresh hook inert avoids per-frame component scans and repeated
    // catalog I/O when an import or service population is not ready yet.
    (void)DeltaSeconds;
}
