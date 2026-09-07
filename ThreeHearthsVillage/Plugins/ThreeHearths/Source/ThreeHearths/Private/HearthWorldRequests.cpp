#include "HearthWorldRequests.h"

#include <initializer_list>

namespace
{
    constexpr int32 MaxTextLength = 512;
    constexpr int32 MaxIdLength = 128;
    constexpr int32 MaxRequesters = 32;
    constexpr int32 MaxResidentContexts = 32;

    FString Normalize(const FString& Value)
    {
        FString Result;
        Result.Reserve(Value.Len());
        bool bInWhitespace = false;
        for (const TCHAR Character : Value)
        {
            if (FChar::IsWhitespace(Character))
            {
                bInWhitespace = Result.Len() > 0;
                continue;
            }
            if (bInWhitespace)
            {
                Result.AppendChar(TEXT(' '));
                bInWhitespace = false;
            }
            Result.AppendChar(Character);
        }
        return Result.TrimStartAndEnd().ToLower();
    }

    bool IsAllowedCategory(const FString& Category)
    {
        return Category == TEXT("narrative") || Category == TEXT("existing_action")
            || Category == TEXT("asset") || Category == TEXT("mechanic")
            || Category == TEXT("clarification");
    }

    bool IsAllowedStatus(const FString& Status)
    {
        return Status == TEXT("proposed") || Status == TEXT("resolved_existing")
            || Status == TEXT("needs_details") || Status == TEXT("deferred");
    }

    bool IsCategoryStatusPairValid(const FString& Category, const FString& Status)
    {
        return (Category == TEXT("narrative") && Status == TEXT("deferred"))
            || (Category == TEXT("existing_action") && Status == TEXT("resolved_existing"))
            || ((Category == TEXT("asset") || Category == TEXT("mechanic")) && Status == TEXT("proposed"))
            || (Category == TEXT("clarification") && Status == TEXT("needs_details"));
    }

    bool IsFinitePosition(const FVector& Position)
    {
        return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
    }

    bool IsSafeId(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Len() > MaxIdLength) return false;
        for (const TCHAR Character : Value)
        {
            if (!((Character >= TEXT('A') && Character <= TEXT('Z'))
                || (Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9'))
                || Character == TEXT('_') || Character == TEXT('.')
                || Character == TEXT(':') || Character == TEXT('-')))
                return false;
        }
        return true;
    }

    bool HasAny(const FString& Text, std::initializer_list<const TCHAR*> Terms)
    {
        for (const TCHAR* Term : Terms)
            if (Text.Contains(Term)) return true;
        return false;
    }

    bool HasToken(const FString& Text, const TCHAR* Token)
    {
        int32 SearchFrom = 0;
        const int32 TokenLength = FCString::Strlen(Token);
        while (SearchFrom < Text.Len())
        {
            const int32 FoundAt = Text.Find(Token, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (FoundAt == INDEX_NONE) return false;
            const bool bLeftBoundary = FoundAt == 0 || !FChar::IsAlnum(Text[FoundAt - 1]);
            const int32 EndAt = FoundAt + TokenLength;
            const bool bRightBoundary = EndAt >= Text.Len() || !FChar::IsAlnum(Text[EndAt]);
            if (bLeftBoundary && bRightBoundary) return true;
            SearchFrom = EndAt;
        }
        return false;
    }

    void Classify(const FString& Need, FString& Category, FString& Status, FString& Resolution)
    {
        FString Text = Normalize(Need);
        // Classify the requested change, not an unrelated object seen in the
        // picture or a capability explicitly described as unknown.
        const int32 Intention=Text.Find(TEXT("打算："));
        if(Intention!=INDEX_NONE) Text=Text.Mid(Intention+3).TrimStartAndEnd();

        const bool bExplicitMechanic = HasAny(Text, {TEXT("how should"), TEXT("规则"), TEXT("机制"), TEXT("系统"),
            TEXT("玩法"), TEXT("决定方式"), TEXT("交互规则")});
        const bool bEnglishMechanic = HasToken(Text, TEXT("rule")) || HasToken(Text, TEXT("rules"))
            || HasToken(Text, TEXT("mechanic")) || HasToken(Text, TEXT("gameplay")) || HasToken(Text, TEXT("system"));
        const bool bVisualAsset = HasToken(Text,TEXT("model")) || HasToken(Text,TEXT("mesh"))
            || HasToken(Text,TEXT("signage")) || HasToken(Text,TEXT("sign")) || HasToken(Text,TEXT("prop"))
            || HasAny(Text, {TEXT("模型"), TEXT("标牌"), TEXT("招牌"), TEXT("道具"), TEXT("装饰"), TEXT("视觉"),
                TEXT("木牌"), TEXT("矮篱"), TEXT("门框"), TEXT("长凳"), TEXT("顶棚"), TEXT("铁砧")});
        const bool bNarrative = HasAny(Text, {TEXT("aspiration"), TEXT("family"), TEXT("imperial"), TEXT("political"),
            TEXT("家族"), TEXT("家人"), TEXT("帝国"), TEXT("政治"), TEXT("野心"), TEXT("皇室"),
            TEXT("皇城"), TEXT("订单"), TEXT("孩子"), TEXT("骑士")});

        // Explicit rule/system language always wins over broad action words. A pure
        // family/imperial wish remains narrative; a request for its rules is a mechanic.
        if (bExplicitMechanic || bEnglishMechanic)
        {
            Category = TEXT("mechanic");
            Status = TEXT("proposed");
            Resolution = TEXT("记录为待定义游戏规则；没有现成规则时不声称功能已实现。");
            return;
        }

        // Visual and physical requests are proposals for review, never claims of
        // an implemented asset, action, cost, or construction result.
        if (bVisualAsset)
        {
            Category = TEXT("asset");
            Status = TEXT("proposed");
            Resolution = TEXT("已记录物件或空间需求，等待尺寸、功能与美术方案核实。");
            return;
        }

        if (bNarrative)
        {
            Category = TEXT("narrative");
            Status = TEXT("deferred");
            Resolution = TEXT("仅作为叙事意图记录；不会据此声称已有家族、帝国或政治系统事实。");
            return;
        }

        if(HasAny(Text,{TEXT("扩建"),TEXT("加层"),TEXT("买下邻屋")}))
        {
            Category=TEXT("mechanic");Status=TEXT("proposed");
            Resolution=TEXT("记录住所改造能力需求；需核对当前建筑是否支持、用地和实际费用，不据文字自动扩建。");
            return;
        }

        // Only concrete, already available options are mapped. Generic build/rest/gather
        // wording remains ambiguous instead of being treated as implemented gameplay.
        if (HasToken(Text, TEXT("eat")) || HasToken(Text, TEXT("eating"))
            || HasToken(Text, TEXT("sleep")) || Text == TEXT("rest") || Text == TEXT("gather")
            || Text == TEXT("build") || HasAny(Text, {TEXT("take a meal"), TEXT("吃饭"), TEXT("进食"),
            TEXT("rest at the hearth"), TEXT("take a rest"), TEXT("休息"), TEXT("睡觉"),
            TEXT("gather resources"), TEXT("build a house"), TEXT("build a wall"), TEXT("build a courtyard"),
            TEXT("采集资源"), TEXT("建房"), TEXT("建造房屋"), TEXT("建造庭院"),
            TEXT("separate courtyard"), TEXT("独立庭院"), TEXT("分开庭院")}))
        {
            Category = TEXT("existing_action");
            Status = TEXT("resolved_existing");
            Resolution = TEXT("已映射到现有可用选项；仅记录提议，不表示行动已完成，也不新增虚构系统。");
            return;
        }

        Category = TEXT("clarification");
        Status = TEXT("needs_details");
        Resolution = TEXT("需要可验证的具体需求；当前文字不足以安全归类或执行。");
    }

    bool IsSameRequest(const FHearthWorldRequest& Request, const FString& NormalizedNeed)
    {
        return Normalize(Request.Summary) == NormalizedNeed;
    }

    bool IsValidId(const FString& Value)
    {
        return IsSafeId(Value);
    }

    bool ValidateAssetContext(const FHearthAssetNeedContext& Context, FString& Error)
    {
        if(Context.Purpose.IsEmpty() || Context.Purpose.Len()>MaxTextLength)
        { Error=TEXT("asset purpose must be 1..512 characters"); return false; }
        if(!Context.TargetId.IsEmpty() && !IsSafeId(Context.TargetId))
        { Error=TEXT("asset target id is empty or invalid"); return false; }
        if(Context.ObservationId.Len()>MaxTextLength)
        { Error=TEXT("asset observation id exceeds 512 characters"); return false; }
        if(Context.bHasTargetPosition && !IsFinitePosition(Context.TargetPositionCm))
        { Error=TEXT("asset target position must be finite"); return false; }
        if(Context.ResidentContexts.IsEmpty() || Context.ResidentContexts.Num()>MaxResidentContexts)
        { Error=TEXT("asset resident contexts must contain 1..32 residents"); return false; }
        TSet<FString> ContextIds;
        for(const FHearthResidentAssetContext& Resident:Context.ResidentContexts)
        {
            if(!IsSafeId(Resident.ResidentId))
            { Error=TEXT("asset resident context id is empty or invalid"); return false; }
            if(ContextIds.Contains(Resident.ResidentId))
            { Error=TEXT("asset resident context ids must be unique"); return false; }
            ContextIds.Add(Resident.ResidentId);
            if(Resident.Name.Len()>MaxTextLength || Resident.Personality.Len()>MaxTextLength
                || Resident.InnerStory.Len()>MaxTextLength || Resident.DesignGoal.Len()>MaxTextLength)
            { Error=TEXT("asset resident context field exceeds 512 characters"); return false; }
        }
        return true;
    }

    FString AssetRequestKey(const FHearthWorldRequest& Request, const FString& ResidentId)
    {
        return Normalize(Request.AssetContext.Purpose)+TEXT("|")+Request.AssetContext.TargetId+TEXT("|")+ResidentId;
    }
}

namespace HearthWorldRequests
{
    void RefreshUnresolved(TArray<FHearthWorldRequest>& Requests)
    {
        for(auto& Request:Requests) if(Request.Category==TEXT("clarification") && Request.Status==TEXT("needs_details"))
            Classify(Request.Summary,Request.Category,Request.Status,Request.Resolution);
    }

    bool Validate(const TArray<FHearthWorldRequest>& Requests, FString& Error)
    {
        Error.Empty();
        if (Requests.Num() > MaxRequests)
        {
            Error = FString::Printf(TEXT("world request board exceeds %d requests"), MaxRequests);
            return false;
        }

        TSet<FString> Ids;
        TSet<FString> LegacyNeeds;
        TSet<FString> AssetKeys;
        for (const FHearthWorldRequest& Request : Requests)
        {
            if (!IsValidId(Request.Id)) { Error = TEXT("request id is empty or invalid"); return false; }
            if (Ids.Contains(Request.Id)) { Error = TEXT("request ids must be unique"); return false; }
            Ids.Add(Request.Id);
            if (!IsAllowedCategory(Request.Category)) { Error = TEXT("request category is not allowed"); return false; }
            if (!IsAllowedStatus(Request.Status)) { Error = TEXT("request status is not allowed"); return false; }
            if (!IsCategoryStatusPairValid(Request.Category, Request.Status))
            { Error = TEXT("request category and status pair is invalid"); return false; }
            if (Request.Summary.IsEmpty() || Request.Summary.Len() > MaxTextLength)
            { Error = TEXT("request summary must be 1..512 characters"); return false; }
            if (Request.Resolution.Len() > MaxTextLength)
            { Error = TEXT("request resolution exceeds 512 characters"); return false; }
            const FString NormalizedNeed = Normalize(Request.Summary);
            if (NormalizedNeed.IsEmpty())
            { Error = TEXT("request summary must contain non-whitespace text"); return false; }
            if(Request.bHasAssetContext)
            {
                if(Request.Category!=TEXT("asset") || Request.Status!=TEXT("proposed")
                    || Normalize(Request.AssetContext.Purpose)!=NormalizedNeed)
                { Error=TEXT("asset context requires an asset proposal with matching purpose"); return false; }
                if(!ValidateAssetContext(Request.AssetContext,Error)) return false;
                for(const FString& ResidentId:Request.RequesterIds)
                {
                    const FString Key=AssetRequestKey(Request,ResidentId);
                    if(AssetKeys.Contains(Key)) { Error=TEXT("asset requests must be unique by purpose, target, and requester"); return false; }
                    AssetKeys.Add(Key);
                }
            }
            else
            {
                if(LegacyNeeds.Contains(NormalizedNeed))
                { Error = TEXT("request summaries must be unique after normalization"); return false; }
                LegacyNeeds.Add(NormalizedNeed);
            }
            if (Request.RequesterIds.IsEmpty()) { Error = TEXT("request must have at least one resident"); return false; }
            if (Request.RequesterIds.Num() > MaxRequesters)
            { Error = TEXT("request has too many residents"); return false; }
            TSet<FString> Requesters;
            for (const FString& ResidentId : Request.RequesterIds)
            {
                if (!IsValidId(ResidentId)) { Error = TEXT("requester id is empty or invalid"); return false; }
                if (Requesters.Contains(ResidentId)) { Error = TEXT("requester ids must be unique"); return false; }
                Requesters.Add(ResidentId);
            }
            if(Request.bHasAssetContext)
            {
                TSet<FString> ContextIds;
                for(const FHearthResidentAssetContext& Resident:Request.AssetContext.ResidentContexts) ContextIds.Add(Resident.ResidentId);
                bool bSameIds=ContextIds.Num()==Requesters.Num();
                for(const FString& ResidentId:Requesters) bSameIds&=ContextIds.Contains(ResidentId);
                if(!bSameIds) { Error=TEXT("asset resident contexts must exactly match requester ids"); return false; }
            }
        }
        return true;
    }

    bool Submit(TArray<FHearthWorldRequest>& Requests, const FString& ResidentId,
        const FString& Need, FString& Error)
    {
        Error.Empty();
        const FString CleanResidentId = ResidentId;
        const FString NormalizedNeed = Normalize(Need);
        if (!IsValidId(CleanResidentId)) { Error = TEXT("resident id is empty or invalid"); return false; }
        if (NormalizedNeed.IsEmpty() || Need.Len() > MaxTextLength)
        { Error = TEXT("need must be 1..512 characters"); return false; }
        if (!Validate(Requests, Error)) return false;

        for (FHearthWorldRequest& Request : Requests)
        {
            if (!Request.bHasAssetContext && IsSameRequest(Request, NormalizedNeed))
            {
                const bool bAlreadyRequested = Request.RequesterIds.ContainsByPredicate(
                    [&CleanResidentId](const FString& ExistingResidentId)
                    { return ExistingResidentId == CleanResidentId; });
                if (!bAlreadyRequested)
                {
                    if (Request.RequesterIds.Num() >= MaxRequesters)
                    { Error = TEXT("request has too many residents"); return false; }
                    Request.RequesterIds.Add(CleanResidentId);
                }
                return true;
            }
        }
        if (Requests.Num() >= MaxRequests)
        {
            Error = TEXT("world request board is full");
            return false;
        }

        FHearthWorldRequest Request;
        for (int32 Index = 1; Index <= MaxRequests; ++Index)
        {
            Request.Id = FString::Printf(TEXT("request_%d"), Index);
            if (!Requests.ContainsByPredicate([&Request](const FHearthWorldRequest& Existing)
                { return Existing.Id == Request.Id; })) break;
            Request.Id.Empty();
        }
        if (Request.Id.IsEmpty()) { Error = TEXT("no stable request id available"); return false; }
        Request.Summary = Need;
        Request.RequesterIds.Add(CleanResidentId);
        Classify(Need, Request.Category, Request.Status, Request.Resolution);
        Requests.Add(MoveTemp(Request));
        return true;
    }

    bool SubmitAsset(TArray<FHearthWorldRequest>& Requests, const FString& ResidentId,
        const FHearthAssetNeedContext& Context, FString& Error)
    {
        Error.Empty();
        const FString CleanResidentId=ResidentId;
        if(!IsValidId(CleanResidentId)) { Error=TEXT("resident id is empty or invalid"); return false; }
        if(!ValidateAssetContext(Context,Error)) return false;
        const FString NormalizedPurpose=Normalize(Context.Purpose);
        if(NormalizedPurpose.IsEmpty()) { Error=TEXT("asset purpose must contain non-whitespace text"); return false; }
        if(Context.ResidentContexts.Num()!=1 || Context.ResidentContexts[0].ResidentId!=CleanResidentId)
        { Error=TEXT("personal asset submission must contain exactly its submitting resident"); return false; }
        if(!Validate(Requests,Error)) return false;

        for(const FHearthWorldRequest& Request:Requests)
        {
            if(Request.bHasAssetContext && Request.Category==TEXT("asset") && Request.Status==TEXT("proposed")
                && Normalize(Request.AssetContext.Purpose)==NormalizedPurpose
                && Request.AssetContext.TargetId==Context.TargetId
                && Request.RequesterIds.Contains(CleanResidentId))
                return true;
        }
        if(Requests.Num()>=MaxRequests) { Error=TEXT("world request board is full"); return false; }

        FHearthWorldRequest Request;
        for(int32 Index=1;Index<=MaxRequests;++Index)
        {
            Request.Id=FString::Printf(TEXT("request_%d"),Index);
            if(!Requests.ContainsByPredicate([&](const FHearthWorldRequest& Existing){return Existing.Id==Request.Id;})) break;
            Request.Id.Empty();
        }
        if(Request.Id.IsEmpty()) { Error=TEXT("no stable request id available"); return false; }
        Request.Category=TEXT("asset"); Request.Status=TEXT("proposed"); Request.Summary=Context.Purpose;
        Request.Resolution=TEXT("已记录物件或空间需求，等待尺寸、功能与美术方案核实。"); Request.RequesterIds.Add(CleanResidentId);
        Request.bHasAssetContext=true; Request.AssetContext=Context;
        Requests.Add(MoveTemp(Request));
        return true;
    }
}
