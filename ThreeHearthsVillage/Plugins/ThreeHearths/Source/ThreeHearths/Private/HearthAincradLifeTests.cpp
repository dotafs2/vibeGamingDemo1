#if WITH_DEV_AUTOMATION_TESTS

#include "HearthAincradLife.h"

#include "HearthAincradTownLayout.h"
#include "HearthAincradWorldStore.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString LifeTestPath()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ThreeHearths/Tests") / (TEXT("AincradLife-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)) / TEXT("world.json"));
    }

    void LifeCleanup(const FString& Path)
    {
        IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
        Files.DeleteFile(*Path); Files.DeleteFile(*(Path + TEXT(".bak"))); Files.DeleteDirectory(*FPaths::GetPath(Path));
    }

    bool LifeTextOf(const TSharedRef<FJsonObject>& Object, FString& Out)
    {
        return FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
    }

    FString LifeResidentId(const TSharedRef<FJsonObject>& World, const FString& Name)
    {
        const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
        if (!World->TryGetArrayField(TEXT("residents"), Residents)) return FString();
        for (const TSharedPtr<FJsonValue>& Value : *Residents)
        {
            const TSharedPtr<FJsonObject> LifeResident = Value->AsObject();
            if (LifeResident->GetStringField(TEXT("name")) == Name) return LifeResident->GetStringField(TEXT("stable_id"));
        }
        return FString();
    }

    TSharedPtr<FJsonObject> LifeResident(const TSharedRef<FJsonObject>& World, const FString& Id)
    {
        const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
        if (!World->TryGetArrayField(TEXT("residents"), Residents)) return nullptr;
        for (const TSharedPtr<FJsonValue>& Value : *Residents) if (Value->AsObject()->GetStringField(TEXT("stable_id")) == Id) return Value->AsObject();
        return nullptr;
    }

    void LifePutAtWork(const TSharedRef<FJsonObject>& World, const FString& ResidentStableId, const FString& BuildingId)
    {
        const HearthAincradTownLayout::FPlan Plan = HearthAincradTownLayout::Build();
        const HearthAincradTownLayout::FBuilding* Building = HearthAincradTownLayout::Find(Plan, BuildingId);
        const TSharedPtr<FJsonObject> Runtime = LifeResident(World, ResidentStableId)->GetObjectField(TEXT("runtime"));
        Runtime->SetArrayField(TEXT("position_cm"), { MakeShared<FJsonValueNumber>(Building->WorkCm.X), MakeShared<FJsonValueNumber>(Building->WorkCm.Y), MakeShared<FJsonValueNumber>(Building->WorkCm.Z) });
    }

    TSharedPtr<FJsonObject> LifeContract(const TSharedRef<FJsonObject>& World, int32 Index = 0)
    {
        const TSharedPtr<FJsonObject> Life = World->GetObjectField(TEXT("life"));
        const TArray<TSharedPtr<FJsonValue>>& Values = Life->GetArrayField(TEXT("contracts"));
        return Values.IsValidIndex(Index) ? Values[Index]->AsObject() : nullptr;
    }

    TSharedPtr<FJsonObject> LifeItem(const TSharedRef<FJsonObject>& World)
    {
        return World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("items"))[0]->AsObject();
    }

    TSharedPtr<FJsonObject> LifeAccount(const TSharedRef<FJsonObject>& World, const FString& Id)
    {
        for (const auto& Value : World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("accounts")))
            if (Value->AsObject()->GetStringField(TEXT("resident_id")) == Id) return Value->AsObject();
        return nullptr;
    }

    bool LifeStep(TSharedPtr<FJsonObject>& World, const FString& Path, const FString& ActorId,
        const FString& Option, const FString& Operation, const FString& Speech, FString& Error)
    {
        if (!HearthAincradLife::Apply(World.ToSharedRef(), ActorId, Option, Operation, Speech, Error)
            || !HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error)) return false;
        TSharedPtr<FJsonObject> Reloaded;
        if (!HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error)) return false;
        World = Reloaded;
        return true;
    }

    int64 LifeCoinsTotal(const TSharedRef<FJsonObject>& World)
    {
        int64 Total = 0;
        for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("residents"))) Total += static_cast<int64>(Value->AsObject()->GetNumberField(TEXT("coins_col")));
        return Total;
    }

    bool LifeStartWorld(TSharedPtr<FJsonObject>& World, FString& Path, FString& Error)
    {
        Path = LifeTestPath();
        if (!HearthAincradWorldStore::LoadOrCreate(Path, World, Error)) return false;
        bool bAdded = false;
        return HearthAincradLife::Initialize(World.ToSharedRef(), bAdded, Error) && bAdded;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeInitializationTest, "ThreeHearths.AincradLife.InitializationAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeInitializationTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error; if (!TestTrue(TEXT("initial world and life extension are created"), LifeStartWorld(World, Path, Error))) { AddError(Error); return false; } ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳")); const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真")); const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    TArray<FString> Ids, Stories; for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("residents"))) { Ids.Add(Value->AsObject()->GetStringField(TEXT("stable_id"))); Stories.Add(Value->AsObject()->GetStringField(TEXT("story"))); }
    TestTrue(TEXT("life validates after initialization"), HearthAincradLife::Validate(World.ToSharedRef(), Error)); TestEqual(TEXT("life version is independent version 1"), World->GetObjectField(TEXT("life"))->GetIntegerField(TEXT("version")), 1); TestEqual(TEXT("three initial important events are inboxed"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), 3);
    const TSharedRef<FJsonObject> ErinContext = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId); TestEqual(TEXT("Erin inbox starts at one"), ErinContext->GetIntegerField(TEXT("inbox_seq")), 1); TestTrue(TEXT("context exposes options array"), ErinContext->HasTypedField<EJson::Array>(TEXT("options"))); TestEqual(TEXT("Takuya has no private Erin item"), HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetArrayField(TEXT("items")).Num(), 0); TestTrue(TEXT("Kashiwagi is known by stable identity"), !KashiwagiId.IsEmpty());
    TestTrue(TEXT("life-enabled state saves"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error)); TSharedPtr<FJsonObject> Reloaded; TestTrue(TEXT("life-enabled state reloads"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error)); TestTrue(TEXT("reloaded life validates"), HearthAincradLife::Validate(Reloaded.ToSharedRef(), Error));
    const TArray<TSharedPtr<FJsonValue>>& ReloadedResidents = Reloaded->GetArrayField(TEXT("residents")); for (int32 Index = 0; Index < ReloadedResidents.Num(); ++Index) { TestEqual(TEXT("stable identity survives life reload"), ReloadedResidents[Index]->AsObject()->GetStringField(TEXT("stable_id")), Ids[Index]); TestEqual(TEXT("story survives life reload"), ReloadedResidents[Index]->AsObject()->GetStringField(TEXT("story")), Stories[Index]); }
    FString Before; LifeTextOf(Reloaded.ToSharedRef(), Before); Reloaded->RemoveField(TEXT("life")); TestFalse(TEXT("life save cannot be overwritten by a life-less state"), HearthAincradWorldStore::Save(Path, Reloaded.ToSharedRef(), Error)); FString After; FFileHelper::LoadFileToString(After, *Path); TestEqual(TEXT("life-enabled file remains intact after rejected overwrite"), After, Before); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeFullRepairTest, "ThreeHearths.AincradLife.TwoStageRepairAndUse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeFullRepairTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error; if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; } ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳")); const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真")); const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木")); const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id")); const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    const FString EdgeOption = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId); LifePutAtWork(World.ToSharedRef(), TakuyaId, TEXT("sao_smithy_01")); TestTrue(TEXT("edge proposal succeeds"), LifeStep(World, Path, ErinId, EdgeOption, TEXT("op-edge-propose"), TEXT("先修刃"), Error)); const FString EdgeContract = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id")); TestTrue(TEXT("worker accepts at own work point"), LifeStep(World, Path, TakuyaId, TEXT("accept:") + EdgeContract, TEXT("op-edge-accept"), TEXT("我接下"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_smithy_01")); TestTrue(TEXT("owner delivers face to face"), LifeStep(World, Path, ErinId, TEXT("deliver:") + EdgeContract, TEXT("op-edge-deliver"), TEXT("斧子给你"), Error)); TestTrue(TEXT("worker completes edge repair"), LifeStep(World, Path, TakuyaId, TEXT("work:") + EdgeContract, TEXT("op-edge-work"), TEXT("开始修刃"), Error)); TestTrue(TEXT("owner collects and pays once"), LifeStep(World, Path, ErinId, TEXT("collect:") + EdgeContract, TEXT("op-edge-collect"), TEXT("取回斧子"), Error));
    const FString HandleOption = FString::Printf(TEXT("repair_handle:%s:%s:5"), *AxeId, *KashiwagiId); LifePutAtWork(World.ToSharedRef(), KashiwagiId, TEXT("sao_carpentry_01")); TestTrue(TEXT("handle proposal succeeds"), LifeStep(World, Path, ErinId, HandleOption, TEXT("op-handle-propose"), TEXT("再修柄"), Error)); const FString HandleContract = LifeContract(World.ToSharedRef(), 1)->GetStringField(TEXT("id")); TestTrue(TEXT("wood worker accepts"), LifeStep(World, Path, KashiwagiId, TEXT("accept:") + HandleContract, TEXT("op-handle-accept"), TEXT("可以"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_carpentry_01")); TestTrue(TEXT("handle delivery succeeds"), LifeStep(World, Path, ErinId, TEXT("deliver:") + HandleContract, TEXT("op-handle-deliver"), TEXT("交给你"), Error)); TestTrue(TEXT("handle work consumes one wood"), LifeStep(World, Path, KashiwagiId, TEXT("work:") + HandleContract, TEXT("op-handle-work"), TEXT("开始修柄"), Error)); TestTrue(TEXT("handle collection settles"), LifeStep(World, Path, ErinId, TEXT("collect:") + HandleContract, TEXT("op-handle-collect"), TEXT("取回"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_inn_01")); TestTrue(TEXT("fully repaired axe makes kindling"), LifeStep(World, Path, ErinId, TEXT("use_tool:") + AxeId, TEXT("op-use-tool"), TEXT("劈柴"), Error));
    TestEqual(TEXT("edge is repaired"), LifeItem(World.ToSharedRef())->GetIntegerField(TEXT("edge")), 100); TestEqual(TEXT("handle is repaired"), LifeItem(World.ToSharedRef())->GetIntegerField(TEXT("handle")), 100); TestEqual(TEXT("custody returns to owner"), LifeItem(World.ToSharedRef())->GetStringField(TEXT("custodian_id")), ErinId); TestEqual(TEXT("money is conserved"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins); TestEqual(TEXT("Erin wood becomes one after kindling use"), LifeAccount(World.ToSharedRef(), ErinId)->GetNumberField(TEXT("wood")), 1.0); TestTrue(TEXT("final operation is recorded without a contract reference"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject()->GetStringField(TEXT("contract_id")).IsEmpty()); TestTrue(TEXT("worker context does not receive owner's private relation"), HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetArrayField(TEXT("relations")).Num() == 0); TestTrue(TEXT("owner context retains directional relation"), HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetArrayField(TEXT("relations")).Num() > 0); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeRejectionAndTransactionTest, "ThreeHearths.AincradLife.RejectionShortageIdempotencyAndDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeRejectionAndTransactionTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error; if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; } ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳")); const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真")); const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id")); const FString Proposal = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId); const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    TestTrue(TEXT("proposal can be applied once"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, Proposal, TEXT("same-operation"), TEXT("请修"), Error)); const int32 EventCount = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(); const int32 ContractCount = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(); TestTrue(TEXT("exact duplicate operation is a successful no-op"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, Proposal, TEXT("same-operation"), TEXT("请修"), Error)); TestEqual(TEXT("duplicate does not append event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), EventCount); TestEqual(TEXT("duplicate does not append contract"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(), ContractCount); TestFalse(TEXT("same operation cannot change actor"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, Proposal, TEXT("same-operation"), TEXT("请修"), Error));
    const FString ContractId = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id")); TestFalse(TEXT("delivery before owner reaches worker point is rejected"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("deliver:") + ContractId, TEXT("too-early"), TEXT("交付"), Error)); TestEqual(TEXT("rejected delivery leaves proposal state"), LifeContract(World.ToSharedRef())->GetStringField(TEXT("status")), TEXT("proposed"));
    LifePutAtWork(World.ToSharedRef(), TakuyaId, TEXT("sao_smithy_01")); TestTrue(TEXT("worker rejects asynchronously at no cost"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("reject:") + ContractId, TEXT("reject-operation"), TEXT("我拒绝"), Error)); TestEqual(TEXT("rejection conserves money"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins); TestEqual(TEXT("contract is terminally rejected"), LifeContract(World.ToSharedRef())->GetStringField(TEXT("status")), TEXT("rejected"));
    const FString SecondProposal = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId); TestTrue(TEXT("a new proposal can be made after rejection"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, SecondProposal, TEXT("shortage-propose"), TEXT("再问一次"), Error)); const FString SecondContract = LifeContract(World.ToSharedRef(), 1)->GetStringField(TEXT("id")); TestTrue(TEXT("second proposal is accepted and reserved"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("accept:") + SecondContract, TEXT("shortage-accept"), TEXT("接单"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_smithy_01")); TestTrue(TEXT("second tool delivery succeeds"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("deliver:") + SecondContract, TEXT("shortage-deliver"), TEXT("送达"), Error)); LifeAccount(World.ToSharedRef(), TakuyaId)->SetNumberField(TEXT("iron"), 0); FString Before, After; LifeTextOf(World.ToSharedRef(), Before); TestFalse(TEXT("missing material refuses work"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("work:") + SecondContract, TEXT("shortage-work"), TEXT("开工"), Error)); LifeTextOf(World.ToSharedRef(), After); TestEqual(TEXT("shortage refusal is transactional"), After, Before); return true;
}

#endif
