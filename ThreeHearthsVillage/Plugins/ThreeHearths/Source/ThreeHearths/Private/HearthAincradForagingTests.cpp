#if WITH_DEV_AUTOMATION_TESTS
#include "HearthAincradForaging.h"
#include "HearthAincradLife.h"
#include "HearthAincradSurvival.h"
#include "HearthAincradWorldStore.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ForagingTestPath() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ThreeHearths/Tests") / (TEXT("AincradForaging-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)) / TEXT("world.json")); }
void Cleanup(const FString& Path) { auto& Files = FPlatformFileManager::Get().GetPlatformFile(); Files.DeleteFile(*Path); Files.DeleteFile(*(Path + TEXT(".bak"))); Files.DeleteDirectory(*FPaths::GetPath(Path)); }
FString Json(const TSharedRef<FJsonObject>& Object) { FString Text; FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text)); return Text; }
TSharedPtr<FJsonObject> ActiveResident(const TSharedRef<FJsonObject>& World)
{
    for (const auto& Value : World->GetArrayField(TEXT("residents"))) { auto Resident = Value->AsObject(); bool bActive = false; if (Resident->GetObjectField(TEXT("runtime"))->TryGetBoolField(TEXT("active"), bActive) && bActive) return Resident; }
    return nullptr;
}
TSharedPtr<FJsonObject> SurvivalAccount(const TSharedRef<FJsonObject>& World, const FString& Id)
{
    for (const auto& Value : World->GetObjectField(TEXT("survival"))->GetArrayField(TEXT("accounts"))) if (Value->AsObject()->GetStringField(TEXT("resident_id")) == Id) return Value->AsObject();
    return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradForagingTest, "ThreeHearths.AincradForaging.PublicBerrySource", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradForagingTest::RunTest(const FString&)
{
    const FString Path = ForagingTestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> World;
    if (!TestTrue(TEXT("create isolated WorldStore fixture"), HearthAincradWorldStore::LoadOrCreate(Path, World, Error))) return false;
    bool Added = false; if (!TestTrue(TEXT("install life"), HearthAincradLife::Initialize(World.ToSharedRef(), Added, Error))) return false;
    if (!TestTrue(TEXT("install survival"), HearthAincradSurvival::Initialize(World.ToSharedRef(), Added, Error))) return false;
    TestTrue(TEXT("optional missing foraging validates"), HearthAincradForaging::Validate(World.ToSharedRef(), Error));
    if (!TestTrue(TEXT("install public source"), HearthAincradForaging::Initialize(World.ToSharedRef(), Added, Error))) return false; TestTrue(TEXT("first install reports added"), Added);
    const FString Installed = Json(World.ToSharedRef()); TestTrue(TEXT("repeat install succeeds"), HearthAincradForaging::Initialize(World.ToSharedRef(), Added, Error)); TestFalse(TEXT("repeat adds nothing"), Added); TestEqual(TEXT("repeat is byte-stable"), Json(World.ToSharedRef()), Installed);

    auto Resident = ActiveResident(World.ToSharedRef()); const FString Id = Resident->GetStringField(TEXT("stable_id")); auto Account = SurvivalAccount(World.ToSharedRef(), Id); Account->SetNumberField(TEXT("food"), 0);
    const FVector Point = HearthAincradForaging::WorkPoint(); Resident->GetObjectField(TEXT("runtime"))->SetArrayField(TEXT("position_cm"), { MakeShared<FJsonValueNumber>(Point.X), MakeShared<FJsonValueNumber>(Point.Y), MakeShared<FJsonValueNumber>(Point.Z) });
    const FString Option = TEXT("harvest_ration:starter_commons_berry_patch"); TestEqual(TEXT("eligible resident sees one source option"), HearthAincradForaging::Options(World.ToSharedRef(), Id).Num(), 1);
    TestTrue(TEXT("harvest transfers one finite ration"), HearthAincradForaging::Apply(World.ToSharedRef(), Id, Option, Error)); auto Source = World->GetObjectField(TEXT("foraging")); TestEqual(TEXT("private food increments"), Account->GetNumberField(TEXT("food")), 1.0); TestEqual(TEXT("public stock decrements"), Source->GetNumberField(TEXT("stock")), 2.0); TestEqual(TEXT("harvest counter increments"), Source->GetNumberField(TEXT("harvested_total")), 1.0);
    TestTrue(TEXT("growth remainder accumulates"), HearthAincradForaging::Tick(World.ToSharedRef(), 1799, Error)); TestEqual(TEXT("stock waits for interval"), Source->GetNumberField(TEXT("stock")), 2.0); TestTrue(TEXT("split interval regrows one"), HearthAincradForaging::Tick(World.ToSharedRef(), 1, Error)); TestEqual(TEXT("stock returns to capacity"), Source->GetNumberField(TEXT("stock")), 3.0); TestEqual(TEXT("full stock clears remainder"), Source->GetNumberField(TEXT("growth_remainder_seconds")), 0.0);

    Source->SetNumberField(TEXT("growth_remainder_seconds"), 1); TestFalse(TEXT("persisted full source with remainder is invalid"), HearthAincradForaging::Validate(World.ToSharedRef(), Error)); Source->SetNumberField(TEXT("growth_remainder_seconds"), 0);
    const FString BeforeRemote = Json(World.ToSharedRef()); Resident->GetObjectField(TEXT("runtime"))->SetArrayField(TEXT("position_cm"), { MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(92) }); const FString BeforeApply = Json(World.ToSharedRef()); TestFalse(TEXT("remote harvest rejected"), HearthAincradForaging::Apply(World.ToSharedRef(), Id, Option, Error)); TestEqual(TEXT("remote rejection writes nothing"), Json(World.ToSharedRef()), BeforeApply); TestTrue(TEXT("prior state existed"), !BeforeRemote.IsEmpty());
    World->SetStringField(TEXT("foraging"), TEXT("bad")); TestFalse(TEXT("bad optional field type is rejected"), HearthAincradForaging::Validate(World.ToSharedRef(), Error)); return true;
}
#endif
