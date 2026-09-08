#if WITH_DEV_AUTOMATION_TESTS
#include "HearthOrganicCatalog.h"
#include "Misc/AutomationTest.h"

namespace
{
    FString TestCatalogJson(const FString& PieceLayer = TEXT("structure"), const FString& PieceKey = TEXT("piece-a"), const FString& Matrix = TEXT("[[100,0,0],[0,100,0],[0,0,100]]"))
    {
        return FString::Printf(TEXT(R"json({
"schema_version":1,
"source_to_unreal_matrix":%s,
"assets":[
 {"module_id":"wall_plain_2m","palette":"warm_lime","layer":"structure","mesh":"Modules/wall_plain_2m.glb","bounds_min_cm":[-100,0,0],"bounds_max_cm":[100,20,240]},
 {"module_id":"wall_plain_2m","palette":"warm_lime","layer":"finish","mesh":"Modules/wall_plain_2m.glb","bounds_min_cm":[-100,0,0],"bounds_max_cm":[100,20,240]}
],
"recipes":[{"id":"test_recipe","label":"Test","palette":"warm_lime","kind":"authored_master","pieces":[{"module":"wall_plain_2m","translation_m":[1,2,3],"yaw_degrees":90,"palette":"warm_lime","layers":["%s"],"purpose":"test","instance_key":"%s"},{"module":"wall_plain_2m","translation_m":[0,0,0],"yaw_degrees":0,"palette":"warm_lime","layers":["finish"],"instance_key":"piece-b"}],"cells":[{"z_m":0.4,"cells":[[1,2]],"wall_height_m":2.4}],"bounds_m":{"min":[0,0,0],"max":[2,3,4]}}],
"growth":{"id":"family_house_expansion","source":"test growth","stages":["family_starter","family_cluster"],"rules":["preserve keys"],"transitions":[{"from":"family_starter","to":"family_cluster","retain":["growth-retained"],"dismantle":[{"module":"wall_plain_2m","palette":"warm_lime","layers":["structure"],"instance_key":"growth-remove"}],"add":[{"module":"wall_plain_2m","palette":"warm_lime","layers":["finish"],"instance_key":"growth-add"}]}]}
})json"), *Matrix, *PieceLayer, *PieceKey);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicCatalogTransformTest, "ThreeHearths.OrganicCatalog.Transform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicCatalogTransformTest::RunTest(const FString&)
{
    FHearthOrganicCatalog Catalog; FString Error;
    TestTrue(TEXT("Valid catalog loads"), HearthOrganicCatalog::LoadFromJson(TestCatalogJson(), Catalog, Error));
    AddInfo(Error);
    const FHearthOrganicRecipe* Recipe = HearthOrganicCatalog::FindRecipe(Catalog, TEXT("test_recipe"));
    TestTrue(TEXT("Recipe is queryable"), Recipe != nullptr);
    if (!Recipe) return false;
    TestEqual(TEXT("Metres map to centimetres"), Recipe->Pieces[0].TranslationCm, FVector(100, 200, 300));
    TestEqual(TEXT("Yaw remains relative to the mapped source basis"), Recipe->Pieces[0].UnrealYawDegrees, 90.f);
    TestEqual(TEXT("Piece layers are retained"), Recipe->Pieces[0].Layers.Num(), 1);
    TestEqual(TEXT("Occupied cells retain source level"), Recipe->OccupiedCells[0].Cell, FIntPoint(1, 2));
    TestEqual(TEXT("Occupied cell height is retained"), Recipe->OccupiedCells[0].Zm, .4f);
    TestEqual(TEXT("Layer path comes from the manifest"), HearthOrganicCatalog::ResolveLayerPath(Catalog, TEXT("wall_plain_2m"), TEXT("warm_lime"), TEXT("finish")), FString(TEXT("Modules/wall_plain_2m.glb")));
    TestEqual(TEXT("Original instance key is retained"), Recipe->Pieces[0].OriginalKey, FString(TEXT("piece-a")));
    TestEqual(TEXT("Recipe bounds map to centimetres"), Recipe->BoundsMaxCm, FVector(200, 300, 400));
    TestTrue(TEXT("Growth stays separate"), HearthOrganicCatalog::FindRecipe(Catalog, TEXT("family_growth")) == nullptr && HearthOrganicCatalog::FindGrowth(Catalog) != nullptr && Catalog.Recipes.Num() == 1);
    TestEqual(TEXT("Growth stages are retained"), HearthOrganicCatalog::FindGrowth(Catalog)->Stages.Num(), 2);
    TestTrue(TEXT("Growth original JSON is retained"), HearthOrganicCatalog::FindGrowth(Catalog)->RawJson.Contains(TEXT("family_house_expansion")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicCatalogMatrixTest, "ThreeHearths.OrganicCatalog.Matrix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicCatalogMatrixTest::RunTest(const FString&)
{
    FHearthOrganicCatalog Catalog; FString Error;
    const FString SwapXY = TEXT("[[0,-100,0],[100,0,0],[0,0,100]]");
    TestTrue(TEXT("Axis mapping is accepted"), HearthOrganicCatalog::LoadFromJson(TestCatalogJson(TEXT("structure"), TEXT("swap"), SwapXY), Catalog, Error));
    if (Catalog.Recipes.Num())
    {
        TestEqual(TEXT("Mapped translation uses matrix rows"), Catalog.Recipes[0].Pieces[0].TranslationCm, FVector(-200, 100, 300));
        TestEqual(TEXT("Mapped yaw derives from transformed basis"), Catalog.Recipes[0].Pieces[0].UnrealYawDegrees, 90.f);
        TestEqual(TEXT("Zero source yaw has no extra axis-swap rotation"), Catalog.Recipes[0].Pieces[1].UnrealYawDegrees, 0.f);
    }
    FHearthOrganicSourceMatrix Reflection;
    Reflection.Values[0][0] = 100.f; Reflection.Values[0][1] = 0.f; Reflection.Values[0][2] = 0.f;
    Reflection.Values[1][0] = 0.f; Reflection.Values[1][1] = -100.f; Reflection.Values[1][2] = 0.f;
    Reflection.Values[2][0] = 0.f; Reflection.Values[2][1] = 0.f; Reflection.Values[2][2] = 100.f;
    TestEqual(TEXT("Reflection reverses relative yaw"), Reflection.TransformYawDegrees(90.f), -90.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicCatalogValidationTest, "ThreeHearths.OrganicCatalog.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicCatalogValidationTest::RunTest(const FString&)
{
    FHearthOrganicCatalog Catalog; FString Error;
    TestFalse(TEXT("Unknown layer reference is rejected"), HearthOrganicCatalog::LoadFromJson(TestCatalogJson(TEXT("made_up")), Catalog, Error));
    TestFalse(TEXT("Duplicate instance keys are rejected"), HearthOrganicCatalog::LoadFromJson(TestCatalogJson(TEXT("structure"), TEXT("piece-b")), Catalog, Error));
    TestFalse(TEXT("Non-orthogonal matrix is rejected"), HearthOrganicCatalog::LoadFromJson(TestCatalogJson(TEXT("structure"), TEXT("piece-a"), TEXT("[[100,20,0],[0,100,0],[0,0,100]]")), Catalog, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicCatalogRealContentTest, "ThreeHearths.OrganicCatalog.RealContent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicCatalogRealContentTest::RunTest(const FString&)
{
    FHearthOrganicCatalog Catalog; FString Error;
    TestTrue(TEXT("Generated OrganicMasterCatalog.json loads"), HearthOrganicCatalog::Load(Catalog, Error));
    AddInfo(Error);
    if (!Catalog.Recipes.IsEmpty())
    {
        TestEqual(TEXT("Generated catalog contains the five authored recipes"), Catalog.Recipes.Num(), 5);
        TestTrue(TEXT("Generated catalog retains array-form occupied cells"), Catalog.Recipes[0].OccupiedCells.Num() > 0);
    }
    TestEqual(TEXT("Generated manifest has 114 layer assets"), Catalog.Assets.Num(), 114);
    TestTrue(TEXT("Generated growth remains separate from renderable recipes"), HearthOrganicCatalog::FindRecipe(Catalog, TEXT("family_growth")) == nullptr && HearthOrganicCatalog::FindGrowth(Catalog) != nullptr);
    return true;
}
#endif
