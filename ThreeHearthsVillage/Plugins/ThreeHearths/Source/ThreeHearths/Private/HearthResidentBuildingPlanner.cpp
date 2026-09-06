#include "HearthResidentBuildingPlanner.h"
#include <initializer_list>

namespace
{
    FHearthStructureMaterialQuantity Material(const TCHAR* Id, int32 Quantity)
    {
        FHearthStructureMaterialQuantity M; M.MaterialId = Id; M.Quantity = Quantity; return M;
    }

    FHearthStructureMaterialRecipe Recipe(const TCHAR* Id, const TCHAR* Catalog, std::initializer_list<FHearthStructureMaterialQuantity> Inputs)
    {
        FHearthStructureMaterialRecipe R; R.RecipeId = Id; R.CatalogId = Catalog;
        for (const auto& M : Inputs) R.Inputs.Add(M);
        return R;
    }

    FHearthStructureComponentSpec Spec(const TCHAR* Catalog, const FString& Key, const FVector2D& Offset, float Z, float Height, const FString& RecipeId,
        TConstArrayView<FHearthStructureMaterialQuantity> Materials, int32 Cost, bool bSupport = false, const TCHAR* Supports = TEXT(""))
    {
        FHearthStructureComponentSpec S; S.CatalogId = Catalog; S.SemanticKey = Key; S.Offset = FVector(Offset.X, Offset.Y, Z); S.Height = Height;
        S.RecipeId = RecipeId; S.Materials.Append(Materials); S.MaterialCost = Cost; S.Size = FVector2D(180.f, 20.f);
        S.CollisionRadius = 18.f; S.bRequiresSupport = bSupport; S.SupportsComponentKey = Supports; return S;
    }

    void RegisterRecipes(FHearthStructurePlan& Plan)
    {
        HearthStructurePlan::RegisterRecipe(Plan, Recipe(TEXT("resident_foundation"), TEXT("foundation_stone_2m"), { Material(TEXT("stone"), 4) }));
        HearthStructurePlan::RegisterRecipe(Plan, Recipe(TEXT("resident_floor"), TEXT("floor_timber_2m"), { Material(TEXT("plank"), 2), Material(TEXT("beam"), 1) }));
        HearthStructurePlan::RegisterRecipe(Plan, Recipe(TEXT("resident_wall"), TEXT("wall_timber_2m"), { Material(TEXT("plank"), 2) }));
        HearthStructurePlan::RegisterRecipe(Plan, Recipe(TEXT("resident_door"), TEXT("wall_door_timber_2m"), { Material(TEXT("plank"), 2) }));
        HearthStructurePlan::RegisterRecipe(Plan, Recipe(TEXT("resident_roof"), TEXT("roof_slope_timber_2m"), { Material(TEXT("plank"), 2), Material(TEXT("beam"), 1) }));
    }

    bool AddRoom(FHearthStructurePlan& Plan, int32 RoomIndex, bool bRoof, const FString& ExtensionId)
    {
        const FHearthStructurePlan Original = Plan;
        auto Fail = [&Plan, &Original]() { Plan = Original; return false; };
        const FString Prefix = FString::Printf(TEXT("room_%d"), RoomIndex);
        if (!HearthStructurePlan::AppendRoom(Plan, Prefix, RoomIndex == 0 ? TEXT("living") : TEXT("additional"), ExtensionId)) return Fail();
        const float X = RoomIndex * 360.f;
        const auto Stone = TArray<FHearthStructureMaterialQuantity>{ Material(TEXT("stone"), 4) };
        const auto Floor = TArray<FHearthStructureMaterialQuantity>{ Material(TEXT("plank"), 2), Material(TEXT("beam"), 1) };
        const auto Wall = TArray<FHearthStructureMaterialQuantity>{ Material(TEXT("plank"), 2) };
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("foundation_stone_2m"), Prefix + TEXT("_foundation"), FVector2D(X, 0.f), 0.f, 24.f, TEXT("resident_foundation"), Stone, 2), ExtensionId)) return Fail();
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("floor_timber_2m"), Prefix + TEXT("_floor"), FVector2D(X, 100.f), 24.f, 16.f, TEXT("resident_floor"), Floor, 2, true, *(Prefix + TEXT("_foundation"))), ExtensionId)) return Fail();
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("wall_timber_2m"), Prefix + TEXT("_back"), FVector2D(X - 145.f, 0.f), 40.f, 200.f, TEXT("resident_wall"), Wall, 1, true, *(Prefix + TEXT("_foundation"))), ExtensionId)) return Fail();
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("wall_timber_2m"), Prefix + TEXT("_side"), FVector2D(X, -150.f), 40.f, 200.f, TEXT("resident_wall"), Wall, 1, true, *(Prefix + TEXT("_foundation"))), ExtensionId)) return Fail();
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("wall_timber_2m"), Prefix + TEXT("_support"), FVector2D(X, 0.f), 40.f, 200.f, TEXT("resident_wall"), Wall, 1, true, *(Prefix + TEXT("_foundation"))), ExtensionId)) return Fail();
        if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("wall_door_timber_2m"), Prefix + TEXT("_door"), FVector2D(X + 145.f, 0.f), 40.f, 200.f, TEXT("resident_door"), Wall, 1, true, *(Prefix + TEXT("_foundation"))), ExtensionId)) return Fail();
        FHearthStructureOpening Opening; Opening.Offset = FVector2D(X + 145.f, 0.f); Opening.AccessDirection = FVector2D(0.f, -1.f); Opening.Width = 90.f; Opening.bDoor = true;
        if (!HearthStructurePlan::AppendOpening(Plan, Prefix + TEXT("_front_door"), Prefix, Opening, ExtensionId)) return Fail();
        if (bRoof)
        {
            const auto Roof = TArray<FHearthStructureMaterialQuantity>{ Material(TEXT("plank"), 2), Material(TEXT("beam"), 1) };
            if (!HearthStructurePlan::AppendComponent(Plan, Spec(TEXT("roof_slope_timber_2m"), Prefix + TEXT("_roof"), FVector2D(X, 0.f), 240.f, 20.f, TEXT("resident_roof"), Roof, 3, true, *(Prefix + TEXT("_support"))), ExtensionId)) return Fail();
        }
        return true;
    }

    bool Has(const FString& Text, const TCHAR* Term)
    { return Text.Contains(Term, ESearchCase::IgnoreCase, ESearchDir::FromStart); }

    FString IssuesText(const FHearthStructureValidationResult& Result)
    {
        FString Text;
        for (const FString& Issue : Result.Issues) { if (!Text.IsEmpty()) Text += TEXT(","); Text += Issue; }
        return Text;
    }

    FHearthStructureValidationContext StructuralContext()
    {
        FHearthStructureValidationContext Context; Context.AvailableBudget = 1000000000; Context.bRoadAccessible = true;
        Context.AvailableMaterials.Add(Material(TEXT("stone"), 1000000000));
        Context.AvailableMaterials.Add(Material(TEXT("plank"), 1000000000));
        Context.AvailableMaterials.Add(Material(TEXT("beam"), 1000000000));
        return Context;
    }
}

FHearthStructureValidationContext HearthResidentBuildingPlanner::ValidationContext(const FHearthResidentBuildingInput& Input)
{
    FHearthStructureValidationContext Context; Context.AvailableBudget = FMath::Max(0, Input.Budget); Context.bRoadAccessible = Input.bRoadAccessible;
    Context.AvailableMaterials.Add(Material(TEXT("stone"), FMath::Max(0, Input.Stone)));
    Context.AvailableMaterials.Add(Material(TEXT("plank"), FMath::Max(0, Input.Planks)));
    Context.AvailableMaterials.Add(Material(TEXT("beam"), FMath::Max(0, Input.Beams)));
    return Context;
}

FHearthResidentBuildingPlan HearthResidentBuildingPlanner::Build(const FHearthResidentBuildingInput& Input)
{
    FHearthResidentBuildingPlan Output;
    const FString PlanId = TEXT("resident_") + (Input.ResidentId.IsEmpty() ? TEXT("unknown") : Input.ResidentId) + TEXT("_house");
    const FString Seed = Input.StableSeed.IsEmpty() ? TEXT("resident-default") : Input.StableSeed;
    const bool bFamily = Input.HouseholdSize >= 3 || Has(Input.Need, TEXT("family")) || Has(Input.Need, TEXT("private"));
    const bool bWorkshop = Has(Input.Occupation, TEXT("craft")) || Has(Input.Occupation, TEXT("carpenter")) || Has(Input.Occupation, TEXT("smith"));
    const int32 DesiredRooms = FMath::Clamp(1 + (bFamily ? 1 : 0) + (bWorkshop && Input.FriendsNearby > 0 ? 1 : 0), 1, 2);
    const int32 AffordableRooms = FMath::Min(FMath::Min(Input.Stone / 4, Input.Planks / 10), FMath::Min(Input.Beams, Input.Budget / 8));
    const int32 RoomCount = FMath::Clamp(FMath::Min(DesiredRooms, AffordableRooms), 0, 2);
    const bool bCanRoof = Input.Planks >= RoomCount * 12 && Input.Beams >= RoomCount * 2 && Input.Budget >= RoomCount * 11;
    FHearthStructureFootprint Footprint; Footprint.Origin = Input.Origin; Footprint.Size = FVector2D(FMath::Max(900.f, RoomCount * 720.f - 250.f), 340.f); Footprint.Orientation = FRotator(0.f, Input.RoadYaw, 0.f);
    FHearthStructureReasonFields Reasons; Reasons.Need = Input.Need; Reasons.Occupation = Input.Occupation;
    Reasons.Budget = FString::Printf(TEXT("budget=%d; affordable_rooms=%d"), Input.Budget, RoomCount);
    Reasons.Relationship = FString::Printf(TEXT("friends_nearby=%d"), Input.FriendsNearby);
    Reasons.RoadAccess = Input.bRoadAccessible ? TEXT("road-accessible") : TEXT("road-inaccessible");
    Output.Plan = HearthStructurePlan::MakePlan(PlanId, Seed, Footprint, Reasons); RegisterRecipes(Output.Plan);
    if (!Input.bRoadAccessible) Output.Reason = TEXT("No buildable plan: the resident has no verified road access.");
    else if (RoomCount == 0) Output.Reason = TEXT("No buildable plan: current stone, planks, beams, or budget cannot fund one core room.");
    else
    {
        const FHearthStructurePlan BeforeAssembly = Output.Plan;
        bool Assembled = true;
        for (int32 Room = 0; Room < RoomCount; ++Room) if (!AddRoom(Output.Plan, Room, bCanRoof, FString())) { Assembled = false; break; }
        if (!Assembled)
        {
            Output.Plan = BeforeAssembly;
            Output.Reason = TEXT("Plan assembly failed and was rolled back atomically.");
        }
        else
        {
            const FHearthStructureValidationResult Validation = HearthStructurePlan::Validate(Output.Plan, ValidationContext(Input));
            Output.bBuildable = Validation.bValid;
            Output.Reason = Validation.bValid
                ? FString::Printf(TEXT("Built %d room(s) from current stone=%d, planks=%d, beams=%d; %s."), RoomCount, Input.Stone, Input.Planks, Input.Beams, bCanRoof ? TEXT("roof materials are affordable") : TEXT("roof deferred until more planks/beams arrive"))
                : TEXT("Plan rejected by structural validation: ") + IssuesText(Validation);
        }
    }

    Output.Expansion.ExtensionKey = Input.ExtensionKey.IsEmpty() ? TEXT("resident_extension_1") : Input.ExtensionKey;
    Output.Expansion.Reason = bFamily || bWorkshop ? TEXT("Reserve one adjoining room for household privacy or workshop growth when supplies improve.") : TEXT("Reserve one adjoining room for changed household needs.");
    Output.Expansion.ResultingPlan = Output.Plan;
    if (Output.bBuildable) AppendExpansion(Output, Input);
    return Output;
}

bool HearthResidentBuildingPlanner::AppendExpansion(FHearthResidentBuildingPlan& Existing, const FHearthResidentBuildingInput& Input)
{
    if (!Existing.bBuildable) return false;
    const FString Key = Input.ExtensionKey.IsEmpty() ? TEXT("resident_extension_1") : Input.ExtensionKey;
    const FHearthStructurePlan Original = Existing.Expansion.ResultingPlan.Components.IsEmpty() ? Existing.Plan : Existing.Expansion.ResultingPlan;
    FHearthStructurePlan Candidate = Original;
    const int32 RoomIndex = Candidate.Rooms.Num();
    if (!AddRoom(Candidate, RoomIndex, true, Key)) return false;
    Candidate.Footprint.Size.X += 720.f;
    const FHearthStructureValidationResult Validation = HearthStructurePlan::Validate(Candidate, StructuralContext());
    if (!Validation.bValid) return false;
    Existing.Expansion.ExtensionKey = Key;
    Existing.Expansion.ResultingPlan = MoveTemp(Candidate);
    Existing.Expansion.Reason = FString::Printf(TEXT("Extension %s appends room_%d with a preserved base and vertical supports; future supplies are required."), *Key, RoomIndex);
    return true;
}
