#if WITH_DEV_AUTOMATION_TESTS

#include "HearthVillage.h"
#include "HearthWorldState.h"
#include "HearthOrganicConstruction.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

namespace
{
    UWorld* MakeOrganicRuntimeWorld()
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthOrganicRuntimeTest,
    "ThreeHearths.Organic.RuntimeInitializationAndColdRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHearthOrganicRuntimeTest::RunTest(const FString&)
{
    const FString PreviousCommandLine=FCommandLine::Get();
    FCommandLine::Set(*(PreviousCommandLine.Replace(TEXT("-HearthCityV3"),TEXT(""))
        +TEXT(" -HearthOrganicVillage -HearthDisableApi -HearthNoWorldPersistence")));
    ON_SCOPE_EXIT { FCommandLine::Set(*PreviousCommandLine); };

    UWorld* World=MakeOrganicRuntimeWorld();
    if(!TestNotNull(TEXT("isolated organic runtime world"),World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    auto* Base=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-47),FRotator::ZeroRotator);
    Base->Tags.Add(TEXT("ThreeHearthsBaseTerrain"));
    Base->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Base->GetStaticMeshComponent()->SetWorldScale3D(FVector(140,140,1));
    auto* Village=World->SpawnActor<AHearthVillage>();
    Village->bUseCropoutMap=true;
    Village->BuildEnvironment();
    Village->ResetVillageState();
    const auto AddPublicMaterial=[&](const FOrganicConstructionStock& S)
    {
        Village->StoneStock+=S.Stone; Village->Produced[2]+=S.Stone;
        Village->PlankStock+=S.Planks; Village->Manufactured[0]+=S.Planks;
        Village->BeamStock+=S.Beams; Village->Manufactured[1]+=S.Beams;
        Village->TileStock+=S.Tiles; Village->ProducedTiles+=S.Tiles;
    };

    TestTrue(TEXT("organic layout is active"),Village->IsOrganicVillage());
    TestEqual(TEXT("v4 starts thirteen residents"),Village->Residents.Num(),13);
    TestEqual(TEXT("nine residents have authored homes"),Village->OrganicHomes.Num(),9);
    TestNotNull(TEXT("organic catalog loaded"),Village->OrganicCatalog.Get());
    if(!Village->OrganicCatalog.IsValid()) return false;
    TestTrue(TEXT("catalog has at least three recipes"),Village->OrganicCatalog->Recipes.Num()>=3);
    if(!TestTrue(TEXT("organic terrain grid generated"),Village->OrganicTerrainGrid.IsValid())) return false;
    FHearthWorldImage InitialImage; FString InitialError;
    TestTrue(TEXT("initial organic world export decodes"),HearthWorld::Decode(Village->ExportWorldState(),InitialImage,InitialError));
    TestEqual(TEXT("initial export has nine homes"),InitialImage.OrganicHomes.Num(),9);
    TestEqual(TEXT("initial organic treasury is 500"),InitialImage.TreasuryCoins,500);
    if(!TestTrue(TEXT("initial export contains residents"),InitialImage.People.Num()>0)) return false;
    TestEqual(TEXT("initial organic wallet is 12"),InitialImage.People[0].Person.Coins,12);

    int32 MinZ=MAX_int32,MaxZ=MIN_int32;
    for(const FVector& Vertex:Village->OrganicTerrainGrid->Vertices)
    { MinZ=FMath::Min(MinZ,FMath::RoundToInt(Vertex.Z)); MaxZ=FMath::Max(MaxZ,FMath::RoundToInt(Vertex.Z)); }
    TestTrue(TEXT("terrain has nonzero relief"),MaxZ>MinZ);
    TArray<UActorComponent*> GroundComponents;
    if(Village->OrganicGroundActor.IsValid()) Village->OrganicGroundActor->GetComponents(GroundComponents);
    int32 MeshCount=0; for(UActorComponent* C:GroundComponents) if(C && C->IsA<UPrimitiveComponent>()) ++MeshCount;
    TestTrue(TEXT("organic ground has native collision mesh"),MeshCount>0);
    const FVector Sample=FVector(0,0,Village->OrganicGroundHeightAt(FVector::ZeroVector));
    FHitResult Hit; const bool bHit=World->LineTraceSingleByChannel(Hit,Sample+FVector(0,0,500),Sample-FVector(0,0,500),ECC_WorldStatic);
    TestTrue(TEXT("line trace reaches organic ground"),bHit);
    if(bHit) TestTrue(TEXT("trace height follows generated terrain"),FMath::Abs(Hit.ImpactPoint.Z-Sample.Z)<3.f);

    int32 Index=INDEX_NONE;
    for(int32 I=0;I<Village->Residents.Num();++I)
    {
        const auto* H=Village->OrganicHomes.Find(Village->Residents[I].StableId);
        if(H && H->CurrentRecipe==TEXT("family_starter")) { Index=I; break; }
    }
    if(!TestTrue(TEXT("a resident starts in family_starter"),Index!=INDEX_NONE)) return false;
    auto& Resident=Village->Residents[Index];
    auto* Home=Village->OrganicHomes.Find(Resident.StableId);
    const int32 InitialInstalled=Home->InstalledKeys.Num();
    TestTrue(TEXT("home has recorded original costs"),Home->InstalledCosts.Num()==InitialInstalled);
    TestTrue(TEXT("home approach has resident foot height"),FMath::Abs(Village->HomeApproach(Resident.Plot).Z-(Village->OrganicGroundHeightAt(Village->HomeApproach(Resident.Plot))+5.2f))<3.f);
    Resident.Actor->SetActorLocation(Village->HomeApproach(Resident.Plot));
    const FString OriginalPersonality=Resident.Personality,OriginalGoal=Resident.DesignGoal;
    const int32 OriginalCoins=Resident.Coins;
    Resident.Personality=TEXT("节俭石匠 · 够住就好"); Resident.Coins=40;
    Resident.DesignGoal=TEXT("将来有家人是我的愿望，目前尚未发生。");
    Village->OrganicNextAttempt.Add(Resident.StableId,Village->Elapsed);
    Village->AdvanceOrganicHomes(0.f);
    TestEqual(TEXT("future-family prose alone does not force a contented resident to expand"),Home->TargetRecipe,Home->CurrentRecipe);
    Resident.Personality=OriginalPersonality; Resident.DesignGoal=OriginalGoal; Resident.Coins=OriginalCoins;
    TestTrue(TEXT("side wing request accepted"),Village->RequestOrganicExpansion(Index,TEXT("family_side_wing")));

    const auto* Wing=HearthOrganicCatalog::FindRecipe(*Village->OrganicCatalog,TEXT("family_side_wing"));
    const auto* NewFloor=Wing->Pieces.FindByPredicate([&](const auto& P)
        {return P.ModuleId==TEXT("floor_cell_2m") && P.SourceTranslationM.Z<1.f && !Home->InstalledKeys.Contains(P.OriginalKey);});
    if(TestNotNull(TEXT("extension has a newly planned ground floor"),NewFloor))
    {
        const FTransform House=Village->OrganicHomeTransform(Resident.Plot);
        const FVector Center=House.TransformPosition(NewFloor->TranslationCm);
        TestFalse(TEXT("an unbuilt target room does not block movement"),Village->OrganicBlocksPoint(Center));
        Home->InstalledKeys.Add(NewFloor->OriginalKey);
        TestTrue(TEXT("the same floor blocks after actual installation"),Village->OrganicBlocksPoint(Center));
        const FVector Inside=House.TransformPosition(NewFloor->TranslationCm+FVector(125,0,0));
        const FVector Outside=House.TransformPosition(NewFloor->TranslationCm+FVector(155,0,0));
        TestFalse(TEXT("a passer-by overlapping a new floor can walk outward"),Village->OrganicBlocksSegment(Inside,Outside));
        TestTrue(TEXT("a passer-by cannot walk inward through that floor"),Village->OrganicBlocksSegment(Outside,Inside));
        Home->InstalledKeys.Remove(NewFloor->OriginalKey);
    }

    // Reproduce an approach that became slightly covered when an empty plot
    // turned into a larger crop field. Recovery must be a real walked route.
    FHearthSite RecoveryField; RecoveryField.Kind=EHearthSiteKind::Corn;
    RecoveryField.Position=FVector(-6500,5000,8); RecoveryField.Radius=260;
    const int32 RecoveryIndex=Village->ProductionSites.Add(RecoveryField);
    const FVector Covered=RecoveryField.Position+FVector(280,0,0),Clear=RecoveryField.Position+FVector(600,0,0);
    TArray<FVector> Escape;
    TestFalse(TEXT("field growth covered its old worker approach"),Village->IsClearPoint(Covered));
    TestTrue(TEXT("worker can route outward after the field grows"),Village->FindProductionPath(Covered,Clear,Escape));
    Village->ProductionSites.RemoveAt(RecoveryIndex);

    FHearthSite DoorTree; DoorTree.Kind=EHearthSiteKind::Tree;
    DoorTree.Position=Village->HomeApproach(Resident.Plot); DoorTree.Radius=160;
    const int32 DoorTreeIndex=Village->ProductionSites.Add(DoorTree);
    const FVector RecoverableDoor=Village->HomeApproach(Resident.Plot);
    TestTrue(TEXT("an old tree at the door leaves a usable exterior home approach"),Village->IsClearPoint(RecoverableDoor));
    TestTrue(TEXT("the alternate home approach stays close to the recorded entrance"),FVector::Dist2D(RecoverableDoor,Village->PlotEntrances[Resident.Plot])<=600.f);
    Village->ProductionSites.RemoveAt(DoorTreeIndex);

    // Drive all three authored removals.  A later add may be free from this
    // home's private reclaimed stock, so keep advancing until public stock is
    // genuinely required and the dry run refuses to start it.
    Village->StoneStock=Village->PlankStock=Village->BeamStock=Village->TileStock=0;
    bool bObservedShortage=false;
    for(int32 Step=0;Step<8;++Step)
    {
        Home=Village->OrganicHomes.Find(Resident.StableId);
        if(Home->ActivePieceKey.IsEmpty())
        {
            Village->OrganicNextAttempt.Add(Resident.StableId,Village->Elapsed);
            Village->AdvanceOrganicHomes(0.f);
            if(Home->ActivePieceKey.IsEmpty() && Home->TargetRecipe==TEXT("family_side_wing")) bObservedShortage=true;
        }
        if(Home->ActivePieceKey.IsEmpty()) break;
        Resident.Actor->SetActorLocation(Village->HomeApproach(Resident.Plot));
        Village->AdvanceOrganicWorker(Index,100.f);
        if(Resident.Task==EHearthTask::OrganicWork) Village->AdvanceOrganicWorker(Index,5.f);
    }
    Home=Village->OrganicHomes.Find(Resident.StableId);
    TestTrue(TEXT("material shortage was observed before funding"),bObservedShortage);
    TestTrue(TEXT("material shortage leaves pending piece unstarted"),Home->ActivePieceKey.IsEmpty());

    // Give the next pending piece a bounded public budget.  The runtime still
    // computes the exact module cost and charges only what it consumes.
    FOrganicConstructionStock Needed; Needed.Stone=8; Needed.Planks=8; Needed.Beams=8; Needed.Tiles=8;
    AddPublicMaterial(Needed);
    const int32 BeforeReserveCheck=Resident.Coins;
    Resident.Coins=4;
    Village->OrganicNextAttempt.Add(Resident.StableId,Village->Elapsed);
    Village->AdvanceOrganicHomes(0.f);
    TestTrue(TEXT("construction leaves the last four food coins available"),Home->ActivePieceKey.IsEmpty());
    TestEqual(TEXT("a rejected purchase preserves the survival wallet"),Resident.Coins,4);
    Resident.Coins=BeforeReserveCheck;
    Village->OrganicNextAttempt.Add(Resident.StableId,Village->Elapsed);
    Village->AdvanceOrganicHomes(0.f);
    TestTrue(TEXT("funded delta enters organic travel"),!Home->ActivePieceKey.IsEmpty());
    TestTrue(TEXT("active construction is safe to show in the live snapshot"),!Village->GetSnapshot().IsEmpty());
    const FString InstalledKey=Home->ActivePieceKey;
    const int32 BeforeInstalled=Home->InstalledKeys.Num();
    const int32 BeforeCoins=Resident.Coins,BeforeTreasury=Village->TreasuryCoins;
    const int32 BeforeTransactions=Village->Transactions.Num();
    Resident.Actor->SetActorLocation(Village->HomeApproach(Resident.Plot));
    Village->AdvanceOrganicWorker(Index,100.f);
    if(Resident.Task==EHearthTask::OrganicWork) Village->AdvanceOrganicWorker(Index,5.f);
    TestEqual(TEXT("one install changes exactly one key"),Home->InstalledKeys.Num(),BeforeInstalled+1);
    TestTrue(TEXT("completed worker clears active key"),Home->ActivePieceKey.IsEmpty());
    TestTrue(TEXT("atomic purchase records a transaction"),Village->Transactions.Num()==BeforeTransactions+1);
    if(Village->Transactions.Num()>BeforeTransactions)
    {
        const auto& T=Village->Transactions.Last();
        TestEqual(TEXT("organic transaction kind"),T.Kind,FString(TEXT("organic_material_purchase")));
        TestEqual(TEXT("resident coin debit equals public payment"),BeforeCoins-Resident.Coins,T.Amount);
        TestEqual(TEXT("treasury receives public payment"),Village->TreasuryCoins-BeforeTreasury,T.Amount);
        TestEqual(TEXT("transaction quantity equals amount"),T.Quantity,T.Amount);
    }

    const FString Payload=Village->ExportWorldState(); FHearthWorldImage Image; FString Error;
    TestTrue(TEXT("organic world export decodes"),HearthWorld::Decode(Payload,Image,Error));
    TestEqual(TEXT("v4 world seed is stable"),Image.OrganicWorldSeed,7919);
    TestEqual(TEXT("cold export contains resident homes"),Image.OrganicHomes.Num(),Village->OrganicHomes.Num());
    TestTrue(TEXT("cold apply succeeds"),Village->ApplyWorldState(Payload,Error));
    const auto* ColdHome=Village->OrganicHomes.Find(Resident.StableId);
    TestNotNull(TEXT("cold apply restores home"),ColdHome);
    if(ColdHome) TestTrue(TEXT("cold apply preserves installed delta"),ColdHome->InstalledKeys.Contains(InstalledKey));
    return true;
}

#endif
