#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthMedievalPopulationMigrationTest,"ThreeHearths.Persistence.MedievalPopulationSchema12",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthMedievalPopulationMigrationTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthOrganicVillage -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated organic population world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->BuildEnvironment(); Village->ResetVillageState(); Village->bAutonomousLifeEnabled=false;
    if(!TestTrue(TEXT("Fresh organic runtime has ten private residents plus three services"),Village->IsOrganicVillage() && Village->Residents.Num()==13)) return false;

    FString Error; FHearthWorldImage Fresh;
    const FString FreshText=Village->ExportWorldState();
    if(!TestTrue(TEXT("Fresh schema 12 world validates"),HearthWorld::Decode(FreshText,Fresh,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Schema 12 is independent of private plot count"),Fresh.Schema,12);
    TestEqual(TEXT("Ten private plots remain"),Fresh.PlotCount,10);
    TestEqual(TEXT("Population count is thirteen"),Fresh.PopulationCount,13);
    TestEqual(TEXT("People array has thirteen records"),Fresh.People.Num(),13);
    TestEqual(TEXT("Service arrivals do not mint food"),Fresh.Food,100);
    FHearthWorldImage BadPlotCount=Fresh; BadPlotCount.PlotCount=9; FHearthWorldImage ContractRejected;
    TestFalse(TEXT("Schema 12 rejects an organic save with fewer than ten private plots"),HearthWorld::Decode(HearthWorld::Encode(BadPlotCount),ContractRejected,Error));
    FHearthWorldImage BadPopulation=Fresh; BadPopulation.PopulationCount=12; BadPopulation.People.SetNum(12);
    TestFalse(TEXT("Schema 12 rejects an organic save with a population other than thirteen"),HearthWorld::Decode(HearthWorld::Encode(BadPopulation),ContractRejected,Error));
    for(int32 I=0;I<10;++I)
    {
        FGuid ParsedId; TestTrue(TEXT("Legacy resident keeps a valid stable ID"),FGuid::Parse(Fresh.People[I].Person.StableId,ParsedId) && ParsedId.IsValid());
        TestEqual(TEXT("Legacy resident remains on its original plot contract"),Fresh.People[I].Person.Plot,Village->Residents[I].Plot);
    }
    const FString GateId=Fresh.People[10].Person.StableId;
    const FString GuardId=Fresh.People[11].Person.StableId;
    const FString CarterId=Fresh.People[12].Person.StableId;
    const int32 OriginalFood=Fresh.Food, OriginalTreasury=Fresh.TreasuryCoins, OriginalWood=Fresh.Wood[0]+Fresh.Wood[1]+Fresh.Wood[2];
    TestEqual(TEXT("Gatekeeper role key persists"),Fresh.People[10].Person.ServiceRoleKey,FString(TEXT("gatekeeper")));
    TestEqual(TEXT("Royal guard role key persists"),Fresh.People[11].Person.ServiceRoleKey,FString(TEXT("royal_guard")));
    TestEqual(TEXT("Carter role key persists"),Fresh.People[12].Person.ServiceRoleKey,FString(TEXT("carter")));
    for(int32 I=10;I<13;++I)
    {
        TestEqual(TEXT("Shared resident has no private plot"),Fresh.People[I].Person.Plot,-1);
        TestEqual(TEXT("Shared resident starts without coins"),Fresh.People[I].Person.Coins,0);
        TestEqual(TEXT("Shared resident starts without personal planks"),Fresh.People[I].Person.PersonalPlanks,0);
        TestEqual(TEXT("Shared resident starts without personal tiles"),Fresh.People[I].Person.PersonalTiles,0);
    }
    TestTrue(TEXT("Shared service resident has no production candidates"),Village->AvailableProductionActions(10).IsEmpty());

    // Simulate an actual schema-11 v4 checkpoint by retaining exactly the
    // original ten people. Applying it to a fresh v4 runtime appends the three
    // current catalog residents once, without inventing their starter balances.
    FHearthWorldImage Legacy=Fresh; Legacy.Schema=11; Legacy.PopulationCount=10; Legacy.People.SetNum(10);
    const FString LegacyText=HearthWorld::Encode(Legacy);
    FHearthWorldImage DecodedLegacy;
    if(!TestTrue(TEXT("Synthetic schema 11 checkpoint validates"),HearthWorld::Decode(LegacyText,DecodedLegacy,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Schema 11 derives population from people"),DecodedLegacy.PopulationCount,10);
    if(!TestTrue(TEXT("Schema 11 v4 checkpoint migrates atomically"),Village->ApplyWorldState(LegacyText,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Migration adds exactly three shared residents"),Village->Residents.Num(),13);
    TestEqual(TEXT("Gatekeeper identity is retained after migration"),Village->Residents[10].StableId,GateId);
    TestEqual(TEXT("Guard identity is retained after migration"),Village->Residents[11].StableId,GuardId);
    TestEqual(TEXT("Carter identity is retained after migration"),Village->Residents[12].StableId,CarterId);
    for(int32 I=10;I<13;++I) TestEqual(TEXT("Migrated service resident remains unpaid"),Village->Residents[I].Coins,0);
    TestEqual(TEXT("Migration preserves food ledger"),Village->FoodStock,OriginalFood);
    TestEqual(TEXT("Migration preserves treasury"),Village->TreasuryCoins,OriginalTreasury);
    TestEqual(TEXT("Migration preserves shared wood ledger"),Village->AvailableWood(),OriginalWood);
    for(int32 I=0;I<10;++I) TestEqual(TEXT("Migration preserves legacy resident wallet"),Village->Residents[I].Coins,Fresh.People[I].Person.Coins);
    const TArray<FString> LegacyIds={Village->Residents[0].StableId,Village->Residents[1].StableId,Village->Residents[2].StableId,Village->Residents[3].StableId,Village->Residents[4].StableId,Village->Residents[5].StableId,Village->Residents[6].StableId,Village->Residents[7].StableId,Village->Residents[8].StableId,Village->Residents[9].StableId};
    const FString Current12=Village->ExportWorldState();
    if(!TestTrue(TEXT("Schema 12 world reloads without changing the roster"),Village->ApplyWorldState(Current12,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Schema 12 reload keeps thirteen residents"),Village->Residents.Num(),13);
    TestEqual(TEXT("Schema 12 reload keeps gatekeeper GUID"),Village->Residents[10].StableId,GateId);
    TestEqual(TEXT("Schema 12 reload keeps guard GUID"),Village->Residents[11].StableId,GuardId);
    TestEqual(TEXT("Schema 12 reload keeps carter GUID"),Village->Residents[12].StableId,CarterId);
    if(!TestTrue(TEXT("Applying the same old checkpoint is idempotent"),Village->ApplyWorldState(LegacyText,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Repeated migration does not append duplicate residents"),Village->Residents.Num(),13);
    for(int32 I=0;I<10;++I) TestEqual(TEXT("Repeated migration preserves legacy identity"),Village->Residents[I].StableId,LegacyIds[I]);

    // Shared residents use the existing public life anchor for rest/social
    // actions; they never need a private plot to be socially available.
    Village->Residents[10].Task=EHearthTask::LifeChoosing; Village->Residents[10].Route.Reset();
    Village->Residents[0].Task=EHearthTask::LifeChoosing; Village->Residents[0].BuildProgress=1.f; Village->Residents[0].Route.Reset();
    TestTrue(TEXT("Shared resident can choose public rest"),Village->AvailableLifeActions(10).Contains(0));
    TestTrue(TEXT("Shared resident can participate in social availability"),Village->IsSociallyAvailable(10));
    TestTrue(TEXT("Private resident remains socially available"),Village->IsSociallyAvailable(0));
    Village->ProductionSites.Reset();
    if(TestTrue(TEXT("Shared resident starts a real public rest action"),Village->StartLifeAction(10,0,TEXT("公共生活区休息"),false)))
    {
        TestEqual(TEXT("Shared rest action has a live travel task"),Village->Residents[10].Task,EHearthTask::LifeTravel);
        TestTrue(TEXT("Shared rest action has a real task GUID"),!Village->Residents[10].ActiveTaskId.IsEmpty());
        Village->Residents[10].Task=EHearthTask::LifeActivity; Village->Residents[10].Timer=0.f;
        Village->AdvanceLife(10,.1f);
        TestEqual(TEXT("Shared rest returns to life selection"),Village->Residents[10].Task,EHearthTask::LifeChoosing);
    }

    FHearthWorldImage Corrupt=Fresh; Corrupt.People[10].Person.ResidenceId=TEXT("plot-0");
    FHearthWorldImage Rejected; TestFalse(TEXT("Corrupt shared residence marker is rejected"),HearthWorld::Decode(HearthWorld::Encode(Corrupt),Rejected,Error));
    TestEqual(TEXT("Corrupt decode does not alter the known service ID"),Fresh.People[10].Person.StableId,GateId);
    return true;
}
#endif
