#include "HearthVillage.h"
#include "HearthWorldRequestJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace HearthRequestRuntime
{
    FString NormalizeNeed(const FString& Text)
    {
        FString Out; bool bPendingSpace=false;
        for(TCHAR C:Text)
        {
            if(FChar::IsWhitespace(C)) { bPendingSpace=!Out.IsEmpty(); continue; }
            if(bPendingSpace) Out.AppendChar(TEXT(' '));
            bPendingSpace=false; Out.AppendChar(FChar::ToLower(C));
        }
        return Out;
    }

    bool HasStoredObservation(const FHearthResident& Resident)
    {
        if(Resident.StableId.IsEmpty() || Resident.LastVisualSignature.IsEmpty()) return false;
        const FString Filename=FPaths::MakeValidFileName(Resident.StableId+TEXT("_")+Resident.LastVisualSignature+TEXT(".png"));
        const FString Path=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/DesignReviews")/Filename;
        return IFileManager::Get().FileExists(*Path) && IFileManager::Get().FileExists(*(Path+TEXT(".json")));
    }
}

bool AHearthVillage::SubmitResidentWorldRequest(int32 Index,const FString& Need)
{
    if(!Residents.IsValidIndex(Index)) return false;
    FString Error;
    if(!HearthWorldRequests::Submit(WorldRequests,Residents[Index].StableId,Need,Error)) return false;
    auto& R=Residents[Index]; R.DesignRequest=Need.TrimStartAndEnd();
    R.LatestEvent=TEXT("主持人已记录我的需求：")+R.DesignRequest;
    SaveWorld(); return true;
}

bool AHearthVillage::SubmitResidentAssetRequest(int32 Index,const FString& Need)
{
    if(!Residents.IsValidIndex(Index)) return false;
    const FHearthResident& Resident=Residents[Index];
    FHearthAssetNeedContext Context; Context.Purpose=Need.TrimStartAndEnd();
    FHearthResidentAssetContext Snapshot;
    Snapshot.ResidentId=Resident.StableId; Snapshot.Name=Resident.Name; Snapshot.Personality=Resident.Personality;
    Snapshot.InnerStory=Resident.InnerStory; Snapshot.DesignGoal=Resident.DesignGoal;
    Context.ResidentContexts.Add(MoveTemp(Snapshot));
    FVector Center=FVector::ZeroVector; double Width=0; FString TargetId;
    if(ResidentObservationTarget(Index,Center,Width,TargetId))
    {
        Context.TargetId=TargetId; Context.TargetPositionCm=Center; Context.bHasTargetPosition=true;
    }
    if(!Resident.LastVisualSignature.IsEmpty() && Resident.LastVisualSignature==VisualSignature(Index)
        && HearthRequestRuntime::HasStoredObservation(Resident))
        Context.ObservationId=Resident.LastVisualSignature;
    FString Error;
    if(!HearthWorldRequests::SubmitAsset(WorldRequests,Resident.StableId,Context,Error)) return false;
    auto& MutableResident=Residents[Index]; MutableResident.DesignRequest=Context.Purpose;
    MutableResident.LatestEvent=TEXT("主持人已记录我的物件或空间需求：")+MutableResident.DesignRequest;
    SaveWorld(); return true;
}

void AHearthVillage::ReconcileWorldRequests()
{
    // Migrate pre-board design requests once on load, preserving any board IDs.
    // A full board leaves the original resident record available for review.
    for(int32 ResidentIndex=0;ResidentIndex<Residents.Num();++ResidentIndex)
    {
        const auto& R=Residents[ResidentIndex];
        if(R.DesignRequest.TrimStartAndEnd().IsEmpty()) continue;
        const bool bHasTypedRequest=WorldRequests.ContainsByPredicate([&](const FHearthWorldRequest& Request)
        {
            return Request.bHasAssetContext && Request.RequesterIds.Contains(R.StableId)
                && HearthRequestRuntime::NormalizeNeed(Request.AssetContext.Purpose)==HearthRequestRuntime::NormalizeNeed(R.DesignRequest);
        });
        if(!bHasTypedRequest)
        {
            FString Error; HearthWorldRequests::Submit(WorldRequests,R.StableId,R.DesignRequest,Error);
        }
    }
}

FString AHearthVillage::ExportWorldRequests() const
{
    auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("schema_version"),1);
    J->SetStringField(TEXT("world_id"),WorldId);
    J->SetArrayField(TEXT("requests"),HearthWorldRequestJson::Encode(WorldRequests));
    FString Text;
    FJsonSerializer::Serialize(J,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    return Text;
}

FString AHearthVillage::WorldRequestSummary(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return FString();
    const auto& R=Residents[Index];
    // Several households can share a request. Show this resident's latest need,
    // without presenting a different request as an answer to it.
    const FString Need=HearthRequestRuntime::NormalizeNeed(R.DesignRequest);
    const FHearthWorldRequest* LatestTyped=nullptr;
    const FHearthWorldRequest* LatestLegacy=nullptr;
    for(int32 RequestIndex=WorldRequests.Num()-1;RequestIndex>=0;--RequestIndex)
    {
        const auto& Candidate=WorldRequests[RequestIndex];
        const FString CandidateNeed=Candidate.bHasAssetContext?Candidate.AssetContext.Purpose:Candidate.Summary;
        if(Candidate.RequesterIds.Contains(R.StableId) && HearthRequestRuntime::NormalizeNeed(CandidateNeed)==Need)
        {
            if(Candidate.bHasAssetContext && !LatestTyped) LatestTyped=&Candidate;
            if(!Candidate.bHasAssetContext && !LatestLegacy) LatestLegacy=&Candidate;
        }
    }
    const FHearthWorldRequest* Request=LatestTyped?LatestTyped:LatestLegacy;
    if(!Request) return R.DesignRequest.IsEmpty()?TEXT("尚未提出新需求"):TEXT("需求保留在个人记录中，尚未进入主持人队列");
    return Request->Resolution;
}
