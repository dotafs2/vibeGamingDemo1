#if WITH_DEV_AUTOMATION_TESTS
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
    TestTrue(TEXT("Deferred finish reason distinguishes preference from installed material"),Plaster.Plan.Reasons.Budget.Contains(TEXT("preferred wall=plaster roof=terracotta")) && Plaster.Plan.Reasons.Budget.Contains(TEXT("no plaster production inventory")) && Plaster.Plan.Reasons.Budget.Contains(TEXT("no tile production inventory")));
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
    return true;
}
#endif
