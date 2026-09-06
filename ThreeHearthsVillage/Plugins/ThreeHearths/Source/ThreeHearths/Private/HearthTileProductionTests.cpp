#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"
#include "HearthPlannedConstructionAdapter.h"
#include "HearthResidentBuildingPlanner.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthTileProductionTest,"ThreeHearths.Production.ClayFuelAndTileKiln",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthTileProductionTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated tile production world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->BuildEnvironment(); Village->ResetVillageState();
    Village->bAutonomousLifeEnabled=false; Village->bApiDisabledThisRun=true;
    auto& Potter=Village->Residents[0]; Potter.Role=TEXT("陶工"); Potter.BuildProgress=1.f; Potter.Task=EHearthTask::LifeChoosing;
    Potter.Actor->SetActorLocation(FVector(-400,0,8)); Potter.Route.Reset();
    Village->LandGrid.Reset(); for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) Village->LandGrid.Add(FIntPoint(X,Y));
    Village->ProductionSites.Reset();
    FHearthSite Pit; Pit.StableId=TEXT("clay-pit"); Pit.Kind=EHearthSiteKind::ClayPit; Pit.Position=FVector(400,0,8); Pit.Approach=FVector(0,0,8); Pit.bReachable=true; Pit.Stage=2; Pit.Units=Pit.Capacity=12;
    FHearthSite Kiln; Kiln.StableId=TEXT("tile-kiln"); Kiln.Kind=EHearthSiteKind::TileKiln; Kiln.Position=FVector(400,400,8); Kiln.Approach=FVector(0,400,8); Kiln.bReachable=true; Kiln.Stage=2;
    Village->ProductionSites={Pit,Kiln}; Village->ClayStock=Village->TileStock=0; Village->ProducedClay=Village->SpentClay=Village->ProducedTiles=Village->SpentTiles=0;
    Village->WoodStock[0]=4; Village->WoodStock[1]=Village->WoodStock[2]=0;
    Village->TreasuryCoins=1;

    TestTrue(TEXT("Potter self-funds clay input labor when the public treasury is exhausted"),Village->StartProduction(0,115,TEXT("gather clay for roof tiles"),false));
    TestEqual(TEXT("Clay extraction owns the shared trowel"),Potter.HeldToolId,FString(TEXT("tool_trowel")));
    Potter.Task=EHearthTask::ProductionWork; Potter.Timer=0; Village->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Clay leaves the pit as physical cargo"),Potter.CargoType,5); TestEqual(TEXT("One clay trip carries six units"),Potter.CargoAmount,6);
    TestEqual(TEXT("Clay is not credited before depot deposit"),Village->ClayStock,0);
    Potter.Task=EHearthTask::ProductionDeposit; Potter.Timer=0; Village->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Deposited clay enters public stock"),Village->ClayStock,6); TestEqual(TEXT("Extracted clay ledger records six"),Village->ProducedClay,6);

    Village->TreasuryCoins=20;
    const int32 WoodBefore=Village->AvailableWood();
    TestTrue(TEXT("Kiln starts only with clay, wood fuel and its tool"),Village->StartProduction(0,131,TEXT("fire a public tile batch"),false));
    TestEqual(TEXT("Kiln atomically consumes four clay"),Village->ClayStock,2); TestEqual(TEXT("Kiln records consumed clay"),Village->SpentClay,4);
    TestEqual(TEXT("Kiln atomically burns two logs"),Village->AvailableWood(),WoodBefore-2);
    Potter.Task=EHearthTask::ProductionWork; Potter.Timer=0; Village->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Fired tiles leave the kiln as physical cargo"),Potter.CargoType,6); TestEqual(TEXT("One firing produces six tiles"),Potter.CargoAmount,6);
    TestEqual(TEXT("Tiles are not credited before depot deposit"),Village->TileStock,0);
    Potter.Task=EHearthTask::ProductionDeposit; Potter.Timer=0; Village->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Deposited tiles enter public stock"),Village->TileStock,6); TestEqual(TEXT("Produced tile ledger records six"),Village->ProducedTiles,6);

    Village->ClayStock=4; Village->WoodStock[0]=2; Village->TreasuryCoins=1; Potter.Task=EHearthTask::LifeChoosing;
    FHearthTileOrder Order; Order.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Order.Status=TEXT("active"); Order.Potter=0; Order.Customer=1; Order.Escrow=4;
    Village->TileOrders.Add(Order); const int32 PublicTilesBeforeOrder=Village->TileStock;
    TestTrue(TEXT("Active customer order lets its potter self-fund the kiln labor"),Village->StartProduction(0,131,TEXT("fire the accepted customer order"),false));
    TestEqual(TEXT("Order id remains the delivery task identity"),Potter.ActiveTaskId,Village->TileOrders[0].Id);
    TestEqual(TEXT("Started order records four committed clay units"),Village->TileOrders[0].ReservedClay,4);
    Potter.Task=EHearthTask::ProductionWork; Potter.Timer=0; Village->AdvanceProduction(0,.1f);
    TestEqual(TEXT("Finished order advances to delivery"),Village->TileOrders[0].Status,FString(TEXT("delivering")));
    TestEqual(TEXT("All six ordered tiles are reserved"),Village->TileOrders[0].ReservedTiles,6);
    TestEqual(TEXT("Potter physically carries the ordered tiles"),Potter.CargoType,6); TestEqual(TEXT("Order handoff uses trade travel"),Potter.Task,EHearthTask::TradeTravel);
    TestEqual(TEXT("Ordered tiles never enter public stock"),Village->TileStock,PublicTilesBeforeOrder);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPrivateTileConstructionTest,"ThreeHearths.Production.PrivateOrderedTilesBuildRoof",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthPrivateTileConstructionTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated private-tile construction world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->BuildEnvironment(); Village->ResetVillageState();
    Village->bAutonomousLifeEnabled=false; Village->bApiDisabledThisRun=true;
    auto& Owner=Village->Residents[0]; Owner.BuildProgress=1.f; Owner.Task=EHearthTask::LifeChoosing; Owner.Coins=20; Owner.PersonalTiles=6;
    Owner.Actor->SetActorLocation(FVector(-400,0,8)); Owner.Route.Reset();
    Village->LandGrid.Reset(); for(int32 X=-30;X<=30;++X) for(int32 Y=-30;Y<=30;++Y) Village->LandGrid.Add(FIntPoint(X,Y));

    FHearthResidentBuildingInput Input; Input.ResidentId=Owner.StableId; Input.StableSeed=TEXT("private-tile-roof-test");
    Input.Need=TEXT("shelter"); Input.Occupation=TEXT("陶工"); Input.WallMaterial=TEXT("timber"); Input.RoofMaterial=TEXT("terracotta");
    Input.Budget=100; Input.bRoadAccessible=true; Input.Origin=FVector(400,0,8); Input.Stone=20; Input.Planks=40; Input.Beams=40; Input.Tiles=12;
    const auto Planned=HearthResidentBuildingPlanner::Build(Input);
    if(!TestTrue(TEXT("A twelve-tile terracotta room plan is executable"),Planned.bBuildable)) return false;
    const auto Converted=HearthPlannedConstructionAdapter::Convert(Planned.Plan,0,{});
    if(!TestTrue(TEXT("Terracotta plan maps to runtime components"),Converted.bAccepted)) return false;

    FHearthSite Home; Home.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Home.Kind=EHearthSiteKind::Land;
    Home.Position=FVector(400,0,8); Home.Approach=FVector(0,0,8); Home.bReachable=true; Home.Owner=0; Home.BuildPlanId=Planned.Plan.PlanId;
    Home.CottageComponents=Converted.Components; Home.Capacity=Home.CottageComponents.Num();
    bool LeftOneRoof=false;
    for(auto& Part:Home.CottageComponents)
    {
        if(!LeftOneRoof && Part.AssetId==TEXT("roof_slope_terracotta_2m")) { LeftOneRoof=true; continue; }
        Part.Status=TEXT("completed"); ++Home.Units;
    }
    Village->StructurePlans={Planned.Plan}; Village->ProductionSites={Home}; Village->TileStock=0;
    TestTrue(TEXT("The remaining modeled terracotta slope exists"),LeftOneRoof);
    TestTrue(TEXT("Owner can start that roof component from delivered personal tiles"),Village->StartProduction(0,105,TEXT("install ordered roof tiles"),false));
    const auto* Reserved=Village->ProductionSites[0].CottageComponents.FindByPredicate([](const auto& Part){return Part.Status==TEXT("reserved");});
    if(!TestNotNull(TEXT("One roof component is reserved"),Reserved)) return false;
    TestEqual(TEXT("Personal order stock pays for the roof component"),Owner.PersonalTiles,0);
    TestEqual(TEXT("Public tile stock is untouched"),Village->TileStock,0);
    TestEqual(TEXT("Component records private ownership source"),Reserved->Source,FString(TEXT("resident_owned")));
    TestEqual(TEXT("Component records tile-order supply policy"),Reserved->SupplyPolicy,FString(TEXT("private_tile_order")));
    TestTrue(TEXT("Interrupted installation can be cancelled"),Village->CancelProduction(0));
    TestEqual(TEXT("Cancellation restores tiles to the owner"),Owner.PersonalTiles,6);
    TestEqual(TEXT("Cancellation still cannot create public tiles"),Village->TileStock,0);
    return true;
}
#endif
