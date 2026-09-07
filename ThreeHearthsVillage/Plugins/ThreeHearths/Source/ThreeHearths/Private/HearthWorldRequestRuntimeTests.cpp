#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "HearthWorldRequestJson.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthWorldRequestRuntimeTest,"ThreeHearths.WorldHost.RuntimePersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthWorldRequestRuntimeTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    const auto OriginalTask=V->Residents[0].Task;
    const int32 Coins=V->Residents[0].Coins,Food=V->FoodStock,ApiCalls=V->ApiRequests;
    const FString Need=TEXT("需要一个纯视觉的铁匠铺招牌模型");
    TestTrue(TEXT("A resident can submit a bounded need"),V->SubmitResidentWorldRequest(0,Need));
    TestTrue(TEXT("Second household can join the same request"),V->SubmitResidentWorldRequest(1,Need));
    TestEqual(TEXT("Runtime uses one shared board item"),V->WorldRequests.Num(),1);
    if(V->WorldRequests.IsEmpty()) return false;
    TestEqual(TEXT("Both true resident IDs are recorded"),V->WorldRequests[0].RequesterIds.Num(),2);
    TestTrue(TEXT("Resident identity case is preserved"),V->WorldRequests[0].RequesterIds.Contains(V->Residents[0].StableId));
    TestEqual(TEXT("Pure art request reaches art lane"),V->WorldRequests[0].Category,FString(TEXT("asset")));
    TestEqual(TEXT("Submission does not spend money"),V->Residents[0].Coins,Coins);
    TestEqual(TEXT("Submission does not make inventory"),V->FoodStock,Food);
    TestEqual(TEXT("Submission makes no model/API call"),V->ApiRequests,ApiCalls);
    TestTrue(TEXT("Current work survives host triage"),V->Residents[0].Task==OriginalTask);
    const FString RequestId=V->WorldRequests[0].Id;
    FHearthWorldImage Restored; FString Error;
    TestTrue(TEXT("Real world envelope persists host board"),HearthWorld::Decode(V->ExportWorldState(),Restored,Error));
    if(!Error.IsEmpty()) AddInfo(Error);
    if(Restored.WorldRequests.Num()) TestEqual(TEXT("Request identity survives serialization"),Restored.WorldRequests[0].Id,RequestId);
    V->ReconcileWorldRequests();
    TestEqual(TEXT("Reload reconciliation cannot duplicate an existing request"),V->WorldRequests.Num(),1);
    TestFalse(TEXT("Invalid resident is rejected"),V->SubmitResidentWorldRequest(-1,Need));
    TSharedPtr<FJsonObject> Board;
    TestTrue(TEXT("Art bridge consumes a valid JSON board"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V->ExportWorldRequests()),Board));
    if(Board.IsValid())
    {
        TestEqual(TEXT("Board is tied to this real world"),Board->GetStringField(TEXT("world_id")),V->WorldId);
        TestEqual(TEXT("Board schema is explicit"),Board->GetIntegerField(TEXT("schema_version")),1);
    }
    // Optional field migration preserves an old saved personal request.
    TSharedPtr<FJsonObject> Legacy;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V->ExportWorldState()),Legacy);
    Legacy->RemoveField(TEXT("world_requests"));
    FString LegacyText; FJsonSerializer::Serialize(Legacy.ToSharedRef(),TJsonWriterFactory<>::Create(&LegacyText));
    TestTrue(TEXT("Existing schema-10 worlds without a board still load"),HearthWorld::Decode(LegacyText,Restored,Error));
    TestEqual(TEXT("No fake new records during decoding"),Restored.WorldRequests.Num(),0);
    V->WorldRequests.Reset(); V->ReconcileWorldRequests();
    TestEqual(TEXT("Existing personal requests migrate into one shared board item"),V->WorldRequests.Num(),1);
    auto Invalid=V->WorldRequests; Invalid[0].RequesterIds={TEXT("missing-person")};
    Legacy->SetArrayField(TEXT("world_requests"),HearthWorldRequestJson::Encode(Invalid));
    FString InvalidText; FJsonSerializer::Serialize(Legacy.ToSharedRef(),TJsonWriterFactory<>::Create(&InvalidText));
    TestFalse(TEXT("Dangling resident references reject the entire save"),HearthWorld::Decode(InvalidText,Restored,Error));
    const FString Before=V->ExportWorldState();
    TestFalse(TEXT("Failed import cannot partially replace active state"),V->ApplyWorldState(InvalidText,Error));
    TestEqual(TEXT("State is unchanged on rejection"),V->ExportWorldState(),Before);
    return true;
}
#endif
