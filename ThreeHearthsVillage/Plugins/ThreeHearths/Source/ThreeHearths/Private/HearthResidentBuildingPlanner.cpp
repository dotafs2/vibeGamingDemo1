#include "HearthResidentBuildingPlanner.h"
#include "HearthStructureCatalog.h"

namespace
{
    FHearthStructureMaterialQuantity Mat(const TCHAR* Id) { FHearthStructureMaterialQuantity M; M.MaterialId=Id; M.Quantity=1; return M; }
    FHearthStructureMaterialRecipe Recipe(const TCHAR* Id,const TCHAR* Catalog,const TCHAR* MaterialId)
    { FHearthStructureMaterialRecipe R; R.RecipeId=Id; R.CatalogId=Catalog; R.Inputs.Add(Mat(MaterialId)); return R; }
    struct FHearthHouseStyle
    {
        FString RequestedWall,RequestedRoof,WallChoice,RoofChoice,WallCatalog,DoorCatalog,RoofCatalog,WallResource,RoofResource,Substitution;
    };
    FHearthHouseStyle StyleFor(const FHearthResidentBuildingInput& I)
    {
        FHearthHouseStyle S;
        S.RequestedWall=I.WallMaterial.ToLower(); S.RequestedRoof=I.RoofMaterial.ToLower();
        S.WallChoice=S.RequestedWall==TEXT("stone")?TEXT("stone"):TEXT("timber");
        S.RoofChoice=TEXT("timber");
        if(S.RequestedWall==TEXT("plaster")) S.Substitution=TEXT("plaster preference deferred: no plaster production inventory; executable timber selected");
        if(S.RequestedRoof==TEXT("terracotta"))
        {
            if(!S.Substitution.IsEmpty()) S.Substitution+=TEXT("; ");
            S.Substitution+=TEXT("terracotta preference deferred: no tile production inventory; executable timber selected");
        }
        else if(S.RequestedRoof==TEXT("slateblue"))
        {
            if(!S.Substitution.IsEmpty()) S.Substitution+=TEXT("; ");
            S.Substitution+=TEXT("slateblue tile preference deferred: no tile production inventory; executable timber selected");
        }
        S.WallCatalog=TEXT("wall_")+S.WallChoice+TEXT("_2m");
        S.DoorCatalog=TEXT("wall_door_")+S.WallChoice+TEXT("_2m");
        S.RoofCatalog=TEXT("roof_slope_")+S.RoofChoice+TEXT("_2m");
        S.WallResource=S.WallChoice==TEXT("timber")?TEXT("plank"):TEXT("stone");
        S.RoofResource=S.RoofChoice==TEXT("timber")?TEXT("plank"):TEXT("stone");
        return S;
    }
    bool MakeSpec(const TCHAR* Catalog,const FString& Key,const FVector& Base,float Yaw,const FString& RecipeId,const TCHAR* MaterialId,int32 Cost,bool Support,const TCHAR* Parent,FHearthStructureComponentSpec& Out)
    {
        const FHearthStructureCatalogEntry* E=HearthStructureCatalog::Find(Catalog); if(!E) return false;
        const FVector Bounds=(E->BoundsMax-E->BoundsMin)*100.f; const FVector BoundsMin=E->BoundsMin*100.f; if(Bounds.X<=0||Bounds.Y<=0||Bounds.Z<=0) return false;
        Out=FHearthStructureComponentSpec(); Out.CatalogId=Catalog; Out.SemanticKey=Key; Out.Offset=Base;
        Out.Height=Bounds.Z; Out.Size=FVector2D(Bounds.X,Bounds.Y); Out.BoundsMin=BoundsMin; Out.BoundsMax=E->BoundsMax*100.f;
        Out.Orientation=E->DefaultRotation+FRotator(0,Yaw,0);
        Out.RecipeId=RecipeId; Out.Materials.Add(Mat(MaterialId)); Out.MaterialCost=Cost; Out.CollisionRadius=FMath::Max(Bounds.X,Bounds.Y)*.5f; Out.bRequiresSupport=Support; Out.SupportsComponentKey=Parent; return true;
    }
    void Register(FHearthStructurePlan& P,const FHearthHouseStyle& S)
    {
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("foundation"),TEXT("foundation_stone_2m"),TEXT("stone")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("floor"),TEXT("floor_timber_2m"),TEXT("plank")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("post"),TEXT("post_timber_2_4m"),TEXT("beam")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("beam"),TEXT("beam_timber_2m"),TEXT("beam")));
        const FString WallRecipe=TEXT("wall_")+S.WallChoice,DoorRecipe=TEXT("door_")+S.WallChoice,RoofRecipe=TEXT("roof_")+S.RoofChoice;
        HearthStructurePlan::RegisterRecipe(P,Recipe(*WallRecipe,*S.WallCatalog,*S.WallResource));
        HearthStructurePlan::RegisterRecipe(P,Recipe(*DoorRecipe,*S.DoorCatalog,*S.WallResource));
        HearthStructurePlan::RegisterRecipe(P,Recipe(*RoofRecipe,*S.RoofCatalog,*S.RoofResource));
    }
    bool Add(FHearthStructurePlan& P,int32 N,float RoadYaw,const FString& Ext,const FHearthHouseStyle& Style)
    {
        const FHearthStructurePlan Backup=P;
        auto Fail=[&](){P=Backup;return false;};
        const FString K=FString::Printf(TEXT("room_%d"),N);
        const FString Boundary=N>0?FString::Printf(TEXT("room_%d_side_right"),N-1):K+TEXT("_side_left");
        if(!HearthStructurePlan::AppendRoom(P,K,N?TEXT("additional"):TEXT("living"),Ext)) return Fail();
        const float X=N*200.f,Frame=16.f,Top=256.f;
        const FString WallRecipe=TEXT("wall_")+Style.WallChoice,DoorRecipe=TEXT("door_")+Style.WallChoice,RoofRecipe=TEXT("roof_")+Style.RoofChoice;
        FHearthStructureComponentSpec S;
        auto Put=[&](const TCHAR* Cat,const FString& Key,FVector Pos,float Yaw,const FString& RecipeId,const TCHAR* Material,bool Need,const FString& Parent)
        {return MakeSpec(Cat,Key,Pos,Yaw,RecipeId,Material,1,Need,*Parent,S)&&HearthStructurePlan::AppendComponent(P,S,Ext);};
        auto Link=[&](const FString& Name,const FString& From,const FString& To)
        {return HearthStructurePlan::AppendConnection(P,K+Name,From,To,true,Ext);};

        if(!Put(TEXT("foundation_stone_2m"),K+TEXT("_foundation"),FVector(X,0,0),0,TEXT("foundation"),TEXT("stone"),false,TEXT(""))) return Fail();
        if(!Put(TEXT("floor_timber_2m"),K+TEXT("_floor"),FVector(X,0,0),0,TEXT("floor"),TEXT("plank"),true,K+TEXT("_foundation"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_bl"),FVector(X-91,-91,Frame),0,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_br"),FVector(X+91,-91,Frame),0,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_fl"),FVector(X-91,91,Frame),0,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_fr"),FVector(X+91,91,Frame),0,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_back"),FVector(X,91,Top),0,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_fl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_front"),FVector(X,-91,Top),0,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_bl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_left"),FVector(X-91,0,Top),90,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_bl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_right"),FVector(X+91,0,Top),90,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_br"))) return Fail();
        if(!Put(*Style.WallCatalog,K+TEXT("_back"),FVector(X,100,0),0,WallRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        if(N==0&&!Put(*Style.WallCatalog,K+TEXT("_side_left"),FVector(X-100,0,0),90,WallRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        if(!Put(*Style.WallCatalog,K+TEXT("_side_right"),FVector(X+100,0,0),90,WallRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        if(!Put(*Style.DoorCatalog,K+TEXT("_door"),FVector(X,-100,0),0,DoorRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        FHearthStructureOpening O; O.Offset=FVector2D(X,-100); O.AccessDirection=FVector2D(0,-1); O.Width=94; O.bDoor=true;
        if(!HearthStructurePlan::AppendOpening(P,K+TEXT("_front_door"),K,O,Ext)) return Fail();
        if(!Put(*Style.RoofCatalog,K+TEXT("_roof_back"),FVector(X,0,Top+13.1f),90,RoofRecipe,*Style.RoofResource,true,K+TEXT("_beam_back"))) return Fail();
        if(!Put(*Style.RoofCatalog,K+TEXT("_roof_front"),FVector(X,0,Top+13.1f),270,RoofRecipe,*Style.RoofResource,true,K+TEXT("_beam_front"))) return Fail();

        const TPair<FString,FString> Links[]={
            {K+TEXT("_floor"),K+TEXT("_foundation")},
            {K+TEXT("_post_bl"),K+TEXT("_floor")},{K+TEXT("_post_br"),K+TEXT("_floor")},{K+TEXT("_post_fl"),K+TEXT("_floor")},{K+TEXT("_post_fr"),K+TEXT("_floor")},
            {K+TEXT("_beam_back"),K+TEXT("_post_fr")},{K+TEXT("_beam_front"),K+TEXT("_post_br")},{K+TEXT("_beam_left"),K+TEXT("_post_fl")},{K+TEXT("_beam_right"),K+TEXT("_post_fr")},
            {K+TEXT("_beam_back"),K+TEXT("_beam_left")},{K+TEXT("_beam_back"),K+TEXT("_beam_right")},{K+TEXT("_beam_front"),K+TEXT("_beam_left")},{K+TEXT("_beam_front"),K+TEXT("_beam_right")},
            {K+TEXT("_back"),K+TEXT("_post_fl")},{K+TEXT("_back"),K+TEXT("_post_fr")},{K+TEXT("_side_right"),K+TEXT("_post_br")},{K+TEXT("_side_right"),K+TEXT("_post_fr")},
            {K+TEXT("_door"),K+TEXT("_post_bl")},{K+TEXT("_door"),K+TEXT("_post_br")},
            {K+TEXT("_roof_back"),K+TEXT("_beam_left")},{K+TEXT("_roof_back"),K+TEXT("_beam_right")},{K+TEXT("_roof_front"),K+TEXT("_beam_left")},{K+TEXT("_roof_front"),K+TEXT("_beam_right")},
            {K+TEXT("_roof_back"),K+TEXT("_roof_front")}
        };
        int32 LinkIndex=0;
        for(const auto& Pair:Links) if(!Link(TEXT("_contact_")+FString::FromInt(LinkIndex++),Pair.Key,Pair.Value)) return Fail();
        if(N==0)
        {
            if(!Link(TEXT("_left_bl"),K+TEXT("_side_left"),K+TEXT("_post_bl"))||!Link(TEXT("_left_fl"),K+TEXT("_side_left"),K+TEXT("_post_fl"))) return Fail();
        }
        else if(!Link(TEXT("_boundary_bl"),Boundary,K+TEXT("_post_bl"))||!Link(TEXT("_boundary_fl"),Boundary,K+TEXT("_post_fl"))) return Fail();
        return true;
    }
    FString Issues(const FHearthStructureValidationResult& V){FString R;for(const FString& I:V.Issues){if(!R.IsEmpty())R+=TEXT(",");R+=I;}return R;}
    bool HasTerm(const FString& S,const TCHAR* T){return S.Contains(T,ESearchCase::IgnoreCase,ESearchDir::FromStart);}
    bool HasExtension(const FHearthStructurePlan& P,const FString& K){return P.Components.ContainsByPredicate([&](const FHearthStructureComponent& C){return C.ExtensionId==K;});}
    void AddCommittedResources(const FHearthStructurePlan& P,FHearthStructureValidationContext& C)
    {
        for(const FHearthStructureComponent& Part:P.Components)
        {
            C.AvailableBudget+=Part.MaterialCost;
            for(const FHearthStructureMaterialQuantity& Used:Part.Materials)
            {
                auto* Available=C.AvailableMaterials.FindByPredicate([&](const auto& Entry){return Entry.MaterialId==Used.MaterialId;});
                if(Available) Available->Quantity+=Used.Quantity;
            }
        }
    }
    FHearthResidentBuildingInput RemainingAfter(const FHearthResidentBuildingInput& I,const FHearthStructurePlan& P)
    {
        FHearthResidentBuildingInput R=I;
        for(const FHearthStructureComponent& Part:P.Components)
        {
            R.Budget=FMath::Max(0,R.Budget-Part.MaterialCost);
            for(const FHearthStructureMaterialQuantity& Used:Part.Materials)
            {
                if(Used.MaterialId==TEXT("stone")) R.Stone=FMath::Max(0,R.Stone-Used.Quantity);
                else if(Used.MaterialId==TEXT("plank")) R.Planks=FMath::Max(0,R.Planks-Used.Quantity);
                else if(Used.MaterialId==TEXT("beam")) R.Beams=FMath::Max(0,R.Beams-Used.Quantity);
            }
        }
        return R;
    }
}

FHearthStructureValidationContext HearthResidentBuildingPlanner::ValidationContext(const FHearthResidentBuildingInput& I)
{
    FHearthStructureValidationContext C; C.AvailableBudget=FMath::Max(0,I.Budget); C.bRoadAccessible=I.bRoadAccessible; C.AvailableMaterials.Add(Mat(TEXT("stone"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Stone); C.AvailableMaterials.Add(Mat(TEXT("plank"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Planks); C.AvailableMaterials.Add(Mat(TEXT("beam"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Beams); return C;
}

FHearthResidentBuildingPlan HearthResidentBuildingPlanner::Build(const FHearthResidentBuildingInput& I)
{
    FHearthResidentBuildingPlan O; const FString Id=TEXT("resident_")+(I.ResidentId.IsEmpty()?TEXT("unknown"):I.ResidentId)+TEXT("_house"); const FString Seed=I.StableSeed.IsEmpty()?TEXT("resident-default"):I.StableSeed; const bool Family=I.HouseholdSize>=3||HasTerm(I.Need,TEXT("family"))||HasTerm(I.Need,TEXT("private"))||HasTerm(I.Need,TEXT("家庭"))||HasTerm(I.Need,TEXT("隐私")); const bool Workshop=HasTerm(I.Occupation,TEXT("craft"))||HasTerm(I.Occupation,TEXT("carpenter"))||HasTerm(I.Occupation,TEXT("smith"))||HasTerm(I.Occupation,TEXT("木匠"))||HasTerm(I.Occupation,TEXT("铁匠"))||HasTerm(I.Occupation,TEXT("陶工"))||HasTerm(I.Occupation,TEXT("织工")); const int32 Desired=FMath::Clamp(1+(Family?1:0)+(Workshop&&I.FriendsNearby>0?1:0),1,2);
    const FHearthHouseStyle Style=StyleFor(I);
    const int32 StonePerRoom=1+(Style.WallResource==TEXT("stone")?4:0)+(Style.RoofResource==TEXT("stone")?2:0);
    const int32 PlanksPerRoom=1+(Style.WallResource==TEXT("plank")?4:0)+(Style.RoofResource==TEXT("plank")?2:0);
    const int32 Affordable=FMath::Min(I.Stone/StonePerRoom,FMath::Min(I.Planks/PlanksPerRoom,FMath::Min(I.Beams/8,I.Budget/16)));
    const int32 Rooms=FMath::Clamp(FMath::Min(Desired,Affordable),0,2);
    FHearthStructureFootprint F; F.Origin=I.Origin; F.Size=FVector2D(FMath::Max(500.f,2.f*((Rooms-1)*200.f+230.f)),480.f); F.Orientation=FRotator(0,I.RoadYaw,0); FHearthStructureReasonFields R; R.Need=I.Need; R.Occupation=I.Occupation; R.Budget=FString::Printf(TEXT("budget=%d; affordable_rooms=%d; preferred wall=%s roof=%s; executable wall=%s roof=%s; conserved inputs stone=%d plank=%d beam=8 per room"),I.Budget,Rooms,*Style.RequestedWall,*Style.RequestedRoof,*Style.WallChoice,*Style.RoofChoice,StonePerRoom,PlanksPerRoom); if(!Style.Substitution.IsEmpty()) R.Budget+=TEXT("; ")+Style.Substitution; R.Relationship=FString::Printf(TEXT("friends_nearby=%d"),I.FriendsNearby); R.RoadAccess=I.bRoadAccessible?TEXT("road-accessible"):TEXT("road-inaccessible"); O.Plan=HearthStructurePlan::MakePlan(Id,Seed,F,R); Register(O.Plan,Style);
    if(!I.bRoadAccessible) O.Reason=TEXT("No buildable plan: the resident has no verified road access."); else if(Rooms==0) O.Reason=TEXT("No buildable plan: current stone, planks, beams, or budget cannot fund one core room."); else {const FHearthStructurePlan Before=O.Plan; bool Good=true;for(int32 N=0;N<Rooms;++N)if(!Add(O.Plan,N,I.RoadYaw,FString(),Style)){Good=false;break;}if(!Good){O.Plan=Before;O.Reason=TEXT("Plan assembly failed and was rolled back atomically.");}else{const auto V=HearthStructurePlan::Validate(O.Plan,ValidationContext(I));O.bBuildable=V.bValid;O.Reason=V.bValid?FString::Printf(TEXT("Built %d room(s) with %s walls and %s roof; stone=%d; planks=%d; beams=%d from finite current resources using native catalog parts."),Rooms,*Style.WallChoice,*Style.RoofChoice,I.Stone,I.Planks,I.Beams):TEXT("Plan rejected by structural validation: ")+Issues(V);}}
    O.Expansion.ExtensionKey=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; O.Expansion.Reason=TEXT("Reserve one adjoining catalog-sized bay for later household growth."); O.Expansion.ResultingPlan=O.Plan; if(O.bBuildable)AppendExpansion(O,RemainingAfter(I,O.Plan)); return O;
}

bool HearthResidentBuildingPlanner::AppendExpansion(FHearthResidentBuildingPlan& Existing,const FHearthResidentBuildingInput& I)
{
    if(!Existing.bBuildable)return false; const FString K=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; const FHearthStructurePlan Original=Existing.Expansion.ResultingPlan.Components.IsEmpty()?Existing.Plan:Existing.Expansion.ResultingPlan; if(HasExtension(Original,K))return false; FHearthStructurePlan Candidate=Original; const int32 N=Candidate.Rooms.Num(); const FHearthHouseStyle Style=StyleFor(I); Register(Candidate,Style); if(!Add(Candidate,N,I.RoadYaw,K,Style))return false; Candidate.Footprint.Size.X=FMath::Max(Candidate.Footprint.Size.X,2.f*(N*200.f+120.f)+240.f);
    const FString Trace=FString::Printf(TEXT("extension[%s] room_%d"),*K,N);
    Candidate.Reasons.Need+=FString::Printf(TEXT(" | %s need=%s"),*Trace,*I.Need);
    Candidate.Reasons.Occupation+=FString::Printf(TEXT(" | %s occupation=%s"),*Trace,*I.Occupation);
    Candidate.Reasons.Budget+=FString::Printf(TEXT(" | %s budget=%d stone=%d planks=%d beams=%d preferred wall=%s roof=%s executable wall=%s roof=%s"),*Trace,I.Budget,I.Stone,I.Planks,I.Beams,*Style.RequestedWall,*Style.RequestedRoof,*Style.WallChoice,*Style.RoofChoice);
    if(!Style.Substitution.IsEmpty()) Candidate.Reasons.Budget+=TEXT("; ")+Style.Substitution;
    Candidate.Reasons.Relationship+=FString::Printf(TEXT(" | %s friends_nearby=%d household=%d"),*Trace,I.FriendsNearby,I.HouseholdSize);
    Candidate.Reasons.RoadAccess+=FString::Printf(TEXT(" | %s %s"),*Trace,I.bRoadAccessible?TEXT("road-accessible"):TEXT("road-inaccessible"));
    auto Context=ValidationContext(I); AddCommittedResources(Original,Context); if(!HearthStructurePlan::Validate(Candidate,Context).bValid)return false; Existing.Expansion.ExtensionKey=K; Existing.Expansion.ResultingPlan=MoveTemp(Candidate); Existing.Expansion.Reason=FString::Printf(TEXT("Extension %s appends room_%d with finite current resources."),*K,N); return true;
}
