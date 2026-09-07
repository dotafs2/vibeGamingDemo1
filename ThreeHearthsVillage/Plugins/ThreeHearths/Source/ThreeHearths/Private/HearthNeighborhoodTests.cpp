#if WITH_DEV_AUTOMATION_TESTS
#include "HearthVillage.h"

#include "HearthWorldState.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthNeighborhoodPersistenceTest,"ThreeHearths.Town.ConnectedStarterNeighborhood",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHearthNeighborhoodPersistenceTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    ON_SCOPE_EXIT {World->DestroyWorld(false);};
    auto* Terrain=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Terrain->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Terrain->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Terrain->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* V=World->SpawnActor<AHearthVillage>(); V->bUseCropoutMap=true;V->BuildEnvironment();V->ResetVillageState();
    TestEqual(TEXT("New world uses generated connected neighborhood"),V->TownLayoutVersion,2);
    TSet<int32> Angles;
    for(int32 I=0;I<10;++I)
    {
        Angles.Add(FMath::RoundToInt(V->PlotYaws[I]));
        TestTrue(TEXT("Every rotated entrance stays outside obstacles"),V->IsClearPoint(V->HomeApproach(I)));
        TArray<FVector> Route;
        TestTrue(FString::Printf(TEXT("Home %d (%s) has an executable route from the shared depot"),I,*V->HomeApproach(I).ToString()),V->FindProductionPath(FVector(-1650,-1050,8),V->HomeApproach(I),Route));
    }
    TestTrue(TEXT("Home frontage follows multiple street angles"),Angles.Num()>3);
    TSet<FString> Stories, Archetypes;
    for(int32 I=0;I<10;++I)
    {
        TestFalse(TEXT("Every resident has a persistent inner story"),V->Residents[I].InnerStory.IsEmpty());
        Stories.Add(V->Residents[I].InnerStory);Archetypes.Add(V->Residents[I].BuildingArchetype);
        TArray<FVector> ArrivalRoute;
        TestTrue(TEXT("Every arrival starts on a reachable point outside generated houses"),V->FindProductionPath(V->Residents[I].Actor->GetActorLocation(),FVector(-1650,-1050,8),ArrivalRoute));
    }
    TestEqual(TEXT("Ten residents have distinct personal narratives"),Stories.Num(),10);
    TestEqual(TEXT("Starter population exercises all six building archetypes"),Archetypes.Num(),6);
    V->Residents[0].InnerStory=TEXT("这是我的持久故事：我想给家人留下一个有树荫的院子。");
    V->EnsureResidentStory(0);
    TestEqual(TEXT("Story refresh preserves authored narrative"),V->Residents[0].InnerStory,FString(TEXT("这是我的持久故事：我想给家人留下一个有树荫的院子。")));
    for(const auto& Site:V->ProductionSites)
        if(Site.Kind==EHearthSiteKind::Empty || Site.Kind==EHearthSiteKind::Tree || Site.Kind==EHearthSiteKind::Carpenter)
            TestFalse(TEXT("Logical ownership does not draw an isolated ground pad"),Site.Soil.IsValid());
    FHearthWorldImage Saved; FString Error;
    TestTrue(TEXT("Generated neighborhood is save valid"),HearthWorld::Decode(V->ExportWorldState(),Saved,Error));
    if(!Error.IsEmpty()) AddInfo(Error);
    for(int32 I=0;I<10;++I)
    {
        TestEqual(TEXT("Facing direction persists"),Saved.PlotYaws[I],V->PlotYaws[I]);TestEqual(TEXT("Saved houses retain exact positions"),Saved.Plots[I],V->PlotPositions[I]);
        TestEqual(TEXT("Persistent story survives serialization exactly"),Saved.People[I].Person.InnerStory,V->Residents[I].InnerStory);
        TestEqual(TEXT("Building archetype survives serialization"),Saved.People[I].Person.BuildingArchetype,V->Residents[I].BuildingArchetype);
    }
    V->Residents[3].Task=EHearthTask::LifeChoosing;V->Residents[3].Route.Reset();
    TestTrue(TEXT("King can reach and approve the reserved royal site"),V->ApprovePublicProject(3));
    TestEqual(TEXT("City selects the phased keep and garden manifest"),V->PublicProject.TemplateId,FString(TEXT("royal_keep_garden_v1")));
    V->Residents[3].Task=EHearthTask::Choosing;
    TestTrue(TEXT("All thirteen royal dependency stages survive canonical validation"),HearthWorld::Decode(V->ExportWorldState(),Saved,Error));
    if(!Error.IsEmpty()) AddInfo(Error);
    V->TownLayoutVersion=0; V->bOrganicTownLayout=false;
    V->RestartVillage();
    TestTrue(TEXT("Explicitly starting a new world upgrades a legacy neighborhood"),V->TownLayoutVersion==2 && V->bOrganicTownLayout);
    for(int32 I=0;I<10;++I) {TArray<FVector> Route; TestTrue(TEXT("Regenerated homes remain reachable after restart"),V->FindProductionPath(FVector(-1650,-1050,8),V->HomeApproach(I),Route));}
    V->FixedObstacles.Reset(); V->ProductionSites.Reset(); V->PublicProject=FHearthPublicProject();
    FHearthSite Vacant; Vacant.Position=FVector(0,0,8); Vacant.Radius=200; Vacant.bExpansion=true; Vacant.Owner=0;
    V->ProductionSites.Add(Vacant);
    const FVector AcrossA(-600,0,8),AcrossB(600,0,8);
    TestTrue(TEXT("Owned vacant land can be crossed"),V->IsClearPoint(Vacant.Position) && V->IsClearSegment(AcrossA,AcrossB));
    V->ProductionSites[0].Kind=EHearthSiteKind::Land;
    TestTrue(TEXT("Cleared land remains walkable before construction"),V->IsClearPoint(Vacant.Position));
    V->ProductionSites[0].BuildPlanId=TEXT("planned-home");
    TestFalse(TEXT("Starting construction reserves a physical footprint"),V->IsClearPoint(Vacant.Position) || V->IsClearSegment(AcrossA,AcrossB));
    V->ProductionSites[0].BuildPlanId.Reset(); V->PublicProject.Id=TEXT("public-wall"); V->PublicProject.Site=0; V->PublicProject.Status=TEXT("building");
    TestFalse(TEXT("A public structure still blocks its occupied ground"),V->IsClearPoint(Vacant.Position) || V->IsClearSegment(AcrossA,AcrossB));
    return true;
}
#endif
