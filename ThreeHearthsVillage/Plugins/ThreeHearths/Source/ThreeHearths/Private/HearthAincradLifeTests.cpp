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

    bool LifeHasOption(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& OptionId)
    {
        for (const TSharedPtr<FJsonValue>& Value : HearthAincradLife::Options(World, ResidentId))
        {
            const TSharedPtr<FJsonObject> Option = Value->AsObject();
            if (Option.IsValid() && Option->GetStringField(TEXT("id")) == OptionId) return true;
        }
        return false;
    }

    int32 LifeOptionCountPrefix(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& Prefix)
    {
        int32 Count = 0;
        for (const TSharedPtr<FJsonValue>& Value : HearthAincradLife::Options(World, ResidentId))
        {
            const TSharedPtr<FJsonObject> Option = Value->AsObject(); FString Id;
            if (Option.IsValid() && Option->TryGetStringField(TEXT("id"), Id) && Id.StartsWith(Prefix)) ++Count;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeInitializationTest, "ThreeHearths.AincradLife.InitializationAndReload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeInitializationTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error; if (!TestTrue(TEXT("initial world and life extension are created"), LifeStartWorld(World, Path, Error))) { AddError(Error); return false; } ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳")); const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真")); const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    TArray<FString> Ids, Stories; for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("residents"))) { Ids.Add(Value->AsObject()->GetStringField(TEXT("stable_id"))); Stories.Add(Value->AsObject()->GetStringField(TEXT("story"))); }
    TestTrue(TEXT("life validates after initialization"), HearthAincradLife::Validate(World.ToSharedRef(), Error)); TestEqual(TEXT("life version is independent version 1"), World->GetObjectField(TEXT("life"))->GetIntegerField(TEXT("version")), 1); TestEqual(TEXT("three initial important events are inboxed"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), 3);
    const TSharedRef<FJsonObject> ErinContext = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId); TestEqual(TEXT("Erin inbox starts at one"), ErinContext->GetIntegerField(TEXT("inbox_seq")), 1); TestTrue(TEXT("context exposes options array"), ErinContext->HasTypedField<EJson::Array>(TEXT("options"))); TestTrue(TEXT("context exposes authoritative work summary"), ErinContext->HasTypedField<EJson::Object>(TEXT("own_work_status"))); TestEqual(TEXT("initial axe is owned"), ErinContext->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("owned_item_count")), 1); TestEqual(TEXT("initial axe is held"), ErinContext->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("held_item_count")), 1); TestEqual(TEXT("initial active contract count is zero"), ErinContext->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("active_contract_count")), 0); TestEqual(TEXT("Takuya has no private Erin item"), HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetArrayField(TEXT("items")).Num(), 0); TestTrue(TEXT("Kashiwagi is known by stable identity"), !KashiwagiId.IsEmpty());
    TestTrue(TEXT("life-enabled state saves"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error)); TSharedPtr<FJsonObject> Reloaded; TestTrue(TEXT("life-enabled state reloads"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error)); TestTrue(TEXT("reloaded life validates"), HearthAincradLife::Validate(Reloaded.ToSharedRef(), Error));
    const TArray<TSharedPtr<FJsonValue>>& ReloadedResidents = Reloaded->GetArrayField(TEXT("residents")); for (int32 Index = 0; Index < ReloadedResidents.Num(); ++Index) { TestEqual(TEXT("stable identity survives life reload"), ReloadedResidents[Index]->AsObject()->GetStringField(TEXT("stable_id")), Ids[Index]); TestEqual(TEXT("story survives life reload"), ReloadedResidents[Index]->AsObject()->GetStringField(TEXT("story")), Stories[Index]); }
    FString Before; LifeTextOf(Reloaded.ToSharedRef(), Before); Reloaded->RemoveField(TEXT("life")); TestFalse(TEXT("life save cannot be overwritten by a life-less state"), HearthAincradWorldStore::Save(Path, Reloaded.ToSharedRef(), Error)); FString After; FFileHelper::LoadFileToString(After, *Path); TestEqual(TEXT("life-enabled file remains intact after rejected overwrite"), After, Before); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifePersonalWorkStatusTest, "ThreeHearths.AincradLife.PersonalWorkStatusBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifePersonalWorkStatusTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error;
    if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; }
    ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳"));
    const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真"));
    const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id"));
    const TSharedRef<FJsonObject> InitialContext = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId);
    TestTrue(TEXT("personal work summary explains tool prerequisites"), InitialContext->GetObjectField(TEXT("own_work_status"))->HasTypedField<EJson::Object>(TEXT("tool_use_requirements")));
    TestTrue(TEXT("tool summary says wood is consumed rather than created"), InitialContext->GetObjectField(TEXT("own_work_status"))->GetObjectField(TEXT("tool_use_requirements"))->GetStringField(TEXT("effect")).Contains(TEXT("no wood is created")));
    const FString FirstProposal = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId);
    TestTrue(TEXT("proposal enters active personal work status"), LifeStep(World, Path, ErinId, FirstProposal, TEXT("work-status-propose"), TEXT("请修刃"), Error));
    TestEqual(TEXT("proposed contract counts as active"), HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("active_contract_count")), 1);
    const FString FirstContract = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id"));
    LifePutAtWork(World.ToSharedRef(), TakuyaId, TEXT("sao_smithy_01"));
    TestTrue(TEXT("worker can reject the proposal"), LifeStep(World, Path, TakuyaId, TEXT("reject:") + FirstContract, TEXT("work-status-reject"), TEXT("暂不接"), Error));
    const TSharedRef<FJsonObject> AfterReject = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId);
    TestEqual(TEXT("rejected contract is no longer active"), AfterReject->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("active_contract_count")), 0);
    TestEqual(TEXT("rejected contract remains in history"), AfterReject->GetArrayField(TEXT("contracts")).Num(), 1);

    const FString SecondProposal = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId);
    TestTrue(TEXT("second proposal can start a new active job"), LifeStep(World, Path, ErinId, SecondProposal, TEXT("work-status-propose-2"), TEXT("再问一次"), Error));
    const FString SecondContract = LifeContract(World.ToSharedRef(), 1)->GetStringField(TEXT("id"));
    TestTrue(TEXT("accepted contract remains active"), LifeStep(World, Path, TakuyaId, TEXT("accept:") + SecondContract, TEXT("work-status-accept"), TEXT("接下"), Error));
    LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_smithy_01"));
    TestTrue(TEXT("delivered contract is active until work"), LifeStep(World, Path, ErinId, TEXT("deliver:") + SecondContract, TEXT("work-status-deliver"), TEXT("送达"), Error));
    LifePutAtWork(World.ToSharedRef(), TakuyaId, TEXT("sao_smithy_01"));
    TestTrue(TEXT("worker can complete the delivered repair"), LifeStep(World, Path, TakuyaId, TEXT("work:") + SecondContract, TEXT("work-status-complete"), TEXT("完成"), Error));
    const TSharedRef<FJsonObject> AfterComplete = HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId);
    TestEqual(TEXT("completed contract remains open until collection"), AfterComplete->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("active_contract_count")), 1);
    TestEqual(TEXT("completed contract remains in history"), AfterComplete->GetArrayField(TEXT("contracts")).Num(), 2);
    LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_smithy_01"));
    TestTrue(TEXT("owner can collect completed work"), LifeStep(World, Path, ErinId, TEXT("collect:") + SecondContract, TEXT("work-status-collect"), TEXT("取回"), Error));
    const TSharedRef<FJsonObject> AfterCollect = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId);
    TestEqual(TEXT("collected contract leaves no active work"), AfterCollect->GetObjectField(TEXT("own_work_status"))->GetIntegerField(TEXT("active_contract_count")), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeFullRepairTest, "ThreeHearths.AincradLife.TwoStageRepairAndUse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeFullRepairTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error; if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; } ON_SCOPE_EXIT { LifeCleanup(Path); };
    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳")); const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真")); const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木")); const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id")); const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    const FString EdgeOption = FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId); LifePutAtWork(World.ToSharedRef(), TakuyaId, TEXT("sao_smithy_01")); TestTrue(TEXT("edge proposal succeeds"), LifeStep(World, Path, ErinId, EdgeOption, TEXT("op-edge-propose"), TEXT("先修刃"), Error)); const FString EdgeContract = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id")); TestTrue(TEXT("worker accepts at own work point"), LifeStep(World, Path, TakuyaId, TEXT("accept:") + EdgeContract, TEXT("op-edge-accept"), TEXT("我接下"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_smithy_01")); TestTrue(TEXT("owner delivers face to face"), LifeStep(World, Path, ErinId, TEXT("deliver:") + EdgeContract, TEXT("op-edge-deliver"), TEXT("斧子给你"), Error)); TestTrue(TEXT("worker completes edge repair"), LifeStep(World, Path, TakuyaId, TEXT("work:") + EdgeContract, TEXT("op-edge-work"), TEXT("开始修刃"), Error)); TestTrue(TEXT("owner collects and pays once"), LifeStep(World, Path, ErinId, TEXT("collect:") + EdgeContract, TEXT("op-edge-collect"), TEXT("取回斧子"), Error));
    const FString HandleOption = FString::Printf(TEXT("repair_handle:%s:%s:5"), *AxeId, *KashiwagiId); LifePutAtWork(World.ToSharedRef(), KashiwagiId, TEXT("sao_carpentry_01")); TestTrue(TEXT("handle proposal succeeds"), LifeStep(World, Path, ErinId, HandleOption, TEXT("op-handle-propose"), TEXT("再修柄"), Error)); const FString HandleContract = LifeContract(World.ToSharedRef(), 1)->GetStringField(TEXT("id")); TestTrue(TEXT("wood worker accepts"), LifeStep(World, Path, KashiwagiId, TEXT("accept:") + HandleContract, TEXT("op-handle-accept"), TEXT("可以"), Error)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_carpentry_01")); TestTrue(TEXT("handle delivery succeeds"), LifeStep(World, Path, ErinId, TEXT("deliver:") + HandleContract, TEXT("op-handle-deliver"), TEXT("交给你"), Error)); const int32 KashiwagiBeforeWorkSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetIntegerField(TEXT("inbox_seq")); TestTrue(TEXT("handle work consumes one wood"), LifeStep(World, Path, KashiwagiId, TEXT("work:") + HandleContract, TEXT("op-handle-work"), TEXT("开始修柄"), Error)); TestTrue(TEXT("worker completion can trigger a resident follow-up"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, KashiwagiBeforeWorkSeq)); const int32 ErinBeforeHandleCollectSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq")); TestTrue(TEXT("handle collection settles"), LifeStep(World, Path, ErinId, TEXT("collect:") + HandleContract, TEXT("op-handle-collect"), TEXT("取回"), Error)); TestTrue(TEXT("owner collection can trigger a resident follow-up"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeHandleCollectSeq)); LifePutAtWork(World.ToSharedRef(), ErinId, TEXT("sao_inn_01")); const int32 ErinBeforeUseSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq")); TestTrue(TEXT("fully repaired axe makes kindling"), LifeStep(World, Path, ErinId, TEXT("use_tool:") + AxeId, TEXT("op-use-tool"), TEXT("劈柴"), Error)); TestTrue(TEXT("tool use can trigger a resident follow-up"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeUseSeq));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeDecisionEventTest, "ThreeHearths.AincradLife.DecisionEventFiltering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeDecisionEventTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error;
    if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; }
    ON_SCOPE_EXIT { LifeCleanup(Path); };

    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳"));
    const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id"));
    const int32 ErinInitialSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    TestTrue(TEXT("developer initial condition can wake an old runtime once"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, 0.0));
    TestFalse(TEXT("initial condition is consumed after its inbox sequence"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinInitialSeq));
    TestFalse(TEXT("fractional inbox cursor fails closed"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, 1.5));
    TestFalse(TEXT("negative inbox cursor fails closed"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, -1.0));

    LifePutAtWork(World.ToSharedRef(), KashiwagiId, TEXT("sao_carpentry_01"));
    const FString Proposal = FString::Printf(TEXT("repair_handle:%s:%s:5"), *AxeId, *KashiwagiId);
    TestTrue(TEXT("real resident proposal persists through reload"), LifeStep(World, Path, ErinId, Proposal, TEXT("decision-propose"), TEXT("请修柄"), Error));
    const FString ContractId = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id"));
    TestFalse(TEXT("resident's own proposal echo does not wake her"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinInitialSeq));
    TestTrue(TEXT("counterparty sees the external proposal event"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, 1.0));

    const int32 ErinBeforeAcceptSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    TestTrue(TEXT("counterparty acceptance persists"), LifeStep(World, Path, KashiwagiId, TEXT("accept:") + ContractId, TEXT("decision-accept"), TEXT("我接下"), Error));
    TestTrue(TEXT("interleaved external acceptance is not hidden by the earlier self event"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeAcceptSeq));
    TestTrue(TEXT("scanning from before the self proposal still finds the later external acceptance"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinInitialSeq));

    const int32 ErinBeforeCancelSeq = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 EventsBeforeCancel = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num();
    TestTrue(TEXT("owner can legally cancel the accepted contract"), LifeStep(World, Path, ErinId, TEXT("cancel:") + ContractId, TEXT("decision-cancel"), TEXT("我先取消"), Error));
    TestFalse(TEXT("resident's own cancellation echo does not wake her"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeCancelSeq));
    TestTrue(TEXT("self cancellation after an external event does not hide that external event"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinInitialSeq));
    TestEqual(TEXT("cancellation remains a durable terminal state"), LifeContract(World.ToSharedRef())->GetStringField(TEXT("status")), TEXT("cancelled"));
    TestTrue(TEXT("exact repeated cancellation is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("cancel:") + ContractId, TEXT("decision-cancel"), TEXT("我先取消"), Error));
    TestEqual(TEXT("repeated cancellation adds no event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), EventsBeforeCancel + 1);
    TestTrue(TEXT("cancelled world reloads"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error));
    TSharedPtr<FJsonObject> Reloaded;
    TestTrue(TEXT("reloaded cancelled world remains valid"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error));
    TestEqual(TEXT("reloaded cancellation remains terminal"), LifeContract(Reloaded.ToSharedRef())->GetStringField(TEXT("status")), TEXT("cancelled"));
    TestFalse(TEXT("reloaded cancellation still does not self-wake"), HearthAincradLife::HasDecisionEvent(Reloaded.ToSharedRef(), ErinId, ErinBeforeCancelSeq));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeRepairOfferTest, "ThreeHearths.AincradLife.RepairOfferPersistenceAndPrivacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeRepairOfferTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error;
    if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; }
    ON_SCOPE_EXIT { LifeCleanup(Path); };

    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳"));
    const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    const FString AxeId = LifeItem(World.ToSharedRef())->GetStringField(TEXT("id"));
    LifePutAtWork(World.ToSharedRef(), KashiwagiId, TEXT("sao_carpentry_01"));
    const FString FirstProposal = FString::Printf(TEXT("repair_handle:%s:%s:5"), *AxeId, *KashiwagiId);
    TestTrue(TEXT("worker can receive a repair proposal"), LifeStep(World, Path, ErinId, FirstProposal, TEXT("offer-contract-1"), TEXT("请修柄"), Error));
    const FString FirstContract = LifeContract(World.ToSharedRef())->GetStringField(TEXT("id"));
    TestTrue(TEXT("worker rejects the first proposal without settlement"), LifeStep(World, Path, KashiwagiId, TEXT("reject:") + FirstContract, TEXT("offer-reject-1"), TEXT("暂不接"), Error));
    TestTrue(TEXT("past terminal contract remains visible to its worker"), HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetArrayField(TEXT("contracts")).Num() > 0);
    TestEqual(TEXT("only the latest terminal contract gets three suggestions"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("offer_repair:%s:"), *FirstContract)), 3);

    const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    const double InitialWood = LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood"));
    const double InitialIron = LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("iron"));
    const int32 InitialContracts = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num();
    const int32 InitialEvents = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num();
    const int32 ErinInboxBeforeOffer = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 KashiwagiInboxBeforeOffer = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetIntegerField(TEXT("inbox_seq"));
    const FString OfferOption = FString::Printf(TEXT("offer_repair:%s:5"), *FirstContract);
    TestTrue(TEXT("worker can apply one explicit repair suggestion"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, OfferOption, TEXT("repair-offer-1"), TEXT("如果你还需要，我可按5 Col修柄"), Error));
    TestEqual(TEXT("repair suggestion creates no contract"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(), InitialContracts);
    TestEqual(TEXT("repair suggestion does not change money"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins);
    TestEqual(TEXT("repair suggestion does not consume wood"), LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood")), InitialWood);
    TestEqual(TEXT("repair suggestion does not consume iron"), LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("iron")), InitialIron);
    TestEqual(TEXT("repair suggestion appends one event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), InitialEvents + 1);
    const TSharedPtr<FJsonObject> OfferEvent = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    TestEqual(TEXT("offer event has explicit type"), OfferEvent->GetStringField(TEXT("type")), TEXT("offer_repair"));
    TestEqual(TEXT("offer event persists the repair part"), OfferEvent->GetStringField(TEXT("part")), TEXT("handle"));
    TestEqual(TEXT("offer event persists the numeric price"), OfferEvent->GetNumberField(TEXT("offer_price_col")), 5.0);
    TestTrue(TEXT("owner is woken by an external offer"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinInboxBeforeOffer));
    TestFalse(TEXT("worker offer echo does not self-wake"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, KashiwagiInboxBeforeOffer));
    TestEqual(TEXT("offer is removed after dispatch"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, TEXT("offer_repair:")), 0);
    TestTrue(TEXT("owner retains the ordinary repair proposal flow"), LifeHasOption(World.ToSharedRef(), ErinId, FString::Printf(TEXT("repair_handle:%s:%s:2"), *AxeId, *KashiwagiId)));
    TestTrue(TEXT("same operation is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, OfferOption, TEXT("repair-offer-1"), TEXT("如果你还需要，我可按5 Col修柄"), Error));
    TestEqual(TEXT("idempotent retry adds no offer event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), InitialEvents + 1);
    TestFalse(TEXT("changing operation and price cannot bypass dedupe"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("offer_repair:%s:2"), *FirstContract), TEXT("repair-offer-2"), TEXT("改报2 Col"), Error));
    TestEqual(TEXT("rejected price retry adds no event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), InitialEvents + 1);

    TestTrue(TEXT("offered state saves"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error));
    TSharedPtr<FJsonObject> Reloaded;
    TestTrue(TEXT("offered state reloads"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error));
    TestTrue(TEXT("reloaded offered state validates"), HearthAincradLife::Validate(Reloaded.ToSharedRef(), Error));
    TestEqual(TEXT("offer dedupe survives reload"), LifeOptionCountPrefix(Reloaded.ToSharedRef(), KashiwagiId, TEXT("offer_repair:")), 0);
    TestEqual(TEXT("owner sees only public offer information"), HearthAincradLife::PersonalContext(Reloaded.ToSharedRef(), ErinId)->GetArrayField(TEXT("accounts")).Num(), 1);

    const FString SecondProposal = FString::Printf(TEXT("repair_handle:%s:%s:2"), *AxeId, *KashiwagiId);
    TestTrue(TEXT("owner can start the existing repair flow after an offer"), LifeStep(Reloaded, Path, ErinId, SecondProposal, TEXT("repair-after-offer"), TEXT("我仍然正式委托"), Error));
    const FString SecondContract = LifeContract(Reloaded.ToSharedRef(), 1)->GetStringField(TEXT("id"));
    TestTrue(TEXT("owner can legally cancel the follow-up proposal"), LifeStep(Reloaded, Path, ErinId, TEXT("cancel:") + SecondContract, TEXT("cancel-after-offer"), TEXT("先取消"), Error));
    TestEqual(TEXT("only the newest terminal contract receives a new suggestion"), LifeOptionCountPrefix(Reloaded.ToSharedRef(), KashiwagiId, TEXT("offer_repair:")), 3);
    TestFalse(TEXT("older terminal contract is not listed again"), LifeHasOption(Reloaded.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("offer_repair:%s:2"), *FirstContract)));
    TestTrue(TEXT("new terminal contract is the listed suggestion target"), LifeHasOption(Reloaded.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("offer_repair:%s:2"), *SecondContract)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeHelpCommunicationTest, "ThreeHearths.AincradLife.HelpQuestionSingleReply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeHelpCommunicationTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error;
    if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; }
    ON_SCOPE_EXIT { LifeCleanup(Path); };

    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳"));
    const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真"));
    FString UnknownNeighbourId;
    for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("residents")))
    {
        const FString Candidate = Value->AsObject()->GetStringField(TEXT("stable_id"));
        if (Candidate != ErinId && Candidate != KashiwagiId && Candidate != TakuyaId) { UnknownNeighbourId = Candidate; break; }
    }
    TestTrue(TEXT("an unknown-neighbour fixture exists"), !UnknownNeighbourId.IsEmpty());
    TestTrue(TEXT("known neighbour question is offered"), LifeHasOption(World.ToSharedRef(), ErinId, TEXT("ask_help:") + KashiwagiId));
    TestFalse(TEXT("self question is not offered"), LifeHasOption(World.ToSharedRef(), ErinId, TEXT("ask_help:") + ErinId));
    TestFalse(TEXT("unknown resident question is not offered"), LifeHasOption(World.ToSharedRef(), ErinId, TEXT("ask_help:") + UnknownNeighbourId));
    TestFalse(TEXT("self question is rejected on apply"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("ask_help:") + ErinId, TEXT("help-self"), TEXT("问自己"), Error));
    TestFalse(TEXT("unknown resident question is rejected on apply"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("ask_help:") + UnknownNeighbourId, TEXT("help-unknown"), TEXT("问陌生人"), Error));
    TestFalse(TEXT("a non-pilot resident cannot ask a pilot resident"), LifeHasOption(World.ToSharedRef(), UnknownNeighbourId, TEXT("ask_help:") + ErinId));
    TestFalse(TEXT("a non-pilot resident question is rejected on apply"), HearthAincradLife::Apply(World.ToSharedRef(), UnknownNeighbourId, TEXT("ask_help:") + ErinId, TEXT("help-nonpilot"), TEXT("请帮忙"), Error));

    const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    const double InitialErinWood = LifeAccount(World.ToSharedRef(), ErinId)->GetNumberField(TEXT("wood"));
    const double InitialKashiwagiWood = LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood"));
    const int32 InitialContracts = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num();
    const int32 InitialRelations = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("relations")).Num();
    const int32 InitialEvents = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num();
    const int32 ErinBeforeQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 KashiwagiBeforeQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetIntegerField(TEXT("inbox_seq"));
    const FString QuestionOption = TEXT("ask_help:") + KashiwagiId;
    TestTrue(TEXT("question applies as one durable communication event"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, QuestionOption, TEXT("help-question-1"), TEXT("你近期愿意帮忙吗？"), Error));
    TestEqual(TEXT("question changes no contracts"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(), InitialContracts);
    TestEqual(TEXT("question changes no money"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins);
    TestEqual(TEXT("question changes no asker material"), LifeAccount(World.ToSharedRef(), ErinId)->GetNumberField(TEXT("wood")), InitialErinWood);
    TestEqual(TEXT("question changes no target material"), LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood")), InitialKashiwagiWood);
    TestEqual(TEXT("question changes no trust records"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("relations")).Num(), InitialRelations);
    TestEqual(TEXT("question appends one event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), InitialEvents + 1);
    const TSharedPtr<FJsonObject> QuestionEvent = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    const FString RequestId = QuestionEvent->GetStringField(TEXT("request_id"));
    TestEqual(TEXT("question has communication metadata"), QuestionEvent->GetBoolField(TEXT("contractual")), false);
    TestEqual(TEXT("question has no contract reference"), QuestionEvent->GetStringField(TEXT("contract_id")), TEXT(""));
    TestFalse(TEXT("question sender own echo does not wake a paid decision"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeQuestion));
    TestTrue(TEXT("question wakes the known target as external information"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, KashiwagiBeforeQuestion));
    const int32 KashiwagiAfterQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetIntegerField(TEXT("inbox_seq"));
    TestTrue(TEXT("exact repeated question operation is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, QuestionOption, TEXT("help-question-1"), TEXT("你近期愿意帮忙吗？"), Error));
    TestFalse(TEXT("a changed operation cannot repeat the directed pair question"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, QuestionOption, TEXT("help-question-2"), TEXT("再问一次"), Error));
    TestEqual(TEXT("repeated question attempts append no event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), InitialEvents + 1);

    const FString WillingReply = FString::Printf(TEXT("reply_help:%s:willing"), *RequestId);
    TestEqual(TEXT("target receives exactly three reply choices"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_help:%s:"), *RequestId)), 3);
    TestFalse(TEXT("a different neighbour cannot answer the question"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, WillingReply, TEXT("help-wrong-target"), TEXT("我愿意"), Error));
    TestTrue(TEXT("target can choose a willing reply"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, WillingReply, TEXT("help-reply-1"), TEXT("我愿意了解具体问题"), Error));
    const int32 EventsAfterReply = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num();
    const TSharedPtr<FJsonObject> ReplyEvent = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    TestEqual(TEXT("reply is explicitly non-contractual"), ReplyEvent->GetBoolField(TEXT("contractual")), false);
    TestEqual(TEXT("reply stores the selected choice"), ReplyEvent->GetStringField(TEXT("reply_choice")), TEXT("willing"));
    TestEqual(TEXT("reply has no contract reference"), ReplyEvent->GetStringField(TEXT("contract_id")), TEXT(""));
    bool bReplyRemembered = false;
    const TSharedRef<FJsonObject> ReplyPersonalContext = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId);
    for (const TSharedPtr<FJsonValue>& Value : ReplyPersonalContext->GetArrayField(TEXT("received_letters")))
    {
        const TSharedPtr<FJsonObject> Letter = Value->AsObject();
        if (Letter.IsValid() && Letter->GetStringField(TEXT("event_id")) == ReplyEvent->GetStringField(TEXT("event_id"))) { bReplyRemembered = true; break; }
    }
    TestTrue(TEXT("reply sender retains a personal audit of their own answer"), bReplyRemembered);
    TestTrue(TEXT("reply wakes the original asker as external information"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeQuestion));
    TestFalse(TEXT("reply sender does not wake from own echo"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, KashiwagiAfterQuestion));
    TestTrue(TEXT("exact repeated reply operation is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, WillingReply, TEXT("help-reply-1"), TEXT("我愿意了解具体问题"), Error));
    TestFalse(TEXT("a second reply choice is rejected"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_help:%s:unavailable"), *RequestId), TEXT("help-reply-2"), TEXT("现在不方便"), Error));
    TestEqual(TEXT("second reply adds no event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), EventsAfterReply);
    TestEqual(TEXT("reply leaves contracts unchanged"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(), InitialContracts);
    TestEqual(TEXT("reply leaves money unchanged"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins);
    TestEqual(TEXT("reply leaves materials unchanged"), LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood")), InitialKashiwagiWood);
    TestEqual(TEXT("reply options are consumed after one answer"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_help:%s:"), *RequestId)), 0);

    TestTrue(TEXT("communication state saves"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error));
    TSharedPtr<FJsonObject> Reloaded;
    TestTrue(TEXT("communication state reloads"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error));
    TestTrue(TEXT("reloaded communication state validates"), HearthAincradLife::Validate(Reloaded.ToSharedRef(), Error));
    TestFalse(TEXT("question remains deduped after serialization"), LifeHasOption(Reloaded.ToSharedRef(), ErinId, QuestionOption));
    TestEqual(TEXT("reply remains single after serialization"), LifeOptionCountPrefix(Reloaded.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_help:%s:"), *RequestId)), 0);
    TestTrue(TEXT("reloaded reply still wakes the asker"), HearthAincradLife::HasDecisionEvent(Reloaded.ToSharedRef(), ErinId, ErinBeforeQuestion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthAincradLifeRepairNeedCommunicationTest, "ThreeHearths.AincradLife.RepairNeedQuestionSingleReply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthAincradLifeRepairNeedCommunicationTest::RunTest(const FString&)
{
    TSharedPtr<FJsonObject> World; FString Path, Error;
    if (!LifeStartWorld(World, Path, Error)) { AddError(Error); return false; }
    ON_SCOPE_EXIT { LifeCleanup(Path); };

    const FString ErinId = LifeResidentId(World.ToSharedRef(), TEXT("艾琳"));
    const FString TakuyaId = LifeResidentId(World.ToSharedRef(), TEXT("拓真"));
    const FString KashiwagiId = LifeResidentId(World.ToSharedRef(), TEXT("柏木"));
    FString NonPilotId;
    for (const TSharedPtr<FJsonValue>& Value : World->GetArrayField(TEXT("residents")))
    {
        const FString Candidate = Value->AsObject()->GetStringField(TEXT("stable_id"));
        if (Candidate != ErinId && Candidate != TakuyaId && Candidate != KashiwagiId) { NonPilotId = Candidate; break; }
    }
    TestTrue(TEXT("repair need fixture has a non-pilot resident"), !NonPilotId.IsEmpty());

    const TSharedPtr<FJsonObject> InitialAxe = LifeItem(World.ToSharedRef());
    const FString AxeId = InitialAxe->GetStringField(TEXT("id"));
    const int64 InitialCoins = LifeCoinsTotal(World.ToSharedRef());
    const double InitialErinEdge = InitialAxe->GetNumberField(TEXT("edge"));
    const double InitialErinHandle = InitialAxe->GetNumberField(TEXT("handle"));
    const double InitialTakuyaIron = LifeAccount(World.ToSharedRef(), TakuyaId)->GetNumberField(TEXT("iron"));
    const double InitialKashiwagiWood = LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood"));
    const int32 InitialContracts = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num();
    const int32 InitialRelations = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("relations")).Num();

    TestFalse(TEXT("resident without a repair skill cannot ask"), LifeHasOption(World.ToSharedRef(), ErinId, TEXT("ask_repair_need:") + KashiwagiId));
    TestFalse(TEXT("resident without a repair skill is rejected on apply"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, TEXT("ask_repair_need:") + KashiwagiId, TEXT("repair-need-no-skill"), TEXT("我想问问"), Error));
    TestFalse(TEXT("non-pilot cannot ask a pilot"), LifeHasOption(World.ToSharedRef(), NonPilotId, TEXT("ask_repair_need:") + ErinId));
    TestFalse(TEXT("non-pilot repair need question is rejected"), HearthAincradLife::Apply(World.ToSharedRef(), NonPilotId, TEXT("ask_repair_need:") + ErinId, TEXT("repair-need-nonpilot"), TEXT("询问"), Error));

    const int32 TakuyaBeforeEmptyQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 KashiwagiBeforeEmptyQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), KashiwagiId)->GetIntegerField(TEXT("inbox_seq"));
    TestTrue(TEXT("metal worker can ask a known wood worker"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("ask_repair_need:") + KashiwagiId, TEXT("repair-need-empty-question"), TEXT("你近期有需要修刃的物品吗？"), Error));
    const TSharedPtr<FJsonObject> EmptyQuestion = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    const FString EmptyRequestId = EmptyQuestion->GetStringField(TEXT("request_id"));
    TestEqual(TEXT("empty responder cannot claim a metal repair need"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_repair_need:%s:has_need"), *EmptyRequestId)), 0);
    TestEqual(TEXT("empty responder gets only non-claiming choices"), LifeOptionCountPrefix(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_repair_need:%s:"), *EmptyRequestId)), 2);
    TestFalse(TEXT("empty responder cannot apply a false has_need claim"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_repair_need:%s:has_need"), *EmptyRequestId), TEXT("repair-need-empty-false"), TEXT("我有剑"), Error));
    TestFalse(TEXT("question sender own echo does not wake"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), TakuyaId, TakuyaBeforeEmptyQuestion));
    TestTrue(TEXT("empty responder may answer no_need"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("reply_repair_need:%s:no_need"), *EmptyRequestId), TEXT("repair-need-empty-reply"), TEXT("目前没有"), Error));
    TestFalse(TEXT("no_need reply cannot be quoted"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:5"), *EmptyRequestId), TEXT("repair-need-no-quote"), TEXT("报价"), Error));
    TestTrue(TEXT("external no_need reply wakes the original question sender"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), TakuyaId, TakuyaBeforeEmptyQuestion));
    TestTrue(TEXT("question wakes the external target"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), KashiwagiId, KashiwagiBeforeEmptyQuestion));

    TestTrue(TEXT("same operation retry is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("ask_repair_need:") + KashiwagiId, TEXT("repair-need-empty-question"), TEXT("你近期有需要修刃的物品吗？"), Error));
    TestFalse(TEXT("changed operation cannot repeat a directed pair and skill"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("ask_repair_need:") + KashiwagiId, TEXT("repair-need-empty-question-2"), TEXT("再问一次"), Error));

    TestTrue(TEXT("wood worker can ask the axe owner about handle repair"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, TEXT("ask_repair_need:") + ErinId, TEXT("repair-need-unsure-question"), TEXT("需要修柄吗"), Error));
    const FString UnsureRequestId = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject()->GetStringField(TEXT("request_id"));
    TestTrue(TEXT("owner may answer unsure"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, FString::Printf(TEXT("reply_repair_need:%s:unsure"), *UnsureRequestId), TEXT("repair-need-unsure-reply"), TEXT("还不确定"), Error));
    TestFalse(TEXT("unsure reply cannot be quoted"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("quote_repair_need:%s:5"), *UnsureRequestId), TEXT("repair-need-unsure-quote"), TEXT("报价"), Error));

    const int32 TakuyaBeforeAxeQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 ErinBeforeAxeQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    TestTrue(TEXT("metal worker can ask the owner of the damaged axe"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("ask_repair_need:") + ErinId, TEXT("repair-need-axe-question"), TEXT("你近期有需要修刃的物品吗？"), Error));
    const TSharedPtr<FJsonObject> AxeQuestion = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    const FString AxeRequestId = AxeQuestion->GetStringField(TEXT("request_id"));
    TestEqual(TEXT("damaged axe owner can claim a metal repair need"), LifeOptionCountPrefix(World.ToSharedRef(), ErinId, FString::Printf(TEXT("reply_repair_need:%s:has_need"), *AxeRequestId)), 1);
    TestEqual(TEXT("damaged axe owner receives three choices"), LifeOptionCountPrefix(World.ToSharedRef(), ErinId, FString::Printf(TEXT("reply_repair_need:%s:"), *AxeRequestId)), 3);
    TestTrue(TEXT("external axe question wakes owner"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeAxeQuestion));
    TestFalse(TEXT("question sender self echo remains filtered"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), TakuyaId, TakuyaBeforeAxeQuestion));
    const FString HasNeedReply = FString::Printf(TEXT("reply_repair_need:%s:has_need"), *AxeRequestId);
    TestFalse(TEXT("a different resident cannot answer the repair need question"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, HasNeedReply, TEXT("repair-need-wrong-target"), TEXT("我有需要"), Error));
    TestTrue(TEXT("original target can answer has_need for the damaged axe"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, HasNeedReply, TEXT("repair-need-axe-reply"), TEXT("我有一把斧子可以再谈"), Error));
    const TSharedPtr<FJsonObject> AxeReply = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    TestEqual(TEXT("reply records the repair topic"), AxeReply->GetStringField(TEXT("topic")), TEXT("repair_work_availability"));
    TestEqual(TEXT("reply records the requested skill"), AxeReply->GetStringField(TEXT("repair_skill")), TEXT("metal_repair"));
    TestEqual(TEXT("reply records the selected choice"), AxeReply->GetStringField(TEXT("reply_choice")), TEXT("has_need"));
    TestFalse(TEXT("repair need reply is non-contractual"), AxeReply->GetBoolField(TEXT("contractual")));
    TestEqual(TEXT("repair need reply has no item reference"), AxeReply->GetStringField(TEXT("item_id")), TEXT(""));
    TestTrue(TEXT("reply wakes original asker"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), TakuyaId, TakuyaBeforeAxeQuestion));
    const int32 ErinAfterAxeQuestion = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    TestFalse(TEXT("reply sender own echo remains filtered"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinAfterAxeQuestion));
    TestFalse(TEXT("a second answer cannot be appended"), HearthAincradLife::Apply(World.ToSharedRef(), ErinId, FString::Printf(TEXT("reply_repair_need:%s:no_need"), *AxeRequestId), TEXT("repair-need-axe-reply-2"), TEXT("改口"), Error));
    TestEqual(TEXT("positive reply gives the skilled requester three quote choices"), LifeOptionCountPrefix(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:"), *AxeRequestId)), 3);
    TestTrue(TEXT("metal worker sees the exact repair consumption rule"), HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetObjectField(TEXT("own_work_status"))->GetObjectField(TEXT("repair_requirements"))->GetObjectField(TEXT("metal_repair"))->GetStringField(TEXT("effect")).Contains(TEXT("1 iron")));
    LifeAccount(World.ToSharedRef(), TakuyaId)->SetNumberField(TEXT("iron"), 0);
    TestEqual(TEXT("quote choices disappear without own repair material"), LifeOptionCountPrefix(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:"), *AxeRequestId)), 0);
    TestFalse(TEXT("direct quote is rejected without own repair material"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:5"), *AxeRequestId), TEXT("repair-need-quote-no-material"), TEXT("报价"), Error));
    LifeAccount(World.ToSharedRef(), TakuyaId)->SetNumberField(TEXT("iron"), InitialTakuyaIron);
    TestFalse(TEXT("unrelated resident cannot quote another worker request"), HearthAincradLife::Apply(World.ToSharedRef(), KashiwagiId, FString::Printf(TEXT("quote_repair_need:%s:5"), *AxeRequestId), TEXT("repair-need-quote-unrelated"), TEXT("报价"), Error));
    TestFalse(TEXT("unknown request cannot be quoted"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, TEXT("quote_repair_need:unknown:5"), TEXT("repair-need-quote-unknown"), TEXT("报价"), Error));
    const int32 TakuyaBeforeQuote = HearthAincradLife::PersonalContext(World.ToSharedRef(), TakuyaId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 ErinBeforeQuote = HearthAincradLife::PersonalContext(World.ToSharedRef(), ErinId)->GetIntegerField(TEXT("inbox_seq"));
    const int32 EventsBeforeQuote = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num();
    const FString Quote = FString::Printf(TEXT("quote_repair_need:%s:5"), *AxeRequestId);
    TestTrue(TEXT("original skilled requester can send one reference quote"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, Quote, TEXT("repair-need-quote"), TEXT("5 Col可以修刃"), Error));
    const TSharedPtr<FJsonObject> QuoteEvent = World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Last()->AsObject();
    TestEqual(TEXT("quote event type is explicit"), QuoteEvent->GetStringField(TEXT("type")), TEXT("quote_repair_need"));
    TestEqual(TEXT("quote retains request id"), QuoteEvent->GetStringField(TEXT("request_id")), AxeRequestId);
    TestEqual(TEXT("quote retains repair skill"), QuoteEvent->GetStringField(TEXT("repair_skill")), TEXT("metal_repair"));
    TestEqual(TEXT("quote retains price"), QuoteEvent->GetNumberField(TEXT("offer_price_col")), 5.0);
    TestEqual(TEXT("quote carries no item id"), QuoteEvent->GetStringField(TEXT("item_id")), TEXT(""));
    TestEqual(TEXT("quote carries no contract id"), QuoteEvent->GetStringField(TEXT("contract_id")), TEXT(""));
    TestTrue(TEXT("quote tells recipients it is not a contract or deduction"), QuoteEvent->GetStringField(TEXT("text")).Contains(TEXT("不是合同")) && QuoteEvent->GetStringField(TEXT("text")).Contains(TEXT("未扣款")));
    TestEqual(TEXT("one quote removes every price choice for that request"), LifeOptionCountPrefix(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:"), *AxeRequestId)), 0);
    TestTrue(TEXT("quote wakes the owner"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), ErinId, ErinBeforeQuote));
    TestFalse(TEXT("quote does not self-wake its worker"), HearthAincradLife::HasDecisionEvent(World.ToSharedRef(), TakuyaId, TakuyaBeforeQuote));
    TestTrue(TEXT("same quote operation retry is idempotent"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, Quote, TEXT("repair-need-quote"), TEXT("5 Col可以修刃"), Error));
    TestEqual(TEXT("idempotent quote appends no event"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("events")).Num(), EventsBeforeQuote + 1);
    TestFalse(TEXT("another operation cannot choose a second price"), HearthAincradLife::Apply(World.ToSharedRef(), TakuyaId, FString::Printf(TEXT("quote_repair_need:%s:8"), *AxeRequestId), TEXT("repair-need-quote-2"), TEXT("改报8 Col"), Error));
    TestTrue(TEXT("ordinary repair proposal remains available after communication"), LifeHasOption(World.ToSharedRef(), ErinId, FString::Printf(TEXT("repair_edge:%s:%s:2"), *AxeId, *TakuyaId)));

    TestEqual(TEXT("communication preserves money"), LifeCoinsTotal(World.ToSharedRef()), InitialCoins);
    TestEqual(TEXT("communication preserves axe edge"), LifeItem(World.ToSharedRef())->GetNumberField(TEXT("edge")), InitialErinEdge);
    TestEqual(TEXT("communication preserves axe handle"), LifeItem(World.ToSharedRef())->GetNumberField(TEXT("handle")), InitialErinHandle);
    TestEqual(TEXT("communication preserves requester material"), LifeAccount(World.ToSharedRef(), TakuyaId)->GetNumberField(TEXT("iron")), InitialTakuyaIron);
    TestEqual(TEXT("communication preserves other material"), LifeAccount(World.ToSharedRef(), KashiwagiId)->GetNumberField(TEXT("wood")), InitialKashiwagiWood);
    TestEqual(TEXT("communication creates no contract"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("contracts")).Num(), InitialContracts);
    TestEqual(TEXT("communication changes no relations"), World->GetObjectField(TEXT("life"))->GetArrayField(TEXT("relations")).Num(), InitialRelations);

    TestTrue(TEXT("repair need state saves"), HearthAincradWorldStore::Save(Path, World.ToSharedRef(), Error));
    TSharedPtr<FJsonObject> Reloaded;
    TestTrue(TEXT("repair need state reloads"), HearthAincradWorldStore::LoadOrCreate(Path, Reloaded, Error));
    TestTrue(TEXT("reloaded repair need state validates"), HearthAincradLife::Validate(Reloaded.ToSharedRef(), Error));
    TestFalse(TEXT("empty pair remains deduped after reload"), LifeHasOption(Reloaded.ToSharedRef(), TakuyaId, TEXT("ask_repair_need:") + KashiwagiId));
    TestFalse(TEXT("answered pair has no reply after reload"), LifeHasOption(Reloaded.ToSharedRef(), ErinId, HasNeedReply));
    return true;
}

#endif
