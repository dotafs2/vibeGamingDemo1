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

    // Reproduce the stalled floor's ownership and cash boundary, while keeping
    // a real organic home beam shortage competing with the public request.
    Village->PublicProject=FHearthPublicProject();
    Village->PublicProject.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Village->PublicProject.TemplateId=TEXT("royal_keep_garden_v2"); Village->PublicProject.Status=TEXT("building");
    FHearthPublicPart Floor; Floor.Stage=2; Floor.Required[1]=5;
    FHearthPublicPart Upper; Upper.Stage=3; Upper.Required[2]=10000;
    Village->PublicProject.Parts={Floor,Floor,Floor,Upper}; Village->PublicProject.Stock[1]=1;
    Village->PlankStock=115; Village->BeamStock=671;
    for(int32 I=0;I<Village->Residents.Num();++I)
    {
        Village->ReturnTool(I); Village->Residents[I].PersonalPlanks=0; Village->Residents[I].ProductionOp=-1;
    }
    TestEqual(TEXT("Current floor private plank shortage wins over all future castle beams"),
        Village->ChooseProductionLocally(ResidentIndex,Options),113);
    Village->BeamStock=0;
    TestEqual(TEXT("Current castle floor supply also wins over organic home beam backlog"),
        Village->ChooseProductionLocally(ResidentIndex,Options),113);
    Resident.PersonalPlanks=14;
    TestEqual(TEXT("Enough owned planks removes the castle milling priority"),Village->ChooseProductionLocally(ResidentIndex,Options),114);
    Resident.PersonalPlanks=0;
    Village->PublicProject.Parts={Floor,Upper}; Village->PublicProject.Stock[1]=4;
    Resident.ProductionOp=13; Resident.Task=EHearthTask::ProductionWork;
    TestEqual(TEXT("One unfinished sawmill job already covers one private share"),Village->ChooseProductionLocally(ResidentIndex,Options),114);
    Resident.Task=EHearthTask::ProductionDeliver; Resident.PersonalPlanks=1;
    Village->PublicProject.Stock[1]=3;
    TestEqual(TEXT("Delivery cargo cannot count the already earned private share twice"),Village->ChooseProductionLocally(ResidentIndex,Options),113);
    Resident.Task=EHearthTask::LifeChoosing; Resident.ProductionOp=-1; Resident.PersonalPlanks=0; Resident.BuildProgress=1.f;
    Village->PublicProject.Stock[1]=1; Village->PublicProject.Parts={Floor,Floor,Floor,Upper};
    Village->WoodStock[0]=40; Village->TreasuryCoins=29; Village->TaxProjectCoins=29;
    TestEqual(TEXT("The reproduced treasury has no ordinary wage capital"),Village->GeneralFunds(),0);
    TestTrue(TEXT("Protected funds can reserve actual current-floor milling labor"),Village->IsProductionAllowed(ResidentIndex,113));
    Village->TreasuryCoins=21; Village->TaxProjectCoins=21;
    TestTrue(TEXT("Unstarted future milling jobs do not block one funded job"),Village->IsProductionAllowed(ResidentIndex,113));
    Village->TreasuryCoins=6; Village->TaxProjectCoins=6;
    TestFalse(TEXT("Milling must retain its purchase price and installation wage"),Village->IsProductionAllowed(ResidentIndex,113));
    Village->TreasuryCoins=7; Village->TaxProjectCoins=7;
    TestTrue(TEXT("Exactly one milling wage, purchase and installation is enough"),Village->IsProductionAllowed(ResidentIndex,113));
    Village->TreasuryCoins=29; Village->TaxProjectCoins=29;
    Village->PublicProject.Parts[0].Required[1]=0;
    TestFalse(TEXT("Later floors alone cannot claim protected milling wages ahead of the next part"),Village->IsProductionAllowed(ResidentIndex,113));

    return true;
}

#endif
