#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "HearthRoyalWorksPlan.h"
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
    Village->Residents[0].Route.Reset();
    Village->AdvancePublicWorker(0, 0.f);
    TestEqual(TEXT("An old empty-route transit remains active while its route is rebuilt"), Village->Residents[0].Task, EHearthTask::PublicTravel);
    TestTrue(TEXT("An old empty-route transit receives a real depot route"), !Village->Residents[0].Route.IsEmpty());
    TestTrue(TEXT("Cancelling transit restores reserved public stock"), Village->CancelPublicWork(0));
    TestEqual(TEXT("Cancelled transit restores one stone"), Village->PublicProject.Stock[0], 1);
    TestEqual(TEXT("Cancelled transit clears phase"), Village->Residents[0].LifeAction, -1);
    TestTrue(TEXT("A second transit can be started after cancellation"), Village->StartPublicPart(0));
    Village->Residents[0].LifeAction = 2; Village->Residents[0].CargoAmount = 0; Village->Residents[0].CargoType = -1; Village->Residents[0].Route.Reset();
    Village->AdvancePublicWorker(0, 0.f);
    TestEqual(TEXT("Malformed return phase cancels instead of taking phantom cargo"), Village->Residents[0].Task, EHearthTask::LifeChoosing);
    TestEqual(TEXT("Malformed return phase refunds stock"), Village->PublicProject.Stock[0], 1);

    // Completing the final part must unlock every unspent project-tax coin for
    // ordinary village production. The release is a reclassification, so it
    // cannot change the treasury balance.
    auto& FinalPart = Village->PublicProject.Parts[0];
    const FString FinalTask = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Village->PublicProject.Status = TEXT("building"); Village->PublicProject.Completed = 0;
    Village->TaxProjectCoins = 4; Village->TaxReleasedCoins = 0;
    TestTrue(TEXT("Final public wage reserves from protected tax"), Village->ReserveWage(0, FinalTask, 2, true));
    FinalPart.Status = TEXT("installing"); FinalPart.Worker = 0; FinalPart.TaskId = FinalTask;
    FinalPart.Reserved[0] = 0; FinalPart.Delivered[0] = 1;
    Village->Residents[0].Task = EHearthTask::PublicWork; Village->Residents[0].ActiveTaskId = FinalTask; Village->Residents[0].Timer = 0.f;
    const int32 TreasuryBeforeCompletion = Village->TreasuryCoins;
    Village->AdvancePublicWorker(0, 0.f);
    TestEqual(TEXT("Final part completes the public project"), Village->PublicProject.Status, FString(TEXT("completed")));
    TestEqual(TEXT("No completed-project tax remains protected"), Village->TaxProjectCoins, 0);
    TestEqual(TEXT("Unspent project tax becomes released tax"), Village->TaxReleasedCoins, 2);
    TestEqual(TEXT("Tax release itself does not mint or destroy treasury cash"), Village->TreasuryCoins, TreasuryBeforeCompletion);
    TestEqual(TEXT("Released cash is available to general production"), Village->GeneralFunds(), Village->TreasuryCoins);
    FHearthSite ReplacementLand; ReplacementLand.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    ReplacementLand.Kind=EHearthSiteKind::Land; ReplacementLand.bExpansion=true; ReplacementLand.bReachable=true; ReplacementLand.Owner=1;
    ReplacementLand.Position=FVector(900,900,8); ReplacementLand.Approach=FVector(700,900,8);
    const int32 ReplacementIndex=Village->ProductionSites.Add(ReplacementLand);
    const int32 PlantTreeAction=100+ReplacementIndex*16+6;
    Village->Residents[1].BuildProgress=1.f;
    TestFalse(TEXT("A houseless claimant's replacement land is not consumed by another use"),
        Village->IsProductionAllowed(1,PlantTreeAction));
    FHearthStructurePlan ExistingHome; ExistingHome.PlanId=TEXT("resident_")+Village->Residents[1].StableId+TEXT("_house");
    Village->StructurePlans.Add(ExistingHome);
    TestTrue(TEXT("The same expansion land can serve production after its claimant owns a home"),
        Village->IsProductionAllowed(1,PlantTreeAction));
    Village->PublicProject.Status=TEXT("building");Village->PublicProject.Completed=0;
    Village->PublicProject.Stock[0]=3;Village->TaxProjectCoins=6;
    FHearthPublicPart A,B,Upper;
    A.Id=TEXT("parallel-a");B.Id=TEXT("parallel-b");Upper.Id=TEXT("upper");
    A.Required[0]=B.Required[0]=Upper.Required[0]=1;A.Stage=B.Stage=1;Upper.Stage=2;
    Village->PublicProject.Parts={A,B,Upper};
    for(int32 I=0;I<3;++I){auto& R=Village->Residents[I];R.Task=EHearthTask::LifeChoosing;R.Route.Reset();Village->ReturnTool(I);}
    TestTrue(TEXT("First worker starts hauling a foundation"),Village->StartPublicPart(0));
    TestTrue(TEXT("Second worker independently hauls another foundation"),Village->StartPublicPart(1));
    TestTrue(TEXT("Hauling does not monopolize the one physical hammer"),Village->ToolAvailableFor(2,5));
    TestFalse(TEXT("Upper structure waits for every supporting foundation"),Village->StartPublicPart(2));
    Village->CancelPublicWork(0);Village->CancelPublicWork(1);
    TestEqual(TEXT("Parallel cancellation refunds every reserved stone"),Village->PublicProject.Stock[0],3);
    Village->bAutonomousLifeEnabled=true;Village->PublicScheduleTimer=.8f;
    for(auto& R:Village->Residents){R.Task=EHearthTask::LifeActivity;R.LifeAction=0;R.Timer=10.f;R.Hunger=10.f;R.Energy=80.f;}
    Village->Residents[0].Timer=0.f;
    Village->AdvanceSimulation(.05f);
    TestEqual(TEXT("A resident finishing rest receives funded public work before the next life decision"),Village->Residents[0].Task,EHearthTask::PublicTravel);
    Village->CancelPublicWork(0);
    for(auto& R:Village->Residents) R.Task=EHearthTask::LifeActivity;
    Village->PublicProject.Stock[0]=0;Village->StoneStock=0;
    Village->TaxProjectCoins=100;Village->TaxReleasedCoins=0;Village->TreasuryCoins=100;Village->PublicScheduleTimer=0;
    Village->AdvancePublicWorks(.05f);
    TestEqual(TEXT("Unfinished three-part project protects its six remaining wage coins"),Village->TaxProjectCoins,6);
    TestEqual(TEXT("Excess tax becomes working capital for material producers"),Village->GeneralFunds(),94);
    TestEqual(TEXT("Releasing excess protection cannot mint cash"),Village->TreasuryCoins,100);

    // Town3 protects only the current stage's next three parts. A later
    // stage must not drain the real stone/beam depots before it is executable.
    Village->PublicProject = FHearthPublicProject();
    Village->PublicProject.TemplateId = TEXT("royal_keep_garden_v2");
    Village->PublicProject.Status = TEXT("building");
    Village->PublicProject.Site = 0;
    FHearthPublicPart BatchA, BatchB, BatchC, Later;
    BatchA.Id = TEXT("v2-a"); BatchB.Id = TEXT("v2-b"); BatchC.Id = TEXT("v2-c"); Later.Id = TEXT("v2-later");
    BatchA.Stage = BatchB.Stage = BatchC.Stage = 1; Later.Stage = 2;
    BatchA.Required[0] = BatchB.Required[0] = BatchC.Required[0] = 10; Later.Required[0] = 80;
    Village->PublicProject.Parts = {BatchA, BatchB, BatchC, Later};
    Village->StoneStock = 100; Village->BeamStock = 100; Village->TaxProjectCoins = 100; Village->TaxReleasedCoins = 0;
    Village->PublicScheduleTimer = 0.f;
    for (auto& R : Village->Residents) { R.Task = EHearthTask::LifeActivity; R.Timer = 10.f; R.Hunger = 10.f; R.Energy = 80.f; }
    Village->AdvancePublicWorks(.05f);
    TestTrue(TEXT("Town3 leaves later-stage stone in the real depot"), Village->StoneStock >= 70);
    TestEqual(TEXT("Town3 protects only the current three-part wage window"), Village->TaxProjectCoins, 6);
    TestEqual(TEXT("Town3 releases excess tax to ordinary working capital"), Village->TaxReleasedCoins, 94);

    // A bounded execution fixture reproduces the real next floor: five planks,
    // one already purchased, abundant communal timber, and 29 protected coins.
    // Advance the actual worker state machines; do not grant output or set a
    // part to completed to make the supply chain pass.
    Village->PublicProject=FHearthPublicProject();
    Village->PublicProject.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Village->PublicProject.TemplateId=TEXT("royal_keep_garden_v2"); Village->PublicProject.Status=TEXT("building");
    Village->PublicProject.Site=0; Village->PublicProject.Stock[1]=1; Village->PublicProject.Stock[2]=18;
    FHearthPublicPart SupplyFloor,SupplyUpper;
    SupplyFloor.Stage=2; SupplyFloor.Required[1]=5; SupplyFloor.Id=TEXT("floor-a");
    SupplyUpper.Stage=3; SupplyUpper.Required[2]=1000; SupplyUpper.Id=TEXT("later-walls");
    Village->PublicProject.Parts={SupplyFloor,SupplyFloor,SupplyFloor,SupplyUpper};
    Village->PublicProject.Parts[1].Id=TEXT("floor-b"); Village->PublicProject.Parts[2].Id=TEXT("floor-c");
    const FString SupplyProjectId=Village->PublicProject.Id;
    Village->TreasuryCoins=29; Village->TaxProjectCoins=29; Village->TaxReleasedCoins=0;
    Village->PlankStock=115; Village->BeamStock=671; Village->StoneStock=14;
    Village->WoodStock[0]=40; Village->WoodStock[1]=Village->WoodStock[2]=0;
    Village->WagePayables.Reset(); Village->Transactions.Reset(); Village->TaxAssessments.Reset();
    Village->TaxRatePercent=25; for(int32& Remainder:Village->TaxRemainders) Remainder=0;
    Village->FixedObstacles.Reset(); Village->ProductionSites.Reset();
    const FVector TestDepot(-250,-400,8);
    FHearthSite SupplySite; SupplySite.Position=FVector(600,500,8); SupplySite.Approach=FVector(0,0,8); SupplySite.bReachable=true;
    FHearthSite Sawmill; Sawmill.Kind=EHearthSiteKind::Carpenter; Sawmill.Position=FVector(650,-400,8);
    Sawmill.Approach=FVector(150,-400,8); Sawmill.Radius=190; Sawmill.bReachable=true;
    Village->ProductionSites={SupplySite,Sawmill};
    for(int32 I=0;I<Village->Residents.Num();++I)
    {
        auto& Person=Village->Residents[I]; Village->ReturnTool(I);
        Person.Task=EHearthTask::LifeActivity; Person.Timer=3600.f; Person.Route.Reset(); Person.ActiveTaskId.Empty();
        Person.PersonalPlanks=0; Person.ProductionOp=-1; Person.ProductionSite=-1;
        Person.CargoType=-1; Person.CargoAmount=0; Person.Hunger=0; Person.Energy=100;
        Person.Actor->SetActorLocation(FVector(-2500,2500+I*200,8));
    }
    auto& Supplier=Village->Residents[0]; Supplier.Task=EHearthTask::LifeChoosing; Supplier.BuildProgress=1.f;
    Supplier.MoveSpeed=200.f; Supplier.Actor->SetActorLocation(TestDepot); Supplier.Timer=0.f;
    const int32 InitialManufactured=Village->Manufactured[0],InitialPlanksSpent=Village->ManufacturedSpent[0];
    const int32 InitialLogsSpent=Village->Spent[1],InitialMillingTotal=Village->ProductionTotals.FindRef(TEXT("mill_planks"));
    const auto MoneyInCirculation=[Village]()
    {
        int32 Total=Village->TreasuryCoins;
        for(const auto& Person:Village->Residents) Total+=Person.Coins;
        for(const auto& Payable:Village->WagePayables) if(Payable.Status==TEXT("reserved")) Total+=Payable.Amount;
        for(const auto& Order:Village->PublicProject.Orders) Total+=Order.Escrow;
        return Total;
    };
    const int32 InitialMoney=MoneyInCirculation();
    Village->PublicScheduleTimer=0.f;
    Village->ProductionSites[1].bReachable=false;
    Village->AdvancePublicWorks(.1f);
    TestEqual(TEXT("Unreachable sawmill cannot reserve wages"),Village->WagePayables.Num(),0);
    TestEqual(TEXT("Unreachable sawmill cannot spend source logs"),Village->AvailableWood(),40);
    TestEqual(TEXT("Procurement never reclassifies communal timber as private property"),Village->PlankStock,115);
    Village->ProductionSites[1].bReachable=true;
    Village->AdvancePublicWorks(.1f);
    if(!TestEqual(TEXT("Stalled floor schedules physical milling before any future beam job"),Supplier.Task,EHearthTask::ProductionTravel)) return false;
    TestEqual(TEXT("The first milling job reserves exactly three protected coins"),Village->TaxProjectCoins,26);
    if(!TestEqual(TEXT("Only one production wage is reserved"),Village->WagePayables.Num(),1)) return false;
    TestTrue(TEXT("Input labor uses the existing persisted tax wage ledger"),Village->WagePayables[0].bTaxFunded);
    TestEqual(TEXT("Milling has not yet earned a private share"),Supplier.PersonalPlanks,0);
    TestEqual(TEXT("Start reserves two real logs"),Village->AvailableWood(),38);
    TestEqual(TEXT("No material output before work"),Village->Manufactured[0],InitialManufactured);
    const FString FirstMillingTask=Supplier.ActiveTaskId;
    Village->AdvancePublicWorks(.1f);
    TestEqual(TEXT("Repeated scheduling cannot reserve a second milling wage"),Village->WagePayables.Num(),1);
    TestEqual(TEXT("Repeated scheduling cannot deduct logs again"),Village->AvailableWood(),38);

    bool bSawTransit=false,bSawWork=false,bSawDeposit=false,bSawSupply=false,bSawInstallation=false;
    bool bMovedDuringTransit=false;
    for(int32 Tick=0;Tick<6000 && Village->PublicProject.Completed==0;++Tick)
    {
        Village->AdvancePublicWorks(.1f);
        Supplier.Timer=FMath::Max(0.f,Supplier.Timer-.1f);
        const FVector Before=Supplier.Actor->GetActorLocation();
        const auto Task=Supplier.Task;
        if(Task==EHearthTask::ProductionTravel || Task==EHearthTask::ProductionWork
            || Task==EHearthTask::ProductionDeliver || Task==EHearthTask::ProductionDeposit)
        {
            bSawTransit|=Task==EHearthTask::ProductionDeliver;
            bSawWork|=Task==EHearthTask::ProductionWork;
            bSawDeposit|=Task==EHearthTask::ProductionDeposit;
            Village->AdvanceProduction(0,.1f);
        }
        else if(Task==EHearthTask::SupplyTravel || Task==EHearthTask::SupplyHandover)
        { bSawSupply=true; Village->AdvanceSupplyWorker(0,.1f); }
        else if(Task==EHearthTask::PublicTravel || Task==EHearthTask::PublicWork)
        { bSawInstallation|=Task==EHearthTask::PublicWork; Village->AdvancePublicWorker(0,.1f); }
        bMovedDuringTransit|=!Before.Equals(Supplier.Actor->GetActorLocation(),.01);
    }
    TestEqual(TEXT("The same project finishes exactly one previously waiting floor"),Village->PublicProject.Completed,1);
    TestEqual(TEXT("Supply recovery retains project identity"),Village->PublicProject.Id,SupplyProjectId);
    TestTrue(TEXT("Recovery includes actual work, public cargo, deposit, purchase and installation"),
        bSawTransit && bSawWork && bSawDeposit && bSawSupply && bSawInstallation && bMovedDuringTransit);
    TestEqual(TEXT("Future-stage walls remain waiting"),Village->PublicProject.Parts.Last().Status,FString(TEXT("waiting")));
    TestEqual(TEXT("Four milling jobs consume eight source logs"),Village->AvailableWood(),32);
    TestEqual(TEXT("Source log expense is recorded once"),Village->Spent[1]-InitialLogsSpent,8);
    TestEqual(TEXT("Four real sawmill jobs manufacture sixteen total planks"),Village->Manufactured[0]-InitialManufactured,16);
    TestEqual(TEXT("Twelve communal planks remain communal"),Village->PlankStock,127);
    TestEqual(TEXT("One purchased plank plus four private shares are installed"),Village->ManufacturedSpent[0]-InitialPlanksSpent,5);
    TestEqual(TEXT("Private shares have all been sold exactly once"),Supplier.PersonalPlanks,0);
    TestEqual(TEXT("Only four plank purchases were opened"),Village->PublicProject.Orders.Num(),4);
    TestEqual(TEXT("Only four completed milling operations are counted"),Village->ProductionTotals.FindRef(TEXT("mill_planks"))-InitialMillingTotal,4);
    TestEqual(TEXT("Future beam demand does not consume communal beams"),Village->BeamStock,671);
    TestEqual(TEXT("Wages, sales and tax transfers conserve total money"),MoneyInCirculation(),InitialMoney);
    const int32 MoneyAfter=MoneyInCirculation(),TransactionsAfter=Village->Transactions.Num();
    TestFalse(TEXT("Already paid milling wage cannot settle a second time"),Village->SettleWage(0,FirstMillingTask));
    if(!Village->PublicProject.Orders.IsEmpty())
        TestTrue(TEXT("Completed private sale remains idempotent"),Village->SettleSupplyOrder(Village->PublicProject.Orders[0]));
    TestEqual(TEXT("Settlement retries cannot add transaction rows"),Village->Transactions.Num(),TransactionsAfter);
    TestEqual(TEXT("Settlement retries cannot create money"),MoneyInCirculation(),MoneyAfter);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTown3PublicWorksTest,
    "ThreeHearths.Economy.Town3CastleCanonicalRecipe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthTown3PublicWorksTest::RunTest(const FString&)
{
    FHearthPublicProject Project;
    Project.TemplateId = TEXT("royal_keep_garden_v2");
    Project.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    HearthPublicWorks::Populate(Project);
    const auto Plan = HearthRoyalWorksPlan::BuildForTemplate(Project.TemplateId);
    // Geometry/BOM category counts and duplicate placements are independently
    // audited by RoyalWorksPlan.Town3CastleIsModularAndStaged. This test checks
    // the production conversion, including every material and construction field.
    if (!TestEqual(TEXT("Populate includes every canonical module"),Project.Parts.Num(),Plan.Modules.Num())
        || !TestTrue(TEXT("Canonical plan is nonempty"),!Plan.Modules.IsEmpty())) return false;
    TSet<FString> Ids;
    FIntVector ActualTotal=FIntVector::ZeroValue, ExpectedTotal=FIntVector::ZeroValue;
    for (int32 Index=0; Index<Plan.Modules.Num(); ++Index)
    {
        const auto& Module=Plan.Modules[Index]; const auto& Part=Project.Parts[Index];
        TestTrue(TEXT("Public part ID is project scoped and unique"),Part.Id.StartsWith(Project.Id+TEXT(":")) && !Ids.Contains(Part.Id));
        Ids.Add(Part.Id);
        TestEqual(TEXT("Canonical module identity"),Part.Asset,Module.Id);
        TestEqual(TEXT("Canonical stage"),Part.Stage,Module.Stage);
        TestTrue(TEXT("Canonical persisted centre"),Part.Offset.Equals(Module.Offset,.001));
        TestEqual(TEXT("New part waits for real construction"),Part.Status,FString(TEXT("waiting")));
        for (int32 Material=0; Material<3; ++Material)
        {
            TestEqual(TEXT("Exact per-part material bill"),Part.Required[Material],Module.Materials[Material]);
            TestEqual(TEXT("Populate cannot grant materials"),Part.Reserved[Material],0);
            TestEqual(TEXT("Populate cannot pretend delivery"),Part.Delivered[Material],0);
            ActualTotal[Material]+=Part.Required[Material];
            ExpectedTotal[Material]+=Module.Materials[Material];
        }
    }
    TestTrue(TEXT("Complete BOM survives public conversion"),ActualTotal==ExpectedTotal);
    TestEqual(TEXT("Construction starts at foundations"),Project.Parts[0].Stage,1);
    TestEqual(TEXT("Construction retains thirteen stages"),Project.Parts.Last().Stage,13);
    TestEqual(TEXT("Populate cannot mark the project complete"),Project.Completed,0);
    const FString FirstId=Project.Parts[0].Id;
    HearthPublicWorks::Populate(Project);
    TestEqual(TEXT("Replay preserves canonical identity"),Project.Parts[0].Id,FirstId);
    TestEqual(TEXT("Replay cannot append duplicate parts"),Project.Parts.Num(),Plan.Modules.Num());
    return true;
}
#endif
