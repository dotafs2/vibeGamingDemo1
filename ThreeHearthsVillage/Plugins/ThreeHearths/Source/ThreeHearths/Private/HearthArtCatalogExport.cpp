#if WITH_EDITOR

#include <initializer_list>

#include "HearthBotanicalCatalog.h"
#include "HearthBuildingAppearance.h"
#include "HearthRoyalWorksPlan.h"

#include "Dom/JsonObject.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHearthArtCatalogExport, Log, All);

namespace HearthArtCatalogExport
{
    TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(static_cast<int32>(Values.size()));
        for (const double Value : Values) Result.Add(MakeShared<FJsonValueNumber>(Value));
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> VectorValues(const FVector& Value)
    {
        return Numbers({Value.X, Value.Y, Value.Z});
    }

    TArray<TSharedPtr<FJsonValue>> ColorValues(const FLinearColor& Value)
    {
        return Numbers({Value.R, Value.G, Value.B, Value.A});
    }

    TSharedRef<FJsonObject> MakePart(const FString& MeshPath, const FVector& Offset,
        float Yaw, const FVector& Scale, const FLinearColor* Color, bool bCenterNativeBounds)
    {
        const TSharedRef<FJsonObject> Part = MakeShared<FJsonObject>();
        Part->SetStringField(TEXT("mesh_path"), MeshPath);
        Part->SetArrayField(TEXT("offset_cm"), VectorValues(Offset));
        Part->SetNumberField(TEXT("yaw_degrees"), Yaw);
        Part->SetArrayField(TEXT("scale"), VectorValues(Scale));
        if (Color) Part->SetArrayField(TEXT("color_linear"), ColorValues(*Color));
        else Part->SetField(TEXT("color_linear"), MakeShared<FJsonValueNull>());
        Part->SetBoolField(TEXT("center_native_bounds"), bCenterNativeBounds);
        return Part;
    }

    void AddBuilding(const FString& Archetype, const FString& NameZh, const FString& WallMaterial,
        const FString& RoofMaterial, uint32 Seed, TArray<TSharedPtr<FJsonValue>>& Entries)
    {
        FHearthBuildingAppearance Appearance;
        if (!HearthBuildingAppearance::Build(Archetype, WallMaterial, RoofMaterial, Seed, true, Appearance))
        {
            UE_LOG(LogHearthArtCatalogExport, Error, TEXT("Hearth.ExportArtCatalog failed building recipe: %s"), *Archetype);
            return;
        }

        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), Archetype);
        Entry->SetStringField(TEXT("name_zh"), NameZh);
        Entry->SetStringField(TEXT("kind"), TEXT("building"));
        Entry->SetArrayField(TEXT("dimensions_cm"), Numbers({
            Appearance.OccupiedFootprintCm.X,
            Appearance.OccupiedFootprintCm.Y,
            Appearance.RoofRidgeHeightCm}));
        Entry->SetNumberField(TEXT("floors"), Appearance.Floors);
        Entry->SetBoolField(TEXT("generated_from_code"), true);

        TArray<TSharedPtr<FJsonValue>> Parts;
        Parts.Reserve(Appearance.Parts.Num());
        for (const FHearthBuildingAppearancePart& SourcePart : Appearance.Parts)
        {
            Parts.Add(MakeShared<FJsonValueObject>(MakePart(
                SourcePart.AssetPath, SourcePart.Offset, SourcePart.Yaw, SourcePart.Scale,
                nullptr, false)));
        }
        Entry->SetArrayField(TEXT("parts"), Parts);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }

    bool AddCastle(TArray<TSharedPtr<FJsonValue>>& Entries)
    {
        const FHearthRoyalWorksPlan Plan = HearthRoyalWorksPlan::BuildForTemplate(TEXT("royal_keep_garden_v2"));
        if (Plan.TemplateId != TEXT("royal_keep_garden_v2") || Plan.Modules.Num() != 1276)
        {
            UE_LOG(LogHearthArtCatalogExport, Error,
                TEXT("Hearth.ExportArtCatalog failed royal_keep_garden_v2 input validation: template=%s modules=%d"),
                *Plan.TemplateId, Plan.Modules.Num());
            return false;
        }

        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), TEXT("royal_keep_garden_v2"));
        Entry->SetStringField(TEXT("name_zh"), TEXT("王家花园城堡"));
        Entry->SetStringField(TEXT("kind"), TEXT("assembly_preview"));
        Entry->SetBoolField(TEXT("preview_only"), true);
        Entry->SetBoolField(TEXT("generated_from_code"), true);
        Entry->SetNumberField(TEXT("plan_stage"), 13);
        Entry->SetNumberField(TEXT("source_module_count"), Plan.Modules.Num());

        TArray<TSharedPtr<FJsonValue>> Parts;
        Parts.Reserve(Plan.Modules.Num() + 64);
        for (const FHearthRoyalModule& Module : Plan.Modules)
        {
            if (!Module.PlantId.IsEmpty())
            {
                const TArray<FHearthPlantPart> PlantParts = HearthBotanicalCatalog::Build(
                    Module.PlantId, static_cast<int32>(FCrc::StrCrc32(*Module.Id)));
                if (PlantParts.IsEmpty())
                {
                    UE_LOG(LogHearthArtCatalogExport, Error,
                        TEXT("Hearth.ExportArtCatalog failed royal plant module: %s (%s)"),
                        *Module.Id, *Module.PlantId);
                    return false;
                }

                const FRotator ModuleRotation(0.f, Module.Yaw, 0.f);
                for (const FHearthPlantPart& PlantPart : PlantParts)
                {
                    const TSharedRef<FJsonObject> Part = MakePart(
                        PlantPart.MeshPath,
                        Module.Offset + ModuleRotation.RotateVector(PlantPart.Offset * Module.Scale),
                        PlantPart.Yaw + Module.Yaw,
                        PlantPart.Scale * Module.Scale,
                        &PlantPart.Color,
                        false);
                    Part->SetStringField(TEXT("source_module_id"), Module.Id);
                    Parts.Add(MakeShared<FJsonValueObject>(Part));
                }
                continue;
            }

            if (Module.MeshPath.IsEmpty())
            {
                UE_LOG(LogHearthArtCatalogExport, Error,
                    TEXT("Hearth.ExportArtCatalog failed royal module with no mesh or plant recipe: %s"), *Module.Id);
                return false;
            }

            // This mirrors AHearthVillage::RefreshPublicVisuals: native
            // centered VillageKit modules keep their authored materials;
            // basic Cube/Cone modules receive the royal module tint.
            const bool bBasicShape = Module.MeshPath.StartsWith(TEXT("/Engine/BasicShapes/"));
            const FLinearColor* ColorOverride = (!Module.bCenterMeshAtOffset && bBasicShape)
                ? &Module.Color : nullptr;
            const TSharedRef<FJsonObject> Part = MakePart(
                Module.MeshPath, Module.Offset, Module.Yaw, Module.Scale,
                ColorOverride, Module.bCenterMeshAtOffset);
            Part->SetStringField(TEXT("source_module_id"), Module.Id);
            Parts.Add(MakeShared<FJsonValueObject>(Part));
        }
        Entry->SetArrayField(TEXT("parts"), Parts);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
        return true;
    }

    void AddBotanical(const FString& SpeciesName, const FString& NameZh, int32 Seed,
        TArray<TSharedPtr<FJsonValue>>& Entries)
    {
        const TArray<FHearthPlantPart> SourceParts = HearthBotanicalCatalog::Build(SpeciesName, Seed);
        if (SourceParts.IsEmpty())
        {
            UE_LOG(LogHearthArtCatalogExport, Error, TEXT("Hearth.ExportArtCatalog failed botanical recipe: %s"), *SpeciesName);
            return;
        }

        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), SpeciesName);
        Entry->SetStringField(TEXT("name_zh"), NameZh);
        Entry->SetStringField(TEXT("kind"), TEXT("botanical"));
        Entry->SetBoolField(TEXT("generated_from_code"), true);

        TArray<TSharedPtr<FJsonValue>> Parts;
        Parts.Reserve(SourceParts.Num());
        for (const FHearthPlantPart& SourcePart : SourceParts)
        {
            Parts.Add(MakeShared<FJsonValueObject>(MakePart(
                SourcePart.MeshPath, SourcePart.Offset, SourcePart.Yaw, SourcePart.Scale,
                &SourcePart.Color, false)));
        }
        Entry->SetArrayField(TEXT("parts"), Parts);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }

    void ExportArtCatalog()
    {
        constexpr uint32 BuildingSeed = 7919u;
        constexpr int32 BotanicalSeed = 7919;

        TArray<TSharedPtr<FJsonValue>> Entries;
        Entries.Reserve(12);
        AddBuilding(TEXT("rowhouse"), TEXT("排屋"), TEXT("plaster"), TEXT("terracotta"), BuildingSeed, Entries);
        AddBuilding(TEXT("shop_house"), TEXT("店屋"), TEXT("plaster"), TEXT("slateblue"), BuildingSeed, Entries);
        AddBuilding(TEXT("courtyard_workshop"), TEXT("庭院工坊"), TEXT("plaster"), TEXT("terracotta"), BuildingSeed, Entries);
        AddBuilding(TEXT("warehouse"), TEXT("仓库"), TEXT("stone"), TEXT("slateblue"), BuildingSeed, Entries);
        AddBuilding(TEXT("inn"), TEXT("旅店"), TEXT("timber"), TEXT("terracotta"), BuildingSeed, Entries);
        if (!AddCastle(Entries)) return;

        const TArray<FString> Species = HearthBotanicalCatalog::Species();
        if (Species.Num() != 6)
        {
            UE_LOG(LogHearthArtCatalogExport, Error,
                TEXT("Hearth.ExportArtCatalog failed botanical species count validation: %d"), Species.Num());
            return;
        }
        const TCHAR* const NamesZh[] = {TEXT("橡树"), TEXT("白桦"), TEXT("果树"), TEXT("柏树"), TEXT("开花灌木"), TEXT("野花")};
        for (int32 Index = 0; Index < Species.Num(); ++Index)
            AddBotanical(Species[Index], NamesZh[Index], BotanicalSeed, Entries);

        if (Entries.Num() != 12)
        {
            UE_LOG(LogHearthArtCatalogExport, Error,
                TEXT("Hearth.ExportArtCatalog aborted: expected 12 entries, generated %d"), Entries.Num());
            return;
        }

        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schema"), 1);
        Root->SetArrayField(TEXT("entries"), Entries);

        FString Json;
        if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json)))
        {
            UE_LOG(LogHearthArtCatalogExport, Error, TEXT("Hearth.ExportArtCatalog failed JSON serialization"));
            return;
        }

        const FString Directory = FPaths::ProjectSavedDir() / TEXT("ThreeHearths/ArtCatalog");
        const FString OutputPath = Directory / TEXT("catalog.json");
        if (!IFileManager::Get().MakeDirectory(*Directory, true)
            || !FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogHearthArtCatalogExport, Error, TEXT("Hearth.ExportArtCatalog failed writing %s"), *OutputPath);
            return;
        }

        UE_LOG(LogHearthArtCatalogExport, Display,
            TEXT("Hearth.ExportArtCatalog wrote %d entries to %s"), Entries.Num(), *OutputPath);
    }
}

static FAutoConsoleCommand HearthExportArtCatalogCommand(
    TEXT("Hearth.ExportArtCatalog"),
    TEXT("Export the current generated Hearth art recipe catalog to JSON for contact sheets."),
    FConsoleCommandDelegate::CreateStatic(&HearthArtCatalogExport::ExportArtCatalog));

#endif
