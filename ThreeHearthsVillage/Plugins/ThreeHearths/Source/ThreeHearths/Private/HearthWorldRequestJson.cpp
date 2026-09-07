#include "HearthWorldRequestJson.h"
#include "Dom/JsonObject.h"

namespace
{
    using FJsonArray=TArray<TSharedPtr<FJsonValue>>;

    TSharedPtr<FJsonValue> EncodeAssetContext(const FHearthAssetNeedContext& Context)
    {
        auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("purpose"),Context.Purpose);
        J->SetStringField(TEXT("target_id"),Context.TargetId);
        FJsonArray Residents;
        for(const FHearthResidentAssetContext& Resident:Context.ResidentContexts)
        {
            auto R=MakeShared<FJsonObject>();
            R->SetStringField(TEXT("resident_id"),Resident.ResidentId);
            R->SetStringField(TEXT("name"),Resident.Name);
            R->SetStringField(TEXT("personality"),Resident.Personality);
            R->SetStringField(TEXT("inner_story"),Resident.InnerStory);
            R->SetStringField(TEXT("design_goal"),Resident.DesignGoal);
            Residents.Add(MakeShared<FJsonValueObject>(R));
        }
        J->SetArrayField(TEXT("resident_contexts"),Residents);
        if(Context.bHasTargetPosition)
        {
            J->SetArrayField(TEXT("target_position_cm"),FJsonArray{
                MakeShared<FJsonValueNumber>(Context.TargetPositionCm.X),
                MakeShared<FJsonValueNumber>(Context.TargetPositionCm.Y),
                MakeShared<FJsonValueNumber>(Context.TargetPositionCm.Z)});
        }
        else J->SetField(TEXT("target_position_cm"),MakeShared<FJsonValueNull>());
        J->SetStringField(TEXT("observation_id"),Context.ObservationId);
        return MakeShared<FJsonValueObject>(J);
    }

    bool DecodeAssetContext(const TSharedPtr<FJsonObject>& J,FHearthAssetNeedContext& Context)
    {
        if(!J.IsValid()) return false;
        const FJsonArray* Residents=nullptr;
        if(!J->TryGetStringField(TEXT("purpose"),Context.Purpose)
            || !J->TryGetStringField(TEXT("target_id"),Context.TargetId)
            || !J->TryGetArrayField(TEXT("resident_contexts"),Residents)
            || !J->HasField(TEXT("target_position_cm"))
            || !J->TryGetStringField(TEXT("observation_id"),Context.ObservationId)) return false;
        if(Residents->Num()>32) return false;
        for(const TSharedPtr<FJsonValue>& Value:*Residents)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) return false;
            const TSharedPtr<FJsonObject> R=Value->AsObject(); FHearthResidentAssetContext Resident;
            if(!R->TryGetStringField(TEXT("resident_id"),Resident.ResidentId)
                || !R->TryGetStringField(TEXT("name"),Resident.Name)
                || !R->TryGetStringField(TEXT("personality"),Resident.Personality)
                || !R->TryGetStringField(TEXT("inner_story"),Resident.InnerStory)
                || !R->TryGetStringField(TEXT("design_goal"),Resident.DesignGoal)) return false;
            Context.ResidentContexts.Add(MoveTemp(Resident));
        }
        const TSharedPtr<FJsonValue> Position=J->TryGetField(TEXT("target_position_cm"));
        if(!Position.IsValid()) return false;
        if(Position->Type==EJson::Null) Context.bHasTargetPosition=false;
        else
        {
            if(Position->Type!=EJson::Array || Position->AsArray().Num()!=3) return false;
            double Values[3]={0,0,0};
            for(int32 Index=0;Index<3;++Index)
                if(!Position->AsArray()[Index].IsValid() || Position->AsArray()[Index]->Type!=EJson::Number
                    || !Position->AsArray()[Index]->TryGetNumber(Values[Index]) || !FMath::IsFinite(Values[Index])) return false;
            Context.TargetPositionCm=FVector(Values[0],Values[1],Values[2]);
            Context.bHasTargetPosition=true;
        }
        return true;
    }
}

namespace HearthWorldRequestJson
{
    TArray<TSharedPtr<FJsonValue>> Encode(const TArray<FHearthWorldRequest>& Requests)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for(const auto& R:Requests)
        {
            auto J=MakeShared<FJsonObject>();
            J->SetStringField(TEXT("id"),R.Id); J->SetStringField(TEXT("category"),R.Category);
            J->SetStringField(TEXT("status"),R.Status); J->SetStringField(TEXT("summary"),R.Summary);
            J->SetStringField(TEXT("resolution"),R.Resolution);
            TArray<TSharedPtr<FJsonValue>> People;
            for(const auto& Id:R.RequesterIds) People.Add(MakeShared<FJsonValueString>(Id));
            J->SetArrayField(TEXT("requester_ids"),People);
            if(R.bHasAssetContext) J->SetField(TEXT("asset_context"),EncodeAssetContext(R.AssetContext));
            Out.Add(MakeShared<FJsonValueObject>(J));
        }
        return Out;
    }

    bool Decode(const TArray<TSharedPtr<FJsonValue>>& Values,TArray<FHearthWorldRequest>& Out,FString& Error)
    {
        Error=TEXT("主持人需求记录无效");
        if(Values.Num()>HearthWorldRequests::MaxRequests) return false;
        TArray<FHearthWorldRequest> Candidate;
        for(const auto& V:Values)
        {
            if(!V.IsValid() || V->Type!=EJson::Object) return false;
            const auto J=V->AsObject(); FHearthWorldRequest R;
            const TArray<TSharedPtr<FJsonValue>>* People=nullptr;
            if(!J->TryGetStringField(TEXT("id"),R.Id) || !J->TryGetStringField(TEXT("category"),R.Category)
                || !J->TryGetStringField(TEXT("status"),R.Status) || !J->TryGetStringField(TEXT("summary"),R.Summary)
                || !J->TryGetStringField(TEXT("resolution"),R.Resolution)
                || !J->TryGetArrayField(TEXT("requester_ids"),People) || People->Num()>32) return false;
            for(const auto& Person:*People)
            {
                FString Id;
                if(!Person.IsValid() || Person->Type!=EJson::String || !Person->TryGetString(Id)) return false;
                R.RequesterIds.Add(MoveTemp(Id));
            }
            if(J->HasField(TEXT("asset_context")))
            {
                const TSharedPtr<FJsonValue> AssetValue=J->TryGetField(TEXT("asset_context"));
                if(!AssetValue.IsValid() || AssetValue->Type!=EJson::Object
                    || !DecodeAssetContext(AssetValue->AsObject(),R.AssetContext)) return false;
                R.bHasAssetContext=true;
                TSet<FString> ContextIds;
                for(const FHearthResidentAssetContext& Resident:R.AssetContext.ResidentContexts) ContextIds.Add(Resident.ResidentId);
                bool bSameIds=ContextIds.Num()==R.RequesterIds.Num();
                for(const FString& Id:R.RequesterIds) bSameIds&=ContextIds.Contains(Id);
                if(!bSameIds) return false;
            }
            Candidate.Add(MoveTemp(R));
        }
        if(!HearthWorldRequests::Validate(Candidate,Error)) return false;
        Out=MoveTemp(Candidate); Error.Empty(); return true;
    }
}
