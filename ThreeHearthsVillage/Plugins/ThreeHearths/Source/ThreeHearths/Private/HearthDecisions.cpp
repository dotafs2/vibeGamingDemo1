#include "HearthVillage.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogHearthDecision, Log, All);

namespace HearthDecision
{
    const FString BudgetBase=TEXT("http://127.0.0.1:18766/v1");
    bool RequiresBudgetGateway(const FString& Base,const FString& Model)
    { return Base.StartsWith(TEXT("https://")) || Model.StartsWith(TEXT("kimi-")); }
    bool ReadBudgetDescriptor(const FString& Text,FString& Token,FString& Ledger)
    {
        TSharedPtr<FJsonObject> Object;
        FString Base,Model,Hash; double Version=0,Cap=0;
        if(Text.Len()>4096 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object) || !Object.IsValid()) return false;
        Object->TryGetStringField(TEXT("base_url"),Base); Object->TryGetStringField(TEXT("model"),Model);
        Object->TryGetStringField(TEXT("api_key"),Token); Object->TryGetStringField(TEXT("ledger_id"),Ledger);
        Object->TryGetStringField(TEXT("policy_sha256"),Hash);
        Object->TryGetNumberField(TEXT("schema_version"),Version); Object->TryGetNumberField(TEXT("allocation_cap_cny"),Cap);
        FGuid Id;
        if(Base!=BudgetBase || Model!=TEXT("kimi-k2.6") || Version!=1 || Cap!=95 || Token.Len()!=64 || Hash.Len()!=64 || !FGuid::Parse(Ledger,Id)) return false;
        for(TCHAR C:Token+Hash) if(!FChar::IsHexDigit(C)) return false;
        return true;
    }
    FString Json(const TSharedRef<FJsonObject>& Object)
    {
        FString Text; auto Writer=TJsonWriterFactory<>::Create(&Text);
        FJsonSerializer::Serialize(Object,Writer); return Text;
    }
    bool ValidBaseUrl(const FString& Url)
    {
        if(Url.Contains(TEXT("@")) || Url.Contains(TEXT("?")) || Url.Contains(TEXT("#"))) return false;
        for(TCHAR C:Url) if(FChar::IsWhitespace(C) || C==TEXT('"') || C==TEXT('\\')) return false;
        if(Url.StartsWith(TEXT("https://")) && Url.Len()>10) return true;
        return Url.StartsWith(TEXT("http://127.0.0.1:")) || Url.StartsWith(TEXT("http://localhost:"));
    }
    bool ParseChoice(FString Text,const FString& Field,int32 MaxChoice,int32& Plot,FString& Reason)
    {
        Text.TrimStartAndEndInline();
        if(Text.StartsWith(TEXT("```json")) && Text.EndsWith(TEXT("```"))) Text=Text.Mid(7,Text.Len()-10).TrimStartAndEnd();
        TSharedPtr<FJsonObject> Object;
        if(Text.Len()>4096 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object) || !Object.IsValid()) return false;
        double Number=0;
        if(Object->Values.Num()!=2 || !Object->HasTypedField<EJson::Number>(Field) || !Object->TryGetNumberField(Field,Number) || !FMath::IsFinite(Number) || Number<0 || Number>MaxChoice || Number!=FMath::FloorToDouble(Number)) return false;
        if(!Object->HasTypedField<EJson::String>(TEXT("reason")) || !Object->TryGetStringField(TEXT("reason"),Reason)) return false;
        Reason.TrimStartAndEndInline();
        if(Reason.IsEmpty() || Reason.Len()>180) return false;
        Reason.ReplaceInline(TEXT("\r"),TEXT(" ")); Reason.ReplaceInline(TEXT("\n"),TEXT(" "));
        Plot=static_cast<int32>(Number); return true;
    }
    bool ParsePlan(FString Text,int32& Plot,int32& HouseStyle,FString& Reason)
    {
        Text.TrimStartAndEndInline();
        if(Text.StartsWith(TEXT("```json")) && Text.EndsWith(TEXT("```"))) Text=Text.Mid(7,Text.Len()-10).TrimStartAndEnd();
        TSharedPtr<FJsonObject> Object;
        if(Text.Len()>4096 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object) || !Object.IsValid() || Object->Values.Num()!=3) return false;
        double PlotNumber=0,StyleNumber=0;
        if(!Object->HasTypedField<EJson::Number>(TEXT("plot_id")) || !Object->TryGetNumberField(TEXT("plot_id"),PlotNumber)
            || !FMath::IsFinite(PlotNumber) || PlotNumber<0 || PlotNumber>9 || PlotNumber!=FMath::FloorToDouble(PlotNumber)) return false;
        if(!Object->HasTypedField<EJson::Number>(TEXT("house_style_id")) || !Object->TryGetNumberField(TEXT("house_style_id"),StyleNumber)
            || !FMath::IsFinite(StyleNumber) || StyleNumber<0 || StyleNumber>2 || StyleNumber!=FMath::FloorToDouble(StyleNumber)) return false;
        if(!Object->HasTypedField<EJson::String>(TEXT("reason")) || !Object->TryGetStringField(TEXT("reason"),Reason)) return false;
        Reason.TrimStartAndEndInline();
        if(Reason.IsEmpty() || Reason.Len()>180) return false;
        Reason.ReplaceInline(TEXT("\r"),TEXT(" ")); Reason.ReplaceInline(TEXT("\n"),TEXT(" "));
        Plot=static_cast<int32>(PlotNumber); HouseStyle=static_cast<int32>(StyleNumber); return true;
    }
    bool ParseLifePlan(FString Text,int32& Action,FString& Reason) { return ParseChoice(Text,TEXT("action_id"),10000,Action,Reason); }
}

void AHearthVillage::StopDecisionRequests()
{
    ++DecisionGeneration;
    for(auto& Pending:PendingDecisions) if(Pending.Request.IsValid())
    {
        Pending.Request->OnProcessRequestComplete().Unbind();
        Pending.Request->CancelRequest(); Pending.Request.Reset();
    }
    PendingDecisions.Reset();
    if(BridgeProcess.IsValid())
    {
        if(FPlatformProcess::IsProcRunning(BridgeProcess)) FPlatformProcess::TerminateProc(BridgeProcess,true);
        FPlatformProcess::CloseProc(BridgeProcess);
        BridgeProcess.Reset();
    }
}

namespace HearthDecision
{
    bool RequestExceededSimulationDeadline(double CurrentSimulationTime,double StartedSimulationTime,float TimeoutSeconds)
    {
        return FMath::IsFinite(CurrentSimulationTime) && FMath::IsFinite(StartedSimulationTime)
            && FMath::IsFinite(TimeoutSeconds) && TimeoutSeconds>=0.f
            && CurrentSimulationTime-StartedSimulationTime>static_cast<double>(TimeoutSeconds);
    }
}

bool AHearthVillage::IsDecisionPending(int32 Index) const
{ return PendingDecisions.IsValidIndex(Index) && PendingDecisions[Index].bActive; }

int32 AHearthVillage::PendingDecisionCount() const
{ int32 Count=0; for(const auto& Pending:PendingDecisions) Count+=Pending.bActive; return Count; }

int32 AHearthVillage::DecisionConcurrencyLimit() const
{ return ApiBackend==TEXT("codex_spark")?1:FMath::Min(10,Residents.Num()); }

bool AHearthVillage::HasDecisionCapacity(int32 Index) const
{
    return Residents.IsValidIndex(Index) && PendingDecisions.IsValidIndex(Index)
        && !IsDecisionPending(Index) && PendingDecisionCount()<DecisionConcurrencyLimit();
}

void AHearthVillage::LoadApiConfig()
{
    bApiReady=false; bApiConfigured=false; bApiDisabledThisRun=false; bHasApiUsage=false;
    bApiBudgeted=false; ApiBudgetLedger.Empty(); ApiBudgetSpent=0; ApiBudgetReserved=0; ApiBudgetRemaining=0;
    ApiRequests=0; ApiSuccesses=0; ApiTokens=0;
    const bool bAutonomyForcedOff=FParse::Param(FCommandLine::Get(),TEXT("HearthNoAutonomousLife"));
    bAutonomousLifeEnabled=!bAutonomyForcedOff; LifeDecisionInterval=6.f; ApiMaxRequests=600;
    ApiBackend=TEXT("local"); ApiKey.Empty(); ApiModel.Empty();
    ApiStatus=TEXT("尚未配置模型 · 本地人设规则");
    FString ConfigPath=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/api-config.json");
    FParse::Value(FCommandLine::Get(),TEXT("HearthApiConfig="),ConfigPath);
    FString Text; TSharedPtr<FJsonObject> Config;
    if(!FFileHelper::LoadFileToString(Text,*ConfigPath)) return;
    if(Text.Len()>16384 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Config) || !Config.IsValid()) { ApiStatus=TEXT("API 配置格式有误 · 使用本地规则"); return; }
    if(!bAutonomyForcedOff) Config->TryGetBoolField(TEXT("autonomous_life"),bAutonomousLifeEnabled);
    double Interval=6; Config->TryGetNumberField(TEXT("life_decision_interval_seconds"),Interval);
    if(FMath::IsFinite(Interval)) LifeDecisionInterval=FMath::Clamp(static_cast<float>(Interval),1.f,120.f);
    if(FParse::Param(FCommandLine::Get(),TEXT("HearthDisableApi")))
    { bApiConfigured=true; bApiDisabledThisRun=true; ApiStatus=TEXT("本轮强制使用本地规则"); return; }
    bool Enabled=false; Config->TryGetBoolField(TEXT("enabled"),Enabled);
    if(!Enabled) { ApiStatus=TEXT("API 已关闭 · 本地人设规则"); return; }
    bApiConfigured=true;
    Config->TryGetStringField(TEXT("backend"),ApiBackend);
    Config->TryGetStringField(TEXT("model"),ApiModel);
    Config->TryGetStringField(TEXT("api_key"),ApiKey);
    FString Base; Config->TryGetStringField(TEXT("base_url"),Base);
    Base.TrimStartAndEndInline(); ApiModel.TrimStartAndEndInline(); ApiKey.TrimStartAndEndInline();
    while(Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);
    if(ApiBackend!=TEXT("codex_spark") && ApiBackend!=TEXT("openai_compatible")) { ApiStatus=TEXT("模型连接类型不支持 · 使用本地规则"); return; }
    if(ApiBackend!=TEXT("codex_spark"))
    {
        const FString KeyFromEnvironment=FPlatformMisc::GetEnvironmentVariable(TEXT("THREE_HEARTHS_API_KEY"));
        if(!KeyFromEnvironment.IsEmpty()) ApiKey=KeyFromEnvironment;
    }
    if(!HearthDecision::ValidBaseUrl(Base) || ApiModel.IsEmpty() || ApiModel.Len()>128 || ApiKey.Contains(TEXT("\r")) || ApiKey.Contains(TEXT("\n"))) { ApiStatus=TEXT("请检查 API 地址、模型与密钥"); return; }
    if(Base.StartsWith(TEXT("https://")) && ApiKey.IsEmpty()) { ApiStatus=TEXT("缺少 API 密钥 · 使用本地规则"); return; }
    ApiEndpoint=Base.EndsWith(TEXT("/chat/completions"))?Base:Base+TEXT("/chat/completions");
    double Value=30; Config->TryGetNumberField(TEXT("timeout_seconds"),Value); ApiTimeout=FMath::Clamp(static_cast<float>(Value),2.f,60.f);
    Value=256; Config->TryGetNumberField(TEXT("max_output_tokens"),Value); ApiMaxTokens=FMath::Clamp(static_cast<int32>(Value),128,2048);
    Value=600; Config->TryGetNumberField(TEXT("max_requests_per_run"),Value); ApiMaxRequests=FMath::IsFinite(Value)?static_cast<int32>(FMath::Clamp(Value,1.0,1000.0)):600;
    int32 RequestedMax=0; if(FParse::Value(FCommandLine::Get(),TEXT("HearthApiMaxRequests="),RequestedMax)) ApiMaxRequests=FMath::Clamp(RequestedMax,1,ApiMaxRequests);
    ApiTokenField=TEXT("max_completion_tokens"); Config->TryGetStringField(TEXT("token_limit_field"),ApiTokenField);
    ApiFormat=TEXT("json_object"); Config->TryGetStringField(TEXT("response_format"),ApiFormat);
    ApiThinkingMode.Empty(); Config->TryGetStringField(TEXT("thinking_mode"),ApiThinkingMode);
    if(!ApiThinkingMode.IsEmpty() && ApiThinkingMode!=TEXT("disabled") && ApiThinkingMode!=TEXT("enabled")) { ApiStatus=TEXT("思考模式配置有误 · 使用本地规则"); return; }
    if((ApiTokenField!=TEXT("max_tokens") && ApiTokenField!=TEXT("max_completion_tokens")) || (ApiFormat!=TEXT("json_object") && ApiFormat!=TEXT("prompt_only"))) { ApiStatus=TEXT("API 参数配置有误 · 使用本地规则"); return; }
    if(HearthDecision::RequiresBudgetGateway(Base,ApiModel))
    {
        // All HTTPS providers are blocked except the verified Kimi profile, which
        // is rerouted to the fixed, separately running persistent budget gateway.
        if(ApiModel!=TEXT("kimi-k2.6") || (Base!=TEXT("https://api.moonshot.cn/v1") && Base!=HearthDecision::BudgetBase))
        { ApiStatus=TEXT("该付费模型尚无已授权预算门禁 · 本地规则"); ApiKey.Empty(); return; }
        FString Descriptor;
        const FString Path=FPaths::ProjectSavedDir()/TEXT("ThreeHearths/Budget/gateway-endpoint.json");
        ApiKey.Empty(); // The upstream secret never leaves UE on the network.
        if(!FFileHelper::LoadFileToString(Descriptor,*Path) || !HearthDecision::ReadBudgetDescriptor(Descriptor,ApiKey,ApiBudgetLedger))
        { ApiStatus=TEXT("人民币预算门禁未启动 · 停止付费调用"); return; }
        bApiBudgeted=true;
        ApiEndpoint=HearthDecision::BudgetBase+TEXT("/chat/completions");
        ApiThinkingMode=TEXT("disabled"); ApiTokenField=TEXT("max_tokens"); ApiFormat=TEXT("json_object");
        ApiMaxTokens=FMath::Min(ApiMaxTokens,512); ApiTimeout=60.f;
    }
    if(ApiBackend==TEXT("codex_spark"))
    {
        // This private bridge is only for the currently signed-in user's local prototype.
        if(Base!=TEXT("http://127.0.0.1:18763/v1") || ApiModel!=TEXT("gpt-5.3-codex-spark") || ApiKey.Len()<32) { ApiStatus=TEXT("Spark 本机连接配置有误"); return; }
        const FString Python=FPaths::ConvertRelativePathToFull(FPaths::EngineDir()/TEXT("Binaries/ThirdParty/Python3/Win64/python.exe"));
        const FString Script=FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir()/TEXT("ThreeHearths/Tools/spark_bridge.py"));
        ConfigPath=FPaths::ConvertRelativePathToFull(ConfigPath);
        const FString Args=FString::Printf(TEXT("\"%s\" --config \"%s\" --parent-pid %u"),*Script,*ConfigPath,FPlatformProcess::GetCurrentProcessId());
        uint32 Pid=0;
        BridgeProcess=FPlatformProcess::CreateProc(*Python,*Args,false,true,true,&Pid,0,nullptr,nullptr);
        if(!BridgeProcess.IsValid()) { ApiStatus=TEXT("Spark 本机程序未能启动 · 使用本地规则"); return; }
        UE_LOG(LogHearthDecision,Display,TEXT("LOCAL_BRIDGE_STARTED pid=%u"),Pid);
    }
    bApiReady=true; ApiStatus=bApiBudgeted?TEXT("Kimi 预算门禁已连接 · 每人独立决策"):(ApiBackend==TEXT("codex_spark")?TEXT("5.3 Spark 已准备 · 等待村民选择"):TEXT("API 已配置 · 等待村民选择"));
}

void AHearthVillage::RequestDecision(int32 Index)
{
    if(!HasDecisionCapacity(Index)) return;
    if(bApiDisabledThisRun || ApiRequests>=ApiMaxRequests) { DecideLocally(Index,TEXT("本轮调用已停止或达到上限")); return; }
    auto Context=MakeShared<FJsonObject>(); auto Person=MakeShared<FJsonObject>();
    Person->SetNumberField(TEXT("id"),Index); Person->SetStringField(TEXT("name"),Residents[Index].Name); Person->SetStringField(TEXT("personality"),Residents[Index].Personality);
    Person->SetStringField(TEXT("stable_id"),Residents[Index].StableId); Person->SetStringField(TEXT("role"),Residents[Index].Role);
    Person->SetBoolField(TEXT("king"),Residents[Index].bKing);
    Context->SetObjectField(TEXT("resident"),Person);
    Context->SetNumberField(TEXT("available_wood"),AvailableWood());
    TArray<TSharedPtr<FJsonValue>> Plots;
    const TCHAR* Descriptions[]={TEXT("安静的林边，较大的小屋"),TEXT("公共花园旁，方便拜访邻居"),TEXT("紧凑的小屋，节约木材")};
    for(int32 P=0;P<HousingPlotCount();++P) if(PlotOwners[P]<0)
    {
        auto Plot=MakeShared<FJsonObject>(); Plot->SetNumberField(TEXT("id"),P); Plot->SetNumberField(TEXT("wood_cost"),PlotCosts[P]);
        Plot->SetStringField(TEXT("description"),P<3?FString(Descriptions[P]):PlotLabel(P)); Plots.Add(MakeShared<FJsonValueObject>(Plot));
    }
    if(Plots.IsEmpty()) { DecideLocally(Index); return; }
    Context->SetArrayField(TEXT("available_plots"),Plots);
    TArray<TSharedPtr<FJsonValue>> Styles;
    const TCHAR* Blueprints[]={TEXT("cottage_terracotta"),TEXT("longhouse_slateblue"),TEXT("townhouse_terracotta")};
    const TCHAR* StyleDescriptions[]={TEXT("奶油灰泥墙与暖色陶瓦的小屋"),TEXT("木构墙与蓝灰石板屋顶的长屋"),TEXT("石墙与暖色陶瓦的两层镇屋")};
    for(int32 Style=0;Style<3;++Style)
    {
        auto Item=MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("id"),Style); Item->SetStringField(TEXT("blueprint"),Blueprints[Style]);
        Item->SetStringField(TEXT("description"),StyleDescriptions[Style]); Styles.Add(MakeShared<FJsonValueObject>(Item));
    }
    Context->SetArrayField(TEXT("available_house_styles"),Styles);
    const FString Prompt=TEXT("You make one high-level decision for a villager in a medieval village simulation. Choose exactly one supplied available plot and one supplied available house style based on the resident's personality, description and wood cost. Return only JSON with exactly plot_id (integer), house_style_id (integer), and reason (brief first-person Chinese, at most 60 Chinese characters). The style controls the real finished house mesh and materials. Do not invent plots, styles, resources or actions. No tools, code, or explanations outside JSON.");
    SendDecisionRequest(Index,Context,Prompt,false);
}

void AHearthVillage::SendDecisionRequest(int32 Index,const TSharedRef<FJsonObject>& Context,const FString& Prompt,bool bLife,bool bSocial)
{
    if(bSimulationPaused || !HasDecisionCapacity(Index) || bApiDisabledThisRun || ApiRequests>=ApiMaxRequests) return;
    // Defense at the actual dispatch site: never issue a direct paid HTTPS call.
    if(ApiEndpoint.StartsWith(TEXT("https://")) || (ApiModel.StartsWith(TEXT("kimi-")) && (!bApiBudgeted || ApiEndpoint!=HearthDecision::BudgetBase+TEXT("/chat/completions"))))
    { bApiDisabledThisRun=true; ApiStatus=TEXT("已阻止绕过人民币预算的请求"); return; }
    auto System=MakeShared<FJsonObject>(); System->SetStringField(TEXT("role"),TEXT("system"));
    System->SetStringField(TEXT("content"),Prompt);
    auto User=MakeShared<FJsonObject>(); User->SetStringField(TEXT("role"),TEXT("user")); User->SetStringField(TEXT("content"),HearthDecision::Json(Context));
    auto Body=MakeShared<FJsonObject>(); Body->SetStringField(TEXT("model"),ApiModel);
    Body->SetArrayField(TEXT("messages"),{MakeShared<FJsonValueObject>(System),MakeShared<FJsonValueObject>(User)});
    Body->SetBoolField(TEXT("stream"),false); Body->SetNumberField(ApiTokenField,ApiMaxTokens);
    if(!ApiThinkingMode.IsEmpty()) { auto Thinking=MakeShared<FJsonObject>(); Thinking->SetStringField(TEXT("type"),ApiThinkingMode); Body->SetObjectField(TEXT("thinking"),Thinking); }
    if(ApiFormat==TEXT("json_object")) { auto Format=MakeShared<FJsonObject>(); Format->SetStringField(TEXT("type"),TEXT("json_object")); Body->SetObjectField(TEXT("response_format"),Format); }
    auto& Pending=PendingDecisions[Index]; Pending=FHearthPendingDecision();
    Pending.OperationId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Pending.bActive=true; Pending.bLife=bLife; Pending.bSocial=bSocial;
    Pending.StartedAt=FPlatformTime::Seconds(); Pending.StartedAtSimulation=Elapsed;
    Pending.ConversationId=bSocial?Residents[Index].ConversationId:FString();
    Pending.Serial=++DecisionSerial;
    Pending.AllowedActions=bSocial?AvailableSocialIntents(Index):bLife?AvailableLifeActions(Index):TArray<int32>();
    if(bSocial)
    {
        FHearthDecisionRecord H; H.Run=CurrentRun; H.Timestamp=FDateTime::Now().ToString(); H.Resident=Index; H.At=Elapsed;
        H.Kind=TEXT("social_turn"); H.Source=TEXT("api"); H.Model=ApiModel; H.Context=HearthDecision::Json(Context); H.Choice=TEXT("准备回应对方");
        Pending.HistoryIndex=DecisionHistory.Add(MoveTemp(H)); ++HistoryRevision; SaveHistory();
    }
    else { StartHistory(Index,bLife,TEXT("api")); Pending.HistoryIndex=Residents[Index].HistoryIndex; }
    Residents[Index].DecisionSource=TEXT("waiting"); Residents[Index].DecisionNote=bLife?TEXT("正在考虑下一项活动"):TEXT("正在向模型询问选址");
    if(bSocial) Residents[Index].DecisionNote=TEXT("正在认真听对方说话，准备回应");
    else Residents[Index].Reason=bLife?TEXT("家已经建好了，想想接下来做什么。"):TEXT("让我想想，这几块地哪一块更适合我。");
    ++ApiRequests;
    ApiStatus=FString::Printf(TEXT("%s正在思考 · 请求 %d / %d"),*Residents[Index].Name,ApiRequests,ApiMaxRequests);
    const uint64 Generation=DecisionGeneration;
    const uint64 Serial=Pending.Serial;
    TWeakObjectPtr<AHearthVillage> WeakThis(this);
    auto Request=FHttpModule::Get().CreateRequest(); Pending.Request=Request;
    Request->SetURL(ApiEndpoint); Request->SetVerb(TEXT("POST")); Request->SetHeader(TEXT("Content-Type"),TEXT("application/json"));
    if(!ApiKey.IsEmpty()) Request->SetHeader(TEXT("Authorization"),TEXT("Bearer ")+ApiKey);
    if(bApiBudgeted)
    {
        Request->SetHeader(TEXT("X-Hearth-Operation"),Pending.OperationId);
        Request->SetHeader(TEXT("X-Hearth-Resident"),Residents[Index].StableId);
    }
    Request->SetContentAsString(HearthDecision::Json(Body)); Request->SetTimeout(ApiTimeout); Request->SetActivityTimeout(ApiTimeout);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    Request->OnProcessRequestComplete().BindLambda([WeakThis,Generation,Serial,Index,bLife](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk) {
        auto* V=WeakThis.Get();
        if(!V || V->DecisionGeneration!=Generation || !V->IsDecisionPending(Index) || V->PendingDecisions[Index].Serial!=Serial) return;
        auto& Reply=V->PendingDecisions[Index];
        Reply.Request.Reset(); Reply.bReturned=true;
        Reply.Latency=FPlatformTime::Seconds()-Reply.StartedAt;
        if(V->DecisionHistory.IsValidIndex(Reply.HistoryIndex)) V->DecisionHistory[Reply.HistoryIndex].Latency=Reply.Latency;
        if(!bOk || !Response.IsValid()) { Reply.Error=TEXT("连接失败或请求超时"); if(V->bApiBudgeted) V->bApiDisabledThisRun=true; return; }
        const int32 Code=Response->GetResponseCode();
        if(Code<200 || Code>=300)
        {
            Reply.Error=FString::Printf(TEXT("接口返回 HTTP %d"),Code);
            if(Code==401 || Code==403 || Code==429 || Code==503 || (V->bApiBudgeted && (Code==402 || Code==502))) V->bApiDisabledThisRun=true;
            return;
        }
        TSharedPtr<FJsonObject> Envelope;
        if(Response->GetContent().Num()>65536 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Envelope) || !Envelope.IsValid()) { Reply.Error=TEXT("接口返回格式无效"); return; }
        if(V->bApiBudgeted)
        {
            const TSharedPtr<FJsonObject>* Budget=nullptr; FString Ledger;
            if(!Envelope->TryGetObjectField(TEXT("_hearth_budget"),Budget) || !(*Budget)->TryGetStringField(TEXT("ledger_id"),Ledger) || Ledger!=V->ApiBudgetLedger)
            { V->bApiDisabledThisRun=true; Reply.Error=TEXT("预算回执无效，已停止付费调用"); return; }
            (*Budget)->TryGetNumberField(TEXT("settled_cny"),V->ApiBudgetSpent);
            (*Budget)->TryGetNumberField(TEXT("reserved_cny"),V->ApiBudgetReserved);
            (*Budget)->TryGetNumberField(TEXT("remaining_allocatable_cny"),V->ApiBudgetRemaining);
        }
        const TSharedPtr<FJsonObject>* Usage=nullptr;
        if(Envelope->TryGetObjectField(TEXT("usage"),Usage))
        {
            double Total=0; if((*Usage)->TryGetNumberField(TEXT("total_tokens"),Total) && FMath::IsFinite(Total) && Total>=0 && Total<1000000)
            {
                Reply.Tokens=static_cast<int32>(Total); Reply.bHasUsage=true; V->ApiTokens+=Reply.Tokens; V->bHasApiUsage=true;
                if(V->DecisionHistory.IsValidIndex(Reply.HistoryIndex))
                { auto& Record=V->DecisionHistory[Reply.HistoryIndex]; Record.Tokens=Reply.Tokens; Record.bHasUsage=true; }
            }
        }
        const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;
        if(!Envelope->TryGetArrayField(TEXT("choices"),Choices) || Choices->Num()!=1 || !(*Choices)[0]->AsObject().IsValid()) { Reply.Error=TEXT("模型没有返回可用选择"); return; }
        const auto Choice=(*Choices)[0]->AsObject(); const TSharedPtr<FJsonObject>* Message=nullptr;
        FString Finish; Choice->TryGetStringField(TEXT("finish_reason"),Finish);
        if(Finish!=TEXT("stop") || !Choice->TryGetObjectField(TEXT("message"),Message)) { Reply.Error=TEXT("模型回答未完整生成"); return; }
        FString Content; FString Refusal; (*Message)->TryGetStringField(TEXT("refusal"),Refusal);
        if(!Refusal.IsEmpty() || !(*Message)->TryGetStringField(TEXT("content"),Content) || !(bLife?HearthDecision::ParseLifePlan(Content,Reply.Choice,Reply.Reason):HearthDecision::ParsePlan(Content,Reply.Choice,Reply.HouseStyle,Reply.Reason))) { Reply.Error=TEXT("模型选择不符合格式要求"); return; }
        V->ApiStatus=TEXT("已收到模型选择，等待执行");
    });
    // Persist the operation ID before a paid request can leave the process. A restart
    // recognizes the interrupted operation and never sends a second paid attempt.
    if(bApiBudgeted && bWorldPersistenceEnabled && !SaveWorld())
    { Request->OnProcessRequestComplete().Unbind(); Pending.Request.Reset(); Pending.Error=TEXT("世界存档失败，未发送付费请求"); Pending.bReturned=true; bApiDisabledThisRun=true; return; }
    if(!Request->ProcessRequest()) { Request->OnProcessRequestComplete().Unbind(); Pending.Request.Reset(); Pending.Error=TEXT("请求未能发出"); Pending.bReturned=true; }
    WriteSnapshot();
}

void AHearthVillage::ConsumeDecision()
{
    if(bSimulationPaused) return;
    for(int32 Index=0;Index<PendingDecisions.Num();++Index)
    {
        auto& Slot=PendingDecisions[Index];
        if(!Slot.bActive) continue;
        const bool bSimulationDeadline=HearthDecision::RequestExceededSimulationDeadline(Elapsed,Slot.StartedAtSimulation,ApiTimeout);
        const bool bWallDeadline=FPlatformTime::Seconds()-Slot.StartedAt>ApiTimeout+2;
        if(!Slot.bReturned && (bSimulationDeadline || bWallDeadline))
        {
            if(Slot.Request.IsValid()) { Slot.Request->OnProcessRequestComplete().Unbind(); Slot.Request->CancelRequest(); Slot.Request.Reset(); }
            Slot.Error=bSimulationDeadline?TEXT("模型未赶上当前游戏倍速，已采用本地规则"):TEXT("等待模型超时");
            Slot.bReturned=true; Slot.Latency=FPlatformTime::Seconds()-Slot.StartedAt;
            if(bApiBudgeted) bApiDisabledThisRun=true;
        }
        if(!Slot.bReturned) continue;
        auto Reply=MoveTemp(Slot); Slot=FHearthPendingDecision();
        if(Reply.bSocial)
        {
            if(!Residents.IsValidIndex(Index) || Residents[Index].ConversationId!=Reply.ConversationId) continue;
            if(Reply.Error.IsEmpty() && (!Reply.AllowedActions.Contains(Reply.Choice) || !ResolveSocialTurn(Index,Reply.Choice,Reply.Reason,TEXT("api"))))
                Reply.Error=TEXT("对话回应已不符合当前提议或会面状态");
            if(DecisionHistory.IsValidIndex(Reply.HistoryIndex))
            {
                auto& H=DecisionHistory[Reply.HistoryIndex]; H.Reason=Reply.Reason; H.Choice=TEXT("回应对话");
                H.Tokens=Reply.Tokens; H.Latency=Reply.Latency; H.bHasUsage=Reply.bHasUsage;
                H.Status=Reply.Error.IsEmpty()?TEXT("completed"):TEXT("failed"); H.Result=Reply.Error.IsEmpty()?TEXT("已当面说出并处理对应提议。"):Reply.Error;
                ++HistoryRevision; SaveHistory();
            }
            if(Reply.Error.IsEmpty())
            {
                ++ApiSuccesses; Residents[Index].DecisionSource=TEXT("api");
                Residents[Index].DecisionNote=FString::Printf(TEXT("%s · %.1f 秒"),*ApiModel,Reply.Latency);
                ApiStatus=FString::Printf(TEXT("已采纳 %d 个模型决定"),ApiSuccesses);
            }
            else { ApiStatus=Reply.Error+TEXT(" · 使用本地规则"); DecideSocialLocally(Index,Reply.Error); }
            WriteSnapshot(); continue;
        }
        if(!Residents.IsValidIndex(Index) || Residents[Index].Task!=(Reply.bLife?EHearthTask::LifeChoosing:EHearthTask::Choosing)) continue;
        if(DecisionHistory.IsValidIndex(Reply.HistoryIndex))
        {
            auto& Record=DecisionHistory[Reply.HistoryIndex];
            Record.Latency=Reply.Latency; Record.Tokens=Reply.Tokens; Record.bHasUsage=Reply.bHasUsage;
        }
        if(Reply.bLife && Reply.Error.IsEmpty() && !Reply.AllowedActions.Contains(Reply.Choice)) Reply.Error=TEXT("模型返回了未提供的技能或目标");
        // HTTP replies are parallel; world mutations remain ordered on the game thread.
        // Recheck present-day ownership, stock, routes and action availability before committing.
        const bool bStarted=Reply.Error.IsEmpty() && (Reply.bLife?StartLifeAction(Index,Reply.Choice,Reply.Reason,true):ReservePlot(Index,Reply.Choice,Reply.Reason,true));
        if(Reply.Error.IsEmpty() && !bStarted) Reply.Error=TEXT("模型选择的目标已不可用");
        if(Reply.Error.IsEmpty() && !Reply.bLife && !SetHouseStyle(Reply.HouseStyle,Residents[Index])) Reply.Error=TEXT("模型选择的房屋样式无效");
        if(!Reply.Error.IsEmpty())
        {
            if(DecisionHistory.IsValidIndex(Reply.HistoryIndex)) DecisionHistory[Reply.HistoryIndex].Context+=TEXT("\n模型调用未采用：")+Reply.Error;
            ApiStatus=Reply.Error+TEXT(" · 使用本地规则");
            UE_LOG(LogHearthDecision,Warning,TEXT("DECISION_FALLBACK resident=%d reason=%s"),Index,*Reply.Error);
            if(Reply.bLife) DecideLifeLocally(Index,Reply.Error); else DecideLocally(Index,Reply.Error);
        }
        else
        {
            ++ApiSuccesses;
            Residents[Index].DecisionNote=FString::Printf(TEXT("%s · %.1f 秒"),ApiBackend==TEXT("codex_spark")?TEXT("5.3 Spark"):*ApiModel,Reply.Latency);
            ApiStatus=FString::Printf(TEXT("已采纳 %d 个模型决定"),ApiSuccesses);
            UE_LOG(LogHearthDecision,Display,TEXT("DECISION_ACCEPTED resident=%d plot=%d backend=%s"),Index,Reply.Choice,*ApiBackend);
        }
        WriteSnapshot();
    }
}

FString AHearthVillage::ApiSummary() const
{
    if(!bApiReady) return ApiStatus;
    const FString Usage=bHasApiUsage?FString::Printf(TEXT(" · %d tokens"),ApiTokens):TEXT("");
    return FString::Printf(TEXT("%s · 本轮调用 %d / %d%s\n同时思考 %d / %d · %s"),ApiBackend==TEXT("codex_spark")?TEXT("5.3 Spark"):*ApiModel,ApiRequests,ApiMaxRequests,*Usage,PendingDecisionCount(),DecisionConcurrencyLimit(),*ApiStatus);
}
FString AHearthVillage::DecisionLabel(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return FString();
    const auto& R=Residents[Index];
    if(R.DecisionSource==TEXT("player")) return TEXT("你安排的任务");
    if(R.DecisionSource==TEXT("api")) return R.DecisionNote;
    if(R.DecisionSource==TEXT("local_fallback")) return TEXT("本地备用规则 · ")+R.DecisionNote;
    if(R.DecisionSource==TEXT("waiting")) return TEXT("模型正在考虑");
    if(R.DecisionSource==TEXT("local")) return TEXT("本地人设规则");
    return TEXT("还没有选定地块");
}
