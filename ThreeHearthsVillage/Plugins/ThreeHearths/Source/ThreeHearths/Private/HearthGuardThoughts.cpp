#include "HearthVillage.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Serialization/JsonSerializer.h"

bool AHearthVillage::RequestGuardDaydream(int32 Index)
{
    if(!IsOrganicVillage() || bSimulationPaused || !bAutonomousLifeEnabled || !bApiReady
        || bApiDisabledThisRun || ApiRequests>=ApiMaxRequests || !IsSharedServiceResident(Index)
        || !HasDecisionCapacity(Index)) return false;
    const auto& R=Residents[Index];
    if((R.ServiceRoleKey!=TEXT("gatekeeper") && R.ServiceRoleKey!=TEXT("royal_guard"))
        || R.Task!=EHearthTask::LifeActivity || !R.Route.IsEmpty() || !R.ConversationId.IsEmpty()
        || R.Hunger>=65.f || R.Energy<=35.f) return false;
    const auto* Duty=ServiceDuties.FindByPredicate([Index,&R](const FHearthServiceDutyRecord& D)
        {return D.Status==TEXT("active") && D.Resident==Index && D.TaskId==R.ActiveTaskId;});
    if(!Duty) return false;
    auto Context=MakeShared<FJsonObject>();
    Context->SetStringField(TEXT("request_kind"),TEXT("guard_daydream"));
    Context->SetStringField(TEXT("name"),R.Name); Context->SetStringField(TEXT("role"),R.Role);
    Context->SetStringField(TEXT("personality"),R.Personality);
    Context->SetStringField(TEXT("duty"),Duty->Kind);
    Context->SetStringField(TEXT("remembered_relationships"),RelationshipSummary(Index));
    Context->SetNumberField(TEXT("hunger"),R.Hunger); Context->SetNumberField(TEXT("energy"),R.Energy);
    // Job and meal explanations overwrite R.Reason; recall an actual private
    // reflection from the persisted history instead of calling those a memory.
    FString PreviousThought;
    for(int32 H=DecisionHistory.Num()-1;H>=0;--H)
        if(DecisionHistory[H].Resident==Index && DecisionHistory[H].Kind==TEXT("guard_daydream")
            && DecisionHistory[H].Status==TEXT("completed"))
        { PreviousThought=DecisionHistory[H].Reason; break; }
    Context->SetStringField(TEXT("previous_private_thought"),PreviousThought);
    auto Choice=MakeShared<FJsonObject>(); Choice->SetNumberField(TEXT("id"),0);
    Choice->SetStringField(TEXT("meaning"),TEXT("继续值勤；只记录一句自己的心事"));
    Context->SetArrayField(TEXT("available_actions"),{MakeShared<FJsonValueObject>(Choice)});
    const FString Prompt=TEXT("You are this named medieval guard, with your own personality and feelings. You are still on duty. In a quiet moment, think one brief first-person Chinese thought, at most 60 Chinese characters, about a hope, a worry, or someone you remember. Respect the supplied facts; uncertain wishes must remain wishes. This is private reflection: nobody has spoken to you, you have not left duty, and no money, marriage, visit or event has happened because you thought of it. Return only JSON: action_id must be 0, reason is that single personal thought. Do not give system advice or a plan for the player.");
    const int32 Before=ApiRequests;
    SendDecisionRequest(Index,Context,Prompt,true,false,FString(),true);
    return ApiRequests>Before;
}

void AHearthVillage::AdvanceGuardThoughts()
{
    // Optional observation of an ordinary, actually met visitor. This never
    // starts a conversation or relocates a resident for the screenshot.
    if(!bGuardVisitCaptured && FParse::Param(FCommandLine::Get(),TEXT("HearthCaptureGateVisit")))
    {
        for(const auto& S:Conversations)
        {
            if(S.bClosed || !S.bMet || S.Lines.Num()<2 || S.Second!=10 || !Residents.IsValidIndex(S.First)) continue;
            const auto& Gate=Residents[10]; const auto& Visitor=Residents[S.First];
            const auto* Duty=ServiceDuties.FindByPredicate([&Gate](const FHearthServiceDutyRecord& D)
                {return D.Resident==10 && D.Status==TEXT("active") && D.TaskId==Gate.ActiveTaskId;});
            if(!Duty || !IsValid(Gate.Actor) || !IsValid(Visitor.Actor)) continue;
            const float Distance=FVector::Dist2D(Gate.Actor->GetActorLocation(),Visitor.Actor->GetActorLocation());
            if(Distance>300.f) continue;
            bGuardVisitCaptured=true;
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/MedievalReview/GateVisitCapture");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/TEXT("gate-visitor.png"),false,false);
            auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("kind"),TEXT("actual_spontaneous_gate_visit"));
            J->SetStringField(TEXT("world_id"),WorldId); J->SetStringField(TEXT("conversation_id"),S.Id);
            J->SetStringField(TEXT("duty_task_id"),Duty->TaskId); J->SetNumberField(TEXT("duty_seconds"),Duty->DutySeconds);
            J->SetStringField(TEXT("visitor"),Visitor.Name); J->SetNumberField(TEXT("distance_cm"),Distance);
            J->SetNumberField(TEXT("simulation_seconds"),Elapsed);
            TArray<TSharedPtr<FJsonValue>> Lines;
            for(const auto& Line:S.Lines)
            {
                auto L=MakeShared<FJsonObject>(); L->SetNumberField(TEXT("speaker"),Line.Speaker);
                L->SetStringField(TEXT("source"),Line.Source); L->SetStringField(TEXT("text"),Line.Text);
                Lines.Add(MakeShared<FJsonValueObject>(L));
            }
            J->SetArrayField(TEXT("lines"),Lines); FString Json;
            FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));
            FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("capture-state.json")));
            UE_LOG(LogTemp,Display,TEXT("GATE_VISIT_CAPTURED visitor=%d distance_cm=%.1f duty=%s"),S.First,Distance,*Duty->TaskId);
            break;
        }
    }
    if(!IsOrganicVillage() || !bApiReady || bApiDisabledThisRun || bSimulationPaused || !bAutonomousLifeEnabled) return;
    const double Now=FPlatformTime::Seconds();
    if(Now<NextGuardThoughtProbeAt) return;
    NextGuardThoughtProbeAt=Now+60.0;
    for(int32 Index=10;Index<Residents.Num();++Index) RequestGuardDaydream(Index);
}

void AHearthVillage::ApplyGuardDaydream(int32 Index,const FHearthPendingDecision& Reply)
{
    const bool bValid=Residents.IsValidIndex(Index) && Reply.Error.IsEmpty()
        && Reply.Choice==0 && !Reply.Reason.TrimStartAndEnd().IsEmpty() && Reply.Reason.Len()<=512;
    if(DecisionHistory.IsValidIndex(Reply.HistoryIndex))
    {
        auto& H=DecisionHistory[Reply.HistoryIndex]; H.Reason=Reply.Reason; H.Latency=Reply.Latency;
        H.Tokens=Reply.Tokens; H.bHasUsage=Reply.bHasUsage;
        H.Status=bValid?TEXT("completed"):TEXT("failed");
        H.Result=bValid?TEXT("已记下内心想法；职责、工资、位置和交谈均未被改变。"):Reply.Error.IsEmpty()?TEXT("白日梦格式无效，继续原有职责。"):Reply.Error;
        ++HistoryRevision; SaveHistory();
    }
    if(bValid)
    {
        auto& R=Residents[Index]; R.Reason=Reply.Reason;
        // Do not replace the current dialogue, duty progress or a newer task
        // event. The private thought is visible in the resident inspector.
        R.DecisionNote=TEXT("心事：")+Reply.Reason;
        ++ApiSuccesses;
        UE_LOG(LogTemp,Display,TEXT("GUARD_DAYDREAM_ACCEPTED resident=%d tokens=%d"),Index,Reply.Tokens);
    }
    else UE_LOG(LogTemp,Display,TEXT("GUARD_DAYDREAM_SKIPPED resident=%d"),Index);
    WriteSnapshot();
}
