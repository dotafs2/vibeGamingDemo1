#if WITH_DEV_AUTOMATION_TESTS
#include "HearthAincradSurvival.h"
#include "HearthAincradTownLayout.h"
#include "HearthAincradWorldStore.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString TestPath() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ThreeHearths/Tests") / (TEXT("AincradSurvival-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)) / TEXT("world.json")); }
void Cleanup(const FString& Path) { auto& Files = FPlatformFileManager::Get().GetPlatformFile(); Files.DeleteFile(*Path); Files.DeleteFile(*(Path + TEXT(".bak"))); Files.DeleteDirectory(*FPaths::GetPath(Path)); }
FString Json(const TSharedRef<FJsonObject>& O) { FString Text; FJsonSerializer::Serialize(O, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text)); return Text; }
TSharedPtr<FJsonObject> Resident(const TSharedRef<FJsonObject>& W, const FString& Id) { for (const auto& V : W->GetArrayField(TEXT("residents"))) if (V->AsObject()->GetStringField(TEXT("stable_id")) == Id) return V->AsObject(); return nullptr; }
FString ActiveId(const TSharedRef<FJsonObject>& W, int32 Wanted = 0) { int32 Seen = 0; for (const auto& V : W->GetArrayField(TEXT("residents"))) { auto R = V->AsObject(); bool b = false; if (R->GetObjectField(TEXT("runtime"))->TryGetBoolField(TEXT("active"), b) && b && Seen++ == Wanted) return R->GetStringField(TEXT("stable_id")); } return FString(); }
TSharedPtr<FJsonObject> Account(const TSharedRef<FJsonObject>& W, const FString& Id) { for (const auto& V : W->GetObjectField(TEXT("survival"))->GetArrayField(TEXT("accounts"))) if (V->AsObject()->GetStringField(TEXT("resident_id")) == Id) return V->AsObject(); return nullptr; }
bool HasOption(const TSharedRef<FJsonObject>& W, const FString& Id, const FString& OptionId, FString* Target = nullptr) { for (const auto& V : HearthAincradSurvival::Options(W, Id)) { auto O = V->AsObject(); if (O->GetStringField(TEXT("id")) == OptionId) { if (Target) *Target = O->GetStringField(TEXT("target_building_id")); return true; } } return false; }
void PutAtWork(const TSharedRef<FJsonObject>& W, const FString& Id, const FString& BuildingId) { const auto Plan = HearthAincradTownLayout::Build(); const auto* Site = HearthAincradTownLayout::Find(Plan, BuildingId); Resident(W, Id)->GetObjectField(TEXT("runtime"))->SetArrayField(TEXT("position_cm"), { MakeShared<FJsonValueNumber>(Site->WorkCm.X), MakeShared<FJsonValueNumber>(Site->WorkCm.Y), MakeShared<FJsonValueNumber>(Site->WorkCm.Z) }); }
int64 Coins(const TSharedRef<FJsonObject>& W) { int64 Total = 0; for (const auto& V : W->GetArrayField(TEXT("residents"))) Total += static_cast<int64>(V->AsObject()->GetNumberField(TEXT("coins_col"))); return Total; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradSurvivalInitializationTest, "ThreeHearths.AincradSurvival.InitializationAndColdReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradSurvivalInitializationTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> World;
    if (!TestTrue(TEXT("create isolated WorldStore fixture"), HearthAincradWorldStore::LoadOrCreate(Path, World, Error))) { AddError(Error); return false; }
    const FString WorldId = World->GetStringField(TEXT("world_id")); const int32 Residents = World->GetArrayField(TEXT("residents")).Num(); const int64 InitialCoins = Coins(World.ToSharedRef()); bool Added = false;
    if (!TestTrue(TEXT("install survival"), HearthAincradSurvival::Initialize(World.ToSharedRef(), Added, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("first install reports added"), Added); auto S = World->GetObjectField(TEXT("survival")); TestEqual(TEXT("three active accounts"), S->GetArrayField(TEXT("accounts")).Num(), 3); TestEqual(TEXT("three matching initial conditions"), S->GetArrayField(TEXT("initial_conditions")).Num(), 3);
    for (const auto& V : S->GetArrayField(TEXT("accounts"))) { auto A = V->AsObject(); const FString Id = A->GetStringField(TEXT("resident_id")); int32 Matches = 0; for (const auto& C : S->GetArrayField(TEXT("initial_conditions"))) if (C->AsObject()->GetStringField(TEXT("resident_id")) == Id) ++Matches; TestEqual(TEXT("exactly one condition per account"), Matches, 1); TestEqual(TEXT("finite initial ration"), A->GetNumberField(TEXT("food")), 2.0); TestEqual(TEXT("bootstrap source"), A->GetStringField(TEXT("source")), FString(TEXT("developer_survival_bootstrap"))); }
    const FString Installed = Json(World.ToSharedRef()); TestTrue(TEXT("repeat install succeeds"), HearthAincradSurvival::Initialize(World.ToSharedRef(), Added, Error)); TestFalse(TEXT("repeat adds nothing"), Added); TestEqual(TEXT("repeat is byte-stable"), Json(World.ToSharedRef()), Installed);
    TestTrue(TEXT("save extension"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error)); TSharedPtr<FJsonObject> Reloaded; TestTrue(TEXT("cold reload extension"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error)); TestTrue(TEXT("reload validates"), HearthAincradSurvival::Validate(Reloaded.ToSharedRef(), Error));
    TestEqual(TEXT("world identity retained"), Reloaded->GetStringField(TEXT("world_id")), WorldId); TestEqual(TEXT("all residents retained"), Reloaded->GetArrayField(TEXT("residents")).Num(), Residents); TestEqual(TEXT("coins untouched"), Coins(Reloaded.ToSharedRef()), InitialCoins); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradSurvivalTickTest, "ThreeHearths.AincradSurvival.TickBoundsAndInvalidState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradSurvivalTickTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> World; if (!TestTrue(TEXT("create fixture"), HearthAincradWorldStore::LoadOrCreate(Path, World, Error))) return false; bool Added = false; if (!TestTrue(TEXT("install"), HearthAincradSurvival::Initialize(World.ToSharedRef(), Added, Error))) return false; const FString Id = ActiveId(World.ToSharedRef());
    TestTrue(TEXT("119 seconds accumulate"), HearthAincradSurvival::Tick(World.ToSharedRef(), 119, Error)); TestEqual(TEXT("no early decay"), Account(World.ToSharedRef(), Id)->GetNumberField(TEXT("energy")), 100.0); TestTrue(TEXT("split interval completes"), HearthAincradSurvival::Tick(World.ToSharedRef(), 1, Error)); TestEqual(TEXT("one energy decays"), Account(World.ToSharedRef(), Id)->GetNumberField(TEXT("energy")), 99.0); TestEqual(TEXT("one hunger decays"), Resident(World.ToSharedRef(), Id)->GetObjectField(TEXT("needs"))->GetNumberField(TEXT("hunger")), 99.0);
    TestTrue(TEXT("huge finite tick saturates"), HearthAincradSurvival::Tick(World.ToSharedRef(), 1.0e300, Error)); TestEqual(TEXT("energy floors"), Account(World.ToSharedRef(), Id)->GetNumberField(TEXT("energy")), 0.0); TestEqual(TEXT("hunger floors"), Resident(World.ToSharedRef(), Id)->GetObjectField(TEXT("needs"))->GetNumberField(TEXT("hunger")), 0.0); TestTrue(TEXT("saturated state validates"), HearthAincradSurvival::Validate(World.ToSharedRef(), Error));
    auto S = World->GetObjectField(TEXT("survival")); S->GetArrayField(TEXT("initial_conditions"))[0]->AsObject()->SetStringField(TEXT("source"), TEXT("forged")); const FString Before = Json(World.ToSharedRef()); TestFalse(TEXT("forged condition rejected"), HearthAincradSurvival::Tick(World.ToSharedRef(), 120, Error)); TestEqual(TEXT("rejected tick is atomic"), Json(World.ToSharedRef()), Before);
    S->GetArrayField(TEXT("initial_conditions"))[0]->AsObject()->SetStringField(TEXT("source"), TEXT("developer_survival_bootstrap")); auto Accounts = S->GetArrayField(TEXT("accounts")); auto Conditions = S->GetArrayField(TEXT("initial_conditions")); Accounts.RemoveAt(Accounts.Num() - 1); Conditions.RemoveAt(Conditions.Num() - 1); S->SetArrayField(TEXT("accounts"), Accounts); S->SetArrayField(TEXT("initial_conditions"), Conditions); TestFalse(TEXT("matching omissions still reject silent under-install"), HearthAincradSurvival::Validate(World.ToSharedRef(), Error)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradSurvivalOptionsTest, "ThreeHearths.AincradSurvival.PrivateFiniteStationActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradSurvivalOptionsTest::RunTest(const FString&)
{
    const FString Path = TestPath(); ON_SCOPE_EXIT { Cleanup(Path); }; FString Error; TSharedPtr<FJsonObject> World; if (!TestTrue(TEXT("create fixture"), HearthAincradWorldStore::LoadOrCreate(Path, World, Error))) return false; bool Added = false; if (!TestTrue(TEXT("install"), HearthAincradSurvival::Initialize(World.ToSharedRef(), Added, Error))) return false;
    const FString Id = ActiveId(World.ToSharedRef()), Other = ActiveId(World.ToSharedRef(), 1); const int64 InitialCoins = Coins(World.ToSharedRef()); TestTrue(TEXT("make needs actionable"), HearthAincradSurvival::Tick(World.ToSharedRef(), 3600, Error)); const FString Eat = TEXT("eat_ration:") + Id, Rest = TEXT("rest:") + Id; FString Target;
    Resident(World.ToSharedRef(), Id)->GetObjectField(TEXT("runtime"))->SetArrayField(TEXT("position_cm"), { MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0) }); TestTrue(TEXT("eat offered before travel"), HasOption(World.ToSharedRef(), Id, Eat, &Target)); TestTrue(TEXT("rest offered before travel"), HasOption(World.ToSharedRef(), Id, Rest)); const FString Before = Json(World.ToSharedRef());
    TestFalse(TEXT("away apply rejected"), HearthAincradSurvival::Apply(World.ToSharedRef(), Id, Eat, Error)); TestEqual(TEXT("away rejection atomic"), Json(World.ToSharedRef()), Before); TestFalse(TEXT("foreign option id rejected"), HearthAincradSurvival::Apply(World.ToSharedRef(), Id, TEXT("eat_ration:") + Other, Error));
    PutAtWork(World.ToSharedRef(), Id, Target); TestTrue(TEXT("eat at own station"), HearthAincradSurvival::Apply(World.ToSharedRef(), Id, Eat, Error)); TestTrue(TEXT("rest at own station"), HearthAincradSurvival::Apply(World.ToSharedRef(), Id, Rest, Error)); auto Context = HearthAincradSurvival::PersonalContext(World.ToSharedRef(), Id);
    TestEqual(TEXT("one ration remains"), Context->GetNumberField(TEXT("food")), 1.0); TestEqual(TEXT("hunger restored"), Context->GetNumberField(TEXT("hunger_satisfaction")), 100.0); TestEqual(TEXT("energy restored"), Context->GetNumberField(TEXT("energy")), 100.0); TestFalse(TEXT("eat hidden when full"), HasOption(World.ToSharedRef(), Id, Eat)); TestFalse(TEXT("rest hidden when full"), HasOption(World.ToSharedRef(), Id, Rest)); TestEqual(TEXT("coins conserved"), Coins(World.ToSharedRef()), InitialCoins);
    auto Unknown = HearthAincradSurvival::PersonalContext(World.ToSharedRef(), TEXT("not-a-resident")); TestFalse(TEXT("unknown gets no food"), Unknown->HasField(TEXT("food"))); TestFalse(TEXT("context has no resident id"), Context->HasField(TEXT("resident_id"))); TestFalse(TEXT("context has no account array"), Context->HasField(TEXT("accounts"))); return true;
}
#endif
