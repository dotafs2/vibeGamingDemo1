#include "HearthResidentBuildingPlanner.h"
#include "HearthBuildingArchetypes.h"
#include "HearthStructureCatalog.h"

namespace
{
    FHearthStructureMaterialQuantity Mat(const TCHAR* Id) { FHearthStructureMaterialQuantity M; M.MaterialId=Id; M.Quantity=1; return M; }
    FHearthStructureMaterialRecipe Recipe(const TCHAR* Id,const TCHAR* Catalog,const TCHAR* MaterialId,int32 Quantity=1)
    { FHearthStructureMaterialRecipe R; R.RecipeId=Id; R.CatalogId=Catalog; R.Inputs.Add(Mat(MaterialId)); R.Inputs.Last().Quantity=Quantity; return R; }
    struct FHearthHouseStyle
    {
        FString RequestedWall,RequestedRoof,WallChoice,RoofChoice,WallCatalog,DoorCatalog,RoofCatalog,WallResource,RoofResource,Substitution;
    };
    FHearthHouseStyle StyleFor(const FHearthResidentBuildingInput& I)
    {
        FHearthHouseStyle S;
        S.RequestedWall=I.WallMaterial.ToLower(); S.RequestedRoof=I.RoofMaterial.ToLower();
        S.WallChoice=S.RequestedWall==TEXT("stone")?TEXT("stone"):TEXT("timber");
        S.RoofChoice=S.RequestedRoof==TEXT("terracotta") && I.Tiles>=12?TEXT("terracotta"):TEXT("timber");
        if(S.RequestedWall==TEXT("plaster")) S.Substitution=TEXT("plaster preference deferred: no plaster production inventory; executable timber selected");
        if(S.RequestedRoof==TEXT("terracotta") && S.RoofChoice!=TEXT("terracotta"))
        {
            if(!S.Substitution.IsEmpty()) S.Substitution+=TEXT("; ");
            S.Substitution+=FString::Printf(TEXT("terracotta preference deferred: need 12 tiles for one room, have %d; executable timber selected"),FMath::Max(0,I.Tiles));
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
        S.RoofResource=S.RoofChoice==TEXT("terracotta")?TEXT("tiles"):TEXT("plank");
        return S;
    }
    bool MakeSpec(const TCHAR* Catalog,const FString& Key,const FVector& Base,float Yaw,const FString& RecipeId,const TCHAR* MaterialId,int32 Cost,bool Support,const TCHAR* Parent,FHearthStructureComponentSpec& Out,int32 Quantity=1)
    {
        const FHearthStructureCatalogEntry* E=HearthStructureCatalog::Find(Catalog); if(!E) return false;
        const FVector Bounds=(E->BoundsMax-E->BoundsMin)*100.f; const FVector BoundsMin=E->BoundsMin*100.f; if(Bounds.X<=0||Bounds.Y<=0||Bounds.Z<=0) return false;
        Out=FHearthStructureComponentSpec(); Out.CatalogId=Catalog; Out.SemanticKey=Key; Out.Offset=Base;
        Out.Height=Bounds.Z; Out.Size=FVector2D(Bounds.X,Bounds.Y); Out.BoundsMin=BoundsMin; Out.BoundsMax=E->BoundsMax*100.f;
        Out.Orientation=E->DefaultRotation+FRotator(0,Yaw,0);
        Out.RecipeId=RecipeId; Out.Materials.Add(Mat(MaterialId)); Out.Materials.Last().Quantity=Quantity; Out.MaterialCost=Cost; Out.CollisionRadius=FMath::Max(Bounds.X,Bounds.Y)*.5f; Out.bRequiresSupport=Support; Out.SupportsComponentKey=Parent; return true;
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
        HearthStructurePlan::RegisterRecipe(P,Recipe(*RoofRecipe,*S.RoofCatalog,*S.RoofResource,S.RoofChoice==TEXT("terracotta")?6:1));
    }
    bool Add(FHearthStructurePlan& P,int32 N,float RoadYaw,const FString& Ext,const FHearthHouseStyle& Style, const FVector2D* Wing=nullptr,const FVector2D& LayoutOffset=FVector2D::ZeroVector)
    {
        const FHearthStructurePlan Backup=P;
        auto Fail=[&](){P=Backup;return false;};
        const FString K=FString::Printf(TEXT("room_%d"),N);
        const FString Boundary=N>0?FString::Printf(TEXT("room_%d_side_right"),N-1):K+TEXT("_side_left");
        if(!HearthStructurePlan::AppendRoom(P,K,N?TEXT("additional"):TEXT("living"),Ext)) return Fail();
        const bool OwnWalls=N==0 || Wing!=nullptr;
        const float X=(Wing?Wing->X:N*200.f)+LayoutOffset.X, Y=(Wing?Wing->Y:0.f)+LayoutOffset.Y,Frame=16.f,Top=256.f;
        const FString WallRecipe=TEXT("wall_")+Style.WallChoice,DoorRecipe=TEXT("door_")+Style.WallChoice,RoofRecipe=TEXT("roof_")+Style.RoofChoice;
        FHearthStructureComponentSpec S;
        auto Put=[&](const TCHAR* Cat,const FString& Key,FVector Pos,float Yaw,const FString& RecipeId,const TCHAR* Material,bool Need,const FString& Parent)
        { Pos.Y+=Y; return MakeSpec(Cat,Key,Pos,Yaw,RecipeId,Material,1,Need,*Parent,S)&&HearthStructurePlan::AppendComponent(P,S,Ext);};
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
        if(OwnWalls&&!Put(*Style.WallCatalog,K+TEXT("_side_left"),FVector(X-100,0,0),90,WallRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        if(!Put(*Style.WallCatalog,K+TEXT("_side_right"),FVector(X+100,0,0),90,WallRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        if(!Put(*Style.DoorCatalog,K+TEXT("_door"),FVector(X,-100,0),0,DoorRecipe,*Style.WallResource,true,K+TEXT("_floor"))) return Fail();
        FHearthStructureOpening O; O.Offset=FVector2D(X,Y-100); O.AccessDirection=FVector2D(0,-1); O.Width=94; O.bDoor=true;
        if(!HearthStructurePlan::AppendOpening(P,K+TEXT("_front_door"),K,O,Ext)) return Fail();
        const int32 RoofQuantity=Style.RoofChoice==TEXT("terracotta")?6:1;
        if(!MakeSpec(*Style.RoofCatalog,K+TEXT("_roof_back"),FVector(X,Y,Top+13.1f),90,RoofRecipe,*Style.RoofResource,1,true,*(K+TEXT("_beam_back")),S,RoofQuantity)||!HearthStructurePlan::AppendComponent(P,S,Ext)) return Fail();
        if(!MakeSpec(*Style.RoofCatalog,K+TEXT("_roof_front"),FVector(X,Y,Top+13.1f),270,RoofRecipe,*Style.RoofResource,1,true,*(K+TEXT("_beam_front")),S,RoofQuantity)||!HearthStructurePlan::AppendComponent(P,S,Ext)) return Fail();

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
        if(OwnWalls)
        {
            if(!Link(TEXT("_left_bl"),K+TEXT("_side_left"),K+TEXT("_post_bl"))||!Link(TEXT("_left_fl"),K+TEXT("_side_left"),K+TEXT("_post_fl"))) return Fail();
        }
        else if(!Link(TEXT("_boundary_bl"),Boundary,K+TEXT("_post_bl"))||!Link(TEXT("_boundary_fl"),Boundary,K+TEXT("_post_fl"))) return Fail();
        return true;
    }
    FVector2D WingPosition(const FHearthStructurePlan& P,int32 Direction)
    {
        float MinX=0,MaxX=0,MaxY=0;
        for(const auto& C:P.Components) if(C.CatalogId==TEXT("floor_timber_2m"))
        { MinX=FMath::Min(MinX,static_cast<float>(C.Offset.X)); MaxX=FMath::Max(MaxX,static_cast<float>(C.Offset.X)); MaxY=FMath::Max(MaxY,static_cast<float>(C.Offset.Y)); }
        // Side bays use 400 cm centers; rear bays need 600 cm for the 450 cm roof.
        // This leaves usable outdoor circulation
        // and separates real roof overhangs, without invented support links.
        const int32 D=Direction==0?1+(P.Rooms.Num()-1)%3:Direction;
        return D==2?FVector2D(MinX-400,0):D==3?FVector2D(0,MaxY+600):FVector2D(MaxX+400,0);
    }
    void Enclose(FHearthStructurePlan& P)
    {
        for(const auto& C:P.Components)
        {
            const FVector Extent=C.BoundsMin.GetAbs().ComponentMax(C.BoundsMax.GetAbs());
            P.Footprint.Size.X=FMath::Max(P.Footprint.Size.X,2*(FMath::Abs(C.Offset.X)+Extent.Size2D()+20));
            P.Footprint.Size.Y=FMath::Max(P.Footprint.Size.Y,2*(FMath::Abs(C.Offset.Y)+Extent.Size2D()+20));
        }
    }
    int32 ArchetypeMinimumRooms(const FString& Archetype)
    {
        if (Archetype == TEXT("rowhouse")) return 1;
        if (Archetype == TEXT("shop_house") || Archetype == TEXT("courtyard_workshop")
            || Archetype == TEXT("warehouse") || Archetype == TEXT("inn")) return 2;
        return 0;
    }
    int32 ArchetypeRoomCount(const FString& Archetype, int32 Cap)
    {
        if (Archetype == TEXT("rowhouse")) return Cap;
        if (Archetype == TEXT("shop_house")) return FMath::Min(Cap, 4);
        if (Archetype == TEXT("courtyard_workshop")) return FMath::Min(Cap, 5);
        if (Archetype == TEXT("warehouse") || Archetype == TEXT("inn")) return FMath::Min(Cap, 6);
        return 0;
    }
    struct FRoomRecipeCost
    {
        int32 Budget = 0, Stone = 0, Planks = 0, Beams = 0, Tiles = 0;
        bool Affordable(const FHearthResidentBuildingInput& I) const
        {
            return Budget <= FMath::Max(0,I.Budget) && Stone <= FMath::Max(0,I.Stone)
                && Planks <= FMath::Max(0,I.Planks) && Beams <= FMath::Max(0,I.Beams)
                && Tiles <= FMath::Max(0,I.Tiles);
        }
    };
    // Mirrors Add: foundation + floor + 4 posts + 4 beams + walls + 2 slopes.
    // Only an attached row omits the left infill on each subsequent bay.
    FRoomRecipeCost RoomRecipeCost(const FHearthResidentBuildingInput& I,int32 Rooms,bool Attached)
    {
        FRoomRecipeCost C;
        if(Rooms<=0 || Rooms>6) return C;
        const int32 Walls=4*Rooms-(Attached?Rooms-1:0);
        const bool StoneWall=I.WallMaterial.Equals(TEXT("stone"),ESearchCase::IgnoreCase);
        const bool TileRoof=I.RoofMaterial.Equals(TEXT("terracotta"),ESearchCase::IgnoreCase) && I.Tiles>=12;
        C.Budget=12*Rooms+Walls;
        C.Stone=Rooms+(StoneWall?Walls:0);
        C.Planks=Rooms+(StoneWall?0:Walls)+(TileRoof?0:2*Rooms);
        C.Beams=8*Rooms;
        C.Tiles=TileRoof?12*Rooms:0;
        return C;
    }
    TArray<FVector2D> ArchetypeCenters(const FString& Archetype)
    {
        TArray<FVector2D> Centers;
        if (Archetype == TEXT("shop_house"))
        {
            // New plans start behind their street frontage. Saved plans are
            // never rebuilt from these centers when appending an extension.
            Centers = { FVector2D(0.f, 0.f), FVector2D(0.f, 600.f), FVector2D(0.f, 1200.f), FVector2D(0.f, 1800.f) };
        }
        else if (Archetype == TEXT("courtyard_workshop"))
        {
            Centers = { FVector2D(-400.f, 0.f), FVector2D(400.f, 0.f), FVector2D(0.f, 600.f), FVector2D(-400.f, 600.f), FVector2D(400.f, 600.f) };
        }
        else if (Archetype == TEXT("warehouse"))
        {
            Centers = { FVector2D(-200.f, 0.f), FVector2D(200.f, 0.f), FVector2D(-200.f, 600.f), FVector2D(200.f, 600.f), FVector2D(-200.f, 1200.f), FVector2D(200.f, 1200.f) };
        }
        else if (Archetype == TEXT("inn"))
        {
            Centers = { FVector2D(-400.f, -300.f), FVector2D(400.f, -300.f), FVector2D(0.f, 400.f), FVector2D(-400.f, 1000.f), FVector2D(400.f, 1000.f), FVector2D(0.f, 1600.f) };
        }
        return Centers;
    }
    FString ArchetypeRoomLabel(const FString& Archetype, int32 Index)
    {
        if (Archetype == TEXT("rowhouse")) return TEXT("rowhouse dwelling");
        if (Archetype == TEXT("shop_house"))
        {
            if (Index == 0) return TEXT("front shop");
            if (Index == 1) return TEXT("rear dwelling");
            return TEXT("shop-house service room");
        }
        if (Archetype == TEXT("courtyard_workshop"))
        {
            if (Index == 0) return TEXT("courtyard workshop");
            return TEXT("courtyard wing");
        }
        if (Archetype == TEXT("warehouse")) return FString::Printf(TEXT("warehouse bay %d"), Index + 1);
        if (Archetype == TEXT("inn"))
        {
            if (Index == 0) return TEXT("inn common room");
            if (Index == 1 || Index == 2) return TEXT("inn guest room");
            return TEXT("inn service room");
        }
        return TEXT("catalog room");
    }
    bool ConnectArchetypeRooms(FHearthStructurePlan& Plan, const FString& Archetype, int32 RoomCount)
    {
        auto Link = [&](int32 Index, int32 Other)
        {
            return HearthStructurePlan::AppendConnection(Plan,
                FString::Printf(TEXT("archetype_room_link_%d_%d"), Index, Other),
                FString::Printf(TEXT("room_%d_floor"), Index), FString::Printf(TEXT("room_%d_floor"), Other), false);
        };
        if (Archetype == TEXT("rowhouse") || Archetype == TEXT("shop_house"))
        {
            for (int32 Index = 1; Index < RoomCount; ++Index) if (!Link(Index - 1, Index)) return false;
            return true;
        }
        if (Archetype == TEXT("courtyard_workshop"))
        {
            for (int32 Index = 1; Index < RoomCount; ++Index) if (!Link(0, Index)) return false;
            return true;
        }
        if (Archetype == TEXT("warehouse"))
        {
            for (int32 Index = 1; Index < RoomCount; ++Index) if (!Link(Index - 1, Index)) return false;
            return true;
        }
        if (Archetype == TEXT("inn"))
        {
            for (int32 Index = 1; Index < RoomCount; ++Index) if (!Link(0, Index)) return false;
            return true;
        }
        return false;
    }
    FString Issues(const FHearthStructureValidationResult& V);
    FHearthResidentBuildingPlan BuildArchetype(const FHearthResidentBuildingInput& I)
    {
        FHearthResidentBuildingPlan Output;
        const FString Archetype = I.Archetype.ToLower();
        const FString Id = TEXT("resident_") + (I.ResidentId.IsEmpty() ? TEXT("unknown") : I.ResidentId) + TEXT("_house_") + Archetype;
        const FString Seed = I.StableSeed.IsEmpty() ? TEXT("resident-default") : I.StableSeed;
        FHearthStructureFootprint Footprint;
        Footprint.Origin = I.Origin;
        Footprint.Size = FVector2D(1000.f, 1000.f);
        Footprint.Orientation = FRotator(0.f, I.RoadYaw, 0.f);
        FHearthStructureReasonFields Reasons;
        Reasons.Need = I.Need;
        Reasons.Occupation = I.Occupation;
        Reasons.Budget = FString::Printf(TEXT("archetype=%s; budget=%d; finite catalog materials stone=%d planks=%d beams=%d tiles=%d"), *Archetype, I.Budget, I.Stone, I.Planks, I.Beams, I.Tiles);
        Reasons.Relationship = FString::Printf(TEXT("friends_nearby=%d"), I.FriendsNearby);
        Reasons.RoadAccess = I.bRoadAccessible ? TEXT("road-accessible") : TEXT("road-inaccessible");
        Output.Plan = HearthStructurePlan::MakePlan(Id, Seed, Footprint, Reasons);
        Output.Expansion.ExtensionKey = I.ExtensionKey.IsEmpty() ? TEXT("resident_extension_1") : I.ExtensionKey;
        Output.Expansion.ResultingPlan = Output.Plan;

        if (Archetype == TEXT("keep"))
        {
            Output.Reason = TEXT("Keep archetype matches the resident use rules, but the main keep module owns its castle plan.");
            Output.Expansion.Reason = TEXT("Keep construction is reserved for the main keep module.");
            return Output;
        }
        if (ArchetypeMinimumRooms(Archetype) == 0)
        {
            Output.Reason = FString::Printf(TEXT("No buildable plan: unknown residential archetype '%s'."), *I.Archetype);
            return Output;
        }
        if (!I.bRoadAccessible)
        {
            Output.Reason = TEXT("No buildable plan: the archetype has no verified road access.");
            return Output;
        }
        if(I.LayoutOffset.ContainsNaN() || I.LayoutOffset.Z!=0.0 || I.Origin.ContainsNaN() || !FMath::IsFinite(I.RoadYaw))
        {
            Output.Reason=TEXT("No buildable plan: unsupported layout transform; only finite ground-level XY offsets are supported.");
            return Output;
        }
        const int32 Cap = FMath::Clamp(I.MaxInitialRooms, 0, 6);
        const int32 Minimum = ArchetypeMinimumRooms(Archetype);
        if (Cap < Minimum)
        {
            Output.Reason = FString::Printf(TEXT("No buildable %s plan: this layout requires at least %d rooms, but MaxInitialRooms=%d."), *Archetype, Minimum, I.MaxInitialRooms);
            return Output;
        }
        const int32 RoomCount = ArchetypeRoomCount(Archetype, Cap);
        const FRoomRecipeCost Required=RoomRecipeCost(I,RoomCount,Archetype==TEXT("rowhouse"));
        if(!Required.Affordable(I))
        {
            FString Shortage;
            if(Required.Budget>FMath::Max(0,I.Budget)) Shortage+=TEXT("budget_exceeded ");
            if(Required.Stone>FMath::Max(0,I.Stone)) Shortage+=TEXT("material_shortage:stone ");
            if(Required.Planks>FMath::Max(0,I.Planks)) Shortage+=TEXT("material_shortage:plank ");
            if(Required.Beams>FMath::Max(0,I.Beams)) Shortage+=TEXT("material_shortage:beam ");
            if(Required.Tiles>FMath::Max(0,I.Tiles)) Shortage+=TEXT("material_shortage:tiles ");
            Output.Reason=FString::Printf(TEXT("No buildable %s plan: %s; required budget=%d stone=%d planks=%d beams=%d tiles=%d; available budget=%d stone=%d planks=%d beams=%d tiles=%d."),
                *Archetype,*Shortage,Required.Budget,Required.Stone,Required.Planks,Required.Beams,Required.Tiles,
                FMath::Max(0,I.Budget),FMath::Max(0,I.Stone),FMath::Max(0,I.Planks),FMath::Max(0,I.Beams),FMath::Max(0,I.Tiles));
            return Output;
        }
        const FHearthHouseStyle Style = StyleFor(I);
        Register(Output.Plan, Style);
        const TArray<FVector2D> Centers = ArchetypeCenters(Archetype);
        const FHearthStructurePlan Before = Output.Plan;
        bool bAssembled = true;
        for (int32 Index = 0; Index < RoomCount; ++Index)
        {
            const FVector2D* Center = Archetype == TEXT("rowhouse") ? nullptr : &Centers[Index];
            if (!Add(Output.Plan, Index, I.RoadYaw, FString(), Style, Center,FVector2D(I.LayoutOffset.X,I.LayoutOffset.Y)))
            {
                bAssembled = false;
                break;
            }
            Output.Plan.Rooms.Last().Label = ArchetypeRoomLabel(Archetype, Index);
        }
        if (bAssembled && Archetype == TEXT("courtyard_workshop"))
        {
            FHearthStructureOpening CourtyardAccess;
            CourtyardAccess.Offset = FVector2D(I.LayoutOffset.X, I.LayoutOffset.Y-100.f);
            CourtyardAccess.AccessDirection = FVector2D(0.f, -1.f);
            CourtyardAccess.Width = 120.f;
            CourtyardAccess.bDoor = false;
            bAssembled = HearthStructurePlan::AppendOpening(Output.Plan, TEXT("courtyard_access"), TEXT("room_0"), CourtyardAccess);
            if (bAssembled) Output.Plan.Reasons.Relationship += TEXT("; open front court with non-door courtyard access");
        }
        if (bAssembled) bAssembled = ConnectArchetypeRooms(Output.Plan, Archetype, RoomCount);
        if (!bAssembled)
        {
            Output.Plan = Before;
            Output.Reason = TEXT("Archetype assembly failed and was rolled back atomically.");
            Output.Expansion.ResultingPlan = Output.Plan;
            return Output;
        }
        Enclose(Output.Plan);
        const FHearthStructureValidationResult Validation = HearthStructurePlan::Validate(Output.Plan, HearthResidentBuildingPlanner::ValidationContext(I));
        Output.bBuildable = Validation.bValid;
        Output.Reason = Validation.bValid
            ? FString::Printf(TEXT("Built %s with %d catalog-backed room(s), distinct single-storey geometry, and connected usable entrances."), *Archetype, RoomCount)
            : TEXT("Plan rejected by structural or finite-resource validation: ") + Issues(Validation);
        Output.Expansion.ResultingPlan = Output.Plan;
        Output.Expansion.Reason = RoomCount>=6?TEXT("Six-room archetype limit reached."):
            TEXT("A separately entered wing may be appended in the reviewed direction, subject to incremental resources and external siting checks.");
        return Output;
    }
    FString Issues(const FHearthStructureValidationResult& V){FString R;for(const FString& I:V.Issues){if(!R.IsEmpty())R+=TEXT(",");R+=I;}return R;}
    bool HasTerm(const FString& S,const TCHAR* T){return S.Contains(T,ESearchCase::IgnoreCase,ESearchDir::FromStart);}
    struct FHearthResidentGoalUse
    {
        bool bPrivateDomestic = false;
        bool bWorkshop = false;
        bool bStorage = false;
        bool bNeighborCourtyard = false;
    };
    FHearthResidentGoalUse ClassifyGoal(const FString& Need)
    {
        FHearthResidentGoalUse G;
        // Narrow building-use phrases; household and role data remain separate.
        G.bPrivateDomestic = HasTerm(Need,TEXT("quiet")) || HasTerm(Need,TEXT("private")) || HasTerm(Need,TEXT("domestic")) || HasTerm(Need,TEXT("安静")) || HasTerm(Need,TEXT("私密")) || HasTerm(Need,TEXT("私人")) || HasTerm(Need,TEXT("家庭"));
        G.bWorkshop = HasTerm(Need,TEXT("workshop")) || HasTerm(Need,TEXT("supporting store")) || HasTerm(Need,TEXT("support store")) || HasTerm(Need,TEXT("作坊")) || HasTerm(Need,TEXT("工坊"));
        G.bStorage = HasTerm(Need,TEXT("storage")) || HasTerm(Need,TEXT("store room")) || HasTerm(Need,TEXT("storeroom")) || HasTerm(Need,TEXT("supporting store")) || HasTerm(Need,TEXT("support store")) || HasTerm(Need,TEXT("储藏")) || HasTerm(Need,TEXT("仓储"));
        G.bNeighborCourtyard = HasTerm(Need,TEXT("neighbor-facing courtyard")) || HasTerm(Need,TEXT("neighbor courtyard")) || HasTerm(Need,TEXT("courtyard")) || HasTerm(Need,TEXT("邻里")) || HasTerm(Need,TEXT("邻居")) || HasTerm(Need,TEXT("庭院")) || HasTerm(Need,TEXT("院落"));
        return G;
    }
    FString RoomPurpose(const FHearthResidentGoalUse& Goal, int32 Index)
    {
        if(Index==0) return Goal.bPrivateDomestic ? TEXT("living / private domestic") : TEXT("living");
        if(Goal.bWorkshop && Index==1) return Goal.bStorage ? TEXT("workshop / supporting store") : TEXT("workshop");
        if(Goal.bStorage && Index==1) return TEXT("storage / support");
        if(Goal.bNeighborCourtyard) return TEXT("neighbor-facing courtyard");
        return TEXT("storage / support");
    }
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
                else if(Used.MaterialId==TEXT("tiles")) R.Tiles=FMath::Max(0,R.Tiles-Used.Quantity);
            }
        }
        return R;
    }
}

int32 HearthResidentBuildingPlanner::MinimumInitialRooms(const FHearthResidentBuildingInput& I)
{
    return I.Archetype.IsEmpty()?1:ArchetypeMinimumRooms(I.Archetype.ToLower());
}

int32 HearthResidentBuildingPlanner::MaxAffordableInitialRooms(const FHearthResidentBuildingInput& I)
{
    const int32 Minimum=MinimumInitialRooms(I);
    if(Minimum==0) return 0;
    const FString A=I.Archetype.ToLower();
    const int32 Cap=I.Archetype.IsEmpty()?FMath::Clamp(I.MaxInitialRooms,0,3):
        ArchetypeRoomCount(A,FMath::Clamp(I.MaxInitialRooms,0,6));
    const bool Attached=A==TEXT("rowhouse") || (A.IsEmpty() && I.GrowthDirection<0);
    for(int32 Rooms=Cap;Rooms>=Minimum;--Rooms)
        if(RoomRecipeCost(I,Rooms,Attached).Affordable(I)) return Rooms;
    return 0;
}

namespace
{
    FBox LocalComponentBounds(const FHearthStructureComponent& C)
    {
        FBox B(ForceInit);
        for(int32 X=0;X<2;++X) for(int32 Y=0;Y<2;++Y) for(int32 Z=0;Z<2;++Z)
            B+=C.Offset+C.Orientation.RotateVector(FVector(X?C.BoundsMax.X:C.BoundsMin.X,
                Y?C.BoundsMax.Y:C.BoundsMin.Y,Z?C.BoundsMax.Z:C.BoundsMin.Z));
        return B;
    }
    bool PassageClear(const FHearthStructurePlan& P,const FVector2D& From,const FVector2D& To,const FString& DoorId)
    {
        // A 94cm-wide, 190cm-high ground-level passage, matching the actual door
        // clearance. Only its own catalog doorway is exempt, never other walls.
        constexpr double HalfWidth=47.0;
        const FBox Passage(FVector(FMath::Min(From.X,To.X)-HalfWidth,FMath::Min(From.Y,To.Y)-HalfWidth,16.1),
            FVector(FMath::Max(From.X,To.X)+HalfWidth,FMath::Max(From.Y,To.Y)+HalfWidth,206.0));
        for(const auto& C:P.Components)
        {
            if(C.Id==DoorId) continue;
            if(Passage.Intersect(LocalComponentBounds(C))) return false;
        }
        return true;
    }
    bool ClearBayEntrances(const FHearthStructurePlan& P)
    {
        for(int32 N=0;N<P.Rooms.Num();++N)
        {
            const FString K=FString::Printf(TEXT("room_%d"),N);
            const FString DoorId=HearthStructurePlan::StableId(P,TEXT("component"),K+TEXT("_door"));
            const FString OpeningId=HearthStructurePlan::StableId(P,TEXT("opening"),K+TEXT("_front_door"));
            const auto* Door=P.Components.FindByPredicate([&](const auto& C){return C.Id==DoorId;});
            const auto* Opening=P.Openings.FindByPredicate([&](const auto& O){return O.Id==OpeningId;});
            const auto* Catalog=Door?HearthStructureCatalog::Find(Door->CatalogId):nullptr;
            if(!Door || !Opening || !Catalog || !Catalog->bHasDoorClearance || !Opening->bDoor
                || Door->Offset.Z!=0.0 || !Door->Orientation.Equals(FRotator::ZeroRotator)
                || !Opening->AccessDirection.Equals(FVector2D(0,-1)) || Opening->Width<94.f
                || Opening->RoomId!=HearthStructurePlan::StableId(P,TEXT("room"),K)
                || !Opening->Offset.Equals(FVector2D(Door->Offset.X,Door->Offset.Y))
                || (Catalog->DoorClearanceMax.X-Catalog->DoorClearanceMin.X)*100.f<Opening->Width-.01f
                || Catalog->DoorClearanceMin.Y>.161f || Catalog->DoorClearanceMax.Y<2.059f) return false;
            if(!PassageClear(P,Opening->Offset,Opening->Offset+FVector2D(0,-200),DoorId)) return false;
        }
        return true;
    }
    FHearthStructureValidationContext ExpansionContext(const FHearthResidentBuildingInput& I,const FHearthStructurePlan& Committed)
    {
        auto C=HearthResidentBuildingPlanner::ValidationContext(I);
        auto AddBounded=[](int32 A,int32 B){return static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(A)+B));};
        for(const auto& Part:Committed.Components)
        {
            C.AvailableBudget=AddBounded(C.AvailableBudget,Part.MaterialCost);
            for(const auto& Used:Part.Materials)
                if(auto* Available=C.AvailableMaterials.FindByPredicate([&](const auto& M){return M.MaterialId==Used.MaterialId;}))
                    Available->Quantity=AddBounded(Available->Quantity,Used.Quantity);
        }
        return C;
    }
    bool AppendArchetypeExpansion(FHearthResidentBuildingPlan& Existing,const FHearthResidentBuildingInput& I)
    {
        const FString A=I.Archetype.ToLower();
        const int32 Minimum=ArchetypeMinimumRooms(A);
        if(!Existing.bBuildable || Minimum==0 || !I.bRoadAccessible || I.GrowthDirection<-1 || I.GrowthDirection>3
            || !RoomRecipeCost(I,1,false).Affordable(I)) return false;
        const auto& Original=Existing.Expansion.ResultingPlan.Components.IsEmpty()?Existing.Plan:Existing.Expansion.ResultingPlan;
        const FString ExpectedId=TEXT("resident_")+(I.ResidentId.IsEmpty()?TEXT("unknown"):I.ResidentId)+TEXT("_house_")+A;
        const int32 N=Original.Rooms.Num();
        const FString Ext=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey;
        if(Original.PlanId!=ExpectedId || Original.PlanId!=Existing.Plan.PlanId || N<Minimum || N>=6
            || Original.Components.Num()>96 || HasExtension(Original,Ext) || Original.Footprint.Origin.ContainsNaN()
            || Original.Footprint.Orientation.ContainsNaN() || Original.Footprint.Orientation.Pitch!=0.0
            || Original.Footprint.Orientation.Roll!=0.0) return false;
        auto Context=ExpansionContext(I,Original);
        // Reject invalid saved input; extending must not repair bad bounds or
        // legitimize overlaps by enlarging the footprint or adding fake links.
        if(!HearthStructurePlan::Validate(Original,Context).bValid) return false;
        FVector2D Min(FLT_MAX,FLT_MAX),Max(-FLT_MAX,-FLT_MAX);
        int32 Floors=0;
        for(const auto& C:Original.Components) if(C.CatalogId==TEXT("floor_timber_2m"))
        {
            if(C.Offset.ContainsNaN() || C.Offset.Z!=0.0 || !C.Orientation.Equals(FRotator::ZeroRotator)) return false;
            Min.X=FMath::Min(Min.X,C.Offset.X); Min.Y=FMath::Min(Min.Y,C.Offset.Y);
            Max.X=FMath::Max(Max.X,C.Offset.X); Max.Y=FMath::Max(Max.Y,C.Offset.Y); ++Floors;
        }
        if(Floors!=N) return false;
        const int32 Direction=I.GrowthDirection<=0?1+(N-1)%3:I.GrowthDirection;
        // Grow the selected edge of the SAVED layout. Shops keep service wings
        // at the rear; courts develop rear corners; warehouses continue a bay
        // column; inns retain their open forecourt. Every new bay has four walls.
        FVector2D Wing;
        if(Direction==3)
            Wing=FVector2D(A==TEXT("warehouse") || A==TEXT("rowhouse")?Min.X:(Min.X+Max.X)*.5,Max.Y+600);
        else
        {
            const double Y=A==TEXT("shop_house") || A==TEXT("warehouse")?Max.Y:
                A==TEXT("courtyard_workshop")?Max.Y+600:Min.Y;
            Wing=FVector2D(Direction==2?Min.X-400:Max.X+400,Y);
        }
        FHearthStructurePlan Candidate=Original;
        const FHearthHouseStyle Style=StyleFor(I);
        Register(Candidate,Style);
        if(!Add(Candidate,N,Original.Footprint.Orientation.Yaw,Ext,Style,&Wing) || !ClearBayEntrances(Candidate)) return false;
        Candidate.Rooms.Last().Label=ArchetypeRoomLabel(A,N)+TEXT(" / independently entered wing");
        Min.X=FMath::Min(Min.X,Wing.X); Min.Y=FMath::Min(Min.Y,Wing.Y);
        Max.X=FMath::Max(Max.X,Wing.X); Max.Y=FMath::Max(Max.Y,Wing.Y);
        const FString DoorId=HearthStructurePlan::StableId(Candidate,TEXT("component"),FString::Printf(TEXT("room_%d_door"),N));
        const FVector2D Door(Wing.X,Wing.Y-100),Landing(Wing.X,Wing.Y-300);
        TArray<FVector2D> Route;
        for(int32 Attempt=0;Attempt<2 && Route.IsEmpty();++Attempt)
        {
            const bool Left=(Direction==2) != (Attempt==1);
            const double SideX=Left?Min.X-200:Max.X+200;
            TArray<FVector2D> Trial={Door,Landing,FVector2D(SideX,Landing.Y),FVector2D(SideX,Min.Y-300)};
            bool Clear=true;
            for(int32 Segment=1;Segment<Trial.Num();++Segment)
                if(!PassageClear(Candidate,Trial[Segment-1],Trial[Segment],DoorId)) {Clear=false;break;}
            if(Clear) Route=MoveTemp(Trial);
        }
        if(Route.IsEmpty()) return false;
        Enclose(Candidate);
        FString RouteText;
        for(const auto& Point:Route)
        {
            Candidate.Footprint.Size.X=FMath::Max(Candidate.Footprint.Size.X,2.0*(FMath::Abs(Point.X)+47.0));
            Candidate.Footprint.Size.Y=FMath::Max(Candidate.Footprint.Size.Y,2.0*(FMath::Abs(Point.Y)+47.0));
            RouteText+=FString::Printf(TEXT(" (%.0f,%.0f)"),Point.X,Point.Y);
        }
        if(!HearthStructurePlan::Validate(Candidate,Context).bValid) return false;
        const FRoomRecipeCost Cost=RoomRecipeCost(I,1,false);
        const FString Trace=FString::Printf(TEXT(" | extension[%s] archetype=%s direction=%d"),*Ext,*A,Direction);
        Candidate.Reasons.Need+=Trace+TEXT(" need=")+I.Need;
        Candidate.Reasons.Occupation+=Trace+TEXT(" occupation=")+I.Occupation;
        Candidate.Reasons.Budget+=Trace+FString::Printf(TEXT(" incremental budget=%d stone=%d planks=%d beams=%d tiles=%d"),Cost.Budget,Cost.Stone,Cost.Planks,Cost.Beams,Cost.Tiles);
        if(!Style.Substitution.IsEmpty()) Candidate.Reasons.Budget+=TEXT("; ")+Style.Substitution;
        Candidate.Reasons.RoadAccess+=Trace+TEXT(" local outdoor passage width=94cm:")+RouteText+TEXT("; external siting still required");
        Existing.Expansion.ExtensionKey=Ext;
        Existing.Expansion.Reason=TEXT("Independent archetype wing; 16 new catalog parts; only incremental materials and budget charged; external siting still required.");
        Existing.Expansion.ResultingPlan=MoveTemp(Candidate);
        return true;
    }

    bool CanopyHasComponent(const FHearthStructurePlan& Plan, const FString& ExtensionKey, const FString& SemanticKey)
    {
        const FString Id=HearthStructurePlan::StableId(Plan,TEXT("component"),SemanticKey);
        return Plan.Components.ContainsByPredicate([&](const FHearthStructureComponent& Component)
        { return Component.Id==Id && Component.ExtensionId==ExtensionKey; });
    }

    FString CanopySemanticKey(const FHearthStructurePlan& Plan, const FString& ComponentId)
    {
        const FString Prefix=Plan.PlanId+TEXT(":")+Plan.StableSeed+TEXT(":component:");
        return ComponentId.StartsWith(Prefix)?ComponentId.Mid(Prefix.Len()):FString();
    }

    bool AppendCanopyFailure(FHearthResidentBuildingPlan& Existing, const FString& Reason)
    {
        Existing.Expansion.Reason=TEXT("Canopy rejected: ")+Reason;
        return false;
    }

    FBox CatalogLocalBounds(const FHearthStructureCatalogEntry& Catalog, float Yaw)
    {
        FHearthStructureComponent Component;
        Component.Offset=FVector::ZeroVector;
        Component.Orientation=Catalog.DefaultRotation+FRotator(0.f,Yaw,0.f);
        Component.BoundsMin=Catalog.BoundsMin*100.f;
        Component.BoundsMax=Catalog.BoundsMax*100.f;
        return LocalComponentBounds(Component);
    }

    float BoxProjectionMin(const FBox& Box, const FVector2D& Axis)
    {
        float Result=FLT_MAX;
        for (int32 X=0;X<2;++X) for (int32 Y=0;Y<2;++Y)
        {
            const FVector2D Point(X?Box.Max.X:Box.Min.X,Y?Box.Max.Y:Box.Min.Y);
            Result=FMath::Min(Result,FVector2D::DotProduct(Point,Axis));
        }
        return Result;
    }

    float BoxProjectionMax(const FBox& Box, const FVector2D& Axis)
    {
        float Result=-FLT_MAX;
        for (int32 X=0;X<2;++X) for (int32 Y=0;Y<2;++Y)
        {
            const FVector2D Point(X?Box.Max.X:Box.Min.X,Y?Box.Max.Y:Box.Min.Y);
            Result=FMath::Max(Result,FVector2D::DotProduct(Point,Axis));
        }
        return Result;
    }

    bool BoxOverlapsXY(const FBox& A, const FBox& B, float Tolerance=0.f)
    {
        return A.Min.X<=B.Max.X+Tolerance && B.Min.X<=A.Max.X+Tolerance
            && A.Min.Y<=B.Max.Y+Tolerance && B.Min.Y<=A.Max.Y+Tolerance;
    }

    bool BoxTouches3D(const FBox& A, const FBox& B, float Tolerance)
    {
        return A.Min.X<=B.Max.X+Tolerance && B.Min.X<=A.Max.X+Tolerance
            && A.Min.Y<=B.Max.Y+Tolerance && B.Min.Y<=A.Max.Y+Tolerance
            && A.Min.Z<=B.Max.Z+Tolerance && B.Min.Z<=A.Max.Z+Tolerance;
    }

    const FHearthStructureComponent* CanopyComponent(const FHearthStructurePlan& Plan, const TCHAR* Key)
    {
        const FString Id=HearthStructurePlan::StableId(Plan,TEXT("component"),Key);
        return Plan.Components.FindByPredicate([&](const FHearthStructureComponent& Component){return Component.Id==Id;});
    }

    bool ValidateCanopyGeometry(const FHearthStructurePlan& Plan, const FHearthStructureComponent* HostAnchor,
        const FHearthStructureComponent* HostFloor, const FHearthStructureComponent* HostRoof,
        float CanopyYaw, FString& OutError)
    {
        const FHearthStructureComponent* Deck=CanopyComponent(Plan,TEXT("canopy_deck"));
        const FHearthStructureComponent* Roof=CanopyComponent(Plan,TEXT("canopy_roof"));
        const FHearthStructureComponent* BenchLeft=CanopyComponent(Plan,TEXT("canopy_bench_left"));
        const FHearthStructureComponent* BenchRight=CanopyComponent(Plan,TEXT("canopy_bench_right"));
        const FHearthStructureComponent* Beam=CanopyComponent(Plan,TEXT("canopy_beam"));
        const FHearthStructureComponent* Ridge=CanopyComponent(Plan,TEXT("canopy_ridge"));
        if (!Deck || !Roof || !BenchLeft || !BenchRight || !Beam || !Ridge) { OutError=TEXT("canopy_geometry_components_missing"); return false; }
        const auto* RoofCatalog=HearthStructureCatalog::Find(Roof->CatalogId);
        const auto* BeamCatalog=HearthStructureCatalog::Find(Beam->CatalogId);
        const auto* RidgeCatalog=HearthStructureCatalog::Find(Ridge->CatalogId);
        auto SocketPoint=[](const FHearthStructureComponent& Component,const FHearthStructureCatalogEntry* Catalog,const TCHAR* SocketId,FVector& Point)
        {
            const auto* Socket=Catalog?Catalog->Sockets.FindByPredicate([&](const FHearthStructureCatalogSocket& S){return S.Id==SocketId;}):nullptr;
            if (!Socket) return false;
            Point=Component.Offset+Component.Orientation.RotateVector(Socket->LocalPosition*100.f);
            return true;
        };
        FVector RoofSupport,BeamTop,RoofRidge,RidgeBase;
        if (!SocketPoint(*Roof,RoofCatalog,TEXT("support_bottom"),RoofSupport)
            || !SocketPoint(*Beam,BeamCatalog,TEXT("support_top"),BeamTop) || !RoofSupport.Equals(BeamTop,.01f)
            || !SocketPoint(*Roof,RoofCatalog,TEXT("ridge"),RoofRidge)
            || !SocketPoint(*Ridge,RidgeCatalog,TEXT("support_bottom"),RidgeBase) || !RoofRidge.Equals(RidgeBase,.01f))
        { OutError=TEXT("canopy_roof_frame_or_ridge_socket_gap"); return false; }
        const FBox DeckBounds=LocalComponentBounds(*Deck), RoofBounds=LocalComponentBounds(*Roof);
        const FBox BenchLeftBounds=LocalComponentBounds(*BenchLeft), BenchRightBounds=LocalComponentBounds(*BenchRight);
        if (!BoxOverlapsXY(DeckBounds,BenchLeftBounds) || !BoxOverlapsXY(DeckBounds,BenchRightBounds)
            || !BoxOverlapsXY(RoofBounds,BenchLeftBounds,1.f) || !BoxOverlapsXY(RoofBounds,BenchRightBounds,1.f))
        { OutError=TEXT("canopy_seats_outside_reachable_deck_or_roof"); return false; }
        if (BoxOverlapsXY(BenchLeftBounds,BenchRightBounds,1.f)) { OutError=TEXT("canopy_seats_overlap"); return false; }
        if (!HostAnchor) return true;

        const FVector Outward3=FRotator(0.f,CanopyYaw,0.f).RotateVector(FVector(0.f,-1.f,0.f));
        const FVector2D Outward(Outward3.X,Outward3.Y);
        const FBox HostBounds=LocalComponentBounds(*HostAnchor);
        const float HostFront=BoxProjectionMax(HostBounds,Outward);
        const float CanopyBack=BoxProjectionMin(RoofBounds,Outward);
        const float DeckBack=BoxProjectionMin(DeckBounds,Outward);
        if (CanopyBack<HostFront || CanopyBack>HostFront+8.f)
        { OutError=FString::Printf(TEXT("canopy_roof_to_host_gap_out_of_tolerance:%.1f"),CanopyBack-HostFront); return false; }
        if (DeckBack<HostFront+1.f || DeckBack>HostFront+25.f)
        { OutError=FString::Printf(TEXT("canopy_deck_to_host_gap_out_of_tolerance:%.1f"),DeckBack-HostFront); return false; }
        if (BoxOverlapsXY(DeckBounds,HostBounds) && DeckBounds.Min.Z<HostBounds.Max.Z && HostBounds.Min.Z<DeckBounds.Max.Z)
        { OutError=TEXT("canopy_deck_blocks_host_entry"); return false; }
        if (HostFloor && HostFloor->Orientation.Equals(FRotator(0.f,CanopyYaw,0.f)))
        {
            const float FloorFront=BoxProjectionMax(LocalComponentBounds(*HostFloor),Outward);
            if (DeckBack<FloorFront || DeckBack>FloorFront+25.f)
            { OutError=FString::Printf(TEXT("canopy_deck_to_porch_gap_out_of_tolerance:%.1f"),DeckBack-FloorFront); return false; }
        }
        if (HostRoof && !BoxTouches3D(RoofBounds,LocalComponentBounds(*HostRoof),5.f))
        { OutError=TEXT("canopy_roof_does_not_reach_host_eave_bounds"); return false; }
        return true;
    }

    bool AppendTavernCanopy(FHearthResidentBuildingPlan& Existing, const FHearthResidentBuildingInput& I)
    {
        const FString ExtensionKey=I.ExtensionKey.IsEmpty()?TEXT("tavern_canopy_v1"):I.ExtensionKey;
        const FHearthStructurePlan Original=Existing.Expansion.ResultingPlan.Components.IsEmpty()?Existing.Plan:Existing.Expansion.ResultingPlan;
        if (Original.PlanId.IsEmpty() || Original.StableSeed.IsEmpty()) return AppendCanopyFailure(Existing,TEXT("host plan has no stable identity"));
        if (!I.bRoadAccessible) return AppendCanopyFailure(Existing,TEXT("host has no verified road access"));

        const FString ExpectedKeys[]={TEXT("canopy_deck"),TEXT("canopy_post_left"),TEXT("canopy_post_right"),TEXT("canopy_beam"),TEXT("canopy_roof"),TEXT("canopy_ridge"),TEXT("canopy_bench_left"),TEXT("canopy_bench_right")};
        const bool bHasAny=Original.Components.ContainsByPredicate([&](const FHearthStructureComponent& Component){return Component.ExtensionId==ExtensionKey;});
        const bool bHostAttachmentExpected=Original.Components.ContainsByPredicate([&](const FHearthStructureComponent& Component)
        { return Component.ExtensionId!=ExtensionKey; });
        bool bComplete=(!bHostAttachmentExpected || (!Original.Attachments.IsEmpty() && Original.Attachments.ContainsByPredicate([&](const FHearthStructureAttachment& Attachment)
        { return Attachment.Id==HearthStructurePlan::StableId(Original,TEXT("attachment"),ExtensionKey); })));
        for (const FString& Key:ExpectedKeys) bComplete=bComplete&&CanopyHasComponent(Original,ExtensionKey,Key);
        if (bHasAny)
        {
            if (!bComplete) return AppendCanopyFailure(Existing,TEXT("host contains a partial canopy extension"));
            Existing.Expansion.ExtensionKey=ExtensionKey;
            Existing.Expansion.ResultingPlan=Original;
            Existing.Expansion.Reason=TEXT("Canopy already present; stable component IDs were reused without mutation.");
            return true;
        }

        const FHearthStructureComponent* HostFloor=Original.Components.FindByPredicate([](const FHearthStructureComponent& Component)
        { return Component.CatalogId==TEXT("floor_timber_2m"); });
        const FHearthStructureComponent* HostDoor=Original.Components.FindByPredicate([](const FHearthStructureComponent& Component)
        { return Component.CatalogId.StartsWith(TEXT("wall_door_")) && FMath::IsNearlyZero(Component.Offset.Z); });
        const FHearthStructureComponent* HostRoof=nullptr;
        if (!HostFloor && Original.Components.Num()>0) return AppendCanopyFailure(Existing,TEXT("native host has no floor component for canopy supports"));

        const FString CanopyCatalog=I.RoofMaterial.Equals(TEXT("slateblue"),ESearchCase::IgnoreCase)?TEXT("canopy_slateblue_2m"):TEXT("canopy_terracotta_2m");
        const FString RidgeCatalog=I.RoofMaterial.Equals(TEXT("slateblue"),ESearchCase::IgnoreCase)?TEXT("roof_ridge_slateblue_2m"):TEXT("roof_ridge_terracotta_2m");
        const FString CanopyMaterialRecipe=TEXT("canopy_roof_")+CanopyCatalog;
        const FString RidgeMaterialRecipe=TEXT("canopy_ridge_")+RidgeCatalog;
        if (I.Tiles<6) return AppendCanopyFailure(Existing,FString::Printf(TEXT("%s requires 6 incremental tiles; available=%d"),*CanopyCatalog,FMath::Max(0,I.Tiles)));
        if (I.Planks<3) return AppendCanopyFailure(Existing,FString::Printf(TEXT("canopy seating requires 3 incremental planks; available=%d"),FMath::Max(0,I.Planks)));
        if (I.Beams<3) return AppendCanopyFailure(Existing,FString::Printf(TEXT("canopy frame requires 3 incremental beams; available=%d"),FMath::Max(0,I.Beams)));
        if (I.Budget<8) return AppendCanopyFailure(Existing,FString::Printf(TEXT("canopy requires 8 incremental budget; available=%d"),FMath::Max(0,I.Budget)));

        FHearthStructurePlan Candidate=Original;
        auto EnsureRecipe=[&](const FString& RecipeId,const FString& CatalogId,const TCHAR* MaterialId,int32 Quantity)
        {
            if (const FHearthStructureMaterialRecipe* Found=Candidate.MaterialRecipes.FindByPredicate([&](const FHearthStructureMaterialRecipe& RecipeValue){return RecipeValue.RecipeId==RecipeId;}))
                return Found->CatalogId==CatalogId && Found->Inputs.Num()==1 && Found->Inputs[0].MaterialId==MaterialId && Found->Inputs[0].Quantity==Quantity;
            return HearthStructurePlan::RegisterRecipe(Candidate,Recipe(*RecipeId,*CatalogId,MaterialId,Quantity));
        };
        if (!EnsureRecipe(TEXT("canopy_deck"),TEXT("floor_timber_2m"),TEXT("plank"),1)
            || !EnsureRecipe(TEXT("canopy_post"),TEXT("post_timber_2_4m"),TEXT("beam"),1)
            || !EnsureRecipe(TEXT("canopy_beam"),TEXT("beam_timber_2m"),TEXT("beam"),1)
            || !EnsureRecipe(CanopyMaterialRecipe,CanopyCatalog,TEXT("tiles"),4)
            || !EnsureRecipe(RidgeMaterialRecipe,RidgeCatalog,TEXT("tiles"),2)
            || !EnsureRecipe(TEXT("canopy_bench"),TEXT("bench_timber"),TEXT("plank"),1))
            return AppendCanopyFailure(Existing,TEXT("canopy material recipe registration failed"));

        const float CanopyYaw=HostDoor?HostDoor->Orientation.Yaw:(HostFloor?HostFloor->Orientation.Yaw:0.f);
        const FVector Outward3=FRotator(0.f,CanopyYaw,0.f).RotateVector(FVector(0.f,-1.f,0.f));
        const FVector Right3=FRotator(0.f,CanopyYaw,0.f).RotateVector(FVector(1.f,0.f,0.f));
        const FVector2D Outward(Outward3.X,Outward3.Y),Right(Right3.X,Right3.Y);
        const FVector2D Anchor=HostDoor?FVector2D(HostDoor->Offset.X,HostDoor->Offset.Y):HostFloor?FVector2D(HostFloor->Offset.X,HostFloor->Offset.Y):FVector2D::ZeroVector;
        const FHearthStructureCatalogEntry* CanopyMetadata=HearthStructureCatalog::Find(CanopyCatalog);
        const FHearthStructureCatalogEntry* DeckMetadata=HearthStructureCatalog::Find(TEXT("floor_timber_2m"));
        const FHearthStructureCatalogEntry* BeamMetadata=HearthStructureCatalog::Find(TEXT("beam_timber_2m"));
        const FHearthStructureCatalogEntry* RidgeMetadata=HearthStructureCatalog::Find(RidgeCatalog);
        if (!CanopyMetadata || !DeckMetadata || !BeamMetadata || !RidgeMetadata) return AppendCanopyFailure(Existing,TEXT("canopy catalog bounds are unavailable"));
        const auto* CanopySupport=CanopyMetadata->Sockets.FindByPredicate([](const FHearthStructureCatalogSocket& Socket){return Socket.Id==TEXT("support_bottom");});
        const auto* CanopyRidge=CanopyMetadata->Sockets.FindByPredicate([](const FHearthStructureCatalogSocket& Socket){return Socket.Id==TEXT("ridge");});
        if (!CanopySupport || !CanopyRidge) return AppendCanopyFailure(Existing,TEXT("canopy catalog support sockets are unavailable"));
        const FBox CanopyAtOrigin=CatalogLocalBounds(*CanopyMetadata,CanopyYaw);
        const FBox DeckAtOrigin=CatalogLocalBounds(*DeckMetadata,CanopyYaw);
        const FBox HostBounds=HostDoor?LocalComponentBounds(*HostDoor):(HostFloor?LocalComponentBounds(*HostFloor):FBox(ForceInit));
        const FVector2D HostFrontAnchor=Anchor;
        const float HostFront=HostDoor||HostFloor?BoxProjectionMax(HostBounds,Outward):0.f;
        const float DeckDistance=HostDoor||HostFloor?HostFront+4.f-FVector2D::DotProduct(HostFrontAnchor,Outward)-BoxProjectionMin(DeckAtOrigin,Outward):175.f;
        const float CanopyDistance=HostDoor||HostFloor?HostFront+2.f-FVector2D::DotProduct(HostFrontAnchor,Outward)-BoxProjectionMin(CanopyAtOrigin,Outward)
            :DeckDistance+BoxProjectionMin(DeckAtOrigin,Outward)-2.f-BoxProjectionMin(CanopyAtOrigin,Outward);
        const FVector2D Centre=Anchor+Outward*CanopyDistance;
        const FVector2D DeckCentre=Anchor+Outward*DeckDistance;
        const FRotator CanopyRotation=CanopyMetadata->DefaultRotation+FRotator(0.f,CanopyYaw,0.f);
        const float BeamZ=240.f;
        const float RoofZ=BeamZ+BeamMetadata->BoundsMax.Z*100.f-CanopySupport->LocalPosition.Z*100.f;
        const FVector RoofOrigin(Centre,RoofZ);
        const FVector SupportPoint=RoofOrigin+CanopyRotation.RotateVector(CanopySupport->LocalPosition*100.f);
        const FVector2D PostCentre(SupportPoint.X,SupportPoint.Y);
        const FVector RidgeOrigin=RoofOrigin+CanopyRotation.RotateVector(CanopyRidge->LocalPosition*100.f)
            -FVector(0.f,0.f,RidgeMetadata->BoundsMin.Z*100.f);
        const FBox PlacedRoofBounds=CanopyAtOrigin.ShiftBy(RoofOrigin);
        // Paired slopes share their origin. Select the contacting, door-facing
        // bounds, never the first slope or the smallest origin Y.
        for (const FHearthStructureComponent& Component:Original.Components)
        {
            if (!Component.CatalogId.StartsWith(TEXT("roof_slope_"))) continue;
            const FBox Bounds=LocalComponentBounds(Component);
            if (BoxTouches3D(PlacedRoofBounds,Bounds,5.f)
                && (!HostRoof || BoxProjectionMax(Bounds,Outward)>BoxProjectionMax(LocalComponentBounds(*HostRoof),Outward))) HostRoof=&Component;
        }
        if (HostFloor && !HostRoof) return AppendCanopyFailure(Existing,TEXT("canopy_roof_does_not_reach_host_eave_bounds"));
        const FString FloorKey=HostFloor?CanopySemanticKey(Original,HostFloor->Id):FString();
        const FString RoofKey=HostRoof?CanopySemanticKey(Original,HostRoof->Id):FString();
        const FString HostKey=HostRoof?RoofKey:(HostDoor?CanopySemanticKey(Original,HostDoor->Id):FloorKey);
        const FString HostSocket=HostRoof?TEXT("eave"):(HostDoor?TEXT("front_canopy_anchor"):TEXT("ground_anchor"));
        FHearthStructureComponentSpec Spec;
        auto Put=[&](const FString& Catalog,const FString& Key,const FVector& Offset,float Yaw,const FString& RecipeId,const TCHAR* Material,int32 Cost,bool bSupport,const FString& Parent,int32 Quantity)
        {
            if (!MakeSpec(*Catalog,Key,Offset,Yaw,RecipeId,Material,Cost,bSupport,*Parent,Spec,Quantity)) return false;
            return HearthStructurePlan::AppendComponent(Candidate,Spec,ExtensionKey);
        };
        const FVector DeckOrigin(DeckCentre,0.f);
        if (!Put(TEXT("floor_timber_2m"),TEXT("canopy_deck"),DeckOrigin,CanopyYaw,TEXT("canopy_deck"),TEXT("plank"),1,false,TEXT(""),1)
            || !Put(TEXT("post_timber_2_4m"),TEXT("canopy_post_left"),FVector(PostCentre+Right*(-100.f),16.f),CanopyYaw,TEXT("canopy_post"),TEXT("beam"),1,true,TEXT("canopy_deck"),1)
            || !Put(TEXT("post_timber_2_4m"),TEXT("canopy_post_right"),FVector(PostCentre+Right*100.f,16.f),CanopyYaw,TEXT("canopy_post"),TEXT("beam"),1,true,TEXT("canopy_deck"),1)
            || !Put(TEXT("beam_timber_2m"),TEXT("canopy_beam"),FVector(PostCentre,BeamZ),CanopyYaw,TEXT("canopy_beam"),TEXT("beam"),1,true,TEXT("canopy_post_left"),1)
            || !Put(CanopyCatalog,TEXT("canopy_roof"),RoofOrigin,CanopyYaw,CanopyMaterialRecipe,TEXT("tiles"),1,true,TEXT("canopy_beam"),4)
            || !Put(RidgeCatalog,TEXT("canopy_ridge"),RidgeOrigin,CanopyYaw+90.f,RidgeMaterialRecipe,TEXT("tiles"),1,true,TEXT("canopy_roof"),2)
            || !Put(TEXT("bench_timber"),TEXT("canopy_bench_left"),FVector(DeckCentre+Right*(-70.f),16.f),CanopyYaw+90.f,TEXT("canopy_bench"),TEXT("plank"),1,true,TEXT("canopy_deck"),1)
            || !Put(TEXT("bench_timber"),TEXT("canopy_bench_right"),FVector(DeckCentre+Right*70.f,16.f),CanopyYaw+90.f,TEXT("canopy_bench"),TEXT("plank"),1,true,TEXT("canopy_deck"),1))
            return AppendCanopyFailure(Existing,TEXT("canopy component assembly failed"));

        auto Link=[&](const TCHAR* Key,const FString& From,const FString& To,bool bLoadBearing)
        { return HearthStructurePlan::AppendConnection(Candidate,Key,From,To,bLoadBearing,ExtensionKey); };
        if (!Link(TEXT("canopy_beam_left_post"),TEXT("canopy_beam"),TEXT("canopy_post_left"),true)
            || !Link(TEXT("canopy_beam_right_post"),TEXT("canopy_beam"),TEXT("canopy_post_right"),true)
            || (!HostKey.IsEmpty() && !Link(TEXT("canopy_roof_host"),TEXT("canopy_roof"),HostKey,true))
            || (!FloorKey.IsEmpty() && !Link(TEXT("canopy_deck_host"),TEXT("canopy_deck"),FloorKey,false)))
            return AppendCanopyFailure(Existing,TEXT("canopy host connection failed"));

        // A 2.2m canopy may meet two adjacent 2m roof bays. Keep each real
        // roof junction explicit; never exempt unrelated components.
        for (const FHearthStructureComponent& HostPart:Original.Components)
        {
            if (!HostPart.CatalogId.StartsWith(TEXT("roof_slope_"))) continue;
            const FString ParentKey=CanopySemanticKey(Original,HostPart.Id);
            for (const TCHAR* ChildKey:{TEXT("canopy_roof"),TEXT("canopy_ridge")})
            {
                if (HostPart.Id==(HostRoof?HostRoof->Id:FString()) && FString(ChildKey)==TEXT("canopy_roof")) continue;
                const auto* Child=CanopyComponent(Candidate,ChildKey);
                if (Child && BoxTouches3D(LocalComponentBounds(*Child),LocalComponentBounds(HostPart),5.f)
                    && !Link(*(FString(ChildKey)+TEXT("_host_")+ParentKey),ChildKey,ParentKey,true))
                    return AppendCanopyFailure(Existing,TEXT("canopy roof junction connection failed"));
            }
        }

        if (!HostKey.IsEmpty())
        {
            FHearthStructureAttachment Attachment;
            Attachment.Id=HearthStructurePlan::StableId(Candidate,TEXT("attachment"),ExtensionKey);
            Attachment.ParentComponentId=HostRoof?HostRoof->Id:(HostDoor?HostDoor->Id:HostFloor->Id);
            Attachment.CatalogId=CanopyCatalog;
            Attachment.Socket=HostSocket;
            Attachment.Offset=RoofOrigin;
            Attachment.Height=(CanopyMetadata->BoundsMax.Z-CanopyMetadata->BoundsMin.Z)*100.f;
            Candidate.Attachments.Add(MoveTemp(Attachment));
        }
        Enclose(Candidate);

        FString GeometryError;
        if (!ValidateCanopyGeometry(Candidate,HostDoor?HostDoor:HostFloor,HostFloor,HostRoof,CanopyYaw,GeometryError))
            return AppendCanopyFailure(Existing,GeometryError);

        const FHearthStructureValidationResult Validation=HearthStructurePlan::Validate(Candidate,ExpansionContext(I,Original));
        if (!Validation.bValid) return AppendCanopyFailure(Existing,TEXT("structural or finite-resource validation failed: ")+Issues(Validation));
        Candidate.Reasons.Need+=FString::Printf(TEXT(" | canopy[%s] need=%s; two outdoor seats remain construction-gated"),*ExtensionKey,*I.Need);
        Candidate.Reasons.Budget+=FString::Printf(TEXT(" | canopy[%s] incremental budget=8 stone=0 planks=3 beams=3 tiles=6 catalog=%s"),*ExtensionKey,*CanopyCatalog);
        Candidate.Reasons.Relationship+=HostKey.IsEmpty()
            ? TEXT(" | half-open tavern seating in a host-derived attached plan")
            : TEXT(" | half-open tavern seating attached to existing house edge");
        Candidate.Reasons.RoadAccess+=TEXT(" | canopy passage remains subject to runtime walkability");
        Existing.Expansion.ExtensionKey=ExtensionKey;
        Existing.Expansion.Reason=TEXT("Half-open canopy appended to the existing plan: 2 posts, beam, roof, ridge, deck and 2 real benches; only incremental materials and budget charged.");
        Existing.Expansion.ResultingPlan=MoveTemp(Candidate);
        return true;
    }
}

FHearthStructureValidationContext HearthResidentBuildingPlanner::ValidationContext(const FHearthResidentBuildingInput& I)
{
    FHearthStructureValidationContext C; C.AvailableBudget=FMath::Max(0,I.Budget); C.bRoadAccessible=I.bRoadAccessible; C.AvailableMaterials.Add(Mat(TEXT("stone"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Stone); C.AvailableMaterials.Add(Mat(TEXT("plank"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Planks); C.AvailableMaterials.Add(Mat(TEXT("beam"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Beams); C.AvailableMaterials.Add(Mat(TEXT("tiles"))); C.AvailableMaterials.Last().Quantity=FMath::Max(0,I.Tiles); return C;
}

FHearthResidentBuildingPlan HearthResidentBuildingPlanner::Build(const FHearthResidentBuildingInput& I)
{
    if (!I.Archetype.IsEmpty()) return BuildArchetype(I);
    FHearthResidentBuildingPlan O; const FString Id=TEXT("resident_")+(I.ResidentId.IsEmpty()?TEXT("unknown"):I.ResidentId)+TEXT("_house"); const FString Seed=I.StableSeed.IsEmpty()?TEXT("resident-default"):I.StableSeed; const FHearthResidentGoalUse Goal=ClassifyGoal(I.Need); const bool Family=I.HouseholdSize>=3||Goal.bPrivateDomestic||HasTerm(I.Need,TEXT("隐私")); const bool Workshop=HasTerm(I.Occupation,TEXT("craft"))||HasTerm(I.Occupation,TEXT("carpenter"))||HasTerm(I.Occupation,TEXT("smith"))||HasTerm(I.Occupation,TEXT("木匠"))||HasTerm(I.Occupation,TEXT("铁匠"))||HasTerm(I.Occupation,TEXT("陶工"))||HasTerm(I.Occupation,TEXT("织工"));
    int32 Desired=1;
    if(I.GrowthDirection>=0)
    {
        // Three rooms require three explicit use signals; role and household
        // data never supply the missing purpose.
        Desired+=(Goal.bPrivateDomestic?1:0)+((Goal.bWorkshop||Goal.bStorage)?1:0)+(Goal.bNeighborCourtyard?1:0);
        Desired=FMath::Max(Desired,(Goal.bPrivateDomestic||Goal.bWorkshop||Goal.bStorage||Goal.bNeighborCourtyard)?2:1);
    }
    else
    {
        // Preserve legacy attached-row room-count behavior.
        Desired=1+(Family?1:0)+(Workshop&&I.FriendsNearby>0?1:0);
    }
    Desired=FMath::Clamp(Desired,1,FMath::Clamp(I.MaxInitialRooms,1,3));
    const FHearthHouseStyle Style=StyleFor(I);
    const int32 StonePerRoom=1+(Style.WallResource==TEXT("stone")?4:0)+(Style.RoofResource==TEXT("stone")?2:0);
    const int32 PlanksPerRoom=1+(Style.WallResource==TEXT("plank")?4:0)+(Style.RoofResource==TEXT("plank")?2:0);
    const int32 TilesPerRoom=Style.RoofResource==TEXT("tiles")?12:0;
    const int32 TileAffordable=TilesPerRoom>0?I.Tiles/TilesPerRoom:FMath::Clamp(I.MaxInitialRooms,0,3);
    const int32 Affordable=FMath::Min(I.Stone/StonePerRoom,FMath::Min(I.Planks/PlanksPerRoom,FMath::Min(I.Beams/8,FMath::Min(TileAffordable,I.Budget/16))));
    const int32 Rooms=FMath::Clamp(FMath::Min(Desired,Affordable),0,FMath::Clamp(I.MaxInitialRooms,0,3));
    FHearthStructureFootprint F; F.Origin=I.Origin; F.Size=FVector2D(FMath::Max(500.f,2.f*((Rooms-1)*200.f+230.f)),480.f); F.Orientation=FRotator(0,I.RoadYaw,0); FHearthStructureReasonFields R; R.Need=I.Need; R.Occupation=I.Occupation; R.Budget=FString::Printf(TEXT("budget=%d; affordable_rooms=%d; preferred wall=%s roof=%s; executable wall=%s roof=%s; conserved inputs stone=%d plank=%d beam=8 tiles=%d per room"),I.Budget,Rooms,*Style.RequestedWall,*Style.RequestedRoof,*Style.WallChoice,*Style.RoofChoice,StonePerRoom,PlanksPerRoom,TilesPerRoom); if(!Style.Substitution.IsEmpty()) R.Budget+=TEXT("; ")+Style.Substitution; R.Relationship=FString::Printf(TEXT("friends_nearby=%d"),I.FriendsNearby); R.RoadAccess=I.bRoadAccessible?TEXT("road-accessible"):TEXT("road-inaccessible"); O.Plan=HearthStructurePlan::MakePlan(Id,Seed,F,R); Register(O.Plan,Style);
    if(!I.bRoadAccessible) O.Reason=TEXT("No buildable plan: the resident has no verified road access."); else if(Rooms==0) O.Reason=TEXT("No buildable plan: current stone, planks, beams, or budget cannot fund one core room."); else {const FHearthStructurePlan Before=O.Plan; bool Good=true;for(int32 N=0;N<Rooms;++N){ const FVector2D Wing=WingPosition(O.Plan,I.GrowthDirection); if(!Add(O.Plan,N,I.RoadYaw,FString(),Style,I.GrowthDirection>=0 && N>0?&Wing:nullptr)){Good=false;break;} O.Plan.Rooms.Last().Label=RoomPurpose(Goal,N); } if(I.GrowthDirection>=0) Enclose(O.Plan);if(!Good){O.Plan=Before;O.Reason=TEXT("Plan assembly failed and was rolled back atomically.");}else{const auto V=HearthStructurePlan::Validate(O.Plan,ValidationContext(I));O.bBuildable=V.bValid;O.Reason=V.bValid?FString::Printf(TEXT("Built %d room(s) with %s walls and %s roof; stone=%d; planks=%d; beams=%d from finite current resources; intended use: %s."),Rooms,*Style.WallChoice,*Style.RoofChoice,I.Stone,I.Planks,I.Beams,*O.Plan.Rooms[0].Label):TEXT("Plan rejected by structural validation: ")+Issues(V);}}
    O.Expansion.ExtensionKey=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; O.Expansion.Reason=TEXT("Reserve one adjoining catalog-sized bay for later household growth."); O.Expansion.ResultingPlan=O.Plan; if(O.bBuildable)AppendExpansion(O,RemainingAfter(I,O.Plan)); return O;
}

bool HearthResidentBuildingPlanner::AppendExpansion(FHearthResidentBuildingPlan& Existing,const FHearthResidentBuildingInput& I)
{
    if (!I.Archetype.IsEmpty()) return AppendArchetypeExpansion(Existing,I);
    // Never append legacy geometry to an archetype save when its caller lost
    // the archetype field. Empty-archetype legacy plans keep their old behavior.
    if(Existing.Plan.Reasons.Budget.StartsWith(TEXT("archetype="))) return false;
    if(!Existing.bBuildable)return false; const FString K=I.ExtensionKey.IsEmpty()?TEXT("resident_extension_1"):I.ExtensionKey; const FHearthStructurePlan Original=Existing.Expansion.ResultingPlan.Components.IsEmpty()?Existing.Plan:Existing.Expansion.ResultingPlan; if(HasExtension(Original,K))return false; FHearthStructurePlan Candidate=Original; const int32 N=Candidate.Rooms.Num(); const FHearthHouseStyle Style=StyleFor(I); Register(Candidate,Style); const FVector2D Wing=WingPosition(Candidate,I.GrowthDirection); if(!Add(Candidate,N,I.RoadYaw,K,Style,I.GrowthDirection>=0?&Wing:nullptr))return false; if(I.GrowthDirection>=0) Enclose(Candidate); Candidate.Footprint.Size.X=FMath::Max(Candidate.Footprint.Size.X,2.f*(N*200.f+120.f)+240.f);
    const FString Trace=FString::Printf(TEXT("extension[%s] room_%d growth_direction=%d"),*K,N,I.GrowthDirection);
    Candidate.Reasons.Need+=FString::Printf(TEXT(" | %s need=%s"),*Trace,*I.Need);
    Candidate.Reasons.Occupation+=FString::Printf(TEXT(" | %s occupation=%s"),*Trace,*I.Occupation);
    Candidate.Reasons.Budget+=FString::Printf(TEXT(" | %s budget=%d stone=%d planks=%d beams=%d tiles=%d preferred wall=%s roof=%s executable wall=%s roof=%s"),*Trace,I.Budget,I.Stone,I.Planks,I.Beams,I.Tiles,*Style.RequestedWall,*Style.RequestedRoof,*Style.WallChoice,*Style.RoofChoice);
    if(!Style.Substitution.IsEmpty()) Candidate.Reasons.Budget+=TEXT("; ")+Style.Substitution;
    Candidate.Reasons.Relationship+=FString::Printf(TEXT(" | %s friends_nearby=%d household=%d"),*Trace,I.FriendsNearby,I.HouseholdSize);
    Candidate.Reasons.RoadAccess+=FString::Printf(TEXT(" | %s %s"),*Trace,I.bRoadAccessible?TEXT("road-accessible"):TEXT("road-inaccessible"));
    auto Context=ValidationContext(I); AddCommittedResources(Original,Context); if(!HearthStructurePlan::Validate(Candidate,Context).bValid)return false; Existing.Expansion.ExtensionKey=K; Existing.Expansion.ResultingPlan=MoveTemp(Candidate); Existing.Expansion.Reason=FString::Printf(TEXT("Extension %s appends room_%d with finite current resources."),*K,N); return true;
}

bool HearthResidentBuildingPlanner::AppendCanopy(FHearthResidentBuildingPlan& Existing,const FHearthResidentBuildingInput& I)
{
    return AppendTavernCanopy(Existing,I);
}
