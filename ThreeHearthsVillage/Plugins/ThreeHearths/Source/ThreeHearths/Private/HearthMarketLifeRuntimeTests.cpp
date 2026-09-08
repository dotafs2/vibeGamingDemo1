#if WITH_DEV_AUTOMATION_TESTS

#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "HearthOrganicConstruction.h"
#include "HearthWorldRequests.h"
#include "HearthTownLayout.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

namespace
{
    UWorld* MakeMarketLifeWorld()
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    }

    void AddBenchRequest(AHearthVillage& Village,int32 Index,const FString& PlotId,const FVector& Door)
    {
        FHearthWorldRequest Request;
        Request.Id=FString::Printf(TEXT("request_%d"),Village.WorldRequests.Num()+1);
        Request.Category=TEXT("asset"); Request.Status=TEXT("proposed"); Request.Summary=TEXT("木长凳");
        Request.RequesterIds.Add(Village.Residents[Index].StableId); Request.bHasAssetContext=true;
        Request.AssetContext.Purpose=TEXT("观察到一处空地；打算：木长凳");
        Request.Summary=Request.AssetContext.Purpose;
        Request.AssetContext.TargetId=PlotId;
        Request.AssetContext.TargetPositionCm=Door;
        Request.AssetContext.bHasTargetPosition=true; Request.AssetContext.ObservationId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        FHearthResidentAssetContext Context; Context.ResidentId=Village.Residents[Index].StableId; Context.Name=Village.Residents[Index].Name;
        Context.Personality=Village.Residents[Index].Personality; Context.InnerStory=Village.Residents[Index].InnerStory; Context.DesignGoal=Village.Residents[Index].DesignGoal;
        Request.AssetContext.ResidentContexts.Add(MoveTemp(Context)); Village.WorldRequests.Add(MoveTemp(Request));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMarketLifeRuntimeTest,
    "ThreeHearths.Organic.MarketLifeKit.RuntimeAndColdRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthMarketLifeRuntimeTest::RunTest(const FString&)
{
    FOrganicConstructionStock Cost;
    TestTrue(TEXT("bench recipe is explicit"),HearthOrganicConstruction::MarketKitCost(TEXT("bench_low"),Cost));
    TestEqual(TEXT("bench uses two planks"),Cost.Planks,2); TestEqual(TEXT("bench uses one beam"),Cost.Beams,1);
    TestFalse(TEXT("unsupported linen asset cannot enter the kit ledger"),HearthOrganicConstruction::MarketKitCost(TEXT("linen_canopy"),Cost));

    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))
        +TEXT(" -HearthOrganicVillage -HearthDisableApi -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    UWorld* World=MakeMarketLifeWorld();
    if(!TestNotNull(TEXT("isolated market kit world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain")); Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState();
    Village->bAutonomousLifeEnabled=true;
    if(!TestTrue(TEXT("market fixture has organic roster"),Village->IsOrganicVillage() && Village->Residents.Num()==13)) return false;

    const int32 Index=0; const auto Person=[&]() -> FHearthResident& { return Village->Residents[Index]; };
    const FString ResidentId=Person().StableId; auto* Home=Village->OrganicHomes.Find(ResidentId);
    if(!TestNotNull(TEXT("private home exists for request owner"),Home)) return false;
    const int32 ShellBefore=Home->InstalledKeys.Num(); const int32 CoinsBefore=Person().Coins;
    const FString SignatureBefore=Village->VisualSignature(Index);
    for(int32 I=0;I<Village->Residents.Num();++I) Village->Residents[I].NextLifeDecision=Village->Elapsed+80000.0;
    AddBenchRequest(*Village,Index,Village->PlotIds[Person().Plot],Village->HomeApproach(Person().Plot));
    FString RequestError; TestTrue(TEXT("asset request fixture satisfies request contract"),HearthWorldRequests::Validate(Village->WorldRequests,RequestError));

    // A dry-run with no public planks/beams leaves the resident and home
    // untouched; no speculative request reservation or visual is created.
    Village->PlankStock=0; Village->BeamStock=0; Village->Manufactured[0]=0; Village->Manufactured[1]=0;
    Village->OrganicNextAttempt.Add(Person().StableId,Village->Elapsed);
    Village->AdvanceMarketLifeKits(0.f);
    TestFalse(TEXT("shortage does not create a market kit job"),Home->bHasMarketKitPending);
    TestEqual(TEXT("shortage does not debit owner"),Person().Coins,CoinsBefore);
    TestEqual(TEXT("shortage does not debit public planks"),Village->PlankStock,0);

    // Add sourced public inventory, then allow the real route and work loop to
    // run.  The owner starts away from the work point so this exercises travel,
    // work, one atomic charge, and the independent kit ledger.
    Village->PlankStock=2; Village->BeamStock=1; Village->Manufactured[0]=2; Village->Manufactured[1]=1;
    Person().Hunger=5.f; Person().Energy=95.f; Person().SocialNeed=5.f;
    Village->OrganicNextAttempt.Add(Person().StableId,Village->Elapsed);
    Village->AdvanceMarketLifeKits(0.f);
    if(!TestTrue(TEXT("funded request creates a pending job"),Home->bHasMarketKitPending)) return false;
    TestEqual(TEXT("pending furniture does not trigger another paid visual review"),Village->VisualSignature(Index),SignatureBefore);
    TestTrue(TEXT("pending kit is a real travel task"),Person().Task==EHearthTask::OrganicTravel && !Person().Route.IsEmpty());
    FString Error; FHearthWorldImage Image;
    const FString Traveling=Village->ExportWorldState();
    if(!TestTrue(TEXT("traveling kit cold image validates"),HearthWorld::Decode(Traveling,Image,Error))) { AddError(Error); return false; }
    for(const auto& Road:HearthTownLayout::VillageRoads(true,Village->TownLayoutVersion))
    {
        FVector A=Road.A,B=Road.B,Q=Home->MarketKitPending.InstallPosition; A.Z=B.Z=Q.Z=0.f;
        TestTrue(TEXT("entire bench footprint and margin stay off every street"),
            FVector::Dist2D(Q,FMath::ClosestPointOnSegment(Q,A,B))>=Road.Width*.5f+FMath::Sqrt(90.f*90.f+55.f*55.f)+44.f);
    }
    FHearthWorldImage Orphan=Image,RejectedPending;
    Orphan.OrganicHomes.FindChecked(ResidentId).MarketKitPending.Status=TEXT("working");
    TestFalse(TEXT("cold load rejects kit work without its resident working"),HearthWorld::Decode(HearthWorld::Encode(Orphan),RejectedPending,Error));
    Village->WorldRequests.Reset(); Village->AdvanceMarketLifeKits(0.f);
    TestFalse(TEXT("revoked source request cancels its pending job"),Home->bHasMarketKitPending);
    TestTrue(TEXT("revoked request releases worker and route"),Person().Task==EHearthTask::LifeChoosing && Person().ActiveTaskId.IsEmpty() && Person().Route.IsEmpty());
    TestEqual(TEXT("revoked request spends no owner coins"),Person().Coins,CoinsBefore);
    TestEqual(TEXT("revoked request spends no public planks"),Village->PlankStock,2);
    if(!TestTrue(TEXT("traveling kit cold apply"),Village->ApplyWorldState(Traveling,Error))) { AddError(Error); return false; }
    Home=Village->OrganicHomes.Find(ResidentId);
    bool bSawTravel=false,bSawWork=false,bColdDuringWork=false;
    for(int32 Step=0;Step<160 && Home->MarketKitInstalled.Num()==0;++Step)
    {
        bSawTravel|=Person().Task==EHearthTask::OrganicTravel; bSawWork|=Person().Task==EHearthTask::OrganicWork;
        Village->AdvanceSimulation(.25f);
        if(!bColdDuringWork && Home->bHasMarketKitPending && Home->MarketKitPending.WorkProgress>.2f)
        {
            const FString Working=Village->ExportWorldState();
            if(!TestTrue(TEXT("partial work cold image validates"),HearthWorld::Decode(Working,Image,Error))) { AddError(Error); return false; }
            if(!TestTrue(TEXT("partial work cold apply"),Village->ApplyWorldState(Working,Error))) { AddError(Error); return false; }
            Home=Village->OrganicHomes.Find(ResidentId); bColdDuringWork=true;
        }
    }
    TestTrue(TEXT("owner actually walked the route"),bSawTravel);
    TestTrue(TEXT("owner accumulated real work time"),bSawWork);
    TestTrue(TEXT("actual partial work resumed from cold state"),bColdDuringWork);
    TestEqual(TEXT("one kit was installed"),Home->MarketKitInstalled.Num(),1);
    if(Home->MarketKitInstalled.Num()!=1) return false;
    TestEqual(TEXT("shell keys remain separate from life kit"),Home->InstalledKeys.Num(),ShellBefore);
    TestEqual(TEXT("installed module is the requested bench"),Home->MarketKitInstalled[0].ModuleId,FString(TEXT("bench_low")));
    const FString InstalledSignature=Village->VisualSignature(Index);
    TestTrue(TEXT("completed furniture changes visual evidence once"),InstalledSignature!=SignatureBefore);
    auto Context=MakeShared<FJsonObject>(); Village->AppendMarketLifeContext(Index,Context);
    const auto& Facts=Context->GetArrayField(TEXT("completed_added_furnishings"));
    TestEqual(TEXT("NPC facts describe exactly the completed furnishing"),Facts.Num(),1);
    if(Facts.Num()==1) TestEqual(TEXT("NPC facts name the built bench"),Facts[0]->AsObject()->GetStringField(TEXT("module")),FString(TEXT("bench_low")));
    TestEqual(TEXT("owner paid exact public unit price"),Person().Coins,CoinsBefore-3);
    TestEqual(TEXT("public stock was consumed exactly once"),Village->PlankStock,0);
    TestEqual(TEXT("public beam stock was consumed exactly once"),Village->BeamStock,0);
    TestTrue(TEXT("purchase transaction proves the charge"),Village->Transactions.ContainsByPredicate([](const FHearthTransaction& T){return T.Kind==TEXT("market_kit_purchase") && T.Quantity==3 && T.Amount==3;}));
    const FVector InstalledPosition=Home->MarketKitInstalled[0].InstallPosition;
    TestFalse(TEXT("installed position is separate from worker standing point"),InstalledPosition.Equals(Home->MarketKitInstalled[0].Anchor,1.f));

    const int32 InstalledCoins=Person().Coins; const int32 InstalledTransactions=Village->Transactions.Num();
    FString Payload=Village->ExportWorldState();
    if(!TestTrue(TEXT("completed market kit cold image validates"),HearthWorld::Decode(Payload,Image,Error))) { AddError(Error); return false; }
    if(!TestTrue(TEXT("completed market kit cold apply succeeds"),Village->ApplyWorldState(Payload,Error))) { AddError(Error); return false; }
    Home=Village->OrganicHomes.Find(ResidentId);
    TestEqual(TEXT("cold restore keeps one kit"),Home?Home->MarketKitInstalled.Num():-1,1);
    TestEqual(TEXT("cold restore does not invent another visual revision"),Village->VisualSignature(Index),InstalledSignature);
    Village->Residents[Index].NextLifeDecision=Village->Elapsed+80000.0; Village->AdvanceSimulation(10.f);
    TestEqual(TEXT("duplicate request does not charge after cold restore"),Village->Residents[Index].Coins,InstalledCoins);
    TestEqual(TEXT("duplicate request does not append a transaction"),Village->Transactions.Num(),InstalledTransactions);

    // A separately stated need for another instance is the resident's choice;
    // idempotency is per request, not a ban on owning two identical benches.
    AddBenchRequest(*Village,Index,Village->PlotIds[Person().Plot],Village->HomeApproach(Person().Plot));
    auto& Additional=Village->WorldRequests.Last();
    Additional.Summary=Additional.AssetContext.Purpose=TEXT("看见：已有一张木长凳；打算：另一侧再添第二张木长凳，接待更多邻居。");
    Village->PlankStock+=2; Village->BeamStock+=1; Village->Manufactured[0]+=2; Village->Manufactured[1]+=1;
    Person().Hunger=5; Person().Energy=95; Person().SocialNeed=5;
    Village->OrganicNextAttempt.Add(ResidentId,Village->Elapsed);
    Village->AdvanceMarketLifeKits(0.f);
    for(int32 Step=0;Step<200 && Home->MarketKitInstalled.Num()<2;++Step) Village->AdvanceSimulation(.25f);
    TestEqual(TEXT("explicit second-instance request creates a second bench"),Home->MarketKitInstalled.Num(),2);
    TestEqual(TEXT("each separately requested bench pays its own material cost"),Person().Coins,CoinsBefore-6);
    if(Home->MarketKitInstalled.Num()!=2) return false;
    TestTrue(TEXT("second bench occupies a distinct clear space"),FVector::Dist2D(Home->MarketKitInstalled[0].InstallPosition,Home->MarketKitInstalled[1].InstallPosition)>350.f);
    if(!TestTrue(TEXT("two same-module purchases conserve and survive cold decode"),HearthWorld::Decode(Village->ExportWorldState(),Image,Error))) { AddError(Error); return false; }

    // Decode is atomic at the image boundary and rejects unsupported or
    // partially forged kit records before they can reach the live village.
    FHearthWorldImage Corrupt=Image;
    auto* CorruptHome=Corrupt.OrganicHomes.Find(ResidentId);
    if(TestNotNull(TEXT("decoded image contains kit ledger"),CorruptHome)) CorruptHome->MarketKitInstalled[0].ModuleId=TEXT("linen_canopy");
    FHearthWorldImage Rejected; TestFalse(TEXT("unsupported kit module is rejected on cold decode"),HearthWorld::Decode(HearthWorld::Encode(Corrupt),Rejected,Error));
    return true;
}

#endif
