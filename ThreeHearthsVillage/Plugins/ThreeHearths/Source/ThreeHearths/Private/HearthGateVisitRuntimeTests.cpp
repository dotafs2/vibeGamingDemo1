#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthGateVisitRuntimeTest,
    "ThreeHearths.Medieval.GateVisitRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthGateVisitRuntimeTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthOrganicVillage -HearthNoWorldPersistence -HearthNoAutonomousLife")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("isolated gate visit world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState(); Village->bAutonomousLifeEnabled=false;
    if(!TestTrue(TEXT("fixture has the service roster"),Village->IsOrganicVillage() && Village->Residents.Num()==13)) return false;
    for(auto& Resident:Village->Residents) Resident.NextLifeDecision=80000.0;

    FString DutyId; float DutyBeforeVisit=0.f;
    {
    auto& Gate=Village->Residents[10]; Gate.Task=EHearthTask::LifeChoosing; Gate.ActiveTaskId.Empty(); Gate.Route.Reset(); Gate.NextLifeDecision=0; Gate.Hunger=10; Gate.Energy=90;
    TestTrue(TEXT("service resident has a shared public residence instead of a private plot"),Gate.ServiceRoleKey==TEXT("gatekeeper") && Gate.ResidenceId==TEXT("v4_public_life_rest_anchor") && Gate.Plot==-1 && Gate.BuildProgress==0.f && Gate.HouseBlueprint.IsEmpty());
    if(!TestTrue(TEXT("gatekeeper starts a real funded shift"),Village->StartServiceDuty(10))) return false;
    DutyId=Gate.ActiveTaskId;
    bool AtPost=false;
    for(int32 Step=0;Step<300 && !AtPost;++Step)
    {
        Village->AdvanceSimulation(1.f);
        AtPost=Gate.Task==EHearthTask::LifeActivity && Gate.Route.IsEmpty();
    }
    if(!TestTrue(TEXT("gatekeeper reaches the duty post before accepting visitors"),AtPost)) return false;
    DutyBeforeVisit=Village->ServiceDuties.FindByPredicate([&DutyId](const FHearthServiceDutyRecord& D){return D.TaskId==DutyId;})->DutySeconds;

    auto& Visitor=Village->Residents[0]; Visitor.BuildProgress=1.f; Visitor.Task=EHearthTask::LifeChoosing; Visitor.ActiveTaskId.Empty(); Visitor.Route.Reset(); Visitor.ConversationId.Empty(); Visitor.NextLifeDecision=0; Visitor.Hunger=10; Visitor.Energy=90;
    const FVector GatePosition=Gate.Actor->GetActorLocation(); bool Placed=false;
    for(const FVector& Offset:{FVector(700,0,0),FVector(-700,0,0),FVector(0,700,0),FVector(0,-700,0)})
    {
        FVector Candidate=GatePosition+Offset; Candidate.Z=Village->GroundHeightAt(Candidate)+5.2f; TArray<FVector> Route;
        if(!Village->IsClearPoint(Candidate)) continue;
        Visitor.Actor->SetActorLocation(Candidate);
        if(Village->FindActivityRoute(0,GatePosition,Route)) { Placed=true; break; }
    }
    if(!TestTrue(TEXT("visitor has a real route to the staffed gate post"),Placed)) return false;
    if(!TestTrue(TEXT("normal resident can discover a staffed gatekeeper"),Village->IsSociallyAvailable(10) && Village->AvailableLifeActions(0).Contains(13))) return false;
    TestFalse(TEXT("staffed gatekeeper cannot initiate a visit"),Village->BeginConversation(10,0,TEXT("值勤中不主动离岗"),false));
    if(!TestTrue(TEXT("visitor starts only after an executable route exists"),Village->BeginConversation(0,10,TEXT("想了解城门情况"),false))) return false;
    TestEqual(TEXT("gatekeeper keeps the duty task while hosting a visit"),Gate.ActiveTaskId,DutyId);
    TestEqual(TEXT("gatekeeper remains in the service activity state"),Gate.LifeAction,60);
    TestTrue(TEXT("gatekeeper is not made the visit initiator"),Gate.Task==EHearthTask::LifeActivity && Gate.Route.IsEmpty());
    }

    FString Error; const FString ActivePayload=Village->ExportWorldState(); FHearthWorldImage ActiveImage;
    if(!TestTrue(TEXT("active gate visit cold image validates"),HearthWorld::Decode(ActivePayload,ActiveImage,Error))) { AddError(Error); return false; }
    if(!TestTrue(TEXT("active gate visit cold reload succeeds"),Village->ApplyWorldState(ActivePayload,Error))) { AddError(Error); return false; }
    TestTrue(TEXT("cold reload preserves the visit and duty identity"),Village->Residents[10].ConversationId.Len()>0 && Village->Residents[10].ActiveTaskId==DutyId);

    auto* Visit=Village->Conversations.FindByPredicate([](const FHearthConversation& C){return !C.bClosed && C.First==0 && C.Second==10;});
    if(!TestNotNull(TEXT("conversation is still open after cold reload"),Visit)) return false;
    bool Met=false;
    for(int32 Step=0;Step<120 && !Met;++Step)
    {
        Village->AdvanceSimulation(1.f);
        Village->AdvanceSocial(1.f);
        Met=Visit->bMet;
    }
    if(!TestTrue(TEXT("visitor and gatekeeper meet only after the routed approach"),Met)) return false;
    for(int32 Step=0;Step<20;++Step) Village->AdvanceSimulation(1.f);
    const auto* FrozenDuty=Village->ServiceDuties.FindByPredicate([&DutyId](const FHearthServiceDutyRecord& D){return D.TaskId==DutyId;});
    if(!TestNotNull(TEXT("duty record survives the visit pause"),FrozenDuty)) return false;
    TestEqual(TEXT("duty progress pauses while the visitor is talking"),FrozenDuty->DutySeconds,DutyBeforeVisit);

    // The visitor speaks first; the service target uses the normal social
    // decision path with the duty conversation's chat/goodbye whitelist, so
    // no accepted choice can pull the guard away from the post.
    if(!TestTrue(TEXT("visitor can speak to the staffed gatekeeper"),Village->ResolveSocialTurn(0,0,TEXT("您好，我想确认城门开放情况。"),TEXT("local_test")))) return false;
    const TArray<int32> GuardChoices=Village->AvailableSocialIntents(10);
    TestTrue(TEXT("service conversation exposes only chat and goodbye"),GuardChoices.Num()==2 && GuardChoices.Contains(0) && GuardChoices.Contains(5) && !GuardChoices.Contains(1) && !GuardChoices.Contains(2) && !GuardChoices.Contains(3) && !GuardChoices.Contains(6) && !GuardChoices.Contains(7) && !GuardChoices.Contains(8));
    Village->AdvanceSocial(5.f);
    if(!TestTrue(TEXT("visitor can end a completed gate conversation"),Village->ResolveSocialTurn(0,5,TEXT("谢谢，我先走了。"),TEXT("local_test")))) return false;
    TestTrue(TEXT("closed visit clears only the conversation link"),Village->Residents[10].ConversationId.IsEmpty() && Village->Residents[10].ActiveTaskId==DutyId && Village->Residents[10].Task==EHearthTask::LifeActivity);
    TestTrue(TEXT("visitor releases the finished social task reservation"),Village->Residents[0].ActiveTaskId.IsEmpty());
    const float DutyAfterVisit=Village->ServiceDuties.FindByPredicate([&DutyId](const FHearthServiceDutyRecord& D){return D.TaskId==DutyId;})->DutySeconds;
    Village->AdvanceSimulation(1.f);
    TestTrue(TEXT("same duty resumes after the visitor leaves"),Village->ServiceDuties.FindByPredicate([&DutyId](const FHearthServiceDutyRecord& D){return D.TaskId==DutyId;})->DutySeconds>DutyAfterVisit);
    TestEqual(TEXT("visiting does not create a second wage transaction"),Village->Transactions.FilterByPredicate([&DutyId](const FHearthTransaction& T){return T.Kind==TEXT("wage") && T.TaskId==DutyId;}).Num(),0);
    // No simulation movement: a later approach times out without fabricating
    // a meeting, and the exact same guard shift remains available afterwards.
    if(!TestTrue(TEXT("same post can accept a later visitor"),Village->BeginConversation(0,10,TEXT("再次来访"),false))) return false;
    const int32 Meetings=Village->Residents[10].Bonds.FindOrAdd(Village->Residents[0].StableId).Meetings;
    Village->AdvanceSocial(91.f);
    TestTrue(TEXT("failed approach releases the conversation and preserves duty"),Village->Residents[10].ConversationId.IsEmpty() && Village->Residents[10].ActiveTaskId==DutyId);
    TestEqual(TEXT("failed approach cannot invent a meeting"),Village->Residents[10].Bonds.FindOrAdd(Village->Residents[0].StableId).Meetings,Meetings);
    for(int32 Step=0;Step<650;++Step)
    {
        const auto* Current=Village->ServiceDuties.FindByPredicate([&DutyId](const FHearthServiceDutyRecord& D){return D.TaskId==DutyId;});
        if(Current->Status!=TEXT("active")) break;
        Village->AdvanceSimulation(1.f);
    }
    TestEqual(TEXT("original shift is paid exactly once after both visits"),Village->Transactions.FilterByPredicate([&DutyId](const FHearthTransaction& T){return T.Kind==TEXT("wage") && T.TaskId==DutyId;}).Num(),1);
    FHearthWorldImage FinalImage;
    TestTrue(TEXT("completed visitor and duty ledger still validates"),HearthWorld::Decode(Village->ExportWorldState(),FinalImage,Error));
    return true;
}
#endif
