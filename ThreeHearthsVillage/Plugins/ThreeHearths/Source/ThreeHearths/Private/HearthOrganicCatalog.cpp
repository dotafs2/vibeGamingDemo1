#include "HearthOrganicCatalog.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr float MatrixScale = 100.f;
    constexpr float MatrixTolerance = .25f;

    bool AnyComponentGreater(const FVector& A, const FVector& B)
    { return A.X > B.X || A.Y > B.Y || A.Z > B.Z; }

    bool ReadFiniteNumber(const TSharedPtr<FJsonValue>& Value, double& Out)
    {
        if (!Value.IsValid() || Value->Type != EJson::Number) return false;
        Out = Value->AsNumber();
        return FMath::IsFinite(Out);
    }

    bool ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 3) return false;
        double X = 0, Y = 0, Z = 0;
        if (!ReadFiniteNumber((*Values)[0], X) || !ReadFiniteNumber((*Values)[1], Y) || !ReadFiniteNumber((*Values)[2], Z)) return false;
        Out = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
        return FMath::IsFinite(Out.X) && FMath::IsFinite(Out.Y) && FMath::IsFinite(Out.Z);
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out, bool bRequired = true)
    {
        if (!Object.IsValid() || !Object->TryGetStringField(Field, Out)) return !bRequired;
        Out.TrimStartAndEndInline();
        return bRequired ? !Out.IsEmpty() : true;
    }

    bool IsKnownLayer(const FString& Layer)
    {
        return Layer == TEXT("structure") || Layer == TEXT("finish") || Layer == TEXT("weathering") || Layer == TEXT("attachments");
    }

    bool ParseMatrix(const TSharedPtr<FJsonObject>& Root, FHearthOrganicSourceMatrix& Out, FString& Error)
    {
        const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
        if (!Root->TryGetArrayField(TEXT("source_to_unreal_matrix"), Rows) || !Rows || (Rows->Num() != 3 && Rows->Num() != 4))
        {
            Error = TEXT("source_to_unreal_matrix must have three rows (or a homogeneous four-row form)"); return false;
        }
        const bool bHomogeneous = Rows->Num() == 4;
        for (int32 Row = 0; Row < 3; ++Row)
        {
            const int32 ColumnCount = (*Rows)[Row].IsValid() && (*Rows)[Row]->Type == EJson::Array ? (*Rows)[Row]->AsArray().Num() : 0;
            if (ColumnCount != (bHomogeneous ? 4 : 3))
            {
                Error = TEXT("source_to_unreal_matrix must be a 3x3 number array or a homogeneous 4x4 array"); return false;
            }
            for (int32 Column = 0; Column < 3; ++Column)
            {
                double Number = 0;
                if (!ReadFiniteNumber((*Rows)[Row]->AsArray()[Column], Number)) { Error = TEXT("source_to_unreal_matrix contains a non-finite value"); return false; }
                Out.Values[Row][Column] = static_cast<float>(Number);
            }
            if (ColumnCount == 4)
            {
                double Translation = 0;
                if (!ReadFiniteNumber((*Rows)[Row]->AsArray()[3], Translation) || !FMath::IsNearlyZero(Translation, MatrixTolerance))
                { Error = TEXT("source_to_unreal_matrix homogeneous rows must have zero translation"); return false; }
            }
        }
        if (bHomogeneous)
        {
            if (!(*Rows)[3].IsValid() || (*Rows)[3]->Type != EJson::Array)
            { Error = TEXT("source_to_unreal_matrix homogeneous row is invalid"); return false; }
            const TArray<TSharedPtr<FJsonValue>>& Last = (*Rows)[3]->AsArray();
            if (Last.Num() != 4) { Error = TEXT("source_to_unreal_matrix homogeneous row is invalid"); return false; }
            for (int32 Column = 0; Column < 3; ++Column)
            {
                double Number = 0;
                if (!ReadFiniteNumber(Last[Column], Number) || !FMath::IsNearlyZero(Number, MatrixTolerance))
                { Error = TEXT("source_to_unreal_matrix homogeneous row must start with zeroes"); return false; }
            }
            double W = 0;
            if (!ReadFiniteNumber(Last[3], W) || !FMath::IsNearlyEqual(W, 1.0, MatrixTolerance))
            { Error = TEXT("source_to_unreal_matrix homogeneous row must end in one"); return false; }
        }
        for (int32 Row = 0; Row < 3; ++Row)
        {
            float LengthSquared = 0.f;
            for (int32 Column = 0; Column < 3; ++Column) LengthSquared += Out.Values[Row][Column] * Out.Values[Row][Column];
            if (!FMath::IsNearlyEqual(FMath::Sqrt(LengthSquared), MatrixScale, MatrixTolerance))
            { Error = TEXT("source_to_unreal_matrix must have uniform 100x scale"); return false; }
            for (int32 Other = Row + 1; Other < 3; ++Other)
            {
                float Dot = 0.f;
                for (int32 Column = 0; Column < 3; ++Column) Dot += Out.Values[Row][Column] * Out.Values[Other][Column];
                if (FMath::Abs(Dot) > MatrixTolerance) { Error = TEXT("source_to_unreal_matrix must be orthogonal"); return false; }
            }
        }
        // Yaw is defined in the source XY plane. Keep that plane horizontal in
        // Unreal so a source Z rotation cannot silently turn into a tilt.
        if (FMath::Abs(Out.Values[0][2]) > MatrixTolerance || FMath::Abs(Out.Values[1][2]) > MatrixTolerance
            || FMath::Abs(Out.Values[2][0]) > MatrixTolerance || FMath::Abs(Out.Values[2][1]) > MatrixTolerance)
        { Error = TEXT("source_to_unreal_matrix must preserve the horizontal plane"); return false; }
        const float Determinant = Out.Values[0][0] * (Out.Values[1][1] * Out.Values[2][2] - Out.Values[1][2] * Out.Values[2][1])
            - Out.Values[0][1] * (Out.Values[1][0] * Out.Values[2][2] - Out.Values[1][2] * Out.Values[2][0])
            + Out.Values[0][2] * (Out.Values[1][0] * Out.Values[2][1] - Out.Values[1][1] * Out.Values[2][0]);
        if (!FMath::IsNearlyEqual(FMath::Abs(Determinant), MatrixScale * MatrixScale * MatrixScale, 7500.f))
        { Error = TEXT("source_to_unreal_matrix has an invalid determinant"); return false; }
        return true;
    }

    bool ParseCell(const FString& Text, FIntPoint& Out)
    {
        TArray<FString> Parts;
        Text.ParseIntoArrayWS(Parts);
        if (Parts.Num() != 2 || Parts[0].IsEmpty() || Parts[1].IsEmpty()) return false;
        TCHAR* EndX = nullptr; TCHAR* EndY = nullptr;
        const int32 X = FCString::Strtoi(*Parts[0], &EndX, 10);
        const int32 Y = FCString::Strtoi(*Parts[1], &EndY, 10);
        if (!EndX || !EndY || *EndX != 0 || *EndY != 0) return false;
        Out = FIntPoint(X, Y); return true;
    }

    bool ParseCellValue(const TSharedPtr<FJsonValue>& Value, FIntPoint& Out)
    {
        if (!Value.IsValid()) return false;
        if (Value->Type == EJson::String)
        {
            FString Text;
            return Value->TryGetString(Text) && ParseCell(Text, Out);
        }
        if (Value->Type != EJson::Array || Value->AsArray().Num() != 2) return false;
        double X = 0, Y = 0;
        if (!ReadFiniteNumber(Value->AsArray()[0], X) || !ReadFiniteNumber(Value->AsArray()[1], Y)) return false;
        if (X < static_cast<double>(MIN_int32) || X > static_cast<double>(MAX_int32)
            || Y < static_cast<double>(MIN_int32) || Y > static_cast<double>(MAX_int32)) return false;
        const int32 IntX = static_cast<int32>(X);
        const int32 IntY = static_cast<int32>(Y);
        if (X != static_cast<double>(IntX) || Y != static_cast<double>(IntY)) return false;
        Out = FIntPoint(IntX, IntY);
        return true;
    }

    bool ParseRecipe(const TSharedPtr<FJsonObject>& Object, const FHearthOrganicSourceMatrix& Matrix,
        const TSet<FString>& AssetKeys, FHearthOrganicRecipe& Out, FString& Error, bool bGrowth)
    {
        if (!ReadString(Object, TEXT("id"), Out.Id)) { Error = TEXT("recipe id is missing or invalid"); return false; }
        ReadString(Object, TEXT("label"), Out.Label, false); ReadString(Object, TEXT("palette"), Out.Palette);
        ReadString(Object, TEXT("kind"), Out.Kind, false); Out.bGrowth = bGrowth;
        const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
        if (!Object->TryGetArrayField(TEXT("pieces"), Pieces) || !Pieces || Pieces->Num() == 0) { Error = TEXT("recipe pieces are missing"); return false; }
        TSet<FString> Keys;
        for (const TSharedPtr<FJsonValue>& PieceValue : *Pieces)
        {
            const TSharedPtr<FJsonObject> PieceObject = PieceValue.IsValid() ? PieceValue->AsObject() : nullptr;
            FHearthOrganicPiece Piece;
            if (!PieceObject.IsValid() || !ReadString(PieceObject, TEXT("instance_key"), Piece.OriginalKey) || Keys.Contains(Piece.OriginalKey))
            { Error = TEXT("recipe piece keys must be non-empty and unique"); return false; }
            Keys.Add(Piece.OriginalKey);
            if (!ReadString(PieceObject, TEXT("module"), Piece.ModuleId) || !ReadString(PieceObject, TEXT("palette"), Piece.Palette)
                || !ReadVector(PieceObject, TEXT("translation_m"), Piece.SourceTranslationM)) { Error = TEXT("recipe piece fields are invalid"); return false; }
            double Yaw = 0;
            if (!PieceObject->TryGetNumberField(TEXT("yaw_degrees"), Yaw) || !FMath::IsFinite(Yaw)) { Error = TEXT("recipe piece yaw is invalid"); return false; }
            Piece.SourceYawDegrees = static_cast<float>(Yaw); Piece.Purpose = TEXT(""); ReadString(PieceObject, TEXT("purpose"), Piece.Purpose, false);
            const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
            if (!PieceObject->TryGetArrayField(TEXT("layers"), Layers) || !Layers || Layers->Num() == 0) { Error = TEXT("recipe piece layers are missing"); return false; }
            TSet<FString> PieceLayers;
            for (const TSharedPtr<FJsonValue>& LayerValue : *Layers)
            {
                FString Layer;
                if (!LayerValue.IsValid() || !LayerValue->TryGetString(Layer) || !IsKnownLayer(Layer) || PieceLayers.Contains(Layer)) { Error = TEXT("recipe piece has an invalid or duplicate layer"); return false; }
                PieceLayers.Add(Layer); Piece.Layers.Add(Layer);
                if (!AssetKeys.Contains(Piece.ModuleId + TEXT("|") + Piece.Palette + TEXT("|") + Layer)) { Error = TEXT("recipe piece references an asset layer absent from the manifest"); return false; }
            }
            Piece.TranslationCm = Matrix.TransformMeters(Piece.SourceTranslationM);
            Piece.UnrealYawDegrees = Matrix.TransformYawDegrees(Piece.SourceYawDegrees);
            Out.Pieces.Add(MoveTemp(Piece));
        }
        const TArray<TSharedPtr<FJsonValue>>* CellGroups = nullptr;
        if (!Object->TryGetArrayField(TEXT("cells"), CellGroups) || !CellGroups || CellGroups->Num() == 0) { Error = TEXT("recipe cells are missing"); return false; }
        for (const TSharedPtr<FJsonValue>& GroupValue : *CellGroups)
        {
            const TSharedPtr<FJsonObject> Group = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
            if (!Group.IsValid()) { Error = TEXT("recipe cell group is invalid"); return false; }
            double Z = 0, WallHeight = 0;
            if (!Group->TryGetNumberField(TEXT("z_m"), Z) || !Group->TryGetNumberField(TEXT("wall_height_m"), WallHeight) || !FMath::IsFinite(Z) || !FMath::IsFinite(WallHeight)) { Error = TEXT("recipe cell heights are invalid"); return false; }
            const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
            if (!Group->TryGetArrayField(TEXT("cells"), Cells) || !Cells || Cells->Num() == 0) { Error = TEXT("recipe cell group is empty"); return false; }
            for (const TSharedPtr<FJsonValue>& CellValue : *Cells)
            {
                FIntPoint Cell;
                if (!ParseCellValue(CellValue, Cell)) { Error = TEXT("recipe occupied cell must be a string or an integer [x,y] pair"); return false; }
                FHearthOrganicOccupiedCell Occupied; Occupied.Cell = Cell; Occupied.Zm = static_cast<float>(Z); Occupied.WallHeightM = static_cast<float>(WallHeight); Out.OccupiedCells.Add(Occupied);
            }
        }
        const TSharedPtr<FJsonObject>* Bounds = nullptr;
        if (!Object->TryGetObjectField(TEXT("bounds_m"), Bounds) || !(*Bounds).IsValid() || !ReadVector(*Bounds, TEXT("min"), Out.SourceBoundsMinM) || !ReadVector(*Bounds, TEXT("max"), Out.SourceBoundsMaxM)
            || AnyComponentGreater(Out.SourceBoundsMinM, Out.SourceBoundsMaxM)) { Error = TEXT("recipe bounds_m are invalid"); return false; }
        Out.BoundsMinCm = Out.BoundsMaxCm = FVector::ZeroVector;
        for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y) for (int32 Z = 0; Z < 2; ++Z)
        {
            const FVector Corner(X ? Out.SourceBoundsMaxM.X : Out.SourceBoundsMinM.X, Y ? Out.SourceBoundsMaxM.Y : Out.SourceBoundsMinM.Y, Z ? Out.SourceBoundsMaxM.Z : Out.SourceBoundsMinM.Z);
            const FVector Transformed = Matrix.TransformMeters(Corner);
            if (X == 0 && Y == 0 && Z == 0) Out.BoundsMinCm = Out.BoundsMaxCm = Transformed;
            else { Out.BoundsMinCm = Out.BoundsMinCm.ComponentMin(Transformed); Out.BoundsMaxCm = Out.BoundsMaxCm.ComponentMax(Transformed); }
        }
        return true;
    }

    bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& Out, bool bRequired, FString& Error)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values) || !Values)
        {
            if (!bRequired) return true;
            Error = FString::Printf(TEXT("growth %s is missing"), Field); return false;
        }
        TSet<FString> Seen;
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString StringValue;
            if (!Value.IsValid() || !Value->TryGetString(StringValue)) { Error = FString::Printf(TEXT("growth %s contains a non-string"), Field); return false; }
            StringValue.TrimStartAndEndInline();
            if (StringValue.IsEmpty() || Seen.Contains(StringValue)) { Error = FString::Printf(TEXT("growth %s contains an empty or duplicate key"), Field); return false; }
            Seen.Add(StringValue); Out.Add(MoveTemp(StringValue));
        }
        if (bRequired && Out.IsEmpty()) { Error = FString::Printf(TEXT("growth %s is empty"), Field); return false; }
        return true;
    }

    bool ParseGrowth(const TSharedPtr<FJsonObject>& Object, const TSet<FString>& AssetKeys, FHearthOrganicGrowth& Out, FString& Error)
    {
        if (!Object.IsValid() || !ReadString(Object, TEXT("id"), Out.Id)) { Error = TEXT("growth id is missing or invalid"); return false; }
        ReadString(Object, TEXT("source"), Out.Source, false);
        if (!ReadStringArray(Object, TEXT("stages"), Out.Stages, true, Error) || !ReadStringArray(Object, TEXT("rules"), Out.Rules, false, Error)) return false;
        const TArray<TSharedPtr<FJsonValue>>* TransitionValues = nullptr;
        if (!Object->TryGetArrayField(TEXT("transitions"), TransitionValues) || !TransitionValues || TransitionValues->Num() == 0) { Error = TEXT("growth transitions are missing"); return false; }
        for (const TSharedPtr<FJsonValue>& Value : *TransitionValues)
        {
            const TSharedPtr<FJsonObject> TransitionObject = Value.IsValid() ? Value->AsObject() : nullptr;
            FHearthOrganicGrowthTransition Transition;
            if (!TransitionObject.IsValid() || !ReadString(TransitionObject, TEXT("from"), Transition.From) || !ReadString(TransitionObject, TEXT("to"), Transition.To)
                || !ReadStringArray(TransitionObject, TEXT("retain"), Transition.RetainKeys, true, Error))
            { if (Error.IsEmpty()) Error = TEXT("growth transition is invalid"); return false; }
            const auto ReadObjectKeys = [&TransitionObject, &AssetKeys, &Error](const TCHAR* Field, TArray<FString>& Keys)
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!TransitionObject->TryGetArrayField(Field, Values) || !Values || Values->Num() == 0) { Error = FString::Printf(TEXT("growth %s is missing"), Field); return false; }
                TSet<FString> Seen;
                for (const TSharedPtr<FJsonValue>& Item : *Values)
                {
                    const TSharedPtr<FJsonObject> Piece = Item.IsValid() ? Item->AsObject() : nullptr;
                    FString Key;
                    FString Module; FString Palette;
                    const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
                    if (!Piece.IsValid() || !ReadString(Piece, TEXT("instance_key"), Key) || Seen.Contains(Key)
                        || !ReadString(Piece, TEXT("module"), Module) || !ReadString(Piece, TEXT("palette"), Palette)
                        || !Piece->TryGetArrayField(TEXT("layers"), Layers) || !Layers || Layers->Num() == 0)
                    { Error = FString::Printf(TEXT("growth %s has an invalid or duplicate piece key"), Field); return false; }
                    TSet<FString> PieceLayers;
                    for (const TSharedPtr<FJsonValue>& LayerValue : *Layers)
                    {
                        FString Layer;
                        if (!LayerValue.IsValid() || !LayerValue->TryGetString(Layer) || !IsKnownLayer(Layer) || PieceLayers.Contains(Layer)
                            || !AssetKeys.Contains(Module + TEXT("|") + Palette + TEXT("|") + Layer))
                        { Error = FString::Printf(TEXT("growth %s references an invalid asset layer"), Field); return false; }
                        PieceLayers.Add(Layer);
                    }
                    Seen.Add(Key); Keys.Add(MoveTemp(Key));
                }
                return true;
            };
            if (!ReadObjectKeys(TEXT("dismantle"), Transition.DismantleKeys) || !ReadObjectKeys(TEXT("add"), Transition.AddKeys)) return false;
            Out.Transitions.Add(MoveTemp(Transition));
        }
        auto Writer = TJsonWriterFactory<>::Create(&Out.RawJson);
        if (!FJsonSerializer::Serialize(Object.ToSharedRef(), Writer)) { Error = TEXT("growth JSON could not be preserved"); return false; }
        return true;
    }
}

FVector FHearthOrganicSourceMatrix::TransformMeters(const FVector& SourceMeters) const
{
    return FVector(
        Values[0][0] * SourceMeters.X + Values[0][1] * SourceMeters.Y + Values[0][2] * SourceMeters.Z,
        Values[1][0] * SourceMeters.X + Values[1][1] * SourceMeters.Y + Values[1][2] * SourceMeters.Z,
        Values[2][0] * SourceMeters.X + Values[2][1] * SourceMeters.Y + Values[2][2] * SourceMeters.Z);
}

FVector FHearthOrganicSourceMatrix::TransformDirection(const FVector& SourceDirection) const
{
    return TransformMeters(SourceDirection);
}

float FHearthOrganicSourceMatrix::TransformYawDegrees(float SourceYawDegrees) const
{
    const float Radians = FMath::DegreesToRadians(SourceYawDegrees);
    const FVector Mapped = TransformDirection(FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.f));
    const FVector MappedSourceX = TransformDirection(FVector(1.f, 0.f, 0.f));
    const float MappedAngle = FMath::RadiansToDegrees(FMath::Atan2(Mapped.Y, Mapped.X));
    const float BaseAngle = FMath::RadiansToDegrees(FMath::Atan2(MappedSourceX.Y, MappedSourceX.X));
    return FMath::UnwindDegrees(MappedAngle - BaseAngle);
}

namespace HearthOrganicCatalog
{
    bool LoadFromJson(const FString& JsonText, FHearthOrganicCatalog& OutCatalog, FString& OutError)
    {
        OutCatalog = FHearthOrganicCatalog(); OutError.Empty();
        if (JsonText.Len() > 32 * 1024 * 1024) { OutError = TEXT("organic catalog is too large"); return false; }
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root) || !Root.IsValid()) { OutError = TEXT("organic catalog JSON is invalid"); return false; }
        double Version = 0;
        if (!Root->TryGetNumberField(TEXT("schema_version"), Version) || Version != 1) { OutError = TEXT("organic catalog schema_version must be 1"); return false; }
        OutCatalog.SchemaVersion = 1;
        if (!ParseMatrix(Root, OutCatalog.SourceToUnrealMatrix, OutError)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        if (!Root->TryGetArrayField(TEXT("assets"), Assets) || !Assets || Assets->Num() == 0) { OutError = TEXT("organic catalog assets are missing"); return false; }
        TSet<FString> AssetKeys;
        for (const TSharedPtr<FJsonValue>& AssetValue : *Assets)
        {
            const TSharedPtr<FJsonObject> Object = AssetValue.IsValid() ? AssetValue->AsObject() : nullptr;
            FHearthOrganicAsset Asset;
            if (!Object.IsValid() || !ReadString(Object, TEXT("module_id"), Asset.ModuleId) || !ReadString(Object, TEXT("palette"), Asset.Palette)
                || !ReadString(Object, TEXT("layer"), Asset.Layer) || !IsKnownLayer(Asset.Layer) || !ReadString(Object, TEXT("mesh"), Asset.Mesh)
                || !ReadVector(Object, TEXT("bounds_min_cm"), Asset.BoundsMinCm) || !ReadVector(Object, TEXT("bounds_max_cm"), Asset.BoundsMaxCm)
                || AnyComponentGreater(Asset.BoundsMinCm, Asset.BoundsMaxCm)) { OutError = TEXT("organic catalog asset is invalid"); return false; }
            const FString Key = Asset.ModuleId + TEXT("|") + Asset.Palette + TEXT("|") + Asset.Layer;
            if (AssetKeys.Contains(Key)) { OutError = TEXT("organic catalog asset keys must be unique"); return false; }
            AssetKeys.Add(Key); OutCatalog.Assets.Add(MoveTemp(Asset));
        }
        const TArray<TSharedPtr<FJsonValue>>* Recipes = nullptr;
        if (!Root->TryGetArrayField(TEXT("recipes"), Recipes) || !Recipes || Recipes->Num() == 0) { OutError = TEXT("organic catalog recipes are missing"); return false; }
        TSet<FString> RecipeIds;
        for (const TSharedPtr<FJsonValue>& RecipeValue : *Recipes)
        {
            FHearthOrganicRecipe Recipe;
            const TSharedPtr<FJsonObject> Object = RecipeValue.IsValid() ? RecipeValue->AsObject() : nullptr;
            if (!ParseRecipe(Object, OutCatalog.SourceToUnrealMatrix, AssetKeys, Recipe, OutError, false)) return false;
            if (Recipe.Id == TEXT("family_growth") || RecipeIds.Contains(Recipe.Id)) { OutError = TEXT("recipe ids must be unique and family_growth is reserved for growth"); return false; }
            RecipeIds.Add(Recipe.Id); OutCatalog.Recipes.Add(MoveTemp(Recipe));
        }
        const TSharedPtr<FJsonObject>* GrowthObject = nullptr;
        if (!Root->TryGetObjectField(TEXT("growth"), GrowthObject) || !(*GrowthObject).IsValid() || !ParseGrowth(*GrowthObject, AssetKeys, OutCatalog.Growth, OutError)) return false;
        OutCatalog.bHasGrowth = true;
        return true;
    }

    FString DefaultPath()
    { return FPaths::ProjectContentDir() / TEXT("ThreeHearths/Data/OrganicMasterCatalog.json"); }

    bool Load(FHearthOrganicCatalog& OutCatalog, FString& OutError)
    {
        FString Json;
        if (!FFileHelper::LoadFileToString(Json, *DefaultPath())) { OutError = FString::Printf(TEXT("organic catalog could not be read: %s"), *DefaultPath()); return false; }
        return LoadFromJson(Json, OutCatalog, OutError);
    }

    const FHearthOrganicRecipe* FindRecipe(const FHearthOrganicCatalog& Catalog, const FString& RecipeId)
    {
        return Catalog.Recipes.FindByPredicate([&RecipeId](const FHearthOrganicRecipe& Recipe) { return Recipe.Id == RecipeId; });
    }

    const FHearthOrganicGrowth* FindGrowth(const FHearthOrganicCatalog& Catalog)
    { return Catalog.bHasGrowth ? &Catalog.Growth : nullptr; }

    const FHearthOrganicAsset* FindAsset(const FHearthOrganicCatalog& Catalog, const FString& ModuleId, const FString& Palette, const FString& Layer)
    {
        return Catalog.Assets.FindByPredicate([&](const FHearthOrganicAsset& Asset) { return Asset.ModuleId == ModuleId && Asset.Palette == Palette && Asset.Layer == Layer; });
    }

    FString ResolveLayerPath(const FHearthOrganicCatalog& Catalog, const FString& ModuleId, const FString& Palette, const FString& Layer)
    {
        const FHearthOrganicAsset* Asset = FindAsset(Catalog, ModuleId, Palette, Layer);
        return Asset ? Asset->Mesh : FString();
    }
}
