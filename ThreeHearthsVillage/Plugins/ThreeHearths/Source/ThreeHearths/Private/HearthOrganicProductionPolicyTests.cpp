#if WITH_DEV_AUTOMATION_TESTS

#include "HearthVillage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicProductionPolicyTest,
    "ThreeHearths.Production.OrganicShortagePolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthOrganicProductionPolicyTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))
        +TEXT(" -HearthOrganicVillage -HearthDisableApi -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };

    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    if(!TestNotNull(TEXT("organic production policy world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Ground=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Ground->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Ground->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>(); Village->bUseCropoutMap=true;
    Village->BuildEnvironment(); Village->ResetVillageState(); Village->bAutonomousLifeEnabled=false;
    if(!TestTrue(TEXT("organic v4 policy fixture is active"),Village->IsOrganicVillage())) return false;
    int32 EmptyExpansionSites=0;
    for(const FHearthSite& Site:Village->ProductionSites)
        if(Site.bExpansion && Site.Kind==EHearthSiteKind::Empty) ++EmptyExpansionSites;
    TestTrue(TEXT("organic initialization has no duplicate empty home plots"),EmptyExpansionSites<=1);

    int32 ResidentIndex=INDEX_NONE;
    for(int32 I=0;I<Village->Residents.Num();++I)
    {
        const auto* Home=Village->OrganicHomes.Find(Village->Residents[I].StableId);
        if(Home && Home->CurrentRecipe==TEXT("family_starter")) { ResidentIndex=I; break; }
    }
    if(!TestTrue(TEXT("policy fixture has a family starter home"),ResidentIndex!=INDEX_NONE)) return false;
    auto& Resident=Village->Residents[ResidentIndex]; Resident.Task=EHearthTask::LifeChoosing; Resident.Hunger=0; Resident.Energy=80; Resident.SocialNeed=0;
    Resident.Actor->SetActorLocation(FVector(0,0,8));
    auto* Home=Village->OrganicHomes.Find(Resident.StableId);
    Home->TargetRecipe=TEXT("family_side_wing");

    Village->ProductionSites.Reset();
    FHearthSite Carpenter; Carpenter.Kind=EHearthSiteKind::Carpenter; Carpenter.Position=FVector(300,0,8);
    Carpenter.Approach=Resident.Actor->GetActorLocation(); Carpenter.Radius=190; Carpenter.bReachable=true; Carpenter.ReservedBy=-1;
    Village->ProductionSites.Add(Carpenter);
    Village->FoodStock=100; Village->StoneStock=32; Village->PlankStock=252; Village->BeamStock=0; Village->TileStock=24;
    const TArray<int32> Options={113,114};
    TestEqual(TEXT("organic pending shortage prioritizes beam production over already abundant planks"),
        Village->ChooseProductionLocally(ResidentIndex,Options),114);

    return true;
}

#endif
