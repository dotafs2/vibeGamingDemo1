#if WITH_DEV_AUTOMATION_TESTS
#include "HearthSiteCandidates.h"
#include "HearthWorldState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicReplacementCandidateTest, "ThreeHearths.Production.OrganicReplacementCandidates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthOrganicReplacementCandidateTest::RunTest(const FString&)
{
    FHearthReplacementCandidateInput Input;
    Input.Roads = HearthTownLayout::VillageRoads(true);
    Input.ResidentAnchor = FVector(-1500.f, 1900.f, 8.f);
    Input.PublicSite = FVector(-2130.f, -1100.f, 8.f);
    Input.Seed = 583;
    Input.MaxCandidates = 24;
    const TArray<FHearthReplacementCandidate> First = HearthSiteCandidates::Generate(Input);
    const TArray<FHearthReplacementCandidate> Repeat = HearthSiteCandidates::Generate(Input);
    TestTrue(TEXT("Organic roads produce bounded replacement candidates"), First.Num() > 0 && First.Num() <= Input.MaxCandidates);
    if (First.IsEmpty()) return false;
    TestEqual(TEXT("Same resident and site state replays the same candidate count"), First.Num(), Repeat.Num());
    for (int32 I = 0; I < First.Num() && I < Repeat.Num(); ++I)
    {
        TestTrue(TEXT("Candidate position is deterministic"), First[I].Position.Equals(Repeat[I].Position, .01f));
        TestTrue(TEXT("Candidate approach is deterministic"), First[I].Approach.Equals(Repeat[I].Approach, .01f));
        TestEqual(TEXT("Candidate key is deterministic"), First[I].StableKey, Repeat[I].StableKey);
        for (int32 J = I + 1; J < First.Num(); ++J)
            TestTrue(TEXT("Candidates do not duplicate a plot"), FVector::Dist2D(First[I].Position, First[J].Position) >= Input.PlotRadius * 1.8f);
    }

    TSet<int32> RoadDirections;
    for (const FHearthReplacementCandidate& Candidate : First)
    {
        const FHearthTownRoadSegment& Road = Input.Roads[Candidate.RoadIndex];
        TestTrue(TEXT("Candidate keeps its plot outside its frontage road"), FVector::Dist2D(Candidate.Position, FMath::ClosestPointOnSegment(Candidate.Position, Road.A, Road.B)) >= Input.PlotRadius + Road.Width * .5f + Input.RoadClearance);
        RoadDirections.Add(Candidate.RoadIndex);
    }
    TestTrue(TEXT("Candidates use multiple organic road directions"), RoadDirections.Num() >= 2);

    FHearthReplacementCandidateInput Blocked = Input;
    Blocked.Occupied.Add({First[0].Position, Input.PlotRadius});
    const TArray<FHearthReplacementCandidate> WithoutOccupied = HearthSiteCandidates::Generate(Blocked);
    TestTrue(TEXT("Occupied resident sites are filtered before runtime insertion"), !WithoutOccupied.ContainsByPredicate([&](const FHearthReplacementCandidate& Candidate)
        { return FVector::Dist2D(Candidate.Position, First[0].Position) < Input.PlotRadius * 1.8f; }));

    FHearthReplacementCandidateInput Curved = Input;
    Curved.Roads = { { FVector(-1000.f, -1000.f, 8.f), FVector(-200.f, -300.f, 8.f), 220.f }, { FVector(-200.f, -300.f, 8.f), FVector(700.f, -850.f, 8.f), 200.f } };
    const TArray<FHearthReplacementCandidate> CurvedCandidates = HearthSiteCandidates::Generate(Curved);
    TestTrue(TEXT("Bent road segments are usable as candidate frontage"), CurvedCandidates.Num() > 0);

    FHearthReplacementCandidateInput TooManyRoads=Input;
    TooManyRoads.Roads.SetNum(33);
    TestTrue(TEXT("Over-limit road input fails closed"), HearthSiteCandidates::Generate(TooManyRoads).IsEmpty());
    FHearthReplacementCandidateInput TooManyBlockers=Input;
    TooManyBlockers.Occupied.SetNum(257);
    TestTrue(TEXT("Over-limit blocker input fails closed"), HearthSiteCandidates::Generate(TooManyBlockers).IsEmpty());

    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Replacement fixture world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140.f,140.f,1.f));
    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState();
    FString BaseError;
    FHearthWorldImage Base;
    const bool bBaseDecoded=HearthWorld::Decode(Village->ExportWorldState(),Base,BaseError);
    TestTrue(TEXT("Initialized village exports a valid save baseline"),bBaseDecoded);
    if(!bBaseDecoded || Village->Residents.IsEmpty() || Village->ProductionSites.IsEmpty()) return false;

    Village->ProductionSites.SetNum(1);
    FHearthSite& PublicSite=Village->ProductionSites[0];
    PublicSite=FHearthSite(); PublicSite.Kind=EHearthSiteKind::Empty; PublicSite.Position=FVector(-2130.f,-5100.f,8.f);
    PublicSite.Radius=350.f; PublicSite.bExpansion=true; PublicSite.bReachable=true; PublicSite.Approach=FVector(-2130.f,-4700.f,8.f);
    Village->Residents[0].Role=TEXT("木匠"); Village->Residents[0].BuildProgress=1.f; Village->Residents[0].Coins=100;
    Village->StructurePlans.Reset();
    Village->PublicProject=FHearthPublicProject(); Village->PublicProject.Id=FGuid(0x10203040,0x50607080,0x90abcdef,0x12345678).ToString(EGuidFormats::DigitsWithHyphens);
    Village->PublicProject.Status=TEXT("completed"); Village->PublicProject.Site=0;
    Village->bReplacementPlotSearchDone=false;
    const int32 BeforeSites=Village->ProductionSites.Num();
    Village->AdvanceProductionWorld(0.f);
    TestTrue(TEXT("Completed public works trigger a real replacement-site insertion"),Village->ProductionSites.Num()==BeforeSites+1);
    if(Village->ProductionSites.Num()!=BeforeSites+1) return false;
    const FHearthSite& Replacement=Village->ProductionSites.Last();
    FGuid ReplacementGuid;
    TestTrue(TEXT("Runtime replacement site uses a save-valid GUID"),FGuid::Parse(Replacement.StableId,ReplacementGuid) && ReplacementGuid.IsValid());

    Base.Sites.Add(Replacement);
    FString RoundTripError;
    FHearthWorldImage Loaded;
    const bool bRoundTrip=HearthWorld::Decode(HearthWorld::Encode(Base),Loaded,RoundTripError);
    TestTrue(TEXT("Runtime replacement site round trips through the real world serializer"),bRoundTrip);
    TestTrue(TEXT("Runtime replacement site identity remains stable after load"),bRoundTrip && Loaded.Sites.ContainsByPredicate([&](const FHearthSite& Site){return Site.StableId==Replacement.StableId;}));
    return true;
}
#endif
