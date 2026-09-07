#if WITH_DEV_AUTOMATION_TESTS
#include "HearthBuildingArchetypes.h"
#include "HearthResidentBuildingPlanner.h"
#include "HearthPlannedConstructionAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
    FHearthResidentBuildingInput Inputs(int32 Household, const TCHAR* Occupation, int32 Friends)
    {
        FHearthResidentBuildingInput I; I.ResidentId = TEXT("resident-1"); I.StableSeed = TEXT("seed-1");
        I.Need = Household >= 3 ? TEXT("family privacy") : TEXT("shelter"); I.Occupation = Occupation; I.HouseholdSize = Household; I.FriendsNearby = Friends;
        I.Budget = 100; I.bRoadAccessible = true; I.RoadYaw = 20.f; I.Origin = FVector(400.f, -600.f, 8.f);
        I.Stone = 16; I.Planks = 40; I.Beams = 40; return I;
    }
    FHearthResidentBuildingInput ArchetypeInput(const TCHAR* Archetype)
    {
        FHearthResidentBuildingInput I = Inputs(2, TEXT("general"), 0);
        I.Archetype = Archetype;
        I.MaxInitialRooms = 6;
        I.Budget = 300;
        I.Stone = 100;
        I.Planks = 100;
        I.Beams = 100;
        return I;
    }
    FBox ResidentCanopyTestBounds(const FHearthStructureComponent& Component)
    {
        FBox Bounds(ForceInit);
        for (int32 X=0;X<2;++X) for (int32 Y=0;Y<2;++Y) for (int32 Z=0;Z<2;++Z)
            Bounds+=Component.Offset+Component.Orientation.RotateVector(FVector(X?Component.BoundsMax.X:Component.BoundsMin.X,
                Y?Component.BoundsMax.Y:Component.BoundsMin.Y,Z?Component.BoundsMax.Z:Component.BoundsMin.Z));
        return Bounds;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthResidentBuildingPlannerTest, "ThreeHearths.StructurePlan.ResidentBuildingPlanner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthResidentBuildingPlannerTest::RunTest(const FString&)
{
    auto SmallInput = Inputs(1, TEXT("general"), 0); SmallInput.ExtensionKey = TEXT("extension-alpha");
    auto FamilyInput = Inputs(4, TEXT("carpenter"), 1); FamilyInput.ExtensionKey = TEXT("extension-family");
    const auto Small = HearthResidentBuildingPlanner::Build(SmallInput);
    const auto Family = HearthResidentBuildingPlanner::Build(FamilyInput);
    AddInfo(TEXT("Small planner: ")+Small.Reason);
    AddInfo(TEXT("Family planner: ")+Family.Reason);
    TestTrue(TEXT("Small household produces a buildable plan"), Small.bBuildable);
    TestTrue(TEXT("Family/workshop household produces a buildable plan"), Family.bBuildable);
    TestTrue(TEXT("Need and occupation change room count"), Family.Plan.Rooms.Num() > Small.Plan.Rooms.Num());
    auto ChineseWorkshopInput=Inputs(1,TEXT("木匠"),1); const auto ChineseWorkshop=HearthResidentBuildingPlanner::Build(ChineseWorkshopInput);
    TestTrue(TEXT("Live Chinese occupation labels drive workshop space"),ChineseWorkshop.Plan.Rooms.Num()>Small.Plan.Rooms.Num());
    TestTrue(TEXT("Family plan has a later expansion proposal"), Family.Expansion.ResultingPlan.Rooms.Num() > Family.Plan.Rooms.Num());
    TestEqual(TEXT("Expansion ID follows caller stable key"), Family.Expansion.ExtensionKey, FString(TEXT("extension-family")));
    TestTrue(TEXT("Expansion need is persisted in the resulting plan"), Family.Expansion.ResultingPlan.Reasons.Need.Contains(TEXT("extension-family")) && Family.Expansion.ResultingPlan.Reasons.Need.Contains(FamilyInput.Need));
    TestTrue(TEXT("Expansion resource decision is persisted in the resulting plan"), Family.Expansion.ResultingPlan.Reasons.Budget.Contains(TEXT("extension-family")) && Family.Expansion.ResultingPlan.Reasons.Budget.Contains(TEXT("stone=")));
    TestTrue(TEXT("Base component IDs survive expansion"), Family.Expansion.ResultingPlan.Components.ContainsByPredicate([&](const FHearthStructureComponent& C) { return Family.Plan.Components.ContainsByPredicate([&](const FHearthStructureComponent& B) { return B.Id == C.Id; }); }));
    const int32 BeforeAppend = Family.Expansion.ResultingPlan.Rooms.Num();
    auto SecondExtensionInput = FamilyInput; SecondExtensionInput.ExtensionKey = TEXT("extension-workshop-2");
    auto Multi = Family; TestTrue(TEXT("A second expansion appends atomically"), HearthResidentBuildingPlanner::AppendExpansion(Multi, SecondExtensionInput));
    TestEqual(TEXT("Second expansion adds one room"), Multi.Expansion.ResultingPlan.Rooms.Num(), BeforeAppend + 1);
    TestTrue(TEXT("Second expansion keeps first expansion IDs"), Multi.Expansion.ResultingPlan.Components.ContainsByPredicate([&](const FHearthStructureComponent& C) { return Family.Expansion.ResultingPlan.Components.ContainsByPredicate([&](const FHearthStructureComponent& B) { return B.Id == C.Id; }); }));
    TestTrue(TEXT("Planner gives explicit resource reason"), Small.Reason.Contains(TEXT("stone=")) && Small.Reason.Contains(TEXT("planks=")));
    TestTrue(TEXT("Planner records road access reason"), Small.Plan.Reasons.RoadAccess == TEXT("road-accessible"));
    auto PrivateGoal=Inputs(1,TEXT("general"),0); PrivateGoal.GrowthDirection=1; PrivateGoal.Need=TEXT("quiet private domestic room");
    auto WorkshopGoal=PrivateGoal; WorkshopGoal.Need=TEXT("workshop and supporting store");
    auto CourtyardGoal=PrivateGoal; CourtyardGoal.Need=TEXT("neighbor-facing courtyard");
    const auto PrivatePlan=HearthResidentBuildingPlanner::Build(PrivateGoal);
    const auto WorkshopPlan=HearthResidentBuildingPlanner::Build(WorkshopGoal);
    const auto CourtyardPlan=HearthResidentBuildingPlanner::Build(CourtyardGoal);
    TestTrue(TEXT("Explicit private goal names the living room purpose"), PrivatePlan.Plan.Rooms.Num() >= 1 && PrivatePlan.Plan.Rooms[0].Label.Contains(TEXT("private domestic")));
    TestTrue(TEXT("Explicit workshop goal names the work purpose"), WorkshopPlan.Plan.Rooms.Num() >= 2 && WorkshopPlan.Plan.Rooms[1].Label.Contains(TEXT("workshop")));
    TestTrue(TEXT("Explicit courtyard goal names the neighbor-facing purpose"), CourtyardPlan.Plan.Rooms.Num() >= 2 && CourtyardPlan.Plan.Rooms[1].Label.Contains(TEXT("neighbor-facing courtyard")));
    auto ChineseGoal=PrivateGoal; ChineseGoal.Need=TEXT("安静的私人居住室");
    const auto ChineseGoalPlan=HearthResidentBuildingPlanner::Build(ChineseGoal);
    TestTrue(TEXT("Chinese private-use goal is conservatively classified"), ChineseGoalPlan.Plan.Rooms.Num() >= 1 && ChineseGoalPlan.Plan.Rooms[0].Label.Contains(TEXT("private domestic")));
    auto BroadSupport=PrivateGoal; BroadSupport.Need=TEXT("support for a neighbor");
    const auto BroadSupportPlan=HearthResidentBuildingPlanner::Build(BroadSupport);
    TestEqual(TEXT("Generic support wording does not invent a storage room"), BroadSupportPlan.Plan.Rooms.Num(), 1);
    TestTrue(TEXT("Different explicit goals produce finite buildable plans"), PrivatePlan.bBuildable && WorkshopPlan.bBuildable && CourtyardPlan.bBuildable);
    auto ThreeGoal=PrivateGoal; ThreeGoal.Need=TEXT("quiet private domestic room, workshop supporting store, neighbor-facing courtyard"); ThreeGoal.MaxInitialRooms=3; ThreeGoal.Budget=200; ThreeGoal.Stone=200; ThreeGoal.Planks=200; ThreeGoal.Beams=200; ThreeGoal.RoofMaterial=TEXT("timber");
    const auto ThreePlan=HearthResidentBuildingPlanner::Build(ThreeGoal);
    TestEqual(TEXT("Three explicit uses may request three affordable rooms"), ThreePlan.Plan.Rooms.Num(), 3);
    TestTrue(TEXT("Three-room plan records the storage or support intention"), ThreePlan.Plan.Rooms.Num() == 3 && ThreePlan.Plan.Rooms[1].Label.Contains(TEXT("workshop")) && ThreePlan.Plan.Rooms[2].Label.Contains(TEXT("courtyard")));
    TestTrue(TEXT("Three-room wood plan preserves separated growth geometry"), ThreePlan.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){ return C.Id.Contains(TEXT("room_0_floor")) && C.Offset.X == 0.f && C.Offset.Y == 0.f; }) && ThreePlan.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){ return C.Id.Contains(TEXT("room_1_floor")) && C.Offset.X == 400.f && C.Offset.Y == 0.f; }) && ThreePlan.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){ return C.Id.Contains(TEXT("room_2_floor")) && C.Offset.X == 800.f && C.Offset.Y == 0.f; }));
    auto CappedGoal=ThreeGoal; CappedGoal.MaxInitialRooms=1;
    const auto CappedPlan=HearthResidentBuildingPlanner::Build(CappedGoal);
    TestEqual(TEXT("Caller cap limits growth-aware initial rooms"), CappedPlan.Plan.Rooms.Num(), 1);
    TestTrue(TEXT("Growth-aware plan remains finite-resource valid"), HearthStructurePlan::Validate(ThreePlan.Plan,HearthResidentBuildingPlanner::ValidationContext(ThreeGoal)).bValid);
    TestTrue(TEXT("Legacy attached-row geometry remains unchanged by goal labeling"), Family.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){ return C.Id.Contains(TEXT("room_1")) && C.Offset.X == 200.f && C.Offset.Y == 0.f; }));
    TestTrue(TEXT("Planner reuses the existing foundation catalog ID"), Small.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C) { return C.CatalogId == TEXT("foundation_stone_2m"); }));
    TestTrue(TEXT("Planner reuses the existing timber door catalog ID"), Small.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C) { return C.CatalogId == TEXT("wall_door_timber_2m"); }));
    const auto SmallValidation = HearthStructurePlan::Validate(Small.Plan, HearthResidentBuildingPlanner::ValidationContext(SmallInput));
    TestTrue(TEXT("Base plan validates against current real resources"), SmallValidation.bValid);
    TestEqual(TEXT("Foundation origin is its catalog top datum"), Small.Plan.Components[0].Offset.Z, 0.0);
    TestEqual(TEXT("Floor origin rests on the same foundation datum"), Small.Plan.Components[1].Offset.Z, 0.0);
    TestTrue(TEXT("Roof points to the beam support"), Small.Plan.Components.ContainsByPredicate([](const FHearthStructureComponent& C) { return C.CatalogId == TEXT("roof_slope_timber_2m") && C.bRequiresSupport && C.SupportsComponentId.Contains(TEXT("_beam")); }));

    auto PlasterInput=Inputs(1,TEXT("mason"),0); PlasterInput.WallMaterial=TEXT("plaster"); PlasterInput.RoofMaterial=TEXT("terracotta");
    const auto Plaster=HearthResidentBuildingPlanner::Build(PlasterInput);
    TestTrue(TEXT("Unsupported finish preference still yields an honest executable plan"),Plaster.bBuildable);
    TestTrue(TEXT("Plaster preference defers to supplied timber wall"),Plaster.Plan.Components.ContainsByPredicate([](const auto& C){return C.CatalogId==TEXT("wall_timber_2m") && C.Materials.Num()==1 && C.Materials[0].MaterialId==TEXT("plank");}));
    TestTrue(TEXT("Terracotta preference defers to supplied timber roof"),Plaster.Plan.Components.ContainsByPredicate([](const auto& C){return C.CatalogId==TEXT("roof_slope_timber_2m") && C.Materials.Num()==1 && C.Materials[0].MaterialId==TEXT("plank");}));
    TestTrue(TEXT("Deferred finish reason distinguishes preference from installed material"),Plaster.Plan.Reasons.Budget.Contains(TEXT("preferred wall=plaster roof=terracotta")) && Plaster.Plan.Reasons.Budget.Contains(TEXT("no plaster production inventory")) && Plaster.Plan.Reasons.Budget.Contains(TEXT("need 12 tiles for one room, have 0")));
    auto TerracottaInput=Inputs(1,TEXT("potter"),0); TerracottaInput.RoofMaterial=TEXT("terracotta"); TerracottaInput.Tiles=12;
    const auto Terracotta=HearthResidentBuildingPlanner::Build(TerracottaInput);
    TestTrue(TEXT("Available tile stock makes the preferred terracotta roof executable"),Terracotta.bBuildable);
    TestEqual(TEXT("One room consumes two six-tile roof batches"),Terracotta.Plan.Components.FilterByPredicate([](const auto& C){return C.CatalogId==TEXT("roof_slope_terracotta_2m") && C.Materials.Num()==1 && C.Materials[0].MaterialId==TEXT("tiles") && C.Materials[0].Quantity==6;}).Num(),2);
    TestTrue(TEXT("Terracotta plan validates against finite tile stock"),HearthStructurePlan::Validate(Terracotta.Plan,HearthResidentBuildingPlanner::ValidationContext(TerracottaInput)).bValid);
    const auto TerracottaRuntime=HearthPlannedConstructionAdapter::Convert(Terracotta.Plan,2,{});
    TestTrue(TEXT("Terracotta plan converts to executable cargo records"),TerracottaRuntime.bAccepted && TerracottaRuntime.Components.ContainsByPredicate([](const FHearthCottageComponent& C){return C.AssetId==TEXT("roof_slope_terracotta_2m") && C.Stage==4 && C.MaterialType==6 && C.MaterialAmount==6;}));
    auto StoneInput=Inputs(1,TEXT("mason"),0); StoneInput.WallMaterial=TEXT("stone"); StoneInput.RoofMaterial=TEXT("slateblue");
    const auto Stone=HearthResidentBuildingPlanner::Build(StoneInput);
    TestTrue(TEXT("Stone and slate choice is buildable"),Stone.bBuildable);
    TestTrue(TEXT("Stone choice changes the wall and door assets"),Stone.Plan.Components.ContainsByPredicate([](const auto& C){return C.CatalogId==TEXT("wall_stone_2m");}) && Stone.Plan.Components.ContainsByPredicate([](const auto& C){return C.CatalogId==TEXT("wall_door_stone_2m");}));
    TestTrue(TEXT("Unavailable slate tile preference defers to supplied timber roof"),Stone.Plan.Components.ContainsByPredicate([](const auto& C){return C.CatalogId==TEXT("roof_slope_timber_2m");}));
    TestTrue(TEXT("Material choice and finite recipe remain in plan reasons"),Stone.Plan.Reasons.Budget.Contains(TEXT("preferred wall=stone roof=slateblue")) && Stone.Plan.Reasons.Budget.Contains(TEXT("executable wall=stone roof=timber")) && Stone.Plan.Reasons.Budget.Contains(TEXT("stone=5")));

    TArray<FHearthCottageComponent> Empty;
    const auto BaseRuntime = HearthPlannedConstructionAdapter::Convert(Family.Plan, 2, Empty);
    TestTrue(TEXT("Base plan converts to executable cottage parts"), BaseRuntime.bAccepted);
    if (BaseRuntime.Components.Num() > 2)
        TestTrue(TEXT("Adapter rotates local component positions toward the road"), !BaseRuntime.Components[2].Offset.Equals(Family.Plan.Components[2].Offset));
    const auto FirstExtensionRuntime = HearthPlannedConstructionAdapter::Convert(Family.Expansion.ResultingPlan, 2, BaseRuntime.Components);
    TestTrue(TEXT("First expansion converts through the adapter"), FirstExtensionRuntime.bAccepted);
    const auto SecondExtensionRuntime = HearthPlannedConstructionAdapter::Convert(Multi.Expansion.ResultingPlan, 2, FirstExtensionRuntime.Components);
    TestTrue(TEXT("Second expansion converts through the adapter"), SecondExtensionRuntime.bAccepted);
    TestEqual(TEXT("Second expansion shares one boundary wall"), SecondExtensionRuntime.Components.Num(), FirstExtensionRuntime.Components.Num() + 15);
    for (const FHearthCottageComponent& Old : FirstExtensionRuntime.Components)
        TestTrue(TEXT("Existing runtime component fields remain unchanged"), SecondExtensionRuntime.Components.ContainsByPredicate([&](const FHearthCottageComponent& Current) { return Current.Id == Old.Id && Current.AssetId == Old.AssetId && Current.Offset == Old.Offset && Current.Yaw == Old.Yaw && Current.Stage == Old.Stage && Current.MaterialType == Old.MaterialType && Current.MaterialAmount == Old.MaterialAmount && Current.Owner == Old.Owner; }));
    const auto StoneRuntime=HearthPlannedConstructionAdapter::Convert(Stone.Plan,4,Empty);
    TestTrue(TEXT("Selected stone wall assets convert to executable NPC units"),StoneRuntime.bAccepted);
    TestTrue(TEXT("Stone wall units request actual stone cargo"),StoneRuntime.Components.ContainsByPredicate([](const auto& C){return C.AssetId==TEXT("wall_stone_2m") && C.MaterialType==2 && C.MaterialAmount==1;}));

    auto Poor = Inputs(1, TEXT("general"), 0); Poor.Stone = 0; Poor.Planks = 1; Poor.Beams = 0; Poor.Budget = 2;
    const auto Unfunded = HearthResidentBuildingPlanner::Build(Poor);
    TestFalse(TEXT("Planner does not invent materials for an unfunded plan"), Unfunded.bBuildable);
    TestTrue(TEXT("Unfunded plan explains the constraint"), Unfunded.Reason.Contains(TEXT("current stone")));

    auto NoRoad = Inputs(1, TEXT("general"), 0); NoRoad.bRoadAccessible = false;
    const auto Inaccessible = HearthResidentBuildingPlanner::Build(NoRoad);
    TestFalse(TEXT("Planner rejects a house without road access"), Inaccessible.bBuildable);
    TestTrue(TEXT("Road failure is explicit"), Inaccessible.Reason.Contains(TEXT("road access")));

    const TCHAR* Archetypes[] = { TEXT("rowhouse"), TEXT("shop_house"), TEXT("courtyard_workshop"), TEXT("warehouse"), TEXT("inn") };
    TArray<FHearthResidentBuildingPlan> ArchetypePlans;
    for (const TCHAR* Archetype : Archetypes)
    {
        const auto Plan = HearthResidentBuildingPlanner::Build(ArchetypeInput(Archetype));
        AddInfo(FString::Printf(TEXT("Archetype %s: %s"), Archetype, *Plan.Reason));
        TestTrue(FString::Printf(TEXT("%s builds from native catalog parts"), Archetype), Plan.bBuildable);
        TestTrue(FString::Printf(TEXT("%s records its archetype"), Archetype), Plan.Plan.Reasons.Budget.Contains(FString::Printf(TEXT("archetype=%s"), Archetype)));
        TestTrue(FString::Printf(TEXT("%s has multiple real rooms"), Archetype), Plan.Plan.Rooms.Num() >= 2);
        TestTrue(FString::Printf(TEXT("%s uses measured catalog bounds"), Archetype), HearthStructurePlan::Validate(Plan.Plan, HearthResidentBuildingPlanner::ValidationContext(ArchetypeInput(Archetype))).bValid);
        const auto Runtime = HearthPlannedConstructionAdapter::Convert(Plan.Plan, 7, {});
        TestTrue(FString::Printf(TEXT("%s converts through the existing adapter"), Archetype), Runtime.bAccepted);
        ArchetypePlans.Add(Plan);
    }
    TestEqual(TEXT("All five buildable archetypes returned a plan"), ArchetypePlans.Num(), 5);
    if (ArchetypePlans.Num() == 5)
    {
        TestTrue(TEXT("Rowhouse keeps a narrow attached row geometry"), ArchetypePlans[0].Plan.Components.ContainsByPredicate([](const auto& C) { return C.Id.Contains(TEXT("room_1_floor")) && C.Offset.X == 200.f && C.Offset.Y == 0.f; }));
        TestTrue(TEXT("New shop house keeps its first room at the site frontage"), ArchetypePlans[1].Plan.Components.ContainsByPredicate([](const auto& C) { return C.Id.Contains(TEXT("room_0_floor")) && C.Offset.Y == 0.f; }) && ArchetypePlans[1].Plan.Components.ContainsByPredicate([](const auto& C) { return C.Id.Contains(TEXT("room_1_floor")) && C.Offset.Y == 600.f; }));
        TestTrue(TEXT("Courtyard workshop contains a non-door courtyard access opening"), ArchetypePlans[2].Plan.Openings.ContainsByPredicate([](const auto& Opening) { return !Opening.bDoor; }));
        TestTrue(TEXT("Warehouse uses a separated stacked bay layout"), ArchetypePlans[3].Plan.Components.ContainsByPredicate([](const auto& C) { return C.Id.Contains(TEXT("room_1_floor")) && C.Offset.X == 200.f; }) && ArchetypePlans[3].Plan.Components.ContainsByPredicate([](const auto& C) { return C.Id.Contains(TEXT("room_2_floor")) && C.Offset.Y == 600.f; }));
        TestTrue(TEXT("Inn exposes connected common and guest rooms"), ArchetypePlans[4].Plan.Rooms.IsValidIndex(0) && ArchetypePlans[4].Plan.Rooms[0].Label.Contains(TEXT("common")) && ArchetypePlans[4].Plan.Connections.ContainsByPredicate([](const auto& Connection) { return Connection.Id.Contains(TEXT("archetype_room_link_0_1")); }));
        auto ArchetypeExpansionInput = ArchetypeInput(TEXT("rowhouse"));
        TestFalse(TEXT("A six-room archetype rejects further growth"), HearthResidentBuildingPlanner::AppendExpansion(ArchetypePlans[0], ArchetypeExpansionInput));
    }

    auto KeepInput = ArchetypeInput(TEXT("keep"));
    const auto Keep = HearthResidentBuildingPlanner::Build(KeepInput);
    TestFalse(TEXT("Keep delegates construction to the main keep module"), Keep.bBuildable);
    TestTrue(TEXT("Keep delegation is explicit"), Keep.Reason.Contains(TEXT("keep module")));

    auto ShortArchetype = ArchetypeInput(TEXT("warehouse"));
    ShortArchetype.Beams = 1;
    ShortArchetype.Planks = 1;
    ShortArchetype.Stone = 1;
    ShortArchetype.Budget = 1;
    const auto UnfundedArchetype = HearthResidentBuildingPlanner::Build(ShortArchetype);
    TestFalse(TEXT("Archetype does not collapse to a cheaper fake house"), UnfundedArchetype.bBuildable);
    TestTrue(TEXT("Archetype shortage is explicit"), UnfundedArchetype.Reason.Contains(TEXT("material_shortage")) || UnfundedArchetype.Reason.Contains(TEXT("budget_exceeded")));

    auto CanopyInput=FamilyInput;
    CanopyInput.ExtensionKey=TEXT("tavern_canopy_v1");
    CanopyInput.Need=TEXT("tavern_canopy_seating");
    CanopyInput.RoofMaterial=TEXT("terracotta");
    CanopyInput.Budget=8; CanopyInput.Planks=3; CanopyInput.Beams=3; CanopyInput.Tiles=6;
    auto NativeCanopy=Family;
    const int32 NativeBefore=NativeCanopy.Expansion.ResultingPlan.Components.Num();
    TestTrue(TEXT("Native host accepts an incremental tavern canopy"),HearthResidentBuildingPlanner::AppendCanopy(NativeCanopy,CanopyInput));
    AddInfo(TEXT("Native canopy: ")+NativeCanopy.Expansion.Reason);
    const FHearthStructurePlan& NativeCanopyPlan=NativeCanopy.Expansion.ResultingPlan;
    TestTrue(TEXT("Canopy keeps the native host PlanId and old component IDs"),NativeCanopyPlan.PlanId==Family.Expansion.ResultingPlan.PlanId
        && NativeCanopyPlan.Components.Num()>NativeBefore
        && NativeCanopyPlan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){return C.ExtensionId==TEXT("tavern_canopy_v1") && C.CatalogId==TEXT("canopy_terracotta_2m");}));
    TestEqual(TEXT("Native canopy adds two real timber benches"),NativeCanopyPlan.Components.FilterByPredicate([](const FHearthStructureComponent& C){return C.ExtensionId==TEXT("tavern_canopy_v1") && C.CatalogId==TEXT("bench_timber");}).Num(),2);
    const FString FrontRoofId=HearthStructurePlan::StableId(NativeCanopyPlan,TEXT("component"),TEXT("room_0_roof_front"));
    TestTrue(TEXT("Native canopy records a host attachment and structural support links"),NativeCanopyPlan.Attachments.ContainsByPredicate([&](const FHearthStructureAttachment& A)
        {return A.CatalogId==TEXT("canopy_terracotta_2m") && A.ParentComponentId==FrontRoofId;})
        && NativeCanopyPlan.Connections.ContainsByPredicate([&](const FHearthStructureConnection& C){return C.Id.EndsWith(TEXT(":canopy_roof_host")) && C.ToComponentId==FrontRoofId && C.bLoadBearing;})
        && NativeCanopyPlan.Connections.ContainsByPredicate([](const FHearthStructureConnection& C){return C.Id.EndsWith(TEXT(":canopy_beam_right_post")) && C.bLoadBearing;}));
    auto FindCanopyPart=[&](const TCHAR* Key)
    {
        const FString Id=HearthStructurePlan::StableId(NativeCanopyPlan,TEXT("component"),Key);
        return NativeCanopyPlan.Components.FindByPredicate([&](const FHearthStructureComponent& C){return C.Id==Id;});
    };
    const auto* CanopyRoof=FindCanopyPart(TEXT("canopy_roof"));
    const auto* CanopyDeck=FindCanopyPart(TEXT("canopy_deck"));
    const auto* CanopyBeam=FindCanopyPart(TEXT("canopy_beam"));
    const auto* CanopyRidge=FindCanopyPart(TEXT("canopy_ridge"));
    const auto* HostDoor=FindCanopyPart(TEXT("room_0_door"));
    const auto* HostFloor=FindCanopyPart(TEXT("room_0_floor"));
    if (CanopyRoof && CanopyDeck && CanopyBeam && CanopyRidge && HostDoor && HostFloor)
    {
        const FBox RoofBounds=ResidentCanopyTestBounds(*CanopyRoof),DeckBounds=ResidentCanopyTestBounds(*CanopyDeck);
        TestTrue(TEXT("Canopy back meets door edge within 2cm"),FMath::IsNearlyEqual(ResidentCanopyTestBounds(*HostDoor).Min.Y-RoofBounds.Max.Y,2.0,.01));
        const double PorchGap=ResidentCanopyTestBounds(*HostFloor).Min.Y-DeckBounds.Max.Y;
        TestTrue(TEXT("Deck meets host porch within the 25cm entry tolerance"),PorchGap>=0.0 && PorchGap<=25.0);
        TestTrue(TEXT("Roof fascia rests on the real front beam"),FMath::IsNearlyEqual(RoofBounds.Min.Z,ResidentCanopyTestBounds(*CanopyBeam).Max.Z,.01)
            && FMath::IsNearlyEqual(CanopyBeam->Offset.Y,CanopyRoof->Offset.Y-125.0,.01));
        TestTrue(TEXT("Ridge uses its raised source datum and spans across the canopy"),FMath::IsNearlyEqual(ResidentCanopyTestBounds(*CanopyRidge).Min.Z,RoofBounds.Max.Z,.01)
            && FMath::IsNearlyEqual(FMath::Abs(FRotator::NormalizeAxis(CanopyRidge->Orientation.Yaw-CanopyRoof->Orientation.Yaw)),90.0,.01));
        TestTrue(TEXT("Planner applies the canopy native import rotation"),CanopyRoof->Orientation.Equals(FRotator(0,180,0)));
    }
    for (const auto& Old:Family.Expansion.ResultingPlan.Components)
        TestTrue(TEXT("Appending canopy preserves every old component and its materials"),NativeCanopyPlan.Components.ContainsByPredicate([&](const FHearthStructureComponent& C)
        { return C.Id==Old.Id && C.CatalogId==Old.CatalogId && C.ExtensionId==Old.ExtensionId && C.Offset==Old.Offset
            && C.Orientation.Equals(Old.Orientation) && C.BoundsMin==Old.BoundsMin && C.BoundsMax==Old.BoundsMax
            && C.MaterialCost==Old.MaterialCost && C.RecipeId==Old.RecipeId && C.SupportsComponentId==Old.SupportsComponentId
            && C.Materials.Num()==Old.Materials.Num() && C.Materials.Num()>0
            && C.Materials[0].MaterialId==Old.Materials[0].MaterialId && C.Materials[0].Quantity==Old.Materials[0].Quantity; }));
    auto CanopyValidationInput=CanopyInput; CanopyValidationInput.Budget=300; CanopyValidationInput.Planks=100; CanopyValidationInput.Beams=100; CanopyValidationInput.Stone=100; CanopyValidationInput.Tiles=100;
    TestTrue(TEXT("Native canopy passes pure structural validation with finite resources"),HearthStructurePlan::Validate(NativeCanopyPlan,HearthResidentBuildingPlanner::ValidationContext(CanopyValidationInput)).bValid);
    if (CanopyRoof)
    {
        auto DetachedCanopy=NativeCanopyPlan;
        auto* DetachedRoof=DetachedCanopy.Components.FindByPredicate([&](const FHearthStructureComponent& C){return C.Id==CanopyRoof->Id;});
        DetachedRoof->Offset.Y-=300.f;
        const auto DetachedValidation=HearthStructurePlan::Validate(DetachedCanopy,HearthResidentBuildingPlanner::ValidationContext(CanopyValidationInput));
        TestTrue(TEXT("Connection metadata cannot legitimize a roof detached from its support"),!DetachedValidation.bValid
            && DetachedValidation.Issues.ContainsByPredicate([](const FString& Issue){return Issue.StartsWith(TEXT("unsupported_component:"));}));
    }
    const auto NativeBaseRuntime=HearthPlannedConstructionAdapter::Convert(Family.Expansion.ResultingPlan,2,{});
    const auto NativeCanopyRuntime=HearthPlannedConstructionAdapter::Convert(NativeCanopyPlan,2,NativeBaseRuntime.Components);
    TestTrue(TEXT("Native canopy converts to real runtime construction components"),NativeBaseRuntime.bAccepted && NativeCanopyRuntime.bAccepted
        && NativeCanopyRuntime.Components.ContainsByPredicate([](const FHearthCottageComponent& C){return C.AssetId==TEXT("bench_timber") && C.Stage==3 && C.MaterialType==3;}));
    AddInfo(TEXT("Native canopy adapter: ")+NativeCanopyRuntime.Reason);
    if (CanopyRoof)
        TestTrue(TEXT("Adapter preserves native canopy orientation relative to the world plan"),NativeCanopyRuntime.Components.ContainsByPredicate([&](const FHearthCottageComponent& C)
        { return C.Id==CanopyRoof->Id && FMath::IsNearlyEqual(C.Yaw,static_cast<float>(FRotator::NormalizeAxis(NativeCanopyPlan.Footprint.Orientation.Yaw+CanopyRoof->Orientation.Yaw)),.01f); }));
    const int32 NativeCanopyCount=NativeCanopyPlan.Components.Num();
    TestTrue(TEXT("Repeated native canopy request is idempotent"),HearthResidentBuildingPlanner::AppendCanopy(NativeCanopy,CanopyInput));
    TestEqual(TEXT("Idempotent canopy request does not duplicate components"),NativeCanopy.Expansion.ResultingPlan.Components.Num(),NativeCanopyCount);

    FHearthResidentBuildingPlan LegacyCanopy;
    FHearthStructureFootprint LegacyFootprint; LegacyFootprint.Size=FVector2D(450.f,350.f); LegacyCanopy.Plan=HearthStructurePlan::MakePlan(TEXT("legacy-site:tavern_canopy_v1"),TEXT("legacy-seed"),LegacyFootprint,FHearthStructureReasonFields());
    LegacyCanopy.Expansion.ResultingPlan=LegacyCanopy.Plan; LegacyCanopy.bBuildable=true;
    auto LegacyInput=CanopyInput; LegacyInput.ResidentId=TEXT("legacy-tavern"); LegacyInput.StableSeed=TEXT("legacy-seed");
    TestTrue(TEXT("Legacy explicit host plot accepts a canopy-only attached plan"),HearthResidentBuildingPlanner::AppendCanopy(LegacyCanopy,LegacyInput));
    AddInfo(TEXT("Legacy canopy: ")+LegacyCanopy.Expansion.Reason);
    const FHearthStructurePlan& LegacyPlan=LegacyCanopy.Expansion.ResultingPlan;
    TestTrue(TEXT("Legacy attached plan contains only incremental canopy pieces"),LegacyPlan.Rooms.IsEmpty()
        && !LegacyPlan.Components.ContainsByPredicate([](const FHearthStructureComponent& C){return C.CatalogId==TEXT("foundation_stone_2m") || C.CatalogId.StartsWith(TEXT("wall_"));}));
    TestTrue(TEXT("Legacy attached plan keeps two real benches without a fake host attachment"),LegacyPlan.Components.FilterByPredicate([](const FHearthStructureComponent& C){return C.CatalogId==TEXT("bench_timber");}).Num()==2 && LegacyPlan.Attachments.IsEmpty());
    const auto LegacyValidation=HearthStructurePlan::Validate(LegacyPlan,HearthResidentBuildingPlanner::ValidationContext(LegacyInput));
    TestTrue(TEXT("Legacy canopy retains real support contacts and fits exact incremental resources"),LegacyValidation.bValid);
    AddInfo(TEXT("Legacy validation: ")+FString::Join(LegacyValidation.Issues,TEXT(", ")));
    int32 CanopyCost=0,CanopyPlanks=0,CanopyBeams=0,CanopyTiles=0;
    for (const auto& Part:LegacyPlan.Components)
    {
        CanopyCost+=Part.MaterialCost;
        for (const auto& Material:Part.Materials)
        {
            if (Material.MaterialId==TEXT("plank")) CanopyPlanks+=Material.Quantity;
            if (Material.MaterialId==TEXT("beam")) CanopyBeams+=Material.Quantity;
            if (Material.MaterialId==TEXT("tiles")) CanopyTiles+=Material.Quantity;
        }
    }
    TestTrue(TEXT("Canopy charges exactly its eight components and finite materials"),CanopyCost==8 && CanopyPlanks==3 && CanopyBeams==3 && CanopyTiles==6);
    const auto LegacyRuntime=HearthPlannedConstructionAdapter::Convert(LegacyPlan,2,{});
    TestTrue(TEXT("Legacy canopy converts all eight incremental pieces"),LegacyRuntime.bAccepted && LegacyRuntime.Components.Num()==8);
    const int32 LegacyCount=LegacyPlan.Components.Num();
    TestTrue(TEXT("Legacy canopy-only plan is idempotent"),HearthResidentBuildingPlanner::AppendCanopy(LegacyCanopy,LegacyInput));
    TestEqual(TEXT("Legacy idempotent request does not duplicate components"),LegacyCanopy.Expansion.ResultingPlan.Components.Num(),LegacyCount);
    for (int32 Missing=0;Missing<4;++Missing)
    {
        auto ShortCanopy=Family; auto ShortInput=CanopyInput;
        if (Missing==0) --ShortInput.Budget;
        if (Missing==1) --ShortInput.Planks;
        if (Missing==2) --ShortInput.Beams;
        if (Missing==3) --ShortInput.Tiles;
        TestFalse(TEXT("Canopy rejects a shortage of any incremental input"),HearthResidentBuildingPlanner::AppendCanopy(ShortCanopy,ShortInput));
        TestEqual(TEXT("Shortage leaves the committed plan revision unchanged"),ShortCanopy.Expansion.ResultingPlan.Revision,Family.Expansion.ResultingPlan.Revision);
        TestEqual(TEXT("Shortage cannot install free components"),ShortCanopy.Expansion.ResultingPlan.Components.Num(),NativeBefore);
    }
    return true;
}
#endif
