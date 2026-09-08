#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace HearthThinkingRuntimeTests
{
    static FString SidecarPath(const FString& WorldName)
    {
        return FPaths::ProjectSavedDir()/TEXT("ThreeHearths/World")
            /(FPaths::MakeValidFileName(WorldName)+TEXT(".thinking.json"));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthThinkingRuntimeTest,
    "ThreeHearths.NpcThinking.RuntimeRoleAndCorruptSidecar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthThinkingRuntimeTest::RunTest(const FString&)
{
    using namespace HearthThinking;
    TestEqual(TEXT("Chinese gatekeeper maps to gatekeeper"),RoleFor(TEXT("门卫")),EHearthNpcThinkingRole::Gatekeeper);
    TestEqual(TEXT("King remains a civilian thinker"),RoleFor(TEXT("国王")),EHearthNpcThinkingRole::Civilian);
    TestEqual(TEXT("English guard maps to royal guard"),RoleFor(TEXT("guard")),EHearthNpcThinkingRole::RoyalGuard);
    TestEqual(TEXT("English carter maps to carter"),RoleFor(TEXT("carter")),EHearthNpcThinkingRole::Carter);

    const FString WorldName=TEXT("thinking-corrupt-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Path=HearthThinkingRuntimeTests::SidecarPath(WorldName);
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); Files.CreateDirectoryTree(*FPaths::GetPath(Path));
    if(!TestTrue(TEXT("Write malformed thinking sidecar"),FFileHelper::SaveStringToFile(TEXT("{\"schema\":1,\"residents\":\"wrong\"}"),*Path))) return false;
    {
        FHearthThinkingRuntime Runtime(WorldName); FHearthNpcThinkingRequest Request;
        Request.ResidentId=TEXT("resident-corrupt"); Request.EventId=TEXT("event-corrupt"); Request.Trigger=EHearthNpcThinkingTrigger::ImportantEvent;
        const FHearthThinkingGateResult Admission=Runtime.Admit(Request,0.0);
        TestTrue(TEXT("Malformed sidecar forces local behavior"),Admission.bUseLocalBehavior);
        TestFalse(TEXT("Malformed sidecar cannot dispatch paid HTTP"),Admission.bDispatch);
    }
    Files.DeleteFile(*Path);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthThinkingRuntimeDaydreamMigrationTest,
    "ThreeHearths.NpcThinking.RuntimeDaydreamJsonMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthThinkingRuntimeDaydreamMigrationTest::RunTest(const FString&)
{
    using namespace HearthThinking;
    auto& Files=FPlatformFileManager::Get().GetPlatformFile();
    TArray<FString> CleanupPaths;
    bool bAllPassed=true;
    auto Check=[&](const TCHAR* Label,const bool Condition)
    {
        const bool Reported=TestTrue(Label,Condition);
        bAllPassed=bAllPassed && Reported;
    };
    auto TrackWorld=[&](const FString& WorldName)->FString
    {
        const FString Result=HearthThinkingRuntimeTests::SidecarPath(WorldName);
        CleanupPaths.Add(Result);
        return Result;
    };
    auto LoadJson=[&](const FString& Path,FString& Text,TSharedPtr<FJsonObject>& Root)->bool
    {
        return FFileHelper::LoadFileToString(Text,*Path)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)
            && Root.IsValid();
    };
    auto SaveJson=[&](const FString& Path,const TSharedPtr<FJsonObject>& Root)->bool
    {
        FString Text;
        if(!Root.IsValid() || !FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text))) return false;
        return FFileHelper::SaveStringToFile(Text,*Path);
    };
    auto MakeValidSidecar=[&](const FString& WorldName,const FString& ResidentId)->bool
    {
        bool bCompleted=false;
        {
            FHearthThinkingRuntime Runtime(WorldName);
            FHearthNpcThinkingRequest Request;
            Request.ResidentId=ResidentId; Request.EventId=TEXT("initial-important");
            Request.Role=EHearthNpcThinkingRole::Civilian;
            Request.Trigger=EHearthNpcThinkingTrigger::ImportantEvent;
            const FHearthThinkingGateResult Admission=Runtime.Admit(Request,-1.0);
            bCompleted=Admission.bDispatch && !Admission.RequestId.IsEmpty()
                && Runtime.Complete(Admission.RequestId,true);
        }
        return bCompleted && Files.FileExists(*HearthThinkingRuntimeTests::SidecarPath(WorldName));
    };

    const FString WorldName=TEXT("thinking-daydream-migrate-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Path=TrackWorld(WorldName);
    Check(TEXT("A legal runtime writes a completed schema1 sidecar"),MakeValidSidecar(WorldName,TEXT("migration-resident")));

    FString OriginalText; TSharedPtr<FJsonObject> Root;
    bool bLoaded=LoadJson(Path,OriginalText,Root);
    Check(TEXT("The completed sidecar is valid JSON"),bLoaded);
    const TArray<TSharedPtr<FJsonValue>>* Residents=nullptr;
    TSharedPtr<FJsonObject> Resident;
    if(bLoaded && Root->TryGetArrayField(TEXT("residents"),Residents) && Residents->Num()==1
        && (*Residents)[0].IsValid() && (*Residents)[0]->Type==EJson::Object)
    {
        Resident=(*Residents)[0]->AsObject();
    }
    Check(TEXT("The completed sidecar has one resident object"),Resident.IsValid());
    double LastApiUtc=0.0;
    Check(TEXT("The new sidecar has the paid daydream fields"),Resident.IsValid()
        && Resident->HasTypedField<EJson::Number>(TEXT("last_daydream_monotonic"))
        && Resident->HasTypedField<EJson::Number>(TEXT("last_daydream_utc"))
        && Resident->TryGetNumberField(TEXT("last_api_utc"),LastApiUtc));

    if(Resident.IsValid())
    {
        // Remove both optional fields to model a schema1 file from before the
        // independent paid daydream clock was introduced.
        Resident->RemoveField(TEXT("last_daydream_monotonic"));
        Resident->RemoveField(TEXT("last_daydream_utc"));
        Check(TEXT("Write the old schema1 sidecar shape"),SaveJson(Path,Root));
    }

    {
        FHearthThinkingRuntime Migrated(WorldName);
        FHearthNpcThinkingRequest Daydream;
        Daydream.ResidentId=TEXT("migration-resident"); Daydream.EventId=TEXT("migration-daydream");
        Daydream.Role=EHearthNpcThinkingRole::Civilian; Daydream.Trigger=EHearthNpcThinkingTrigger::Daydream;
        const FHearthThinkingGateResult DreamAdmission=Migrated.Admit(Daydream,-1.0);
        Check(TEXT("Migrated old JSON keeps an immediate daydream local"),!DreamAdmission.bDispatch
            && DreamAdmission.bUseLocalBehavior);

        FHearthNpcThinkingRequest Interaction;
        Interaction.ResidentId=TEXT("migration-resident"); Interaction.EventId=TEXT("migration-interaction");
        Interaction.Role=EHearthNpcThinkingRole::Civilian; Interaction.Trigger=EHearthNpcThinkingTrigger::ActualInteraction;
        const FHearthThinkingGateResult ImmediateInteraction=Migrated.Admit(Interaction,-1.0);
        Check(TEXT("Migrated old JSON preserves the 120 second interaction guard"),!ImmediateInteraction.bDispatch
            && ImmediateInteraction.bUseLocalBehavior);
    }

    FString MigratedText; TSharedPtr<FJsonObject> MigratedRoot;
    bool bMigratedLoaded=LoadJson(Path,MigratedText,MigratedRoot);
    const TArray<TSharedPtr<FJsonValue>>* MigratedResidents=nullptr;
    TSharedPtr<FJsonObject> MigratedResident;
    if(bMigratedLoaded && MigratedRoot->TryGetArrayField(TEXT("residents"),MigratedResidents)
        && MigratedResidents->Num()==1 && (*MigratedResidents)[0].IsValid())
    {
        MigratedResident=(*MigratedResidents)[0]->AsObject();
    }
    double MigratedApiUtc=0.0, MigratedDaydreamUtc=0.0;
    Check(TEXT("Runtime rewrites migrated daydream timestamps"),MigratedResident.IsValid()
        && MigratedResident->TryGetNumberField(TEXT("last_api_utc"),MigratedApiUtc)
        && MigratedResident->TryGetNumberField(TEXT("last_daydream_utc"),MigratedDaydreamUtc)
        && MigratedApiUtc==MigratedDaydreamUtc);

    // Move only the API clock 121 seconds into the past and remove the new
    // fields again. This lets the next runtime prove that ordinary interaction
    // pacing is still 120 seconds while the migrated daydream remains six-hour gated.
    if(MigratedResident.IsValid())
    {
        // Preserve a valid known pair; cold runtime uses the UTC value.
        MigratedResident->SetNumberField(TEXT("last_api_monotonic"),0.0);
        MigratedResident->SetNumberField(TEXT("last_api_utc"),LastApiUtc-121.0);
        MigratedResident->RemoveField(TEXT("last_daydream_monotonic"));
        MigratedResident->RemoveField(TEXT("last_daydream_utc"));
        Check(TEXT("Write the elapsed interaction pacing fixture"),SaveJson(Path,MigratedRoot));
    }
    {
        FHearthThinkingRuntime Elapsed(WorldName);
        FHearthNpcThinkingRequest Daydream;
        Daydream.ResidentId=TEXT("migration-resident"); Daydream.EventId=TEXT("elapsed-daydream");
        Daydream.Role=EHearthNpcThinkingRole::Civilian; Daydream.Trigger=EHearthNpcThinkingTrigger::Daydream;
        const FHearthThinkingGateResult DreamAdmission=Elapsed.Admit(Daydream,-1.0);
        Check(TEXT("A migrated daydream remains blocked before six hours"),!DreamAdmission.bDispatch
            && DreamAdmission.bUseLocalBehavior);

        FHearthNpcThinkingRequest Interaction;
        Interaction.ResidentId=TEXT("migration-resident"); Interaction.EventId=TEXT("elapsed-interaction");
        Interaction.Role=EHearthNpcThinkingRole::Civilian; Interaction.Trigger=EHearthNpcThinkingTrigger::ActualInteraction;
        const FHearthThinkingGateResult InteractionAdmission=Elapsed.Admit(Interaction,-1.0);
        Check(TEXT("An elapsed migrated interaction dispatches after 120 seconds"),InteractionAdmission.bDispatch);
        if(InteractionAdmission.bDispatch)
            Check(TEXT("The elapsed interaction completes cleanly"),Elapsed.Complete(InteractionAdmission.RequestId,true));
    }

    const FString MissingWorld=TEXT("thinking-daydream-missing-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString MissingPath=TrackWorld(MissingWorld);
    Check(TEXT("Create an independent sidecar for the missing-field case"),MakeValidSidecar(MissingWorld,TEXT("missing-resident")));
    FString MissingBefore; TSharedPtr<FJsonObject> MissingRoot;
    bLoaded=LoadJson(MissingPath,MissingBefore,MissingRoot);
    const TArray<TSharedPtr<FJsonValue>>* MissingResidents=nullptr;
    if(bLoaded && MissingRoot->TryGetArrayField(TEXT("residents"),MissingResidents) && MissingResidents->Num()==1)
        (*MissingResidents)[0]->AsObject()->RemoveField(TEXT("last_daydream_utc"));
    Check(TEXT("Write the one-field-missing sidecar"),bLoaded && SaveJson(MissingPath,MissingRoot));
    FString MissingCorrupt;
    Check(TEXT("Capture the one-field-missing bytes"),FFileHelper::LoadFileToString(MissingCorrupt,*MissingPath));
    {
        FHearthThinkingRuntime Runtime(MissingWorld); FHearthNpcThinkingRequest Request;
        Request.ResidentId=TEXT("missing-resident"); Request.EventId=TEXT("should-stay-local");
        Request.Role=EHearthNpcThinkingRole::Civilian; Request.Trigger=EHearthNpcThinkingTrigger::ImportantEvent;
        const FHearthThinkingGateResult Admission=Runtime.Admit(Request,-1.0);
        Check(TEXT("One missing daydream field forces local behavior"),!Admission.bDispatch && Admission.bUseLocalBehavior);
    }
    FString MissingAfter; FFileHelper::LoadFileToString(MissingAfter,*MissingPath);
    Check(TEXT("One-field corruption is not overwritten"),MissingAfter==MissingCorrupt);

    const FString WrongWorld=TEXT("thinking-daydream-type-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString WrongPath=TrackWorld(WrongWorld);
    Check(TEXT("Create an independent sidecar for the wrong-type case"),MakeValidSidecar(WrongWorld,TEXT("wrong-type-resident")));
    FString WrongBefore; TSharedPtr<FJsonObject> WrongRoot;
    bLoaded=LoadJson(WrongPath,WrongBefore,WrongRoot);
    const TArray<TSharedPtr<FJsonValue>>* WrongResidents=nullptr;
    if(bLoaded && WrongRoot->TryGetArrayField(TEXT("residents"),WrongResidents) && WrongResidents->Num()==1)
        (*WrongResidents)[0]->AsObject()->SetStringField(TEXT("last_daydream_utc"),TEXT("wrong-type"));
    Check(TEXT("Write the wrong-type daydream sidecar"),bLoaded && SaveJson(WrongPath,WrongRoot));
    FString WrongCorrupt;
    Check(TEXT("Capture the wrong-type bytes"),FFileHelper::LoadFileToString(WrongCorrupt,*WrongPath));
    {
        FHearthThinkingRuntime Runtime(WrongWorld); FHearthNpcThinkingRequest Request;
        Request.ResidentId=TEXT("wrong-type-resident"); Request.EventId=TEXT("should-stay-local");
        Request.Role=EHearthNpcThinkingRole::Civilian; Request.Trigger=EHearthNpcThinkingTrigger::ImportantEvent;
        const FHearthThinkingGateResult Admission=Runtime.Admit(Request,-1.0);
        Check(TEXT("Wrong-type daydream field forces local behavior"),!Admission.bDispatch && Admission.bUseLocalBehavior);
    }
    FString WrongAfter; FFileHelper::LoadFileToString(WrongAfter,*WrongPath);
    Check(TEXT("Wrong-type corruption is not overwritten"),WrongAfter==WrongCorrupt);

    for(const FString& CleanupPath:CleanupPaths) Files.DeleteFile(*CleanupPath);
    return bAllPassed;
}

#endif
