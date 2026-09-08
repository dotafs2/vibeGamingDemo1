#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradWorldStore.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString TestPath()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ThreeHearths/Tests") / (TEXT("Aincrad-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)) / TEXT("world.json"));
    }

    void Cleanup(const FString& Path)
    {
        IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
        Files.DeleteFile(*Path); Files.DeleteFile(*(Path + TEXT(".bak"))); Files.DeleteFile(*(Path + TEXT(".pre-v2"))); Files.DeleteDirectory(*FPaths::GetPath(Path));
    }

    bool SerializeState(const TSharedPtr<FJsonObject>& State, FString& OutText)
    {
        return State.IsValid() && FJsonSerializer::Serialize(State.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutText));
    }

    bool WriteJson(const FString& Path, const TSharedPtr<FJsonObject>& State)
    {
        FString Text;
        return SerializeState(State, Text) && FFileHelper::SaveStringToFile(Text, *Path);
    }

    bool ReadText(const FString& Path, FString& OutText)
    {
        return FFileHelper::LoadFileToString(OutText, *Path);
    }

    void MakeV1Fixture(TSharedPtr<FJsonObject>& State)
    {
        State->SetNumberField(TEXT("schema_version"), 1); State->RemoveField(TEXT("town_layout_revision")); State->RemoveField(TEXT("buildings")); State->RemoveField(TEXT("building_bindings"));
        const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr; State->TryGetArrayField(TEXT("residents"), Residents);
        for (const TSharedPtr<FJsonValue>& Value : *Residents) Value->AsObject()->RemoveField(TEXT("runtime"));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradColdLoadTest, "ThreeHearths.AincradWorldStore.ColdLoadIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradColdLoadTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); };
    FString Error; TSharedPtr<FJsonObject> First, Second;
    if (!TestTrue(TEXT("Initial Aincrad v2 state is created"), HearthAincradWorldStore::LoadOrCreate(Path, First, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Initial schema is v2"), First->GetIntegerField(TEXT("schema_version")), 2); TestEqual(TEXT("Geographical layout remains revision 1"), First->GetIntegerField(TEXT("layout_revision")), 1); TestEqual(TEXT("Town layout starts at revision 1"), First->GetIntegerField(TEXT("town_layout_revision")), 1);
    TArray<FString> FirstIds, FirstStories, FirstHomes; TArray<double> FirstBalances; const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr; First->TryGetArrayField(TEXT("residents"), Residents);
    for (const TSharedPtr<FJsonValue>& Value : *Residents) { const TSharedPtr<FJsonObject> Resident = Value->AsObject(); FirstIds.Add(Resident->GetStringField(TEXT("stable_id"))); FirstStories.Add(Resident->GetStringField(TEXT("story"))); FirstHomes.Add(Resident->GetStringField(TEXT("home_id"))); FirstBalances.Add(Resident->GetNumberField(TEXT("coins_col"))); }
    (*Residents)[0]->AsObject()->GetObjectField(TEXT("runtime"))->SetStringField(TEXT("parent_runtime_key"), TEXT("roundtrip"));
    if (!TestTrue(TEXT("v2 runtime state saves"), HearthAincradWorldStore::Save(Path, First.ToSharedRef(), Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Cold load succeeds"), HearthAincradWorldStore::LoadOrCreate(Path, Second, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("World identity survives cold load"), Second->GetStringField(TEXT("world_id")), First->GetStringField(TEXT("world_id"))); const TArray<TSharedPtr<FJsonValue>>* Reloaded = nullptr; Second->TryGetArrayField(TEXT("residents"), Reloaded); TestEqual(TEXT("13 residents persist"), Reloaded->Num(), 13);
    for (int32 Index = 0; Index < Reloaded->Num(); ++Index) { const TSharedPtr<FJsonObject> Resident = (*Reloaded)[Index]->AsObject(); TestEqual(TEXT("Stable ID survives cold load"), Resident->GetStringField(TEXT("stable_id")), FirstIds[Index]); TestEqual(TEXT("Story survives cold load exactly"), Resident->GetStringField(TEXT("story")), FirstStories[Index]); TestEqual(TEXT("Home ID survives cold load exactly"), Resident->GetStringField(TEXT("home_id")), FirstHomes[Index]); TestEqual(TEXT("Balance survives cold load exactly"), Resident->GetNumberField(TEXT("coins_col")), FirstBalances[Index]); }
    TestEqual(TEXT("Additional runtime keys roundtrip"), (*Reloaded)[0]->AsObject()->GetObjectField(TEXT("runtime"))->GetStringField(TEXT("parent_runtime_key")), FString(TEXT("roundtrip"))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradMigrationTest, "ThreeHearths.AincradWorldStore.V1ToV2Migration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradMigrationTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); };
    FString Error; TSharedPtr<FJsonObject> State; if (!TestTrue(TEXT("Fixture source is created"), HearthAincradWorldStore::LoadOrCreate(Path, State, Error))) { AddError(Error); return false; }
    State->SetNumberField(TEXT("elapsed_seconds"), 1234.5); const FString WorldId = State->GetStringField(TEXT("world_id")); TArray<FString> Ids, Stories, Homes; TArray<double> Balances; const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr; State->TryGetArrayField(TEXT("residents"), Residents); for (const TSharedPtr<FJsonValue>& Value : *Residents) { const TSharedPtr<FJsonObject> Resident = Value->AsObject(); Ids.Add(Resident->GetStringField(TEXT("stable_id"))); Stories.Add(Resident->GetStringField(TEXT("story"))); Homes.Add(Resident->GetStringField(TEXT("home_id"))); Balances.Add(Resident->GetNumberField(TEXT("coins_col"))); }
    MakeV1Fixture(State); if (!TestTrue(TEXT("v1 fixture is written"), WriteJson(Path, State))) return false; FString OriginalV1; ReadText(Path, OriginalV1);
    TSharedPtr<FJsonObject> Migrated; if (!TestTrue(TEXT("Explicit v1 migration succeeds"), HearthAincradWorldStore::MigrateV1ToV2(Path, Migrated, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Migration preserves world identity"), Migrated->GetStringField(TEXT("world_id")), WorldId); TestEqual(TEXT("Migration preserves elapsed seconds"), Migrated->GetNumberField(TEXT("elapsed_seconds")), 1234.5); TestEqual(TEXT("Migration writes schema v2"), Migrated->GetIntegerField(TEXT("schema_version")), 2);
    const TArray<TSharedPtr<FJsonValue>>* MigratedResidents = nullptr; Migrated->TryGetArrayField(TEXT("residents"), MigratedResidents); for (int32 Index = 0; Index < MigratedResidents->Num(); ++Index) { const TSharedPtr<FJsonObject> Resident = (*MigratedResidents)[Index]->AsObject(); TestEqual(TEXT("Migration preserves stable identity"), Resident->GetStringField(TEXT("stable_id")), Ids[Index]); TestEqual(TEXT("Migration preserves story"), Resident->GetStringField(TEXT("story")), Stories[Index]); TestEqual(TEXT("Migration preserves home"), Resident->GetStringField(TEXT("home_id")), Homes[Index]); TestEqual(TEXT("Migration preserves balance"), Resident->GetNumberField(TEXT("coins_col")), Balances[Index]); }
    TestTrue(TEXT("One-time v1 backup exists"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Path + TEXT(".pre-v2")))); FString Backup; ReadText(Path + TEXT(".pre-v2"), Backup); TestEqual(TEXT("One-time backup contains original v1"), Backup, OriginalV1);
    FString ErrorOnRestart; TSharedPtr<FJsonObject> Restarted; TestTrue(TEXT("Restart load is idempotent"), HearthAincradWorldStore::LoadOrCreate(Path, Restarted, ErrorOnRestart)); FString BackupAfterRestart; ReadText(Path + TEXT(".pre-v2"), BackupAfterRestart); TestEqual(TEXT("Restart does not overwrite v1 backup"), BackupAfterRestart, OriginalV1); TSharedPtr<FJsonObject> ExplicitRestart; TestTrue(TEXT("Explicit migration is idempotent on v2"), HearthAincradWorldStore::MigrateV1ToV2(Path, ExplicitRestart, ErrorOnRestart));
    TestEqual(TEXT("Inn spawn is stored in UE centimeters"), (*MigratedResidents)[0]->AsObject()->GetObjectField(TEXT("runtime"))->GetArrayField(TEXT("position_cm"))[0]->AsNumber(), -400.0); TestEqual(TEXT("Carpenter spawn is stored in UE centimeters"), (*MigratedResidents)[7]->AsObject()->GetObjectField(TEXT("runtime"))->GetArrayField(TEXT("position_cm"))[1]->AsNumber(), 463700.0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradMigrationOrderTest, "ThreeHearths.AincradWorldStore.MigrationIgnoresResidentArrayOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradMigrationOrderTest::RunTest(const FString&)
{
    const FString Path=TestPath(); ON_SCOPE_EXIT { Cleanup(Path); };
    FString Error; TSharedPtr<FJsonObject> State;
    if(!HearthAincradWorldStore::LoadOrCreate(Path,State,Error))return false;
    MakeV1Fixture(State); auto Residents=State->GetArrayField(TEXT("residents"));
    const FString InnId=Residents[0]->AsObject()->GetStringField(TEXT("stable_id"));
    Residents.Swap(0,4);Residents.Swap(1,7);State->SetArrayField(TEXT("residents"),Residents);
    if(!WriteJson(Path,State))return false;
    TSharedPtr<FJsonObject> Migrated;
    if(!TestTrue(TEXT("Reordered valid v1 migrates"),HearthAincradWorldStore::MigrateV1ToV2(Path,Migrated,Error))){AddError(Error);return false;}
    TestEqual(TEXT("Inn owner follows identity and role"),Migrated->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetStringField(TEXT("owner_id")),InnId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradFutureVersionTest, "ThreeHearths.AincradWorldStore.FutureVersionRefusesOverwrite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradFutureVersionTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> State; HearthAincradWorldStore::LoadOrCreate(Path, State, Error); State->SetNumberField(TEXT("schema_version"), 99); if (!WriteJson(Path, State)) return false; FString Before; ReadText(Path, Before); TSharedPtr<FJsonObject> Loaded; TestFalse(TEXT("Unknown future schema is rejected"), HearthAincradWorldStore::LoadOrCreate(Path, Loaded, Error)); FString After; ReadText(Path, After); TestEqual(TEXT("Future schema file is preserved"), After, Before); TestFalse(TEXT("Future schema does not create migration backup"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Path + TEXT(".pre-v2")))); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradCorruptFileTest, "ThreeHearths.AincradWorldStore.CorruptFileRefusesReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradCorruptFileTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; const FString Corrupt = TEXT("{\"world_id\":"); FString Error; FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path)); if (!TestTrue(TEXT("Corrupt fixture was actually written"), FFileHelper::SaveStringToFile(Corrupt, *Path))) return false; TSharedPtr<FJsonObject> State; TestFalse(TEXT("Corrupt existing file fails"), HearthAincradWorldStore::LoadOrCreate(Path, State, Error)); FString Preserved; FFileHelper::LoadFileToString(Preserved, *Path); TestEqual(TEXT("Corrupt file is not reset"), Preserved, Corrupt); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradCrossWorldTest, "ThreeHearths.AincradWorldStore.CrossWorldOverwriteRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradCrossWorldTest::RunTest(const FString&)
{
    const FString Path = TestPath(); const FString OtherPath = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); Cleanup(OtherPath); }; FString Error; TSharedPtr<FJsonObject> State, Other; HearthAincradWorldStore::LoadOrCreate(Path, State, Error); HearthAincradWorldStore::LoadOrCreate(OtherPath, Other, Error); Other->SetNumberField(TEXT("elapsed_seconds"), 55); FString Before; ReadText(Path, Before); Other->SetStringField(TEXT("world_id"), FGuid::NewGuid().ToString()); TestFalse(TEXT("Different world identity cannot overwrite"), HearthAincradWorldStore::Save(Path, Other.ToSharedRef(), Error)); FString After; ReadText(Path, After); TestEqual(TEXT("Cross-world rejection preserves target"), After, Before); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradWrongSettingTest, "ThreeHearths.AincradWorldStore.WrongSettingRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradWrongSettingTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> State; HearthAincradWorldStore::LoadOrCreate(Path, State, Error); State->SetStringField(TEXT("setting_id"), TEXT("other_setting_profile")); if (!WriteJson(Path, State)) return false; TSharedPtr<FJsonObject> Loaded; TestFalse(TEXT("Wrong setting is rejected"), HearthAincradWorldStore::LoadOrCreate(Path, Loaded, Error)); return true;
}

#endif
