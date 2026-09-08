#include "HearthVillage.h"
#include "HearthResidentDesignChoice.h"
#include "HearthMovement.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FOrganicConstructionPiece ConstructionPiece(const FHearthOrganicPiece& P)
    {
        FOrganicConstructionPiece C; C.Key=P.OriginalKey; C.Module=P.ModuleId;
        if(P.ModuleId.Contains(TEXT("foundation"))) C.Cost.Stone=2;
        else if(P.ModuleId.Contains(TEXT("roof"))) { C.Cost.Beams=1; C.Cost.Tiles=2; }
        else if(P.ModuleId.Contains(TEXT("chimney")) || P.ModuleId.Contains(TEXT("step"))) C.Cost.Stone=2;
        else if(P.ModuleId.Contains(TEXT("wall"))) { C.Cost.Planks=1; C.Cost.Beams=1; }
        else C.Cost.Planks=1;
        return C;
    }
    int32 BuildPhase(const FString& Module)
    {
        if(Module.Contains(TEXT("foundation"))) return 0;
        if(Module.Contains(TEXT("floor"))) return 1;
        if(Module.Contains(TEXT("wall")) || Module.Contains(TEXT("post"))) return 2;
        if(Module.Contains(TEXT("roof"))) return 3;
        return 4;
    }
    TArray<FOrganicConstructionPiece> ConstructionPieces(const FHearthOrganicRecipe& Recipe)
    {
        TArray<FOrganicConstructionPiece> Result;
        for(const auto& P:Recipe.Pieces) Result.Add(ConstructionPiece(P));
        Result.StableSort([](const auto& A,const auto& B){return BuildPhase(A.Module)<BuildPhase(B.Module);});
        return Result;
    }
    FHearthResidentDesignInput DesignInput(const AHearthVillage& V,int32 Index)
    {
        const auto& R=V.Residents[Index]; FHearthResidentDesignInput I;
        I.StableId=R.StableId; I.Name=R.Name; I.Role=R.Role; I.Occupation=R.Role;
        I.Personality=R.Personality; I.PersistentGoals=R.DesignGoal+TEXT(" ")+R.InnerStory;
        I.RelationshipSummary=V.RelationshipSummary(Index); I.HouseholdSize=1;
        for(const auto& Bond:R.Bonds) if(Bond.Value.Affinity>45) ++I.CloseRelationships;
        I.Coins=R.Coins; I.Stone=V.StoneStock; I.Planks=V.PlankStock; I.Beams=V.BeamStock; I.Tiles=V.TileStock;
        I.WorldSeed=V.OrganicWorldSeed;
        return I;
    }
    FString PaletteFor(const FHearthResident& R)
    {
        const TCHAR* Palettes[]={TEXT("warm_lime"),TEXT("ochre_slate"),TEXT("sage_clay")};
        return Palettes[FCrc::StrCrc32(*R.StableId)%3];
    }
    template<typename Callback>
    bool AnyInstalledGroundCell(const FHearthOrganicCatalog& Catalog,const FOrganicConstructionHomeState& Home,Callback Matches)
    {
        // A planned room is not a physical obstacle before its floor exists.
        // Both recipes are searched because retained and newly installed keys
        // can coexist during an extension.
        for(const FString& Id:{Home.CurrentRecipe,Home.TargetRecipe})
        {
            const auto* Recipe=HearthOrganicCatalog::FindRecipe(Catalog,Id); if(!Recipe) continue;
            for(const auto& Piece:Recipe->Pieces)
                if(Piece.ModuleId==TEXT("floor_cell_2m") && Piece.SourceTranslationM.Z<1.f
                    && Home.InstalledKeys.Contains(Piece.OriginalKey) && Matches(Piece.TranslationCm)) return true;
        }
        return false;
    }
}

FTransform AHearthVillage::OrganicHomeTransform(int32 Plot) const
{
    const FRotator Rotation(0,PlotYaws[Plot]+180.f,0);
    const FVector Pivot=OrganicCatalog->SourceToUnrealMatrix.TransformMeters(FVector(2,1,0));
    return FTransform(Rotation,PlotPositions[Plot]-Rotation.RotateVector(Pivot));
}

void AHearthVillage::InitializeOrganicHomes(bool bFresh)
{
    if(!IsOrganicVillage()) return;
    if(!OrganicCatalog.IsValid())
    {
        OrganicCatalog=MakeShared<FHearthOrganicCatalog>(); FString Error;
        if(!HearthOrganicCatalog::Load(*OrganicCatalog,Error))
        { TownLayoutError=TEXT("Organic art catalog: ")+Error; OrganicCatalog.Reset(); bSimulationPaused=true;
          UE_LOG(LogTemp,Error,TEXT("ORGANIC_CATALOG_ERROR %s"),*Error); return; }
    }
    OrganicVisualRevision.Reset(); OrganicNextAttempt.Reset();
    if(bFresh)
    {
        OrganicHomes.Reset();
        for(int32 I=0;I<Residents.Num();++I)
        {
            if(IsSharedServiceResident(I)) continue;
            if(I>=HousingPlotCount()) break;
            auto& R=Residents[I]; R.Plot=I; PlotOwners[I]=I;
            R.DeliveredWood=PlotCosts[I]; WoodStock[I%3]-=R.DeliveredWood;
            R.BuildProgress=1; R.Task=EHearthTask::LifeChoosing; R.Timer=0; R.NextLifeDecision=Elapsed+2+I;
            if(R.bKing) { RefreshStarterArchitecture(I,3); continue; }
            const auto Input=DesignInput(*this,I);
            const auto Alternatives=HearthResidentDesignChoice::RankAlternatives(Input);
            FString RecipeId=TEXT("family_starter"), Reason=TEXT("先住进紧凑的小家，再按需要扩建。");
            // Existing founding homes are explicit starting-world property. Later changes are paid construction.
            if(R.Role==TEXT("商人") || R.Role==TEXT("merchant")) { RecipeId=TEXT("merchant_steps"); Reason=TEXT("临街经营需要店面、仓储和高低相接的生活空间。"); }
            else if(R.Role==TEXT("木匠") || R.Role==TEXT("铁匠") || R.Role==TEXT("陶工") || R.Role==TEXT("carpenter") || R.Role==TEXT("blacksmith") || R.Role==TEXT("potter"))
            { RecipeId=TEXT("carpenter_court"); Reason=TEXT("手艺人的家围绕工作院落和材料棚展开。"); }
            else if(!Alternatives.IsEmpty() && Alternatives[0].Family==TEXT("family_cluster") && (R.Personality.Contains(TEXT("邻居")) || R.Personality.Contains(TEXT("友情"))))
            { RecipeId=TEXT("family_side_wing"); Reason=TEXT("希望侧院能接待朋友，保留向后扩建的余地。"); }
            const auto* Recipe=HearthOrganicCatalog::FindRecipe(*OrganicCatalog,RecipeId);
            if(!Recipe) continue;
            FOrganicConstructionHomeState Home;
            Home.CurrentRecipe=RecipeId; Home.TargetRecipe=RecipeId; Home.ChoiceReason=Reason;
            Home.Source=TEXT("founding_estate_local_rules"); Home.Seed=FString::FromInt(HearthResidentDesignChoice::StableSeed(Input));
            for(const auto& Piece:ConstructionPieces(*Recipe)) { Home.InstalledKeys.Add(Piece.Key); Home.InstalledCosts.Add(Piece.Key,Piece.Cost); }
            OrganicHomes.Add(R.StableId,Home); OrganicNextAttempt.Add(R.StableId,Elapsed+12+I*2);
            R.DesignFeedback=Reason; R.LatestEvent=TEXT("这所现有住宅属于我，扩建要用自己的收入和材料。");
        }
    }
    RefreshOrganicHomes();
}

void AHearthVillage::RefreshOrganicHome(int32 Index)
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid() || !Residents.IsValidIndex(Index)) return;
    const auto& R=Residents[Index]; const auto* Home=OrganicHomes.Find(R.StableId);
    if(!Home || R.Plot<0) return;
    auto& Meshes=StarterArchitectureMeshes[R.Plot];
    for(auto& Mesh:Meshes) if(Mesh.IsValid()) Mesh->DestroyComponent();
    Meshes.Reset();
    const FTransform House=OrganicHomeTransform(R.Plot);
    const FString Palette=PaletteFor(R);
    TSet<FString> Rendered;
    // Both recipes are needed while a wing is only partly built or being dismantled.
    for(const FString& Id:{Home->CurrentRecipe,Home->TargetRecipe})
    {
        const auto* Recipe=HearthOrganicCatalog::FindRecipe(*OrganicCatalog,Id); if(!Recipe) continue;
        for(const auto& Piece:Recipe->Pieces)
        {
            if(!Home->InstalledKeys.Contains(Piece.OriginalKey) || Rendered.Contains(Piece.OriginalKey)) continue;
            Rendered.Add(Piece.OriginalKey);
            for(const auto& Layer:Piece.Layers)
            {
                const FString Path=HearthOrganicCatalog::ResolveLayerPath(*OrganicCatalog,Piece.ModuleId,Palette,Layer);
                if(Path.IsEmpty()) continue;
                if(auto* Mesh=AddMesh(Path,House.TransformPosition(Piece.TranslationCm),FVector(1)))
                {
                    Mesh->SetWorldRotation(House.GetRotation()*FQuat(FRotator(0,Piece.UnrealYawDegrees,0)));
                    Mesh->ComponentTags.Add(*FString(TEXT("OrganicPiece:")+Piece.OriginalKey));
                    Meshes.Add(Mesh);
                }
            }
        }
    }
    if(HouseMeshes.IsValidIndex(R.Plot)) HouseMeshes[R.Plot]->SetVisibility(false);
    OrganicVisualRevision.Add(R.StableId,Home->Revision);
}

void AHearthVillage::RefreshOrganicHomes()
{
    if(!IsOrganicVillage()) return;
    if(!OrganicCatalog.IsValid()) { InitializeOrganicHomes(false); return; }
    for(int32 I=0;I<Residents.Num();++I) { RefreshOrganicHome(I); RefreshMarketLifeKitHome(I); }
}

bool AHearthVillage::RequestOrganicExpansion(int32 Index,const FString& RecipeId)
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid() || !Residents.IsValidIndex(Index)) return false;
    auto* Home=OrganicHomes.Find(Residents[Index].StableId);
    if(!Home || !Home->ActivePieceKey.IsEmpty() || Home->TargetRecipe!=Home->CurrentRecipe) return false;
    // Approved shell-preserving transitions; arbitrary cross-family replacements need a relocation plan.
    const auto* Growth=HearthOrganicCatalog::FindGrowth(*OrganicCatalog);
    bool Allowed=false;
    if(Growth) for(const auto& T:Growth->Transitions) if(T.From==Home->CurrentRecipe && T.To==RecipeId) Allowed=true;
    if(!Allowed || !HearthOrganicCatalog::FindRecipe(*OrganicCatalog,RecipeId)) return false;
    Home->TargetRecipe=RecipeId; Home->Source=TEXT("user_request");
    Home->ChoiceReason=TEXT("按提出的住宅扩建目标，逐件采购、回收和搭建。"); ++Home->Revision;
    OrganicNextAttempt.Add(Residents[Index].StableId,Elapsed); return true;
}

void AHearthVillage::AdvanceOrganicHomes(float Dt)
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid() || !bAutonomousLifeEnabled) return;
    AdvanceMarketLifeKits(Dt);
    for(int32 I=0;I<Residents.Num();++I)
    {
        auto& R=Residents[I]; auto* H=OrganicHomes.Find(R.StableId);
        if(!H || R.Task!=EHearthTask::LifeChoosing || !R.Route.IsEmpty() || IsDecisionPending(I) || !R.ConversationId.IsEmpty()
            || R.Hunger>65 || R.Energy<25 || R.SocialNeed>80 || OrganicNextAttempt.FindRef(R.StableId)>Elapsed) continue;
        OrganicNextAttempt.Add(R.StableId,Elapsed+18);
        if(H->TargetRecipe==H->CurrentRecipe)
        {
            int32 Visits=0,CloseFriends=0;
            for(const auto& Bond:R.Bonds)
            { Visits+=Bond.Value.Meetings; CloseFriends+=Bond.Value.Affinity>=35 && Bond.Value.Trust>=40; }
            const bool Compact=R.Personality.Contains(TEXT("节俭")) || R.Personality.Contains(TEXT("够住"));
            const bool Hosts=R.Personality.Contains(TEXT("邻居")) || R.Personality.Contains(TEXT("友情")) || R.DesignGoal.Contains(TEXT("招待"));
            // Unfulfilled "future family" prose is not a present household.
            // A contented frugal owner can keep a small home indefinitely;
            // larger social courts require actual visits and a cash reserve.
            const bool NeedSpace=H->CurrentRecipe==TEXT("family_starter")
                ? R.Coins>=18 && ((Compact && CloseFriends>=2) || (!Compact && ((Hosts && Visits>=1)
                    || CloseFriends>=1 || (R.Personality.Contains(TEXT("建自己的家")) && R.Coins>=24))))
                : R.Coins>=24 && ((Hosts && Visits>=3) || CloseFriends>=2);
            if(!NeedSpace) continue;
            const FString Target=H->CurrentRecipe==TEXT("family_starter")?TEXT("family_side_wing"):
                H->CurrentRecipe==TEXT("family_side_wing")?TEXT("family_cluster"):TEXT("");
            if(Target.IsEmpty() || !RequestOrganicExpansion(I,Target)) continue;
            H->Source=TEXT("local_rules");
            H->ChoiceReason=Visits>0?FString::Printf(TEXT("已经有 %d 次邻里交往，留足生活钱后增加能相聚的空间。"),Visits)
                :TEXT("用积攒的收入扩建一间侧翼，保留原来的小家和生活储备。");
            R.DesignFeedback=H->ChoiceReason;
        }
        const auto* Target=HearthOrganicCatalog::FindRecipe(*OrganicCatalog,H->TargetRecipe); if(!Target) continue;
        FOrganicConstructionDelta Delta;
        if(!HearthOrganicConstruction::PlanDelta(*H,ConstructionPieces(*Target),Delta)) continue;
        if(Delta.RemoveKeys.IsEmpty() && Delta.AddPieces.IsEmpty())
        { H->CurrentRecipe=H->TargetRecipe; H->ActivePieceKey.Empty(); H->WorkProgress=0; ++H->Revision; R.LatestEvent=TEXT("扩建完成，原有构件和新的房间连成了一个家。"); continue; }
        FString Next;
        if(!Delta.RemoveKeys.IsEmpty()) Next=Delta.RemoveKeys[0];
        else
        {
            // Dry run validates the exact atomic transaction without reserving or spending anything.
            const auto& Piece=Delta.AddPieces[0]; auto Trial=*H;
            FOrganicConstructionStock Stock{StoneStock,PlankStock,BeamStock,TileStock}; int32 Wallet=R.Coins,Treasury=TreasuryCoins;
            FOrganicConstructionInstallResult Check;
            if(!HearthOrganicConstruction::InstallAtomic(Piece,Trial,Stock,Wallet,Treasury,Check)
                || (Check.CoinsPaid>0 && Wallet<4))
            { R.DesignFeedback=H->ChoiceReason+TEXT(" 当前需要先生产材料或赚取工钱。"); continue; }
            Next=Piece.Key;
        }
        TArray<FVector> Route;
        if(!FindActivityRoute(I,HomeApproach(R.Plot),Route)) continue;
        if(H->ActivePieceKey!=Next) H->WorkProgress=0;
        H->ActivePieceKey=Next; R.Route=MoveTemp(Route); R.Task=EHearthTask::OrganicTravel;
        R.ActiveTaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        R.ProductionSite=-1; R.ProductionOp=-1; R.LifeAction=-1;
        R.LatestEvent=TEXT("回到自己家，安装或调整一个建筑构件。");
    }
}

void AHearthVillage::AdvanceOrganicWorker(int32 Index,float Dt)
{
    if(AdvanceMarketKitWorker(Index,Dt)) return;
    auto& R=Residents[Index]; auto* H=OrganicHomes.Find(R.StableId);
    if(!H || !OrganicCatalog.IsValid()) {R.Task=EHearthTask::LifeChoosing;return;}
    if(R.Task==EHearthTask::OrganicTravel) { if(MoveResident(Index,Dt)) R.Task=EHearthTask::OrganicWork; return; }
    H->WorkProgress=FMath::Min(1.f,H->WorkProgress+Dt/5.f);
    if(H->WorkProgress<1) return;
    const auto* Target=HearthOrganicCatalog::FindRecipe(*OrganicCatalog,H->TargetRecipe);
    const auto* Piece=Target?Target->Pieces.FindByPredicate([&](const auto& P){return P.OriginalKey==H->ActivePieceKey;}):nullptr;
    bool Done=false;
    if(!Piece)
    {
        FOrganicConstructionPiece Remove; Remove.Key=H->ActivePieceKey; Remove.Module=TEXT("reclaimed_component");
        FOrganicConstructionDismantleResult Result; Done=HearthOrganicConstruction::Dismantle(Remove,*H,Result);
    }
    else
    {
        FOrganicConstructionStock Stock{StoneStock,PlankStock,BeamStock,TileStock}; FOrganicConstructionInstallResult Result;
        auto Trial=*H; int32 Wallet=R.Coins,Treasury=TreasuryCoins;
        Done=HearthOrganicConstruction::InstallAtomic(ConstructionPiece(*Piece),Trial,Stock,Wallet,Treasury,Result)
            && (Result.CoinsPaid==0 || Wallet>=4);
        if(Done)
        {
            *H=MoveTemp(Trial); R.Coins=Wallet; TreasuryCoins=Treasury;
            StoneStock=Stock.Stone; PlankStock=Stock.Planks; BeamStock=Stock.Beams; TileStock=Stock.Tiles;
            Spent[2]+=Result.PublicConsumed.Stone; ManufacturedSpent[0]+=Result.PublicConsumed.Planks;
            ManufacturedSpent[1]+=Result.PublicConsumed.Beams; SpentTiles+=Result.PublicConsumed.Tiles;
            if(Result.CoinsPaid>0)
            {
                FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=TEXT("organic_material_purchase");
                T.TaskId=R.ActiveTaskId; T.Item=Piece->ModuleId; T.From=Index; T.To=-1; T.Amount=Result.CoinsPaid; T.Quantity=Result.CoinsPaid; T.At=Elapsed;
                Transactions.Add(T);
            }
        }
    }
    H->ActivePieceKey.Empty(); H->WorkProgress=0; R.ActiveTaskId.Empty(); R.Task=EHearthTask::LifeChoosing;
    R.NextLifeDecision=Elapsed+1; OrganicNextAttempt.Add(R.StableId,Elapsed+15);
    R.LatestEvent=Done?TEXT("完成一个构件；继续生活、挣钱，再进行下一步扩建。"):TEXT("材料或钱不够，这次没有扣款，先去工作。");
    if(Done) RefreshOrganicHome(Index);
}

bool AHearthVillage::OrganicBlocksPoint(const FVector& Position) const
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid()) return false;
    if(IsMarketLifeKitBlockingPoint(Position)) return true;
    for(const auto& R:Residents)
    {
        if(R.Plot<0 || FVector::DistSquared2D(Position,PlotPositions[R.Plot])>1400.f*1400.f) continue;
        if(R.bKing)
        { const FVector Local=FRotator(0,PlotYaws[R.Plot],0).UnrotateVector(Position-PlotPositions[R.Plot]);
          if(FMath::Abs(Local.X)<360 && FMath::Abs(Local.Y)<360) return true; }
        const auto* H=OrganicHomes.Find(R.StableId); if(!H || R.Plot<0) continue;
        const FVector Local=OrganicHomeTransform(R.Plot).InverseTransformPosition(Position);
        if(AnyInstalledGroundCell(*OrganicCatalog,*H,[&](const FVector& Center)
            {return FMath::Abs(Local.X-Center.X)<138 && FMath::Abs(Local.Y-Center.Y)<138;})) return true;
    }
    return false;
}

bool AHearthVillage::OrganicBlocksSegment(const FVector& A,const FVector& B) const
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid()) return false;
    if(IsMarketLifeKitBlockingSegment(A,B)) return true;
    const auto Hits=[](const FVector& Start,const FVector& End,const FVector& Center,float Radius)
    {
        if(!HearthMovement::SegmentHitsBox(Start,End,Center,Radius)) return false;
        const FVector2D From(Start.X-Center.X,Start.Y-Center.Y),Delta(End.X-Start.X,End.Y-Start.Y);
        const bool Inside=FMath::Abs(From.X)<Radius && FMath::Abs(From.Y)<Radius;
        // The same outward-only rule as production obstacles: no teleport,
        // no entry through a wall and no shortcut through another cell.
        return !(Inside && !Delta.IsNearlyZero() && FVector2D::DotProduct(From,Delta)>=0.f);
    };
    for(const auto& R:Residents)
    {
        if(R.Plot<0) continue;
        if(R.bKing)
        {
            const FTransform T(FRotator(0,PlotYaws[R.Plot],0),PlotPositions[R.Plot]);
            if(Hits(T.InverseTransformPosition(A),T.InverseTransformPosition(B),FVector::ZeroVector,360)) return true;
        }
        const auto* H=OrganicHomes.Find(R.StableId); if(!H) continue;
        const auto T=OrganicHomeTransform(R.Plot);
        const FVector From=T.InverseTransformPosition(A),To=T.InverseTransformPosition(B);
        if(AnyInstalledGroundCell(*OrganicCatalog,*H,[&](const FVector& Center){return Hits(From,To,Center,138);})) return true;
    }
    return false;
}

FString AHearthVillage::ExportOrganicState() const
{
    auto Root=MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("organic_village"),IsOrganicVillage());
    Root->SetNumberField(TEXT("seed"),OrganicWorldSeed); Root->SetNumberField(TEXT("population"),Residents.Num());
    Root->SetNumberField(TEXT("api_requests"),ApiRequests); Root->SetStringField(TEXT("error"),TownLayoutError);
    TArray<TSharedPtr<FJsonValue>> Homes;
    for(const auto& R:Residents) if(const auto* H=OrganicHomes.Find(R.StableId))
    {
        auto J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("resident"),R.Name); J->SetStringField(TEXT("resident_id"),R.StableId);
        J->SetStringField(TEXT("recipe"),H->CurrentRecipe); J->SetStringField(TEXT("target"),H->TargetRecipe);
        J->SetStringField(TEXT("reason"),H->ChoiceReason); J->SetStringField(TEXT("source"),H->Source);
        J->SetStringField(TEXT("palette"),PaletteFor(R)); J->SetNumberField(TEXT("installed"),H->InstalledKeys.Num());
        J->SetStringField(TEXT("active_piece"),H->ActivePieceKey); J->SetNumberField(TEXT("progress"),H->WorkProgress);
        J->SetNumberField(TEXT("visible_layers"),R.Plot>=0?StarterArchitectureMeshes[R.Plot].Num():0);
        if(IsValid(R.Actor)) J->SetNumberField(TEXT("feet_error_cm"),R.Actor->GetActorLocation().Z-GroundHeightAt(R.Actor->GetActorLocation())-5.2);
        Homes.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("homes"),Homes); FString Text; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)); return Text;
}
