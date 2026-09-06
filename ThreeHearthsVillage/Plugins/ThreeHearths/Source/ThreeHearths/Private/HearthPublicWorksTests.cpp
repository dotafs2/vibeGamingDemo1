#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPublicWallTest, "ThreeHearths.Economy.PublicConstructionRecipe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPublicWallTest::RunTest(const FString&)
{
    FHearthPublicProject Project;
    Project.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    HearthPublicWorks::Populate(Project);
    TestEqual(TEXT("Canonical wall has fifteen stable parts"), Project.Parts.Num(), 15);
    int32 Totals[3] = {0, 0, 0};
    for (int32 I = 0; I < Project.Parts.Num(); ++I)
    {
        const auto& Part = Project.Parts[I];
        TestTrue(TEXT("Part has stable project identity"), Part.Id.StartsWith(Project.Id + TEXT(":")));
        TestTrue(TEXT("Part has a native public wall asset"), Part.Asset.StartsWith(TEXT("public_wall_")));
        for (int32 M = 0; M < 3; ++M) Totals[M] += Part.Required[M];
    }
    TestEqual(TEXT("Stone recipe is thirty six"), Totals[0], 36);
    TestEqual(TEXT("Plank recipe is twenty one"), Totals[1], 21);
    TestEqual(TEXT("Beam recipe is twelve"), Totals[2], 12);
    TestEqual(TEXT("Foundations are stage one"), Project.Parts[0].Stage, 1);
    TestEqual(TEXT("Stone courses are stage two"), Project.Parts[3].Stage, 2);
    TestEqual(TEXT("Walkways are stage three"), Project.Parts[6].Stage, 3);
    TestEqual(TEXT("Parapets are stage four"), Project.Parts.Last().Stage, 4);
    const FString FirstId = Project.Parts[0].Id;
    HearthPublicWorks::Populate(Project);
    TestEqual(TEXT("Populate keeps canonical stable part order"), Project.Parts[0].Id, FirstId);

    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Public construction test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AHearthVillage* Village = World->SpawnActor<AHearthVillage>();
    Village->BuildEnvironment(); Village->ResetVillageState(); Village->bApiDisabledThisRun = true;
    if (!TestTrue(TEXT("Test resident exists"), Village->Residents.Num() > 0)) return false;
    Village->PublicProject.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Village->PublicProject.Status = TEXT("building"); Village->PublicProject.Stock[0] = 1; Village->PublicProject.Site = 0;
    FHearthPublicPart Work; Work.Id = Village->PublicProject.Id + TEXT(":test"); Work.Required[0] = 1; Village->PublicProject.Parts = { Work };
    Village->ProductionSites.Reset(); Village->LandGrid.Reset();
    for (int32 X = -30; X <= 30; ++X) for (int32 Y = -30; Y <= 30; ++Y) Village->LandGrid.Add(FIntPoint(X, Y));
    FHearthSite Site; Site.Position = FVector(400.f, 0.f, 8.f); Site.Approach = FVector(0.f, 0.f, 8.f); Site.bReachable = true;
    Village->ProductionSites.Add(Site); Village->Residents[0].Actor->SetActorLocation(FVector(-400.f, 0.f, 8.f));
    Village->TaxProjectCoins = 2;
    Village->Residents[0].Task = EHearthTask::LifeChoosing; Village->Residents[0].Route.Reset();
    TArray<FVector> DepotRoute;
    TestTrue(TEXT("Public depot is reachable in the test village"), Village->FindActivityRoute(0, FVector(-250.f, -400.f, 8.f), DepotRoute));
    TestTrue(TEXT("Public worker can borrow the construction hammer"), Village->ToolAvailableFor(0, 5));
    TestTrue(TEXT("Public worker can be assigned before transit"), Village->CanAssignActivity(0));
    TestTrue(TEXT("A public part enters explicit depot phase"), Village->StartPublicPart(0));
    TestEqual(TEXT("Initial public haul phase is depot bound"), Village->Residents[0].LifeAction, 1);
    TestTrue(TEXT("Cancelling transit restores reserved public stock"), Village->CancelPublicWork(0));
    TestEqual(TEXT("Cancelled transit restores one stone"), Village->PublicProject.Stock[0], 1);
    TestEqual(TEXT("Cancelled transit clears phase"), Village->Residents[0].LifeAction, -1);
    TestTrue(TEXT("A second transit can be started after cancellation"), Village->StartPublicPart(0));
    Village->Residents[0].LifeAction = 2; Village->Residents[0].CargoAmount = 0; Village->Residents[0].CargoType = -1; Village->Residents[0].Route.Reset();
    Village->AdvancePublicWorker(0, 0.f);
    TestEqual(TEXT("Malformed return phase cancels instead of taking phantom cargo"), Village->Residents[0].Task, EHearthTask::LifeChoosing);
    TestEqual(TEXT("Malformed return phase refunds stock"), Village->PublicProject.Stock[0], 1);
    return true;
}
#endif
