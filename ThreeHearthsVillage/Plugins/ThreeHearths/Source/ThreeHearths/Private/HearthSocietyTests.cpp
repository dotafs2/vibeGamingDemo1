#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthSocietyPopulationTest,"ThreeHearths.Society.PopulationMigrationAndMeals",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthSocietyPopulationTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("Isolated society world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* V=World->SpawnActor<AHearthVillage>(); V->BuildEnvironment(); V->ResetVillageState(); V->bAutonomousLifeEnabled=false;
    FString Error; FHearthWorldImage Original;
    if(!TestTrue(TEXT("Read existing three residents"),HearthWorld::Decode(V->ExportWorldState(),Original,Error))) return false;
    // Reconstruct the exact schema-1 field layout, including resource columns on plots.
    TSharedPtr<FJsonObject> Legacy;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(V->ExportWorldState()),Legacy);
    Legacy->SetNumberField(TEXT("schema"),1); Legacy->RemoveField(TEXT("plot_count"));
    const auto Resources=Legacy->GetArrayField(TEXT("resources"));
    for(int32 I=0;I<3;++I) for(const auto& Pair:Resources[I]->AsObject()->Values)
        Legacy->GetArrayField(TEXT("plots"))[I]->AsObject()->SetField(Pair.Key,Pair.Value);
    Legacy->RemoveField(TEXT("resources"));
    for(const auto& Plot:Legacy->GetArrayField(TEXT("plots"))) Plot->AsObject()->RemoveField(TEXT("entry"));
    for(const auto& Person:Legacy->GetArrayField(TEXT("people")))
        for(const TCHAR* Field:{TEXT("Role"),TEXT("Hunger"),TEXT("Mood"),TEXT("Age"),TEXT("king")}) Person->AsObject()->RemoveField(Field);
    FString OldText; FJsonSerializer::Serialize(Legacy.ToSharedRef(),TJsonWriterFactory<>::Create(&OldText));
    FHearthWorldImage Migrated;
    Migrated.PlotEntrances[0]=FVector(123.f,456.f,8.f);
    if(!TestTrue(TEXT("Read schema 1 without invented inventory"),HearthWorld::Decode(OldText,Migrated,Error))) return false;
    TestEqual(TEXT("Old resident identity retained"),Migrated.People[0].Person.StableId,Original.People[0].Person.StableId);
    for(int32 I=0;I<HearthVillageLimits::MaxPopulation;++I)
        TestTrue(TEXT("Missing legacy entrances have a deterministic empty value"),Migrated.PlotEntrances[I].IsZero());
    // Layout is synthetic here; terrain and ten-person traffic are checked in the actual island separately.
    V->bUseCropoutMap=true; Migrated.bIsland=true;
    for(int32 I=3;I<10;++I)
    {
        FHearthResident R; V->InitializeResidentIdentity(I,R); R.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        R.Actor=World->SpawnActor<AHearthVillager>(); V->Residents.Add(R);
        V->PlotIds[I]=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); V->PlotPositions[I]=FVector(3000,I*700,8);
        // The synthetic fixture must initialize the same plot contract as
        // ResetVillageState; the expanded arrays deliberately start at zero.
        V->PlotCosts[I]=6; V->PlotOwners[I]=-1; V->PlotYaws[I]=(I-3)*7.f;
        V->PlotEntrances[I]=V->PlotPositions[I]+FRotator(0,V->PlotYaws[I],0).RotateVector(FVector(-370.f,0.f,0.f));
    }
    FHearthSite Vacant; Vacant.StableId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Vacant.Position=V->PlotPositions[3]; Vacant.bExpansion=true; Vacant.bReachable=true;
    Migrated.Sites.Add(Vacant);
    auto Conflict=Migrated; Conflict.Sites[0].Owner=0;
    TestFalse(TEXT("Migration will not erase owned land"),V->MigrateWorldPopulation(Conflict,Error));
    if(!TestTrue(TEXT("Seven arrivals can migrate unowned vacant land"),V->MigrateWorldPopulation(Migrated,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Ten residents"),Migrated.People.Num(),10); TestEqual(TEXT("Arrival food accounted"),Migrated.Food,100);
    TestEqual(TEXT("Arrival wood accounted"),Migrated.Wood[0]+Migrated.Wood[1]+Migrated.Wood[2],99);
    TestEqual(TEXT("Old site identity retained"),Migrated.Sites[0].StableId,Vacant.StableId);
    for(int32 I=0;I<3;++I)
        TestTrue(TEXT("Migration leaves old plot positions and yaws unchanged"),Migrated.Plots[I].Equals(Original.Plots[I],.001f) && Migrated.PlotYaws[I]==Original.PlotYaws[I]);
    for(int32 I=3;I<10;++I)
    {
        TestEqual(TEXT("Arrival plot keeps its legitimate construction cost"),Migrated.Costs[I],V->PlotCosts[I]);
        TestTrue(TEXT("Arrival plot retains the configured entry and yaw"),Migrated.PlotEntrances[I].Equals(V->PlotEntrances[I],.001f) && Migrated.PlotYaws[I]==V->PlotYaws[I]);
    }
    auto InvalidCost=Migrated; InvalidCost.Costs[9]=0; FHearthWorldImage Rejected;
    TestFalse(TEXT("Zero-cost arrival plots remain invalid"),HearthWorld::Decode(HearthWorld::Encode(InvalidCost),Rejected,Error));
    TestFalse(TEXT("Only vacant future plot is retired"),Migrated.Sites[0].bExpansion);
    TestTrue(TEXT("Exactly one king at population start"),Migrated.People.FilterByPredicate([](const auto& P) { return P.Person.bKing; }).Num()==1);
    TestTrue(TEXT("Reapplying migration does not grant another arrival kit"),V->MigrateWorldPopulation(Migrated,Error));
    TestEqual(TEXT("Food grant not replayed"),Migrated.Food,100);
    // Two finished meal timers contend for the final food. Only the eater gains satiety.
    V->FoodStock=1; V->Spent[0]=0;
    for(int32 I=8;I<10;++I) { auto& R=V->Residents[I]; R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); R.Task=EHearthTask::LifeActivity; R.LifeAction=50; R.Timer=0; R.Hunger=80; }
    V->AdvanceLife(8,.1f); V->AdvanceLife(9,.1f);
    TestEqual(TEXT("Final meal cannot make stock negative"),V->FoodStock,0); TestEqual(TEXT("Only one food consumed"),V->Spent[0],1);
    TestEqual(TEXT("Actual eater is satiated"),V->Residents[8].Hunger,25.f); TestEqual(TEXT("Unserved resident stays hungry"),V->Residents[9].Hunger,80.f);
    V->AdvanceLife(8,.1f); TestEqual(TEXT("Completed meal cannot debit again"),V->Spent[0],1);
    TestFalse(TEXT("Empty food stock removes meal action"),V->AvailableLifeActions(9).Contains(50));
    const float Before=V->Residents[9].Hunger; V->AdvanceNeeds(10); TestTrue(TEXT("Hunger advances with simulated time"),V->Residents[9].Hunger>Before);
    FHearthWorldImage Checked;
    Migrated.People[9].Person.Hunger=67; Migrated.People[9].Person.Mood=42;
    TestTrue(TEXT("Ten-person needs persist"),HearthWorld::Decode(HearthWorld::Encode(Migrated),Checked,Error));
    TestEqual(TEXT("Last resident hunger restored"),Checked.People[9].Person.Hunger,67.f);
    TestEqual(TEXT("Last resident mood restored"),Checked.People[9].Person.Mood,42.f);
    return true;
}
#endif
