#if WITH_DEV_AUTOMATION_TESTS
#include "HearthResidentBuildingPlanner.h"
#include "HearthPlannedConstructionAdapter.h"
#include "HearthTownLayout.h"
#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthCourtyardGrowthTest,"ThreeHearths.Design.CourtyardGrowth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthCourtyardGrowthTest::RunTest(const FString&)
{
    FHearthResidentBuildingInput Input; Input.ResidentId=TEXT("courtyard-owner"); Input.StableSeed=TEXT("courtyard-seed");
    Input.Budget=300; Input.Stone=100; Input.Planks=200; Input.Beams=200; Input.GrowthDirection=0;
    auto House=HearthResidentBuildingPlanner::Build(Input);
    TestTrue(TEXT("Core is structurally buildable"),House.bBuildable);
    House.Expansion.ResultingPlan=House.Plan;
    const auto Original=House.Plan;
    for(int32 Direction=1;Direction<=3;++Direction)
    {
        const auto Before=House.Expansion.ResultingPlan;
        Input.GrowthDirection=Direction; Input.ExtensionKey=FString::Printf(TEXT("wing-%d"),Direction);
        TestTrue(TEXT("A finite catalog wing can grow on either axis"),HearthResidentBuildingPlanner::AppendExpansion(House,Input));
        const auto& After=House.Expansion.ResultingPlan;
        TestEqual(TEXT("One new separately entered room"),After.Rooms.Num(),Before.Rooms.Num()+1);
        for(const auto& C:Before.Components)
        {
            const auto* Kept=After.Components.FindByPredicate([&](const auto& V){return V.Id==C.Id;});
            TestTrue(TEXT("No old component moves or changes material"),Kept && Kept->Offset==C.Offset && Kept->Orientation==C.Orientation && Kept->CatalogId==C.CatalogId);
        }
        TestTrue(TEXT("All roof and wall contacts validate"),HearthStructurePlan::Validate(After,HearthResidentBuildingPlanner::ValidationContext(Input)).bValid);
        TestTrue(TEXT("Existing paid components remain executable"),HearthPlannedConstructionAdapter::Convert(After,0,HearthPlannedConstructionAdapter::Convert(Before,0,{}).Components).bAccepted);
    }
    const auto& Result=House.Expansion.ResultingPlan;
    TestTrue(TEXT("Footprint grows in two dimensions"),Result.Components.ContainsByPredicate([](const auto& C){return C.Offset.X<0;}) && Result.Components.ContainsByPredicate([](const auto& C){return C.Offset.Y>200;}));
    Input.ExtensionKey=TEXT("unfunded-wing"); Input.Beams=0;
    const int32 Count=Result.Components.Num();
    TestFalse(TEXT("A desired shape cannot invent beams"),HearthResidentBuildingPlanner::AppendExpansion(House,Input));
    TestEqual(TEXT("Unfunded revision is atomic"),House.Expansion.ResultingPlan.Components.Num(),Count);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthDesignFeedbackTest,"ThreeHearths.Design.FeedbackAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthDesignFeedbackTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState();
    auto& R=V->Residents[0]; R.BuildProgress=1; R.DesignGoal=TEXT("安静院落与更大的工作空间"); R.InnerStory=TEXT("我会记住这所房子的安静角落。");
    R.Task=EHearthTask::ProductionTravel;
    FHearthPendingDecision Reply; Reply.bVisual=true; Reply.Choice=3; Reply.AllowedActions={0,1,2,3,4}; Reply.Reason=TEXT("屋内拥挤，我要向后扩建。"); Reply.VisualSignature=V->VisualSignature(0);
    V->ApplyVisualReview(0,Reply);
    TestEqual(TEXT("Unsupported visual inference cannot change a building intention"),R.GrowthDirection,0);
    TestTrue(TEXT("Rejected image inference is explained"),!Reply.Error.IsEmpty());
    Reply.Error.Empty(); Reply.Reason=TEXT("看见：前院有空地；未知：屋内如何分隔；打算：为工作向后扩建。");
    V->ApplyVisualReview(0,Reply);
    TestEqual(TEXT("Visual response selects actual planner direction"),R.GrowthDirection,3);
    TestTrue(TEXT("Visual response cannot cancel worker task"),R.Task==EHearthTask::ProductionTravel);
    Reply.Choice=0; Reply.VisualSignature=TEXT("old-observation"); V->ApplyVisualReview(0,Reply);
    TestFalse(TEXT("Stale satisfaction cannot stop new construction"),R.bDesignSatisfied);
    Reply.Error.Empty(); Reply.VisualSignature=V->VisualSignature(0); Reply.Choice=4; Reply.Reason=TEXT("看见：木墙和屋顶；未知：是否能接外地生意；打算：申请尚未实现的皇城订单规则。"); V->ApplyVisualReview(0,Reply);
    TestEqual(TEXT("Unsupported aspirations remain a pending request"),R.DesignRequest,Reply.Reason);
    TestEqual(TEXT("Visual option four enters the real host board"),V->WorldRequests.Num(),1);
    if(V->WorldRequests.Num()) TestEqual(TEXT("Request for order rules is proposed gameplay, not created facts"),V->WorldRequests[0].Category,FString(TEXT("mechanic")));
    R.BuildingArchetype=TEXT("shop_house");
    Reply=FHearthPendingDecision(); Reply.bVisual=true; Reply.Choice=5; Reply.AllowedActions={5};
    Reply.Reason=TEXT("看见：门前有空地；未知：是否能遮雨；打算：申请一处小型遮雨工作空间。"); Reply.VisualSignature=V->VisualSignature(0);
    V->ApplyVisualReview(0,Reply);
    const auto* AssetRequest=V->WorldRequests.FindByPredicate([](const FHearthWorldRequest& Request){return Request.bHasAssetContext;});
    TestTrue(TEXT("Action five accepts a physical request for a non-inn resident"),AssetRequest!=nullptr);
    if(AssetRequest)
    {
        TestEqual(TEXT("Generic action five remains an asset proposal"),AssetRequest->Category,FString(TEXT("asset")));
        TestEqual(TEXT("Asset context snapshots persistent story"),AssetRequest->AssetContext.ResidentContexts[0].InnerStory,R.InnerStory);
        TestTrue(TEXT("Asset context is bound to the requesting resident"),AssetRequest->AssetContext.ResidentContexts[0].ResidentId==R.StableId);
        TestTrue(TEXT("No screenshot id is invented without a saved capture"),AssetRequest->AssetContext.ObservationId.IsEmpty());
    }
    // Reuse valid world fixtures; serialize optional intention fields with schema 10.
    R.BuildProgress=0; R.Task=EHearthTask::Choosing;
    FHearthWorldImage Saved; FString Error;
    TestTrue(TEXT("Design state survives world encoding"),HearthWorld::Decode(V->ExportWorldState(),Saved,Error));
    AddInfo(Error);
    TestEqual(TEXT("Visual host requests survive world persistence"),Saved.WorldRequests.Num(),2);
    TestTrue(TEXT("Typed physical request survives world persistence"),Saved.WorldRequests.ContainsByPredicate([](const FHearthWorldRequest& Request){return Request.bHasAssetContext;}));
    if(Saved.People.Num()) { TestEqual(TEXT("Persistent desire"),Saved.People[0].Person.DesignGoal,R.DesignGoal); TestEqual(TEXT("Persistent feedback"),Saved.People[0].Person.GrowthDirection,3); }
    R.Task=EHearthTask::LifeChoosing; R.Route.Reset(); R.NextLifeDecision=0; R.Hunger=90; R.Energy=80; R.Coins=12;
    V->bAutonomousLifeEnabled=true; V->bSimulationPaused=false; V->Elapsed=10;
    V->PendingDecisions[0]=FHearthPendingDecision(); V->PendingDecisions[0].bActive=true; V->PendingDecisions[0].bVisual=true;
    const int32 BeforeRequests=V->ApiRequests;
    V->UpdateLifeDecisions();
    TestTrue(TEXT("Hungry resident continues local life while the image request is pending"),R.Task!=EHearthTask::LifeChoosing);
    TestEqual(TEXT("No second request for the same resident"),V->ApiRequests,BeforeRequests);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicRoadsTest,"ThreeHearths.Design.OrganicStreets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthOrganicRoadsTest::RunTest(const FString&)
{
    FHearthTownLayoutInput I; I.Roads=HearthTownLayout::VillageRoads(true); I.bOrganic=true; I.RequestedHomes=18; I.Seed=583;
    const auto P=HearthTownLayout::Build(I), Again=HearthTownLayout::Build(I);
    TestTrue(TEXT("Organic streets keep bounded clear plots and road access"),HearthTownLayout::IsValid(P,I));
    TestTrue(TEXT("Enough choices for ten residents"),P.Homes.Num()>=10);
    TSet<int32> Yaws;
    for(int32 N=0;N<P.Homes.Num();++N){Yaws.Add(FMath::RoundToInt(P.Homes[N].Yaw));TestTrue(TEXT("Same world seed restores placement"),P.Homes[N].Center==Again.Homes[N].Center);}
    TestTrue(TEXT("Streets are not one orthogonal row"),Yaws.Num()>5);
    return true;
}
#endif
