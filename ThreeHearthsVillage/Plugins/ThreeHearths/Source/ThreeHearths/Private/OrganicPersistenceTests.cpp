#if WITH_DEV_AUTOMATION_TESTS
#include "HearthWorldState.h"
#include "HearthOrganicConstruction.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

namespace
{
    UWorld* OrganicTestWorld()
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrganicWorldPersistenceTest,
    "ThreeHearths.Persistence.OrganicConstructionColdRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrganicWorldPersistenceTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))+TEXT(" -HearthNoWorldPersistence -HearthOrganicVillage")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };
    UWorld* World=OrganicTestWorld(); if(!TestNotNull(TEXT("Isolated organic persistence world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47.f),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140.f,140.f,1.f));
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->bUseCropoutMap=true; Village->BuildEnvironment(); Village->ResetVillageState();
    if(!TestEqual(TEXT("v4 fixture has thirteen residents"),Village->Residents.Num(),13)) return false;
    const FString ResidentId=Village->Residents[0].StableId;
    FOrganicConstructionHomeState Home; Home.InstalledKeys={TEXT("starter.foundation")}; Home.InstalledCosts.Add(TEXT("starter.foundation"),FOrganicConstructionStock{3,2,1,0});
    Home.Reclaimed={1,2,0,1}; Home.ActivePieceKey=TEXT("starter.wing"); Home.WorkProgress=.42f; Home.CurrentRecipe=TEXT("family_starter"); Home.TargetRecipe=TEXT("family_growth"); Home.ChoiceReason=TEXT("需要更多家庭空间"); Home.Source=TEXT("local_rules"); Home.Seed=TEXT("7919"); Home.Revision=4;
    Village->OrganicWorldSeed=7919; Village->OrganicHomes.Add(ResidentId,Home);
    // Keep this test's input as an explicit schema-11 legacy fixture.  Fresh v4
    // exports now contain the three service residents, while the old fixture
    // contract intentionally contains only the original ten people.
    FHearthWorldImage Current; FString Error;
    if(!TestTrue(TEXT("Current v4 image decodes"),HearthWorld::Decode(Village->ExportWorldState(),Current,Error))) { AddError(Error); return false; }
    Current.Schema=11;
    Current.PopulationCount=10;
    Current.People.SetNum(10);
    const int32 RoyalSite=Current.Sites.IndexOfByPredicate([](const auto& Site)
        { return FVector::Dist2D(Site.Position,FVector(6500,6500,0))<1.f; });
    if(!TestTrue(TEXT("Central castle land exists above trace origin"),RoyalSite!=INDEX_NONE)) return false;
    Current.Sites[RoyalSite].Position.Z=355.f; // Previous low terrain checkpoint.
    for(int32 I=0;I<Current.PlotCount;++I) Current.Plots[I].Z-=75.f;
    for(int32 I=0;I<3;++I) Current.Stocks[I].Z-=75.f;
    const FString Payload=HearthWorld::Encode(Current); FHearthWorldImage Image;
    if(!TestTrue(TEXT("Schema 11 organic image decodes"),HearthWorld::Decode(Payload,Image,Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Organic schema is 11"),Image.Schema,11); TestEqual(TEXT("Legacy fixture retains ten people"),Image.People.Num(),10); TestEqual(TEXT("Organic seed survives"),Image.OrganicWorldSeed,7919); TestTrue(TEXT("Resident home survives"),Image.OrganicHomes.Contains(ResidentId));
    const auto& Decoded=Image.OrganicHomes[ResidentId]; TestEqual(TEXT("Installed key survives"),Decoded.InstalledKeys[0],FString(TEXT("starter.foundation"))); TestEqual(TEXT("Pending key survives"),Decoded.ActivePieceKey,FString(TEXT("starter.wing"))); TestEqual(TEXT("Work progress survives"),Decoded.WorkProgress,.42f); TestEqual(TEXT("Choice source survives"),Decoded.Source,FString(TEXT("local_rules")));
    if(!TestTrue(TEXT("Cold apply restores organic home"),Village->ApplyWorldState(Payload,Error))) { AddError(Error); return false; }
    const auto* Restored=Village->OrganicHomes.Find(ResidentId); TestNotNull(TEXT("Restored resident home exists"),Restored);
    if(Restored) { TestEqual(TEXT("Cold restore keeps target recipe"),Restored->TargetRecipe,FString(TEXT("family_growth"))); TestEqual(TEXT("Cold restore keeps active progress"),Restored->WorkProgress,.42f); }
    TestTrue(TEXT("Legacy castle elevation reprojects to real central plateau"),Village->ProductionSites[RoyalSite].Position.Z>3400.f);
    TestEqual(TEXT("Terrain revision does not create tax money"),Village->TaxProjectCoins,Current.TaxProjectCoins);
    TestEqual(TEXT("Terrain revision does not create treasury money"),Village->TreasuryCoins,Current.TreasuryCoins);
    FHearthWorldImage Migrated;
    if(!TestTrue(TEXT("Migrated world remains serializable"),HearthWorld::Decode(Village->ExportWorldState(),Migrated,Error))) return false;
    for(int32 I=0;I<Current.PlotCount;++I)
    {
        TestEqual(TEXT("Vertical migration retains plot identity"),Migrated.PlotIds[I],Current.PlotIds[I]);
        TestTrue(TEXT("Vertical migration retains horizontal ownership"),FVector::Dist2D(Migrated.Plots[I],Current.Plots[I])<.01);
    }
    if(!TestTrue(TEXT("Migrated image can load a second time"),Village->ApplyWorldState(HearthWorld::Encode(Migrated),Error))) return false;
    TestTrue(TEXT("Reload does not raise castle a second time"),Village->ProductionSites[RoyalSite].Position.Equals(Migrated.Sites[RoyalSite].Position,.01));
    FString Negative=Payload; Negative.ReplaceInline(TEXT("\"stone\":3"),TEXT("\"stone\":-1")); FHearthWorldImage Rejected; TestFalse(TEXT("Negative organic stock is rejected"),HearthWorld::Decode(Negative,Rejected,Error));
    return true;
}
#endif
