#if WITH_DEV_AUTOMATION_TESTS
#include "HearthResidentBuildingPlanner.h"
#include "HearthPlannedConstructionAdapter.h"
#include "Misc/AutomationTest.h"
#include "UObject/Class.h"

namespace
{
    const TCHAR* HomeArchetypes[]={TEXT("rowhouse"),TEXT("shop_house"),TEXT("courtyard_workshop"),TEXT("warehouse"),TEXT("inn")};

    FHearthResidentBuildingInput TwoRoomInput(const TCHAR* Archetype)
    {
        FHearthResidentBuildingInput I;
        I.ResidentId=TEXT("archetype-owner"); I.StableSeed=TEXT("archetype-regression"); I.Archetype=Archetype;
        I.MaxInitialRooms=2; I.Budget=32; I.Stone=2; I.Planks=14; I.Beams=16;
        I.RoadYaw=37; I.Origin=FVector(1300,-2100,8); I.GrowthDirection=0;
        return I;
    }

    FHearthResidentBuildingInput OneWingInput(const FHearthResidentBuildingInput& Base)
    {
        auto I=Base;
        I.Budget=16; I.Stone=1; I.Planks=7; I.Beams=8; I.Tiles=0;
        I.WallMaterial=TEXT("timber"); I.RoofMaterial=TEXT("timber");
        I.ExtensionKey=TEXT("funded-wing");
        return I;
    }

    bool SamePlan(const FHearthStructurePlan& A,const FHearthStructurePlan& B)
    {
        return FHearthStructurePlan::StaticStruct()->CompareScriptStruct(&A,&B,0);
    }

    FHearthStructureValidationContext TotalContext(const FHearthResidentBuildingInput& Delta,const FHearthStructurePlan& Before)
    {
        auto C=HearthResidentBuildingPlanner::ValidationContext(Delta);
        for(const auto& Part:Before.Components)
        {
            C.AvailableBudget+=Part.MaterialCost;
            for(const auto& Material:Part.Materials)
                if(auto* Available=C.AvailableMaterials.FindByPredicate([&](const auto& M){return M.MaterialId==Material.MaterialId;}))
                    Available->Quantity+=Material.Quantity;
        }
        return C;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthArchetypeAffordabilityTest,"ThreeHearths.StructurePlan.ArchetypeAffordability",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthArchetypeAffordabilityTest::RunTest(const FString&)
{
    for(const TCHAR* A:HomeArchetypes)
    {
        auto I=TwoRoomInput(A);
        const int32 Minimum=I.Archetype==TEXT("rowhouse")?1:2;
        TestEqual(TEXT("Recipe minimum is explicit"),HearthResidentBuildingPlanner::MinimumInitialRooms(I),Minimum);
        TestEqual(TEXT("Two real timber rooms fit exact resources"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(I),2);
        I.MaxInitialRooms=Minimum; I.Budget=16*Minimum; I.Stone=Minimum; I.Planks=7*Minimum; I.Beams=8*Minimum;
        for(int32 Resource=0;Resource<4;++Resource)
        {
            auto Poor=I;
            if(Resource==0) --Poor.Budget;
            if(Resource==1) --Poor.Stone;
            if(Resource==2) --Poor.Planks;
            if(Resource==3) --Poor.Beams;
            Poor.Wood=MAX_int32;
            TestEqual(TEXT("One missing unit cannot fund the minimum; raw logs are not substitutes"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Poor),0);
            const auto Rejected=HearthResidentBuildingPlanner::Build(Poor);
            TestFalse(TEXT("Underfunded initial archetype rejects"),Rejected.bBuildable);
            TestEqual(TEXT("Underfunded rejection assembles no geometry"),Rejected.Plan.Components.Num(),0);
            TestEqual(TEXT("Minimum remains useful for procurement while poor"),HearthResidentBuildingPlanner::MinimumInitialRooms(Poor),Minimum);
        }
        I.MaxInitialRooms=Minimum-1;
        TestEqual(TEXT("A cap below the minimum rejects"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(I),0);
        I.MaxInitialRooms=-1;
        TestEqual(TEXT("Negative room cap never invents a room"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(I),0);
    }
    auto Row=TwoRoomInput(TEXT("rowhouse")); Row.Budget=31; Row.Planks=13;
    TestEqual(TEXT("Shared infill saves one plank and one paid component"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Row),2);
    TestTrue(TEXT("Exact shared-wall recipe is executable"),HearthResidentBuildingPlanner::Build(Row).bBuildable);
    auto Tile=TwoRoomInput(TEXT("warehouse")); Tile.WallMaterial=TEXT("stone"); Tile.RoofMaterial=TEXT("terracotta");
    Tile.Stone=10; Tile.Planks=2; Tile.Tiles=24;
    TestEqual(TEXT("Stone walls and four six-tile slopes are counted"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),2);
    TestTrue(TEXT("Exact stone and tile recipe builds"),HearthResidentBuildingPlanner::Build(Tile).bBuildable);
    Tile.Tiles=23;
    TestEqual(TEXT("One missing roof tile cannot fund a two-room archetype"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),0);
    Tile.Tiles=12;
    TestEqual(TEXT("One roof's stock is insufficient for two roofs"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),0);
    Tile.Tiles=0; Tile.Planks=6;
    TestEqual(TEXT("Existing unavailable-tile substitution uses real timber planks"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),2);
    Tile.bRoadAccessible=false; Tile.LayoutOffset=FVector(0,0,300);
    TestEqual(TEXT("Recipe query does not perform road or geometry checks"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),2);
    for(const TCHAR* Unsupported:{TEXT("keep"),TEXT("unknown")})
    {
        Tile.Archetype=Unsupported;
        TestEqual(TEXT("Unsupported construction has no resident recipe minimum"),HearthResidentBuildingPlanner::MinimumInitialRooms(Tile),0);
        TestEqual(TEXT("Unsupported construction has no affordable resident plan"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Tile),0);
    }
    for(const TCHAR* A:HomeArchetypes)
    {
        auto Rich=TwoRoomInput(A); Rich.MaxInitialRooms=MAX_int32;
        Rich.Budget=Rich.Stone=Rich.Planks=Rich.Beams=Rich.Tiles=MAX_int32;
        const int32 Expected=Rich.Archetype==TEXT("shop_house")?4:Rich.Archetype==TEXT("courtyard_workshop")?5:6;
        TestEqual(TEXT("Integer extremes stay within archetype capacity"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Rich),Expected);
    }
    auto Legacy=TwoRoomInput(TEXT("")); Legacy.MaxInitialRooms=6;
    Legacy.Budget=100; Legacy.Stone=Legacy.Planks=Legacy.Beams=100;
    TestEqual(TEXT("Empty-archetype helper preserves the legacy three-room ceiling"),HearthResidentBuildingPlanner::MaxAffordableInitialRooms(Legacy),3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthArchetypeExpansionTest,"ThreeHearths.StructurePlan.ArchetypeExpansion",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthArchetypeExpansionTest::RunTest(const FString&)
{
    for(const TCHAR* A:HomeArchetypes)
    {
        const auto Input=TwoRoomInput(A);
        const auto Unshifted=HearthResidentBuildingPlanner::Build(Input);
        auto ShiftedInput=Input; ShiftedInput.LayoutOffset=FVector(300,600,0);
        const auto Base=HearthResidentBuildingPlanner::Build(ShiftedInput);
        TestTrue(FString::Printf(TEXT("%s builds a funded core"),A),Base.bBuildable && Unshifted.bBuildable);
        if(!Base.bBuildable || !Unshifted.bBuildable) continue;
        TestTrue(TEXT("LayoutOffset leaves the world anchor unchanged"),Base.Plan.Footprint.Origin==Input.Origin);
        TestEqual(TEXT("Layout shift preserves room identity"),Base.Plan.Rooms[0].Id,Unshifted.Plan.Rooms[0].Id);
        for(const auto& Old:Unshifted.Plan.Components)
        {
            const auto* Shifted=Base.Plan.Components.FindByPredicate([&](const auto& C){return C.Id==Old.Id;});
            TestTrue(TEXT("Every catalog piece uses the same local layout shift"),Shifted && Shifted->Offset.Equals(Old.Offset+ShiftedInput.LayoutOffset) && Shifted->Orientation==Old.Orientation);
        }
        for(const auto& Old:Unshifted.Plan.Openings)
        {
            const auto* Shifted=Base.Plan.Openings.FindByPredicate([&](const auto& O){return O.Id==Old.Id;});
            TestTrue(TEXT("Doors and court access shift with the structure"),Shifted && Shifted->Offset.Equals(Old.Offset+FVector2D(300,600)) && Shifted->AccessDirection==Old.AccessDirection);
        }
        const auto BaseRuntime=HearthPlannedConstructionAdapter::Convert(Base.Plan,7,{});
        TestTrue(TEXT("Shifted plan converts"),BaseRuntime.bAccepted);
        for(int32 Direction=1;Direction<=3;++Direction)
        {
            auto Expanded=Base;
            auto Delta=OneWingInput(ShiftedInput); Delta.GrowthDirection=Direction;
            // A new siting suggestion or changed input transform cannot move a save.
            Delta.LayoutOffset=FVector(-900,1700,300); Delta.Origin=FVector(-8000,7000,99); Delta.RoadYaw=-113;
            const bool Accepted=HearthResidentBuildingPlanner::AppendExpansion(Expanded,Delta);
            TestTrue(FString::Printf(TEXT("%s appends an independent wing in direction %d"),A,Direction),Accepted);
            if(!Accepted) continue;
            const auto& Result=Expanded.Expansion.ResultingPlan;
            TestTrue(TEXT("Initial plan remains byte-field equivalent"),SamePlan(Base.Plan,Expanded.Plan));
            TestTrue(TEXT("Saved world anchor and yaw remain unchanged"),Result.Footprint.Origin==Base.Plan.Footprint.Origin && Result.Footprint.Orientation==Base.Plan.Footprint.Orientation);
            TestEqual(TEXT("One new room"),Result.Rooms.Num(),Base.Plan.Rooms.Num()+1);
            TestEqual(TEXT("Independent wing includes all 16 paid parts"),Result.Components.Num(),Base.Plan.Components.Num()+16);
            TestEqual(TEXT("Wing adds a real door"),Result.Openings.Num(),Base.Plan.Openings.Num()+1);
            TestTrue(TEXT("Outdoor passage is recorded for external siting"),Result.Reasons.RoadAccess.Contains(TEXT("outdoor passage width=94cm")));
            int32 AddedBudget=0,AddedStone=0,AddedPlanks=0,AddedBeams=0;
            const FHearthStructureComponent* NewFloor=nullptr;
            for(const auto& C:Result.Components) if(C.ExtensionId==Delta.ExtensionKey)
            {
                AddedBudget+=C.MaterialCost;
                for(const auto& M:C.Materials)
                {
                    if(M.MaterialId==TEXT("stone")) AddedStone+=M.Quantity;
                    if(M.MaterialId==TEXT("plank")) AddedPlanks+=M.Quantity;
                    if(M.MaterialId==TEXT("beam")) AddedBeams+=M.Quantity;
                }
                if(C.CatalogId==TEXT("floor_timber_2m")) NewFloor=&C;
            }
            TestEqual(TEXT("Incremental paid components"),AddedBudget,16);
            TestEqual(TEXT("Incremental stone"),AddedStone,1);
            TestEqual(TEXT("Incremental planks"),AddedPlanks,7);
            TestEqual(TEXT("Incremental beams"),AddedBeams,8);
            TestNotNull(TEXT("Wing has a catalog floor, not only an opening or label"),NewFloor);
            for(const auto& Old:Base.Plan.Openings)
            {
                const auto* Current=Result.Openings.FindByPredicate([&](const auto& O){return O.Id==Old.Id;});
                TestTrue(TEXT("Previous opening geometry and ownership survive"),Current && FHearthStructureOpening::StaticStruct()->CompareScriptStruct(&Old,Current,0));
            }
            for(const auto& Old:Base.Plan.Connections)
            {
                const auto* Current=Result.Connections.FindByPredicate([&](const auto& C){return C.Id==Old.Id;});
                TestTrue(TEXT("Previous connection identity survives"),Current && FHearthStructureConnection::StaticStruct()->CompareScriptStruct(&Old,Current,0));
            }
            for(const auto& Old:Base.Plan.Components)
            {
                const auto* Current=Result.Components.FindByPredicate([&](const auto& C){return C.Id==Old.Id;});
                TestTrue(TEXT("All previous component fields survive"),Current && FHearthStructureComponent::StaticStruct()->CompareScriptStruct(&Old,Current,0));
                if(NewFloor && Old.CatalogId==TEXT("floor_timber_2m"))
                {
                    if(Direction==1) TestTrue(TEXT("Right wing has a real lateral passage gap"),NewFloor->Offset.X>=Old.Offset.X+399.9);
                    if(Direction==2) TestTrue(TEXT("Left wing has a real lateral passage gap"),NewFloor->Offset.X<=Old.Offset.X-399.9);
                    if(Direction==3) TestTrue(TEXT("Rear wing clears the full 450cm roof envelope"),NewFloor->Offset.Y>=Old.Offset.Y+599.9);
                }
            }
            auto FullContext=TotalContext(Delta,Base.Plan);
            TestTrue(TEXT("Saved investment plus exact delta validates"),HearthStructurePlan::Validate(Result,FullContext).bValid);
            TestFalse(TEXT("Delta alone cannot pretend to purchase the whole plan"),HearthStructurePlan::Validate(Result,HearthResidentBuildingPlanner::ValidationContext(Delta)).bValid);
            const auto Runtime=HearthPlannedConstructionAdapter::Convert(Result,7,BaseRuntime.Components);
            TestTrue(TEXT("Converter accepts expansion without changing installed transforms"),Runtime.bAccepted);
            for(const auto& Old:BaseRuntime.Components)
            {
                const auto* C=Runtime.Components.FindByPredicate([&](const auto& V){return V.Id==Old.Id;});
                TestTrue(TEXT("Runtime offsets, yaw and material cargo remain unchanged"),C && C->Offset==Old.Offset && C->Yaw==Old.Yaw && C->AssetId==Old.AssetId && C->MaterialAmount==Old.MaterialAmount && C->Owner==Old.Owner);
            }
            if(NewFloor)
            {
                FHearthStructureOccupiedVolume Blocker; Blocker.Center=FVector2D(NewFloor->Offset.X,NewFloor->Offset.Y); Blocker.Radius=40; Blocker.Z=0; Blocker.Height=300;
                FullContext.Occupied.Add(Blocker);
                TestFalse(TEXT("External occupancy can still reject a funded wing"),HearthStructurePlan::Validate(Result,FullContext).bValid);
            }
            const auto BeforeReplay=Result;
            TestFalse(TEXT("Extension key cannot be replayed"),HearthResidentBuildingPlanner::AppendExpansion(Expanded,Delta));
            TestTrue(TEXT("Replay rejection is atomic"),SamePlan(BeforeReplay,Expanded.Expansion.ResultingPlan));
        }
        for(int32 Failure=0;Failure<14;++Failure)
        {
            auto Rejected=Base; auto Delta=OneWingInput(ShiftedInput);
            if(Failure==0) --Delta.Budget;
            if(Failure==1) --Delta.Stone;
            if(Failure==2) --Delta.Planks;
            if(Failure==3) --Delta.Beams;
            if(Failure==4) Delta.bRoadAccessible=false;
            if(Failure==5) Delta.GrowthDirection=4;
            if(Failure==6) Delta.Archetype=TEXT("unknown");
            if(Failure==7) Delta.Archetype.Empty();
            if(Failure==8) Rejected.Expansion.ResultingPlan.Footprint.Size=FVector2D(10,10);
            if(Failure==9) Rejected.Expansion.ResultingPlan.Components.Last().SupportsComponentId=TEXT("missing-support");
            if(Failure==10)
            {
                auto Duplicate=Rejected.Expansion.ResultingPlan.Components[0];
                Duplicate.Id+=TEXT("_overlapping_foundation");
                Rejected.Expansion.ResultingPlan.Components.Add(Duplicate);
            }
            if(Failure==11) Rejected.Expansion.ResultingPlan.Openings[0].Offset.X+=25;
            if(Failure==12) Rejected.Expansion.ResultingPlan.Footprint.Orientation.Pitch=15;
            if(Failure==13) Delta.Archetype=TEXT("keep");
            const auto Before=Rejected;
            TestFalse(TEXT("Invalid or underfunded wing is rejected"),HearthResidentBuildingPlanner::AppendExpansion(Rejected,Delta));
            TestTrue(TEXT("Rejected candidate leaves both saved plans unchanged"),SamePlan(Before.Plan,Rejected.Plan) && SamePlan(Before.Expansion.ResultingPlan,Rejected.Expansion.ResultingPlan));
            TestEqual(TEXT("Rejected candidate leaves expansion metadata unchanged"),Rejected.Expansion.Reason,Before.Expansion.Reason);
        }
        auto Full=Base; auto Delta=OneWingInput(ShiftedInput); Delta.GrowthDirection=3;
        for(int32 Room=3;Room<=6;++Room)
        {
            Delta.ExtensionKey=FString::Printf(TEXT("rear-wing-%d"),Room);
            TestTrue(TEXT("Incremental funding can grow to six rooms"),HearthResidentBuildingPlanner::AppendExpansion(Full,Delta));
        }
        const auto BeforeSeventh=Full.Expansion.ResultingPlan;
        Delta.ExtensionKey=TEXT("seventh-room"); Delta.Budget=Delta.Stone=Delta.Planks=Delta.Beams=MAX_int32;
        TestFalse(TEXT("Even unlimited supplied integers cannot exceed six rooms"),HearthResidentBuildingPlanner::AppendExpansion(Full,Delta));
        TestTrue(TEXT("Room-cap rejection is atomic"),SamePlan(BeforeSeventh,Full.Expansion.ResultingPlan));
    }
    // Model an older shop save whose front room was at Y=-300. Appending must
    // consume its actual saved coordinates, never the revised initial centers.
    auto OldInput=TwoRoomInput(TEXT("shop_house")); OldInput.LayoutOffset=FVector(0,-300,0);
    auto OldShop=HearthResidentBuildingPlanner::Build(OldInput);
    const auto OldPlan=OldShop.Plan; auto Delta=OneWingInput(OldInput); Delta.GrowthDirection=3;
    TestTrue(TEXT("Older shop geometry remains expandable"),HearthResidentBuildingPlanner::AppendExpansion(OldShop,Delta));
    for(const auto& Old:OldPlan.Components)
        TestTrue(TEXT("No migration moves the old shop"),OldShop.Expansion.ResultingPlan.Components.ContainsByPredicate([&](const auto& C){return C.Id==Old.Id && C.Offset==Old.Offset;}));
    auto Elevated=TwoRoomInput(TEXT("shop_house")); Elevated.LayoutOffset.Z=300;
    TestFalse(TEXT("Initial offset cannot invent an unsupported second storey"),HearthResidentBuildingPlanner::Build(Elevated).bBuildable);
    auto LegacyInput=TwoRoomInput(TEXT(""));
    const auto Legacy=HearthResidentBuildingPlanner::Build(LegacyInput);
    LegacyInput.LayoutOffset=FVector(300,600,300);
    const auto LegacyWithOffset=HearthResidentBuildingPlanner::Build(LegacyInput);
    TestTrue(TEXT("Empty archetype preserves old behavior even with an unused offset"),SamePlan(Legacy.Plan,LegacyWithOffset.Plan)
        && SamePlan(Legacy.Expansion.ResultingPlan,LegacyWithOffset.Expansion.ResultingPlan));
    auto MixedInput=TwoRoomInput(TEXT("warehouse"));
    auto Mixed=HearthResidentBuildingPlanner::Build(MixedInput);
    auto StoneWing=OneWingInput(MixedInput); StoneWing.GrowthDirection=1;
    StoneWing.WallMaterial=TEXT("stone"); StoneWing.RoofMaterial=TEXT("terracotta");
    StoneWing.Stone=5; StoneWing.Planks=1; StoneWing.Tiles=12;
    TestTrue(TEXT("Saved timber core can fund exactly one stone and tile wing"),HearthResidentBuildingPlanner::AppendExpansion(Mixed,StoneWing));
    int32 NewTiles=0,NewStone=0;
    for(const auto& C:Mixed.Expansion.ResultingPlan.Components) if(C.ExtensionId==StoneWing.ExtensionKey)
        for(const auto& M:C.Materials)
        {
            if(M.MaterialId==TEXT("tiles")) NewTiles+=M.Quantity;
            if(M.MaterialId==TEXT("stone")) NewStone+=M.Quantity;
        }
    TestEqual(TEXT("Two real slopes consume twelve new tiles"),NewTiles,12);
    TestEqual(TEXT("New foundation and four infill pieces consume five stones"),NewStone,5);
    TestTrue(TEXT("Mixed-material expansion still passes the runtime converter"),HearthPlannedConstructionAdapter::Convert(Mixed.Expansion.ResultingPlan,7,{}).bAccepted);
    return true;
}
#endif
