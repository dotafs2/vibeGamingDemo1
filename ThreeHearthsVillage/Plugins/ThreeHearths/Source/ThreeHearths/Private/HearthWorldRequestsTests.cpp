#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldRequests.h"
#include "HearthWorldRequestJson.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthWorldRequestsTest, "ThreeHearths.WorldRequests.TriageBoard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthWorldRequestsTest::RunTest(const FString&)
{
    TArray<FHearthWorldRequest> Requests;
    FString Error;
    TestTrue(TEXT("First request is accepted"), HearthWorldRequests::Submit(Requests, TEXT("resident_a"), TEXT("  Add a model of a well  "), Error));
    TestEqual(TEXT("Visual request is an asset"), Requests[0].Category, FString(TEXT("asset")));
    TestEqual(TEXT("Visual request remains proposed"), Requests[0].Status, FString(TEXT("proposed")));
    TestEqual(TEXT("Visual asset resolution stays a truthful pending review"), Requests[0].Resolution, FString(TEXT("已记录物件或空间需求，等待尺寸、功能与美术方案核实。")));
    const FString StableId = Requests[0].Id;
    TestTrue(TEXT("Case and whitespace replay deduplicates"), HearthWorldRequests::Submit(Requests, TEXT("resident_b"), TEXT("add   A MODEL of a WELL"), Error));
    TestEqual(TEXT("Replay keeps stable id"), Requests[0].Id, StableId);
    TestEqual(TEXT("Replay joins resident ids"), Requests[0].RequesterIds.Num(), 2);
    TestTrue(TEXT("Idempotent resident replay"), HearthWorldRequests::Submit(Requests, TEXT("resident_a"), TEXT("add a model of a well"), Error));
    TestEqual(TEXT("Idempotent replay does not duplicate resident"), Requests[0].RequesterIds.Num(), 2);

    TestTrue(TEXT("Existing eating action is redirected"), HearthWorldRequests::Submit(Requests, TEXT("resident_c"), TEXT("I need an eating action"), Error));
    TestEqual(TEXT("Eating is existing action"), Requests[1].Category, FString(TEXT("existing_action")));
    TestEqual(TEXT("Eating is resolved existing"), Requests[1].Status, FString(TEXT("resolved_existing")));
    TestTrue(TEXT("Existing mapping does not claim completion"), Requests[1].Resolution.Contains(TEXT("不表示行动已完成")));
    TestTrue(TEXT("Separate courtyard maps to a genuine existing option"), HearthWorldRequests::Submit(Requests, TEXT("resident_c"), TEXT("Use the separate courtyard option"), Error));
    TestEqual(TEXT("Separate courtyard is existing action"), Requests[2].Category, FString(TEXT("existing_action")));
    TestTrue(TEXT("Family aspiration is narrative"), HearthWorldRequests::Submit(Requests, TEXT("resident_d"), TEXT("My family should inherit an imperial title"), Error));
    TestEqual(TEXT("Family aspiration does not become mechanic"), Requests[3].Category, FString(TEXT("narrative")));
    TestEqual(TEXT("Family aspiration is deferred"), Requests[3].Status, FString(TEXT("deferred")));
    TestTrue(TEXT("Family system request becomes an unimplemented mechanic proposal"), HearthWorldRequests::Submit(Requests, TEXT("resident_d"), TEXT("Build a family system for the child"), Error));
    TestEqual(TEXT("Explicit family system goes to game rules"), Requests[4].Category, FString(TEXT("mechanic")));
    TestTrue(TEXT("Missing rules become mechanic"), HearthWorldRequests::Submit(Requests, TEXT("resident_e"), TEXT("Define the rules for how inheritance works"), Error));
    TestEqual(TEXT("Rules request is mechanic"), Requests[5].Category, FString(TEXT("mechanic")));
    TestTrue(TEXT("Bridge crossing rule is mechanic"), HearthWorldRequests::Submit(Requests, TEXT("resident_e"), TEXT("Build a bridge with a new crossing rule"), Error));
    TestEqual(TEXT("Bridge rule is not resolved as a build action"), Requests[6].Category, FString(TEXT("mechanic")));
    TestTrue(TEXT("Unsupported custom building remains unresolved"), HearthWorldRequests::Submit(Requests, TEXT("resident_e"), TEXT("Build a custom bridge"), Error));
    TestEqual(TEXT("Custom building needs clarification"), Requests[7].Category, FString(TEXT("clarification")));
    TestTrue(TEXT("Ambiguous request asks for details"), HearthWorldRequests::Submit(Requests, TEXT("resident_f"), TEXT("Make it better"), Error));
    TestEqual(TEXT("Ambiguous request is clarification"), Requests[8].Category, FString(TEXT("clarification")));
    TestEqual(TEXT("Ambiguous request needs details"), Requests[8].Status, FString(TEXT("needs_details")));
    TArray<FHearthWorldRequest> Chinese;
    TestTrue(TEXT("Imperial order text is narrative"), HearthWorldRequests::Submit(Chinese, TEXT("rider.A"), TEXT("皇城订单"), Error));
    TestEqual(TEXT("Imperial order is deferred narrative"), Chinese[0].Category, FString(TEXT("narrative")));
    TestTrue(TEXT("Imperial rule text is mechanic"), HearthWorldRequests::Submit(Chinese, TEXT("rider.B"), TEXT("皇城订单规则"), Error));
    TestEqual(TEXT("Imperial rule is proposed mechanic"), Chinese[1].Category, FString(TEXT("mechanic")));
    TestTrue(TEXT("Child model is an asset proposal"), HearthWorldRequests::Submit(Chinese, TEXT("rider.C"), TEXT("孩子模型"), Error));
    TestEqual(TEXT("Child model does not create family mechanics"), Chinese[2].Category, FString(TEXT("asset")));
    TArray<FHearthWorldRequest> Identity;
    const FString UpperGuid = TEXT("ABCDEF12-3456-7890-ABCD-EF1234567890");
    const FString RawNeed = TEXT("line one\nline two");
    TestTrue(TEXT("Uppercase resident identity is accepted"), HearthWorldRequests::Submit(Identity, UpperGuid, RawNeed, Error));
    TestEqual(TEXT("Resident identity is stored byte-for-byte"), Identity[0].RequesterIds[0], UpperGuid);
    TestEqual(TEXT("Newline request text is retained"), Identity[0].Summary, RawNeed);
    TestFalse(TEXT("Whitespace cannot bypass the raw persisted text bound"),HearthWorldRequests::Submit(Identity,UpperGuid,FString::ChrN(513,TEXT(' '))+TEXT("x"),Error));
    TestTrue(TEXT("Generic English proposal is accepted for clarification"),HearthWorldRequests::Submit(Identity,UpperGuid,TEXT("propose a better future"),Error));
    TestEqual(TEXT("Proposal substring cannot create art jobs"),Identity.Last().Category,FString(TEXT("clarification")));
    TestTrue(TEXT("Board validates"), HearthWorldRequests::Validate(Requests, Error));

    TArray<FHearthWorldRequest> Malformed = Requests;
    Malformed[0].Category = TEXT("code");
    TestFalse(TEXT("Malformed category is rejected"), HearthWorldRequests::Validate(Malformed, Error));
    Malformed = Requests;
    Malformed[0].Status = TEXT("needs_details");
    TestFalse(TEXT("Invalid category and status pair is rejected"), HearthWorldRequests::Validate(Malformed, Error));
    Malformed = Requests;
    Malformed[0].RequesterIds.Reset();
    TestFalse(TEXT("Empty requester list is rejected"), HearthWorldRequests::Validate(Malformed, Error));
    TArray<FHearthWorldRequest> Full;
    for (int32 Index = 0; Index < HearthWorldRequests::MaxRequests; ++Index)
    {
        TestTrue(TEXT("Fill board request accepted"), HearthWorldRequests::Submit(Full, TEXT("resident"), FString::Printf(TEXT("unique need %d"), Index), Error));
    }
    const int32 FullCount = Full.Num();
    TestFalse(TEXT("Full board refuses mutation"), HearthWorldRequests::Submit(Full, TEXT("resident"), TEXT("another unique need"), Error));
    TestEqual(TEXT("Full board remains unchanged"), Full.Num(), FullCount);
    TestFalse(TEXT("Full board refuses a keyword-overlapping different need"), HearthWorldRequests::Submit(Full, TEXT("resident"), TEXT("unique need 1 with a model"), Error));
    TArray<FHearthWorldRequest> Separate;
    TestTrue(TEXT("Different needs sharing a keyword remain separate"), HearthWorldRequests::Submit(Separate, TEXT("resident"), TEXT("a model of a gate"), Error));
    TestTrue(TEXT("Second distinct model need remains separate"), HearthWorldRequests::Submit(Separate, TEXT("resident"), TEXT("a model of a well"), Error));
    TestEqual(TEXT("Distinct model needs are not merged"), Separate.Num(), 2);
    TArray<FHearthWorldRequest> Visual;
    HearthWorldRequests::Submit(Visual,TEXT("innkeeper"),TEXT("看见：临街的房子；未知：能否坐下休息；打算：加一张木长凳。"),Error);
    TestEqual(TEXT("A bench requested in a real visual review is an asset proposal"),Visual.Last().Category,FString(TEXT("asset")));
    TestEqual(TEXT("The proposed bench is not already built or usable for resting"),Visual.Last().Status,FString(TEXT("proposed")));
    HearthWorldRequests::Submit(Visual,TEXT("potter"),TEXT("看见：烟囱；未知：内部规则；打算：在自家地块扩建独立住所。"),Error);
    TestEqual(TEXT("Changing a prefab asks for capability review"),Visual.Last().Category,FString(TEXT("mechanic")));
    HearthWorldRequests::Submit(Visual,TEXT("smith"),TEXT("看见：木牌；未知：家族系统；打算：改善一下。"),Error);
    TestEqual(TEXT("Observed objects and unknown systems cannot create a false art job"),Visual.Last().Category,FString(TEXT("clarification")));
    Visual[0].Category=TEXT("clarification");Visual[0].Status=TEXT("needs_details");
    const FString BeforeId=Visual[0].Id,BeforeSummary=Visual[0].Summary;
    HearthWorldRequests::RefreshUnresolved(Visual);
    TestEqual(TEXT("Previously unresolved model receipt can be classified on reload"),Visual[0].Category,FString(TEXT("asset")));
    TestEqual(TEXT("Reclassification preserves the request identity"),Visual[0].Id,BeforeId);
    TestEqual(TEXT("Reclassification preserves the actual model wording"),Visual[0].Summary,BeforeSummary);

    auto MakeAssetContext=[](const FString& Purpose,const FString& Target,const FString& ResidentId,const FVector& Position,bool bHasPosition)
    {
        FHearthAssetNeedContext Context; Context.Purpose=Purpose; Context.TargetId=Target;
        Context.TargetPositionCm=Position; Context.bHasTargetPosition=bHasPosition;
        FHearthResidentAssetContext Resident; Resident.ResidentId=ResidentId; Resident.Name=ResidentId;
        Resident.Personality=TEXT("careful"); Resident.InnerStory=TEXT("persistent story"); Resident.DesignGoal=TEXT("useful place");
        Context.ResidentContexts.Add(MoveTemp(Resident)); return Context;
    };
    TArray<FHearthWorldRequest> Assets;
    auto HouseA=MakeAssetContext(TEXT("Add a covered work space"),TEXT("house_alpha"),TEXT("resident_a"),FVector(100,200,0),true);
    auto HouseB=MakeAssetContext(TEXT("add   a covered work space"),TEXT("house_beta"),TEXT("resident_b"),FVector(-100,200,0),true);
    TestTrue(TEXT("Typed asset request for first resident is accepted"),HearthWorldRequests::SubmitAsset(Assets,TEXT("resident_a"),HouseA,Error));
    TestTrue(TEXT("Same purpose for another target and resident stays distinct"),HearthWorldRequests::SubmitAsset(Assets,TEXT("resident_b"),HouseB,Error));
    if(!TestEqual(TEXT("Personal target contexts create two requests"),Assets.Num(),2)) return false;
    TestTrue(TEXT("Exact typed replay is idempotent"),HearthWorldRequests::SubmitAsset(Assets,TEXT("resident_a"),HouseA,Error));
    TestEqual(TEXT("Typed replay remains one request per resident and target"),Assets.Num(),2);
    TestEqual(TEXT("Typed request is still proposed asset work"),Assets[0].Category,FString(TEXT("asset")));
    TestEqual(TEXT("Typed resolution remains pending review in the host language"),Assets[0].Resolution,FString(TEXT("已记录物件或空间需求，等待尺寸、功能与美术方案核实。")));
    TestTrue(TEXT("Typed contexts validate"),HearthWorldRequests::Validate(Assets,Error));
    const auto Encoded=HearthWorldRequestJson::Encode(Assets);
    const auto AssetValue=Encoded[0]->AsObject()->TryGetField(TEXT("asset_context"));
    TestTrue(TEXT("asset_context is a JSON object"),AssetValue.IsValid() && AssetValue->Type==EJson::Object);
    // JSON values are shared pointers; use an independent fixture for corruption.
    auto WrongPosition=HearthWorldRequestJson::Encode(Assets);
    WrongPosition[0]->AsObject()->TryGetField(TEXT("asset_context"))->AsObject()->SetArrayField(TEXT("target_position_cm"),
        TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueString>(TEXT("x")),MakeShared<FJsonValueNumber>(200),MakeShared<FJsonValueNumber>(0)});
    TArray<FHearthWorldRequest> RejectedJson;
    TestFalse(TEXT("JSON position rejects string items before numeric conversion"),HearthWorldRequestJson::Decode(WrongPosition,RejectedJson,Error));
    TArray<FHearthWorldRequest> DecodedAssets;
    if(!TestTrue(TEXT("Typed request JSON roundtrips through the world bridge"),HearthWorldRequestJson::Decode(Encoded,DecodedAssets,Error))) return false;
    if(!TestEqual(TEXT("Roundtrip preserves typed request count"),DecodedAssets.Num(),2)) return false;
    TestTrue(TEXT("Roundtrip preserves target position"),DecodedAssets[0].AssetContext.bHasTargetPosition && DecodedAssets[0].AssetContext.TargetPositionCm==FVector(100,200,0));
    TestEqual(TEXT("Roundtrip preserves persistent resident story"),DecodedAssets[0].AssetContext.ResidentContexts[0].InnerStory,FString(TEXT("persistent story")));
    TArray<FHearthWorldRequest> LegacyOnly;
    TestTrue(TEXT("Legacy request still encodes without optional context"),HearthWorldRequests::Submit(LegacyOnly,TEXT("legacy_resident"),TEXT("a plain request"),Error));
    TArray<FHearthWorldRequest> LegacyDecoded;
    if(!TestTrue(TEXT("Old request JSON without asset_context remains valid"),HearthWorldRequestJson::Decode(HearthWorldRequestJson::Encode(LegacyOnly),LegacyDecoded,Error))) return false;
    if(!TestEqual(TEXT("Old request has one decoded record"),LegacyDecoded.Num(),1)) return false;
    TestFalse(TEXT("Old request keeps asset_context absent"),LegacyDecoded[0].bHasAssetContext);
    TestTrue(TEXT("Legacy text can coexist with a typed request of the same purpose"),HearthWorldRequests::Submit(Assets,TEXT("legacy_resident"),HouseA.Purpose,Error));
    TestEqual(TEXT("Legacy same text does not merge into another target"),Assets.Num(),3);
    const int32 BeforeBad=Assets.Num(); auto Bad=HouseA; Bad.TargetPositionCm.X=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite target position is rejected"),HearthWorldRequests::SubmitAsset(Assets,TEXT("resident_a"),Bad,Error));
    TestEqual(TEXT("Bad typed input does not mutate the board"),Assets.Num(),BeforeBad);
    auto MixedContext=HouseA;
    MixedContext.ResidentContexts.Add(HouseB.ResidentContexts[0]);
    TestFalse(TEXT("Personal submission cannot silently add another resident's context"),HearthWorldRequests::SubmitAsset(Assets,TEXT("resident_a"),MixedContext,Error));
    TestEqual(TEXT("Rejected mixed context does not corrupt the existing board"),Assets.Num(),BeforeBad);
    TestTrue(TEXT("Board remains serializable after a rejected mixed context"),HearthWorldRequests::Validate(Assets,Error));
    auto Forged=Assets[0]; Forged.AssetContext.ResidentContexts[0].ResidentId=TEXT("resident_b");
    TestFalse(TEXT("Resident context ids must match requester ids"),HearthWorldRequests::Validate(TArray<FHearthWorldRequest>{Forged},Error));
    auto NullTarget=MakeAssetContext(TEXT("Add a cellar"),TEXT(""),TEXT("resident_c"),FVector::ZeroVector,false);
    TArray<FHearthWorldRequest> NullAssets;
    TestTrue(TEXT("Asset without an actual target is accepted with null position"),HearthWorldRequests::SubmitAsset(NullAssets,TEXT("resident_c"),NullTarget,Error));
    auto NullRoundtrip=HearthWorldRequestJson::Encode(NullAssets); TArray<FHearthWorldRequest> NullDecoded;
    if(!TestTrue(TEXT("Null target position roundtrips"),HearthWorldRequestJson::Decode(NullRoundtrip,NullDecoded,Error))) return false;
    if(!TestEqual(TEXT("Null target request has one decoded record"),NullDecoded.Num(),1)) return false;
    TestFalse(TEXT("Missing actual target stays null after roundtrip"),NullDecoded[0].AssetContext.bHasTargetPosition);
    return true;
}
#endif
