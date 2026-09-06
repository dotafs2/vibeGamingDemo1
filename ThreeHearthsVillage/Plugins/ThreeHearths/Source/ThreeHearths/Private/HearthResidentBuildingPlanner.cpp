#include "HearthResidentBuildingPlanner.h"
#include "HearthStructureCatalog.h"

namespace
{
    FHearthStructureMaterialQuantity Mat(const TCHAR* Id) { FHearthStructureMaterialQuantity M; M.MaterialId=Id; M.Quantity=1; return M; }
    FHearthStructureMaterialRecipe Recipe(const TCHAR* Id,const TCHAR* Catalog,const TCHAR* MaterialId)
    { FHearthStructureMaterialRecipe R; R.RecipeId=Id; R.CatalogId=Catalog; R.Inputs.Add(Mat(MaterialId)); return R; }
    bool MakeSpec(const TCHAR* Catalog,const FString& Key,const FVector& Base,float Yaw,const FString& RecipeId,const TCHAR* MaterialId,int32 Cost,bool Support,const TCHAR* Parent,FHearthStructureComponentSpec& Out)
    {
        const FHearthStructureCatalogEntry* E=HearthStructureCatalog::Find(Catalog); if(!E) return false;
        const FVector Bounds=(E->BoundsMax-E->BoundsMin)*100.f; const FVector BoundsMin=E->BoundsMin*100.f; if(Bounds.X<=0||Bounds.Y<=0||Bounds.Z<=0) return false;
        Out=FHearthStructureComponentSpec(); Out.CatalogId=Catalog; Out.SemanticKey=Key; Out.Offset=Base-FVector(0,0,BoundsMin.Z);
        Out.Height=Bounds.Z; Out.Size=FVector2D(Bounds.X,Bounds.Y); Out.Orientation=E->DefaultRotation+FRotator(0,Yaw,0);
        Out.RecipeId=RecipeId; Out.Materials.Add(Mat(MaterialId)); Out.MaterialCost=Cost; Out.CollisionRadius=FMath::Max(Bounds.X,Bounds.Y)*.5f; Out.bRequiresSupport=Support; Out.SupportsComponentKey=Parent; return true;
    }
    void Register(FHearthStructurePlan& P)
    {
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("foundation"),TEXT("foundation_stone_2m"),TEXT("stone")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("floor"),TEXT("floor_timber_2m"),TEXT("plank")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("post"),TEXT("post_timber_2_4m"),TEXT("beam")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("beam"),TEXT("beam_timber_2m"),TEXT("beam")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("wall"),TEXT("wall_timber_2m"),TEXT("plank")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("door"),TEXT("wall_door_timber_2m"),TEXT("plank")));
        HearthStructurePlan::RegisterRecipe(P,Recipe(TEXT("roof"),TEXT("roof_slope_timber_2m"),TEXT("plank")));
    }
    bool Add(FHearthStructurePlan& P,int32 N,float RoadYaw,const FString& Ext)
    {
        const FHearthStructurePlan Backup=P; auto Fail=[&](){P=Backup;return false;}; const FString K=FString::Printf(TEXT("room_%d"),N); if(!HearthStructurePlan::AppendRoom(P,K,N?TEXT("additional"):TEXT("living"),Ext)) return Fail();
        const float X=N*200.f, Floor=0.f, Frame=16.f, Top=256.f; FHearthStructureComponentSpec S;
        auto Put=[&](const TCHAR* Cat,const FString& Key,FVector Pos,float Yaw,const FString& R,const TCHAR* M,bool Need,const FString& Parent){return MakeSpec(Cat,Key,Pos,Yaw,R,M,1,Need,*Parent,S)&&HearthStructurePlan::AppendComponent(P,S,Ext);};
        if(!Put(TEXT("foundation_stone_2m"),K+TEXT("_foundation"),FVector(X,0,0),RoadYaw,TEXT("foundation"),TEXT("stone"),false,TEXT(""))) return Fail();
        if(!Put(TEXT("floor_timber_2m"),K+TEXT("_floor"),FVector(X,0,Floor),RoadYaw,TEXT("floor"),TEXT("plank"),true,K+TEXT("_foundation"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_bl"),FVector(X-91.f,-91.f,Frame),RoadYaw,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_br"),FVector(X+91.f,-91.f,Frame),RoadYaw,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_fl"),FVector(X-91.f,91.f,Frame),RoadYaw,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("post_timber_2_4m"),K+TEXT("_post_fr"),FVector(X+91.f,91.f,Frame),RoadYaw,TEXT("post"),TEXT("beam"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_back"),FVector(X,91.f,Top),RoadYaw,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_fl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_front"),FVector(X,-91.f,Top),RoadYaw,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_bl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_left"),FVector(X-91.f,0,Top),RoadYaw+90,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_bl"))) return Fail();
        if(!Put(TEXT("beam_timber_2m"),K+TEXT("_beam_right"),FVector(X+91.f,0,Top),RoadYaw+90,TEXT("beam"),TEXT("beam"),true,K+TEXT("_post_br"))) return Fail();
        if(!Put(TEXT("wall_timber_2m"),K+TEXT("_back"),FVector(X,100,Frame),RoadYaw+90,TEXT("wall"),TEXT("plank"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("wall_timber_2m"),K+TEXT("_side"),FVector(X-100,0,Frame),RoadYaw,TEXT("wall"),TEXT("plank"),true,K+TEXT("_floor"))) return Fail();
        if(!Put(TEXT("wall_door_timber_2m"),K+TEXT("_door"),FVector(X,-100,Frame),RoadYaw+90,TEXT("door"),TEXT("plank"),true,K+TEXT("_floor"))) return Fail();
        FHearthStructureOpening O; O.Offset=FVector2D(X,-100); O.AccessDirection=FVector2D(0,-1); O.Width=94; O.bDoor=true; if(!HearthStructurePlan::AppendOpening(P,K+TEXT("_front_door"),K,O,Ext)) return Fail();
        if(!Put(TEXT("roof_slope_timber_2m"),K+TEXT("_roof_left"),FVector(X,0,Top),RoadYaw,TEXT("roof"),TEXT("plank"),true,K+TEXT("_beam_right"))) return Fail();
        if(!Put(TEXT("roof_slope_timber_2m"),K+TEXT("_roof_right"),FVector(X,0,Top),RoadYaw+180,TEXT("roof"),TEXT("plank"),true,K+TEXT("_beam_left"))) return Fail();
        auto Link=[&](const TCHAR* Name,const FString& From,const FString& To){return HearthStructurePlan::AppendConnection(P,K+Name,From,To,true,Ext);};
        if(!Link(TEXT("_floor_support"),K+TEXT("_floor"),K+TEXT("_foundation")) || !Link(TEXT("_back_side"),K+TEXT("_back"),K+TEXT("_side")) || !Link(TEXT("_side_door"),K+TEXT("_side"),K+TEXT("_door")) || !Link(TEXT("_post_bl_support"),K+TEXT("_post_bl"),K+TEXT("_floor")) || !Link(TEXT("_post_br_support"),K+TEXT("_post_br"),K+TEXT("_floor")) || !Link(TEXT("_post_fl_support"),K+TEXT("_post_fl"),K+TEXT("_floor")) || !Link(TEXT("_post_fr_support"),K+TEXT("_post_fr"),K+TEXT("_floor")) || !Link(TEXT("_beam_back_support"),K+TEXT("_beam_back"),K+TEXT("_post_fl")) || !Link(TEXT("_beam_front_support"),K+TEXT("_beam_front"),K+TEXT("_post_bl")) || !Link(TEXT("_beam_left_support"),K+TEXT("_beam_left"),K+TEXT("_post_bl")) || !Link(TEXT("_beam_right_support"),K+TEXT("_beam_right"),K+TEXT("_post_br")) || !Link(TEXT("_beam_back_left"),K+TEXT("_beam_back"),K+TEXT("_beam_left")) || !Link(TEXT("_beam_back_right"),K+TEXT("_beam_back"),K+TEXT("_beam_right")) || !Link(TEXT("_beam_front_left"),K+TEXT("_beam_front"),K+TEXT("_beam_left")) || !Link(TEXT("_beam_front_right"),K+TEXT("_beam_front"),K+TEXT("_beam_right")) || !Link(TEXT("_back_support"),K+TEXT("_back"),K+TEXT("_floor")) || !Link(TEXT("_side_support"),K+TEXT("_side"),K+TEXT("_floor")) || !Link(TEXT("_door_support"),K+TEXT("_door"),K+TEXT("_floor")) || !Link(TEXT("_roof_left_support"),K+TEXT("_roof_left"),K+TEXT("_beam_right")) || !Link(TEXT("_roof_right_support"),K+TEXT("_roof_right"),K+TEXT("_beam_left")) || !Link(TEXT("_roof_pair"),K+TEXT("_roof_left"),K+TEXT("_roof_right"))) return Fail();
        return true;
    }
    FString Issues(const FHearthStructureValidationResult& V){FString R;for(const FString& I:V.Issues){if(!R.IsEmpty())R+=TEXT(",");R+=I;}return R;}
    bool HasTerm(const FString& S,const TCHAR* T){return S.Contains(T,ESearchCase::IgnoreCase,ESearchDir::FromStart);}
    bool HasExtension(const FHearthStructurePlan& P,const FString& K){return P.Components.ContainsByPredicate([&](const FHearthStructureComponent& C){return C.ExtensionId==K;});}
}

FHearthStructureValidationContext HearthResidentBuildingPlanner::ValidationContext(const FHearthResidentBuildingInput& I)
{
    FHearthStructureValidationContext C; C.AvailableBudget=FMath::Max(0,I.Budget); C.bRoadAccessible=I.bRoadAccessible; C.AvailableMaterials.Add(Mat(TEXT("stone"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Stone); C.AvailableMaterials.Add(Mat(TEXT("plank"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Planks); C.AvailableMaterials.Add(Mat(TEXT("beam"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Beams); return C;
}

FHearthResidentBuildingPlan HearthResidentBuildingPlanner::Build(const FHearthResidentBuildingInput& I)
{
    FHearthResidentBuildingPlan O; const FString Id=TEXT("resident_")+(I.ResidentId.IsEmpty()?TEXT("unknown"):I.ResidentId)+TEXT("_house"); const FString Seed=I.StableSeed.IsEmpty()?TEXT("resident-default"):I.StableSeed; const bool Family=I.HouseholdSize>=3||HasTerm(I.Need,TEXT("family"))||HasTerm(I.Need,TEXT("private")); const bool Workshop=HasTerm(I.Occupation,TEXT("craft"))||HasTerm(I.Occupation,TEXT("carpenter"))||HasTerm(I.Occupation,TEXT("smith")); const int32 Desired=FMath::Clamp(1+(Family?1:0)+(Workshop&&I.FriendsNearby>0?1:0),1,2); const int32 Affordable=FMath::Min3(I.Stone,I.Planks/7,FMath::Min(I.Beams/3,I.Budget/9)); const int32 Rooms=FMath::Clamp(FMath::Min(Desired,Affordable),0,2);
    FHearthStructureFootprint F; F.Origin=I.Origin; F.Size=FVector2D(FMath::Max(440.f,Rooms*200.f+240.f),280.f); F.Orientation=FRotator(0,I.RoadYaw,0); FHearthStructureReasonFields R; R.Need=I.Need; R.Occupation=I.Occupation; R.Budget=FString::Printf(TEXT("budget=%d; affordable_rooms=%d"),I.Budget,Rooms); R.Relationship=FString::Printf(TEXT("friends_nearby=%d"),I.FriendsNearby); R.RoadAccess=I.bRoadAccessible?TEXT("road-accessible"):TEXT("road-inaccessible"); O.Plan=HearthStructurePlan::MakePlan(Id,Seed,F,R); Register(O.Plan);
    if(!I.bRoadAccessible) O.Reason=TEXT("No buildable plan: the resident has no verified road access."); else if(Rooms==0) O.Reason=TEXT("No buildable plan: current stone, planks, beams, or budget cannot fund one core room."); else {const FHearthStructurePlan Before=O.Plan; bool Good=true;for(int32 N=0;N<Rooms;++N)if(!Add(O.Plan,N,I.RoadYaw,FString())){Good=false;break;}if(!Good){O.Plan=Before;O.Reason=TEXT("Plan assembly failed and was rolled back atomically.");}else{const auto V=HearthStructurePlan::Validate(O.Plan,ValidationContext(I));O.bBuildable=V.bValid;O.Reason=V.bValid?FString::Printf(TEXT("Built %d room(s) from finite current resources using native catalog parts."),Rooms):TEXT("Plan rejected by structural validation: ")+Issues(V);}}
    O.Expansion.ExtensionKey=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; O.Expansion.Reason=TEXT("Reserve one adjoining catalog-sized bay for later household growth."); O.Expansion.ResultingPlan=O.Plan; if(O.bBuildable)AppendExpansion(O,I); return O;
}

bool HearthResidentBuildingPlanner::AppendExpansion(FHearthResidentBuildingPlan& Existing,const FHearthResidentBuildingInput& I)
{
    if(!Existing.bBuildable)return false; const FString K=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; const FHearthStructurePlan Original=Existing.Expansion.ResultingPlan.Components.IsEmpty()?Existing.Plan:Existing.Expansion.ResultingPlan; if(HasExtension(Original,K))return false; FHearthStructurePlan Candidate=Original; const int32 N=Candidate.Rooms.Num(); if(!Add(Candidate,N,I.RoadYaw,K))return false; Candidate.Footprint.Size.X=FMath::Max(Candidate.Footprint.Size.X,2.f*(N*200.f+120.f)+240.f); if(!HearthStructurePlan::Validate(Candidate,ValidationContext(I)).bValid)return false; Existing.Expansion.ExtensionKey=K; Existing.Expansion.ResultingPlan=MoveTemp(Candidate); Existing.Expansion.Reason=FString::Printf(TEXT("Extension %s appends room_%d with finite current resources."),*K,N); return true;
}
