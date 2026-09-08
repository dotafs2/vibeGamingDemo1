#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <cstdio>
#endif

namespace HearthThinking
{
    EHearthNpcThinkingRole RoleFor(const FString& Role)
    {
        if(Role.Contains(TEXT("门卫")) || Role.Contains(TEXT("gatekeeper"),ESearchCase::IgnoreCase))
            return EHearthNpcThinkingRole::Gatekeeper;
        if(Role.Contains(TEXT("王室护卫")) || Role.Contains(TEXT("守卫")) || Role.Contains(TEXT("护卫"))
            || Role.Contains(TEXT("guard"),ESearchCase::IgnoreCase))
            return EHearthNpcThinkingRole::RoyalGuard;
        if(Role.Contains(TEXT("车夫")) || Role.Contains(TEXT("carter"),ESearchCase::IgnoreCase))
            return EHearthNpcThinkingRole::Carter;
        return EHearthNpcThinkingRole::Civilian;
    }

    static FHearthNpcThinkingClock Clock()
    {
        FHearthNpcThinkingClock Result;
        Result.MonotonicSeconds=FPlatformTime::Seconds();
        Result.UtcSeconds=static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
        return Result;
    }

    static FString RoleString(const EHearthNpcThinkingRole Role)
    { return FHearthNpcThinkingPolicy::RoleName(Role); }

    static FString TriggerString(const EHearthNpcThinkingTrigger Trigger)
    { return FHearthNpcThinkingPolicy::TriggerName(Trigger); }

    static bool ParseRole(const FString& Text, EHearthNpcThinkingRole& Out)
    {
        if(Text==TEXT("gatekeeper")) Out=EHearthNpcThinkingRole::Gatekeeper;
        else if(Text==TEXT("royal_guard")) Out=EHearthNpcThinkingRole::RoyalGuard;
        else if(Text==TEXT("carter")) Out=EHearthNpcThinkingRole::Carter;
        else if(Text==TEXT("civilian")) Out=EHearthNpcThinkingRole::Civilian;
        else return false;
        return true;
    }

    static bool ParseTrigger(const FString& Text, EHearthNpcThinkingTrigger& Out)
    {
        if(Text==TEXT("routine")) Out=EHearthNpcThinkingTrigger::Routine;
        else if(Text==TEXT("actual_interaction")) Out=EHearthNpcThinkingTrigger::ActualInteraction;
        else if(Text==TEXT("important_event")) Out=EHearthNpcThinkingTrigger::ImportantEvent;
        else if(Text==TEXT("daydream")) Out=EHearthNpcThinkingTrigger::Daydream;
        else return false;
        return true;
    }

    static TSharedRef<FJsonObject> RequestJson(const FHearthNpcThinkingRequest& Request)
    {
        auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("resident_id"),Request.ResidentId);
        J->SetStringField(TEXT("role"),RoleString(Request.Role));
        J->SetStringField(TEXT("trigger"),TriggerString(Request.Trigger));
        J->SetStringField(TEXT("event_id"),Request.EventId);
        J->SetStringField(TEXT("coalesce_key"),Request.CoalesceKey);
        J->SetStringField(TEXT("context"),Request.Context);
        J->SetBoolField(TEXT("urgent"),Request.bUrgent);
        J->SetNumberField(TEXT("deadline_utc"),Request.DeadlineUtcSeconds);
        return J;
    }

    static bool RequestFromJson(const TSharedPtr<FJsonObject>& J, FHearthNpcThinkingRequest& Out)
    {
        if(!J.IsValid()) return false;
        FString Role,Trigger;
        if(!J->TryGetStringField(TEXT("resident_id"),Out.ResidentId)
            || !J->TryGetStringField(TEXT("role"),Role)
            || !J->TryGetStringField(TEXT("trigger"),Trigger)
            || !J->TryGetStringField(TEXT("event_id"),Out.EventId)) return false;
        J->TryGetStringField(TEXT("coalesce_key"),Out.CoalesceKey);
        J->TryGetStringField(TEXT("context"),Out.Context);
        J->TryGetBoolField(TEXT("urgent"),Out.bUrgent);
        if(J->HasTypedField<EJson::Number>(TEXT("deadline_utc")) && (!J->TryGetNumberField(TEXT("deadline_utc"),Out.DeadlineUtcSeconds)
            || !FMath::IsFinite(Out.DeadlineUtcSeconds) || (Out.DeadlineUtcSeconds!=-1.0 && Out.DeadlineUtcSeconds<0.0))) return false;
        return ParseRole(Role,Out.Role) && ParseTrigger(Trigger,Out.Trigger)
            && Out.ResidentId.Len()<=256 && Out.EventId.Len()<=512 && Out.CoalesceKey.Len()<=512 && Out.Context.Len()<=65536;
    }

    static void AddRequestArray(TArray<TSharedPtr<FJsonValue>>& Values,const TArray<FHearthNpcThinkingRequest>& Requests)
    { for(const auto& Request:Requests) Values.Add(MakeShared<FJsonValueObject>(RequestJson(Request))); }

    static bool ReadRequestArray(const TSharedPtr<FJsonObject>& Root,const TCHAR* Name,TArray<FHearthNpcThinkingRequest>& Out,int32 Max)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!Root->TryGetArrayField(Name,Values) || Values->Num()>Max) return false;
        for(const auto& Value:*Values)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
            FHearthNpcThinkingRequest Request;
            if(!RequestFromJson(Value->AsObject(),Request)) return false;
            Out.Add(MoveTemp(Request));
        }
        return true;
    }

    static TSharedRef<FJsonObject> PersistenceJson(const FHearthNpcThinkingPersistence& Persistence)
    {
        auto Root=MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("schema"),1);
        Root->SetNumberField(TEXT("suspended_until_utc"),Persistence.SuspendedUntilUtcSeconds);
        TArray<TSharedPtr<FJsonValue>> Residents;
        for(const auto& R:Persistence.Residents)
        {
            auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("resident_id"),R.ResidentId); J->SetStringField(TEXT("role"),RoleString(R.Role));
            J->SetNumberField(TEXT("last_api_monotonic"),R.LastApiMonotonicSeconds); J->SetNumberField(TEXT("last_api_utc"),R.LastApiUtcSeconds);
            J->SetNumberField(TEXT("last_daydream_monotonic"),R.LastDaydreamMonotonicSeconds); J->SetNumberField(TEXT("last_daydream_utc"),R.LastDaydreamUtcSeconds);
            J->SetNumberField(TEXT("urgent_window_monotonic"),R.UrgentWindowMonotonicSeconds); J->SetNumberField(TEXT("urgent_window_utc"),R.UrgentWindowUtcSeconds);
            J->SetNumberField(TEXT("urgent_count"),R.UrgentDispatchCount); J->SetNumberField(TEXT("local_routine_monotonic"),R.LastLocalRoutineMonotonicSeconds);
            J->SetNumberField(TEXT("local_routine_utc"),R.LastLocalRoutineUtcSeconds); J->SetNumberField(TEXT("local_daydream_monotonic"),R.LastLocalDaydreamMonotonicSeconds);
            J->SetNumberField(TEXT("local_daydream_utc"),R.LastLocalDaydreamUtcSeconds);
            TArray<TSharedPtr<FJsonValue>> Seen; for(const FString& Id:R.SeenEventIds) Seen.Add(MakeShared<FJsonValueString>(Id)); J->SetArrayField(TEXT("seen_event_ids"),Seen);
            Residents.Add(MakeShared<FJsonValueObject>(J));
        }
        Root->SetArrayField(TEXT("residents"),Residents);
        TArray<TSharedPtr<FJsonValue>> Pending; AddRequestArray(Pending,Persistence.Pending); Root->SetArrayField(TEXT("pending"),Pending);
        TArray<TSharedPtr<FJsonValue>> Uncertain; AddRequestArray(Uncertain,Persistence.UncertainInFlight); Root->SetArrayField(TEXT("uncertain_in_flight"),Uncertain);
        return Root;
    }

    static bool PersistenceFromJson(const TSharedPtr<FJsonObject>& Root,FHearthNpcThinkingPersistence& Out)
    {
        if(!Root.IsValid() || Root->Values.Num()>8) return false;
        double Schema=0; if(!Root->TryGetNumberField(TEXT("schema"),Schema) || Schema!=1) return false;
        if(!Root->HasTypedField<EJson::Number>(TEXT("suspended_until_utc")) || !Root->TryGetNumberField(TEXT("suspended_until_utc"),Out.SuspendedUntilUtcSeconds)
            || !FMath::IsFinite(Out.SuspendedUntilUtcSeconds) || (Out.SuspendedUntilUtcSeconds!=-1.0 && Out.SuspendedUntilUtcSeconds<0.0)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!Root->TryGetArrayField(TEXT("residents"),Values) || Values->Num()>40) return false;
        for(const auto& Value:*Values)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
            const auto J=Value->AsObject(); FHearthNpcThinkingResidentPersistence R; FString Role;
            if(!J->TryGetStringField(TEXT("resident_id"),R.ResidentId) || !J->TryGetStringField(TEXT("role"),Role) || !ParseRole(Role,R.Role)) return false;
            auto Number=[&](const TCHAR* Name,double& Target)
            {
                return J->HasTypedField<EJson::Number>(Name) && J->TryGetNumberField(Name,Target)
                    && FMath::IsFinite(Target) && (Target==-1.0 || Target>=0.0);
            };
            double UrgentCount=0;
            if(!Number(TEXT("last_api_monotonic"),R.LastApiMonotonicSeconds) || !Number(TEXT("last_api_utc"),R.LastApiUtcSeconds)
                || (J->Values.Contains(TEXT("last_daydream_monotonic")) != J->Values.Contains(TEXT("last_daydream_utc")))
                || (J->Values.Contains(TEXT("last_daydream_monotonic"))
                    && (!Number(TEXT("last_daydream_monotonic"),R.LastDaydreamMonotonicSeconds) || !Number(TEXT("last_daydream_utc"),R.LastDaydreamUtcSeconds)))
                || !Number(TEXT("urgent_window_monotonic"),R.UrgentWindowMonotonicSeconds) || !Number(TEXT("urgent_window_utc"),R.UrgentWindowUtcSeconds)
                || !Number(TEXT("local_routine_monotonic"),R.LastLocalRoutineMonotonicSeconds) || !Number(TEXT("local_routine_utc"),R.LastLocalRoutineUtcSeconds)
                || !Number(TEXT("local_daydream_monotonic"),R.LastLocalDaydreamMonotonicSeconds) || !Number(TEXT("local_daydream_utc"),R.LastLocalDaydreamUtcSeconds)
                || !Number(TEXT("urgent_count"),UrgentCount) || UrgentCount<0 || UrgentCount>FHearthNpcThinkingPolicy::MaxUrgentDispatchesPerWindow
                || UrgentCount!=FMath::FloorToDouble(UrgentCount)) return false;
            if (!J->Values.Contains(TEXT("last_daydream_monotonic")))
            {
                // schema1 sidecars written before paid daydream had its own
                // clock are migrated conservatively: an old paid dispatch is
                // also treated as the last daydream, avoiding a restart burst.
                R.LastDaydreamMonotonicSeconds=R.LastApiMonotonicSeconds;
                R.LastDaydreamUtcSeconds=R.LastApiUtcSeconds;
            }
            R.UrgentDispatchCount=static_cast<int32>(UrgentCount);
            const TArray<TSharedPtr<FJsonValue>>* Seen=nullptr;
            if(!J->TryGetArrayField(TEXT("seen_event_ids"),Seen) || Seen->Num()>FHearthNpcThinkingPolicy::MaxRememberedEventIdsPerResident) return false;
            for(const auto& Item:*Seen) { FString Id; if(!Item.IsValid() || !Item->TryGetString(Id) || Id.IsEmpty() || Id.Len()>512) return false; R.SeenEventIds.Add(MoveTemp(Id)); }
            Out.Residents.Add(MoveTemp(R));
        }
        return ReadRequestArray(Root,TEXT("pending"),Out.Pending,FHearthNpcThinkingPolicy::MaxPendingRequests)
            && ReadRequestArray(Root,TEXT("uncertain_in_flight"),Out.UncertainInFlight,FHearthNpcThinkingPolicy::MaxConcurrentRequests);
    }

    static bool AtomicText(const FString& Path,const FString& Text)
    {
        auto& Files=FPlatformFileManager::Get().GetPlatformFile(); Files.CreateDirectoryTree(*FPaths::GetPath(Path));
        const FString Temp=Path+TEXT(".tmp-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        bool Written=false;
        { TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Temp)); FTCHARToUTF8 Bytes(*Text); Written=Handle.IsValid() && Handle->Write(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length()) && Handle->Flush(true); }
        if(Written)
        {
#if PLATFORM_WINDOWS
            Written=!!MoveFileExW(*FPaths::ConvertRelativePathToFull(Temp),*FPaths::ConvertRelativePathToFull(Path),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#else
            Written=std::rename(TCHAR_TO_UTF8(*Temp),TCHAR_TO_UTF8(*Path))==0;
#endif
        }
        if(!Written) Files.DeleteFile(*Temp); return Written;
    }
}

class FHearthThinkingRuntime::FImpl
{
public:
    explicit FImpl(const FString& InWorldName):WorldName(InWorldName)
    {
        FString DeadlineText;
        if(FParse::Value(FCommandLine::Get(),TEXT("HearthApiDeadlineUtc="),DeadlineText))
        {
            FDateTime Deadline;
            if(!FDateTime::ParseIso8601(*DeadlineText,Deadline)) bDeadlineInvalid=true;
            else DeadlineUtc=static_cast<double>(Deadline.ToUnixTimestamp());
        }
        SavePath=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/World")/(FPaths::MakeValidFileName(WorldName.IsEmpty()?TEXT("organic-world"):WorldName)+TEXT(".thinking.json"));
        Initialize();
    }

    FHearthThinkingGateResult Admit(FHearthNpcThinkingRequest Request)
    {
        FHearthThinkingGateResult Result;
        const auto Now=HearthThinking::Clock();
        if(bDeadlineInvalid || !Now.IsValid()) { Result.bUseLocalBehavior=true; Result.Reason=TEXT("思考 deadline 或真实时钟无效，使用本地规则"); return Result; }
        if(LiveResidents.Contains(Request.ResidentId))
        { Result.bUseLocalBehavior=true; Result.Reason=TEXT("该居民已有在途思考，请先按本地规则继续"); return Result; }
        if(FMath::Max(Policy.InFlightCount(),LiveRequestResidents.Num())>=FHearthNpcThinkingPolicy::MaxConcurrentRequests)
        { Result.bUseLocalBehavior=true; Result.Reason=TEXT("思考并发上限已满，使用本地规则"); return Result; }
        if(!bPersistenceValid)
        { Result.bUseLocalBehavior=true; Result.Reason=TEXT("思考状态未能安全保存，使用本地规则"); return Result; }
        if(DeadlineUtc>0.0) Request.DeadlineUtcSeconds=DeadlineUtc;
        const FHearthNpcThinkingAdmissionResult Admission=Policy.Submit(Request,Now);
        Persist();
        if(!Admission.bPaidKimiEligible)
        {
            Result.bUseLocalBehavior=true; Result.Reason=Admission.Reason; return Result;
        }
        FHearthNpcThinkingRequest Ready; FString RequestId;
        if(!Policy.TryDequeueReady(Now,Ready,RequestId))
        {
            DropPending(Request);
            Result.bUseLocalBehavior=true; Result.Reason=Admission.Reason.IsEmpty()?TEXT("当前请求不可立即执行，使用本地规则"):Admission.Reason;
            Persist(); return Result;
        }
        if(!Persist())
        {
            Policy.Complete(RequestId,Now,false); Result.bUseLocalBehavior=true; Result.Reason=TEXT("思考状态未能保存，使用本地规则"); Persist(); return Result;
        }
        LiveResidents.Add(Ready.ResidentId); LiveRequestResidents.Add(RequestId,Ready.ResidentId);
        Result.bDispatch=true; Result.RequestId=RequestId; Result.Reason=Admission.Reason; return Result;
    }

    bool Complete(const FString& RequestId,bool bAccepted)
    {
        const bool bDone=Policy.Complete(RequestId,HearthThinking::Clock(),bAccepted);
        if(const FString* Resident=LiveRequestResidents.Find(RequestId)) { LiveResidents.Remove(*Resident); LiveRequestResidents.Remove(RequestId); }
        Persist(); return bDone;
    }

    void Flush() { Persist(); }

private:
    FHearthNpcThinkingPolicy Policy;
    FString WorldName,SavePath;
    double DeadlineUtc=-1.0;
    bool bDeadlineInvalid=false, bPersistenceValid=true;
    TSet<FString> LiveResidents;
    TMap<FString,FString> LiveRequestResidents;

    void Initialize()
    {
        const auto Now=HearthThinking::Clock();
        auto& Files=FPlatformFileManager::Get().GetPlatformFile();
        if(Files.FileExists(*SavePath))
        {
            FString Text; TSharedPtr<FJsonObject> Root; FHearthNpcThinkingPersistence Persisted; FString Error;
            if(!FFileHelper::LoadFileToString(Text,*SavePath) || Text.Len()>2*1024*1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)
                || !HearthThinking::PersistenceFromJson(Root,Persisted) || !Policy.ImportPersistence(Persisted,Error)) bPersistenceValid=false;
            if(bPersistenceValid)
            {
                // A queued request carries an old HTTP context. It is never
                // replayed after restart; only cooldowns and uncertain paid
                // outcomes survive the world boundary.
                if(Persisted.Pending.Num()>0)
                {
                    FHearthNpcThinkingPersistence Cold=Persisted; Cold.Pending.Reset();
                    FHearthNpcThinkingPolicy Fresh; FString DropError;
                    if(!Fresh.ImportPersistence(Cold,DropError)) bPersistenceValid=false;
                    else Policy=MoveTemp(Fresh);
                }
                if(bPersistenceValid) Policy.BeginSession(Now,true);
            }
        }
        else Policy.BeginSession(Now,false);
    }

    bool Persist()
    {
        if(!bPersistenceValid) return false;
        FHearthNpcThinkingPersistence Persistence; Policy.ExportPersistence(Persistence);
        FString Text; FJsonSerializer::Serialize(HearthThinking::PersistenceJson(Persistence),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        const bool bWritten=HearthThinking::AtomicText(SavePath,Text); if(!bWritten) bPersistenceValid=false; return bWritten;
    }

    void DropPending(const FHearthNpcThinkingRequest& Target)
    {
        Policy.DiscardPending(Target.ResidentId,Target.CoalesceKey.IsEmpty()?Target.EventId:Target.CoalesceKey);
    }
};

FHearthThinkingRuntime::FHearthThinkingRuntime(const FString& WorldName):Impl(new FImpl(WorldName)) {}
FHearthThinkingRuntime::~FHearthThinkingRuntime() { if(Impl) { Impl->Flush(); delete Impl; Impl=nullptr; } }
FHearthThinkingGateResult FHearthThinkingRuntime::Admit(const FHearthNpcThinkingRequest& Request,double) { return Impl?Impl->Admit(Request):FHearthThinkingGateResult{false,true,FString(),TEXT("思考运行时未准备好")}; }
bool FHearthThinkingRuntime::Complete(const FString& RequestId,bool bAccepted) { return Impl?Impl->Complete(RequestId,bAccepted):false; }
void FHearthThinkingRuntime::Flush() { if(Impl) Impl->Flush(); }
