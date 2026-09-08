#include "HearthVillage.h"
#include "HearthOrganicConstruction.h"
#include "HearthTownLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr float KitWorkSeconds=5.f;
    constexpr float KitRetrySeconds=30.f;

    bool ContainsAny(const FString& Text,const TArray<FString>& Terms)
    {
        for(const FString& Term:Terms) if(Text.Contains(Term,ESearchCase::IgnoreCase)) return true;
        return false;
    }

    bool MarketKitModuleForPurpose(const FString& Purpose,FString& OutModule)
    {
        FString Plan=Purpose;
        const int32 PlanAt=Plan.Find(TEXT("打算："));
        if(PlanAt!=INDEX_NONE) Plan=Plan.Mid(PlanAt+3);
        else
        {
            const FString Exact=Purpose.TrimStartAndEnd();
            if(Exact!=TEXT("bench_low") && Exact!=TEXT("work_table") && Exact!=TEXT("tool_rack")
                && Exact!=TEXT("low bench") && Exact!=TEXT("work table") && Exact!=TEXT("tool rack")) return false;
            Plan=Exact;
        }
        if(Plan.Contains(TEXT("不要")) || Plan.Contains(TEXT("不需要")) || Plan.Contains(TEXT("不想")) || Plan.Contains(TEXT("无需"))) return false;
        const bool bBench=ContainsAny(Plan,{TEXT("bench_low"),TEXT("low bench"),TEXT("低矮长凳"),TEXT("矮长凳"),TEXT("木长凳")});
        const bool bTable=ContainsAny(Plan,{TEXT("work_table"),TEXT("work table"),TEXT("工作桌"),TEXT("工作台"),TEXT("晾坯桌"),TEXT("晾坯工作桌")});
        const bool bRack=ContainsAny(Plan,{TEXT("tool_rack"),TEXT("tool rack"),TEXT("工具架")});
        const int32 Matches=(bBench?1:0)+(bTable?1:0)+(bRack?1:0);
        if(Matches!=1) return false;
        OutModule=bBench?TEXT("bench_low"):bTable?TEXT("work_table"):TEXT("tool_rack");
        return true;
    }



    bool IsInstalledRequest(const FOrganicConstructionHomeState& Home,const FString& RequestId)
    {
        return Home.MarketKitInstalled.ContainsByPredicate([&](const FOrganicMarketKitRecord& Record)
        { return Record.RequestId==RequestId; });
    }

    FVector KitHalfExtents(const FString& Module)
    {
        if(Module==TEXT("work_table")) return FVector(110.f,70.f,80.f);
        if(Module==TEXT("tool_rack")) return FVector(70.f,50.f,100.f);
        return FVector(90.f,55.f,60.f);
    }

    bool CatalogMeshPath(const FString& Module,const FString& Layer,FString& OutPath)
    {
        FString Text;
        const FString CatalogPath=FPaths::ProjectContentDir()/TEXT("ThreeHearths/Data/MedievalMarketLifeCatalog.json");
        if(!FFileHelper::LoadFileToString(Text,*CatalogPath) || Text.Len()>4*1024*1024) return false;
        TSharedPtr<FJsonObject> Root;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid()) return false;
        double Schema=0.; FString Destination;
        if(!Root->TryGetNumberField(TEXT("schema_version"),Schema) || Schema!=1.
            || !Root->TryGetStringField(TEXT("destination_root"),Destination)
            || Destination!=TEXT("/Game/ThreeHearths/Generated/MedievalLife/MarketLifeKit")) return false;
        const TArray<TSharedPtr<FJsonValue>>* Assets=nullptr;
        if(!Root->TryGetArrayField(TEXT("assets"),Assets)) return false;
        const FString WantedId=Module+TEXT("/")+Layer;
        for(const TSharedPtr<FJsonValue>& Value:*Assets)
        {
            if(!Value.IsValid() || Value->Type!=EJson::Object) continue;
            const TSharedPtr<FJsonObject> Asset=Value->AsObject(); FString Id,ModuleId,AssetLayer,Mesh;
            if(!Asset->TryGetStringField(TEXT("id"),Id) || !Asset->TryGetStringField(TEXT("module_id"),ModuleId)
                || !Asset->TryGetStringField(TEXT("layer"),AssetLayer) || !Asset->TryGetStringField(TEXT("mesh"),Mesh)) continue;
            if(Id==WantedId && ModuleId==Module && AssetLayer==Layer && !Mesh.IsEmpty()) { OutPath=Mesh; return true; }
        }
        return false;
    }

    bool KitMeshesAvailable(const FString& Module,FString& OutStructure,FString& OutFinish)
    {
        return CatalogMeshPath(Module,TEXT("structure"),OutStructure)
            && CatalogMeshPath(Module,TEXT("finish"),OutFinish);
    }



    FOrganicMarketKitRecord PendingRecord(const FHearthWorldRequest& Request,const FString& Module,
        const FVector& Work,const FVector& Install,float Yaw)
    {
        FOrganicMarketKitRecord Record; Record.RequestId=Request.Id; Record.TaskId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Record.ModuleId=Module; Record.Status=TEXT("pending");
        HearthOrganicConstruction::MarketKitCost(Module,Record.Cost); Record.Anchor=Work; Record.InstallPosition=Install; Record.InstallYaw=Yaw; return Record;
    }
}

bool AHearthVillage::IsEligibleMarketLifeRequest(int32 Index,const FHearthWorldRequest& Request,FString& OutModule) const
{
        if(!Residents.IsValidIndex(Index) || Request.Category!=TEXT("asset") || Request.Status!=TEXT("proposed")
            || !Request.bHasAssetContext || Request.RequesterIds.Num()!=1 || Request.RequesterIds[0]!=Residents[Index].StableId
            || Residents[Index].Plot<0 || Residents[Index].Plot>=HousingPlotCount()
            || Request.AssetContext.TargetId.IsEmpty() || Request.AssetContext.TargetId!=PlotIds[Residents[Index].Plot])
            return false;
        if(!MarketKitModuleForPurpose(Request.AssetContext.Purpose,OutModule)) return false;
        if(const auto* Home=OrganicHomes.Find(Residents[Index].StableId))
            if(Home->MarketKitInstalled.ContainsByPredicate([&](const auto& Kit){return Kit.ModuleId==OutModule;}))
            {
                const FString& Purpose=Request.AssetContext.Purpose;
                const int32 At=Purpose.Find(TEXT("打算："));
                const FString Plan=At==INDEX_NONE?Purpose:Purpose.Mid(At+3);
                if(!ContainsAny(Plan,{TEXT("另一"),TEXT("再添"),TEXT("再加"),TEXT("第二"),TEXT("第三"),TEXT("another"),TEXT("additional")})) return false;
            }
        return true;
    }

bool AHearthVillage::FindMarketLifeAnchor(int32 Index,const FString& Module,
        const FOrganicConstructionHomeState& Home,FVector& OutWork,FVector& OutInstall,float& OutYaw) const
{
        if(!Residents.IsValidIndex(Index)) return false;
        const auto& R=Residents[Index];
        if(R.Plot<0 || R.Plot>=HousingPlotCount()) return false;
        const FVector Door=HomeApproach(R.Plot);
        FVector Forward=(Door-PlotPositions[R.Plot]).GetSafeNormal2D();
        if(Forward.IsNearlyZero()) Forward=FRotator(0,PlotYaws[R.Plot],0).RotateVector(FVector(0,1,0)).GetSafeNormal2D();
        const FVector Side=FVector(-Forward.Y,Forward.X,0);
        TArray<FVector> Candidates;
        // Keep the whole furnishing off the street. Try the recess beside
        // the house first, then progressively larger reachable yard pockets.
        for(float Depth:{-260.f,-400.f,260.f,420.f,-560.f})
            for(float Lateral:{300.f,-300.f,460.f,-460.f,620.f,-620.f})
                Candidates.Add(Door+Side*Lateral+Forward*Depth);
        const FVector Half=KitHalfExtents(Module);
        const float Yaw=PlotYaws[R.Plot];
        const FRotator Rot(0,Yaw,0);
        FString StructurePath,FinishPath;
        if(!KitMeshesAvailable(Module,StructurePath,FinishPath)) return false;
        const auto* Structure=LoadObject<UStaticMesh>(nullptr,*StructurePath);
        const auto* Finish=LoadObject<UStaticMesh>(nullptr,*FinishPath);
        if(!Structure || !Finish) return false;
        const float MeshBottom=FMath::Min(Structure->GetBoundingBox().Min.Z,Finish->GetBoundingBox().Min.Z);
        const auto Roads=HearthTownLayout::VillageRoads(true,TownLayoutVersion);
        for(FVector Install:Candidates)
        {
            const FVector TowardDoor=(Door-Install).GetSafeNormal2D();
            FVector Candidate=Install+TowardDoor*190.f;
            Candidate.Z=GroundHeightAt(Candidate)+5.2f;
            bool bRoad=false;
            for(const auto& Road:Roads)
            {
                FVector A=Road.A,B=Road.B,Q=Install; A.Z=B.Z=Q.Z=0.f;
                const float Clearance=Road.Width*.5f+FMath::Sqrt(Half.X*Half.X+Half.Y*Half.Y)+45.f;
                if(FVector::DistSquared2D(Q,FMath::ClosestPointOnSegment(Q,A,B))<FMath::Square(Clearance))
                { bRoad=true; break; }
            }
            if(bRoad) continue;
            const FVector RawCorners[]= {
                Install+Rot.RotateVector(FVector( Half.X, Half.Y,0)),
                Install+Rot.RotateVector(FVector(-Half.X, Half.Y,0)),
                Install+Rot.RotateVector(FVector(-Half.X,-Half.Y,0)),
                Install+Rot.RotateVector(FVector( Half.X,-Half.Y,0)) };
            float MinGround=BIG_NUMBER,MaxGround=-BIG_NUMBER;
            FVector Corners[4];
            for(int32 CornerIndex=0;CornerIndex<4;++CornerIndex)
            {
                const float Ground=GroundHeightAt(RawCorners[CornerIndex]);
                MinGround=FMath::Min(MinGround,Ground); MaxGround=FMath::Max(MaxGround,Ground);
                Corners[CornerIndex]=RawCorners[CornerIndex]; Corners[CornerIndex].Z=Ground+5.2f;
            }
            // Mesh source origins are at the feet; the pedestrian 5.2cm
            // offset must not make a four-legged furnishing float.
            Install.Z=MinGround-MeshBottom;
            bool bFootprint=IsClearPoint(Candidate) && !IsMarketLifeKitBlockingPoint(Candidate);
            const FVector X=Rot.RotateVector(FVector::ForwardVector),Y=Rot.RotateVector(FVector::RightVector);
            const float HX=Half.X+25.f,HY=Half.Y+25.f;
            for(int32 Plot=0;bFootprint && Plot<HousingPlotCount();++Plot)
                for(const auto& WeakMesh:StarterArchitectureMeshes[Plot])
                {
                    const auto* Mesh=WeakMesh.Get(); if(!IsValid(Mesh) || !Mesh->IsRegistered()) continue;
                    const FVector D=Mesh->Bounds.Origin-Install,E=Mesh->Bounds.BoxExtent;
                    if(FMath::Abs(D.X)<=HX*FMath::Abs(X.X)+HY*FMath::Abs(Y.X)+E.X
                        && FMath::Abs(D.Y)<=HX*FMath::Abs(X.Y)+HY*FMath::Abs(Y.Y)+E.Y
                        && FMath::Abs(FVector::DotProduct(D,X))<=HX+E.X*FMath::Abs(X.X)+E.Y*FMath::Abs(X.Y)
                        && FMath::Abs(FVector::DotProduct(D,Y))<=HY+E.X*FMath::Abs(Y.X)+E.Y*FMath::Abs(Y.Y))
                    { bFootprint=false; break; }
                }
            for(const FVector& Corner:Corners)
            {
                if(!IsClearPoint(Corner) || IsMarketLifeKitBlockingPoint(Corner)) { bFootprint=false; break; }
            }
            if(MaxGround-MinGround>5.f) bFootprint=false;
            for(int32 Edge=0;bFootprint && Edge<4;++Edge)
                if(OrganicBlocksSegment(Corners[Edge],Corners[(Edge+1)%4])
                    || IsMarketLifeKitBlockingSegment(Corners[Edge],Corners[(Edge+1)%4])) bFootprint=false;
            const float NewRadius=FMath::Sqrt(Half.X*Half.X+Half.Y*Half.Y)+80.f;
            for(const FOrganicMarketKitRecord& Existing:Home.MarketKitInstalled)
            {
                const FVector ExistingCenter=Existing.InstallPosition.IsNearlyZero()?Existing.Anchor:Existing.InstallPosition;
                const FVector ExistingHalf=KitHalfExtents(Existing.ModuleId);
                const float ExistingRadius=FMath::Sqrt(ExistingHalf.X*ExistingHalf.X+ExistingHalf.Y*ExistingHalf.Y)+80.f;
                if(FVector::DistSquared2D(Install,ExistingCenter)<FMath::Square(NewRadius+ExistingRadius)) { bFootprint=false; break; }
            }
            if(!bFootprint) continue;
            TArray<FVector> Route;
            if(FindActivityRoute(Index,Candidate,Route))
            { OutWork=Candidate; OutInstall=Install; OutYaw=Yaw; return true; }
        }
        return false;
    }

bool AHearthVillage::HasMarketKitWork(int32 Index) const
{
    if(!Residents.IsValidIndex(Index)) return false;
    const auto* Home=OrganicHomes.Find(Residents[Index].StableId);
    return Home && Home->bHasMarketKitPending && !Home->MarketKitPending.TaskId.IsEmpty()
        && !Residents[Index].ActiveTaskId.IsEmpty()
        && Residents[Index].ActiveTaskId==Home->MarketKitPending.TaskId
        && Home->ActivePieceKey.IsEmpty() && FMath::IsNearlyZero(Home->WorkProgress)
        && Residents[Index].ProductionSite==-1 && Residents[Index].ProductionOp==-1
        && Residents[Index].CargoAmount==0 && Residents[Index].CarriedWood==0
        && Residents[Index].ProductionComponentId.IsEmpty() && Residents[Index].HeldToolId.IsEmpty()
        && (Residents[Index].Task==EHearthTask::OrganicTravel || Residents[Index].Task==EHearthTask::OrganicWork);
}

void AHearthVillage::AppendMarketLifeContext(int32 Index,const TSharedRef<FJsonObject>& Context) const
{
    TArray<TSharedPtr<FJsonValue>> Fixtures;
    if(Residents.IsValidIndex(Index))
        if(const auto* Home=OrganicHomes.Find(Residents[Index].StableId))
            for(const auto& Kit:Home->MarketKitInstalled)
            {
                auto Item=MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("module"),Kit.ModuleId);
                Item->SetStringField(TEXT("source_request"),Kit.RequestId);
                Item->SetStringField(TEXT("position_cm"),Kit.InstallPosition.ToString());
                Item->SetBoolField(TEXT("built_and_paid"),true);
                Fixtures.Add(MakeShared<FJsonValueObject>(Item));
            }
    Context->SetArrayField(TEXT("completed_added_furnishings"),Fixtures);
    Context->SetStringField(TEXT("furnishing_truth"),TEXT("Listed furnishings were really built and paid for by this resident. Do not describe them as missing or request the same completed installation again. An additional instance is a new proposal only if you explicitly need another one and explain its purpose. They are exterior structures; sitting, pottery drying, equipped tools and workstation production are not implemented by their presence. Residents may stand nearby to talk. The list excludes decorative parts originally included in the house."));
}

bool AHearthVillage::IsMarketLifeKitBlockingPoint(const FVector& Position) const
{
    if(!IsOrganicVillage()) return false;
    for(const auto& Pair:OrganicHomes)
        for(const FOrganicMarketKitRecord& Record:Pair.Value.MarketKitInstalled)
        {
            const bool bLegacyPose=Record.InstallPosition.IsNearlyZero();
            const FVector Center=bLegacyPose?Record.Anchor:Record.InstallPosition;
            const float Yaw=bLegacyPose?0.f:Record.InstallYaw;
            const FVector Local=FRotator(0,Yaw,0).UnrotateVector(Position-Center);
            const FVector Half=KitHalfExtents(Record.ModuleId)+FVector(55.f,55.f,0.f);
            if(FMath::Abs(Local.X)<=Half.X && FMath::Abs(Local.Y)<=Half.Y) return true;
        }
    return false;
}

bool AHearthVillage::IsMarketLifeKitBlockingSegment(const FVector& A,const FVector& B) const
{
    const int32 Samples=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/40.f));
    for(int32 I=0;I<=Samples;++I)
        if(IsMarketLifeKitBlockingPoint(FMath::Lerp(A,B,float(I)/float(Samples)))) return true;
    return false;
}

void AHearthVillage::AdvanceMarketLifeKits(float Dt)
{
    if(!IsOrganicVillage() || !OrganicCatalog.IsValid() || !bAutonomousLifeEnabled) return;
    for(int32 I=0;I<Residents.Num();++I)
    {
        auto& R=Residents[I];
        if(IsSharedServiceResident(I) || R.Plot<0 || R.Plot>=HousingPlotCount() || IsDecisionPending(I) || !R.ConversationId.IsEmpty()) continue;
        auto* Home=OrganicHomes.Find(R.StableId);
        if(!Home || !HearthOrganicConstruction::Validate(*Home)) continue;
        if(Home->bHasMarketKitPending)
        {
            const auto* Request=WorldRequests.FindByPredicate([&](const auto& Q){ return Q.Id==Home->MarketKitPending.RequestId; });
            FString Module;
            if(!Request || !IsEligibleMarketLifeRequest(I,*Request,Module) || Module!=Home->MarketKitPending.ModuleId)
            {
                if(R.ActiveTaskId==Home->MarketKitPending.TaskId)
                { R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.Route.Reset(); R.Timer=0; R.LifeAction=-1; }
                Home->MarketKitPending=FOrganicMarketKitRecord(); Home->bHasMarketKitPending=false; ++Home->Revision;
                R.LatestEvent=TEXT("原生活物件需求已失效，停止施工，未扣材料或金币。");
                continue;
            }
        }

        if(!Home->bHasMarketKitPending
            && (!Home->ActivePieceKey.IsEmpty() || !FMath::IsNearlyZero(Home->WorkProgress)
                || R.ProductionSite!=-1 || R.ProductionOp!=-1 || !R.ProductionComponentId.IsEmpty()
                || R.CargoAmount!=0 || R.CarriedWood!=0 || !R.HeldToolId.IsEmpty())) continue;

        if(!Home->bHasMarketKitPending && OrganicNextAttempt.FindRef(R.StableId)>Elapsed) continue;
        if(!Home->bHasMarketKitPending)
        {
            if(Home->MarketKitInstalled.Num()>=HearthOrganicConstruction::MaxMarketKitsPerHome) continue;
            const FHearthWorldRequest* Found=nullptr; FString Module;
            for(const FHearthWorldRequest& Request:WorldRequests)
            {
                FString Candidate;
                if(!IsEligibleMarketLifeRequest(I,Request,Candidate) || IsInstalledRequest(*Home,Request.Id)) continue;
                Found=&Request; Module=Candidate; break;
            }
            if(!Found) continue;
            // Dry-run the exact atomic charge before reserving a resident's
            // time.  A proposed kit waits for material production rather than
            // creating a work task that cannot finish.
            FOrganicConstructionPiece DryPiece; DryPiece.Key=TEXT("market_kit:")+Found->Id; DryPiece.Module=Module;
            HearthOrganicConstruction::MarketKitCost(Module,DryPiece.Cost);
            FOrganicConstructionHomeState DryHome; FOrganicConstructionStock DryStock{StoneStock,PlankStock,BeamStock,TileStock};
            int32 DryCoins=R.Coins,DryTreasury=TreasuryCoins; FOrganicConstructionInstallResult DryResult;
            if(!HearthOrganicConstruction::InstallAtomic(DryPiece,DryHome,DryStock,DryCoins,DryTreasury,DryResult) || DryCoins<4) continue;
            FVector Work,Install; float Yaw=0.f;
            if(!FindMarketLifeAnchor(I,Module,*Home,Work,Install,Yaw))
            {
                OrganicNextAttempt.Add(R.StableId,Elapsed+KitRetrySeconds);
                R.LatestEvent=TEXT("门前没有满足物件足迹和通行余量的安全位置，需求保留。");
                continue;
            }
            Home->MarketKitPending=PendingRecord(*Found,Module,Work,Install,Yaw); Home->bHasMarketKitPending=true; ++Home->Revision;
            R.LatestEvent=TEXT("已确认物件需求，等走到自家门前再施工。");
        }

        if(R.Task!=EHearthTask::LifeChoosing || !R.Route.IsEmpty() || !R.ActiveTaskId.IsEmpty()
            || R.Hunger>65.f || R.Energy<25.f || R.SocialNeed>80.f
            || OrganicNextAttempt.FindRef(R.StableId)>Elapsed) continue;
        TArray<FVector> Route;
        if(!FindActivityRoute(I,Home->MarketKitPending.Anchor,Route))
        {
            OrganicNextAttempt.Add(R.StableId,Elapsed+KitRetrySeconds);
            R.LatestEvent=TEXT("门前空间或道路暂不可达，物件需求保留。");
            continue;
        }
        Home->MarketKitPending.Status=TEXT("traveling"); ++Home->Revision;
        R.ActiveTaskId=Home->MarketKitPending.TaskId; R.LifeAction=-1; R.Task=EHearthTask::OrganicTravel;
        R.Timer=0.f; R.MoveRetry=0; R.bMovementBlocked=false; R.Route=MoveTemp(Route);
        R.LatestEvent=TEXT("走到自家门前，准备安装一件生活物件。");
    }
}

bool AHearthVillage::AdvanceMarketKitWorker(int32 Index,float Dt)
{
    if(!HasMarketKitWork(Index)) return false;
    auto& R=Residents[Index]; auto* Home=OrganicHomes.Find(R.StableId); if(!Home) return false;
    auto& Pending=Home->MarketKitPending;
    if(R.Task==EHearthTask::OrganicTravel)
    {
        if(MoveResident(Index,Dt))
        {
            R.Task=EHearthTask::OrganicWork; Pending.Status=TEXT("working"); ++Home->Revision;
            R.LatestEvent=TEXT("已到门前，开始施工。");
        }
        return true;
    }
    if(R.Task!=EHearthTask::OrganicWork) return true;
    Pending.WorkProgress=FMath::Min(1.f,Pending.WorkProgress+Dt/KitWorkSeconds);
    if(Pending.WorkProgress<1.f) return true;

    FString StructurePath,FinishPath;
    if(!KitMeshesAvailable(Pending.ModuleId,StructurePath,FinishPath)
        || !LoadObject<UStaticMesh>(nullptr,*StructurePath) || !LoadObject<UStaticMesh>(nullptr,*FinishPath))
    {
        Pending.Status=TEXT("pending"); Pending.WorkProgress=0.f; ++Home->Revision;
        R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.NextLifeDecision=Elapsed+KitRetrySeconds;
        R.LatestEvent=TEXT("施工物件的结构或完成层尚未准备好，需求保留且没有扣款。");
        OrganicNextAttempt.Add(R.StableId,Elapsed+KitRetrySeconds);
        return true;
    }

    FOrganicConstructionStock PublicStock{StoneStock,PlankStock,BeamStock,TileStock};
    FOrganicConstructionPiece Piece; Piece.Key=TEXT("market_kit:")+Pending.RequestId; Piece.Module=Pending.ModuleId; Piece.Cost=Pending.Cost;
    FOrganicConstructionHomeState Trial; int32 Wallet=R.Coins; int32 Treasury=TreasuryCoins; FOrganicConstructionInstallResult Result;
    const bool bInstalled=HearthOrganicConstruction::InstallAtomic(Piece,Trial,PublicStock,Wallet,Treasury,Result) && Wallet>=4;
    if(!bInstalled)
    {
        Pending.Status=TEXT("pending"); Pending.WorkProgress=0.f; ++Home->Revision;
        R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.NextLifeDecision=Elapsed+KitRetrySeconds;
        R.LatestEvent=Result.FailureReason==TEXT("insufficient_owner_coins")?TEXT("生活物件需要保留至少四枚钱，先继续工作。"):TEXT("材料不足，物件需求保留且没有扣款。");
        OrganicNextAttempt.Add(R.StableId,Elapsed+KitRetrySeconds);
        return true;
    }

    const FString CompletedRequestId=Pending.RequestId;
    const FString CompletedModule=Pending.ModuleId;
    const FString CompletedTaskId=Pending.TaskId;
    R.Coins=Wallet; TreasuryCoins=Treasury; StoneStock=PublicStock.Stone; PlankStock=PublicStock.Planks; BeamStock=PublicStock.Beams; TileStock=PublicStock.Tiles;
    ManufacturedSpent[0]+=Result.PublicConsumed.Planks; ManufacturedSpent[1]+=Result.PublicConsumed.Beams;
    FOrganicMarketKitRecord Installed=Pending; Installed.Status=TEXT("installed"); Installed.WorkProgress=1.f; ++Installed.Revision;
    Home->MarketKitInstalled.Add(MoveTemp(Installed)); Home->MarketKitPending=FOrganicMarketKitRecord(); Home->bHasMarketKitPending=false; ++Home->Revision;
    if(Result.CoinsPaid>0)
    {
        FHearthTransaction Transaction; Transaction.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        Transaction.Kind=TEXT("market_kit_purchase"); Transaction.TaskId=CompletedTaskId; Transaction.Item=CompletedModule;
        Transaction.From=Index; Transaction.To=-1; Transaction.Amount=Result.CoinsPaid; Transaction.Quantity=Result.CoinsPaid; Transaction.At=Elapsed;
        Transactions.Add(MoveTemp(Transaction));
    }
    for(auto& Request:WorldRequests) if(Request.Id==CompletedRequestId)
    {
        Request.Resolution=FString::Printf(TEXT("已由居民走到自家门前并完成 %s 的结构与完成层安装；材料和金币已按账本结算。"),*CompletedModule);
        break;
    }
    R.Task=EHearthTask::LifeChoosing; R.ActiveTaskId.Empty(); R.NextLifeDecision=Elapsed+1.f;
    R.LatestEvent=FString::Printf(TEXT("已完成 %s 结构与完成层；工具设备层未自动赠送。"),*CompletedModule);
    OrganicNextAttempt.Add(R.StableId,Elapsed+KitRetrySeconds);
    RefreshMarketLifeKitHome(Index);
    return true;
}

void AHearthVillage::RefreshMarketLifeKitHome(int32 Index)
{
    if(!IsOrganicVillage() || !Residents.IsValidIndex(Index)) return;
    const auto& R=Residents[Index]; if(R.Plot<0 || R.Plot>=HousingPlotCount()) return;
    auto& Meshes=MarketLifeKitMeshes[R.Plot];
    for(auto& Mesh:Meshes) if(Mesh.IsValid()) Mesh->DestroyComponent();
    Meshes.Reset();
    const auto* Home=OrganicHomes.Find(R.StableId); if(!Home) return;
    for(const auto& Record:Home->MarketKitInstalled)
    {
        FString StructurePath,FinishPath;
        if(!KitMeshesAvailable(Record.ModuleId,StructurePath,FinishPath)) continue;
        UStaticMesh* Structure=LoadObject<UStaticMesh>(nullptr,*StructurePath);
        UStaticMesh* Finish=LoadObject<UStaticMesh>(nullptr,*FinishPath);
        if(!Structure || !Finish) continue;
        const bool bLegacyPose=Record.InstallPosition.IsNearlyZero();
        const FVector InstallPosition=bLegacyPose?Record.Anchor:Record.InstallPosition;
        const float InstallYaw=bLegacyPose?PlotYaws[R.Plot]:Record.InstallYaw;
        if(auto* A=AddMesh(StructurePath,InstallPosition,FVector(1.f)))
        {
            A->ComponentTags.Add(*FString(TEXT("MarketLifeKit:")+Record.ModuleId+TEXT(":structure")));
            A->SetWorldRotation(FRotator(0,InstallYaw,0));
            A->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            A->SetCollisionResponseToAllChannels(ECR_Block);
            Meshes.Add(A);
        }
        if(auto* A=AddMesh(FinishPath,InstallPosition,FVector(1.f)))
        {
            A->ComponentTags.Add(*FString(TEXT("MarketLifeKit:")+Record.ModuleId+TEXT(":finish")));
            A->SetWorldRotation(FRotator(0,InstallYaw,0));
            A->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            A->SetCollisionResponseToAllChannels(ECR_Block);
            Meshes.Add(A);
        }
    }
}
