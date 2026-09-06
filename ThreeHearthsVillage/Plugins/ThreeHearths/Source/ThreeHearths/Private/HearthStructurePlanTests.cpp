#if WITH_DEV_AUTOMATION_TESTS
#include "HearthStructurePlan.h"
#include "Misc/AutomationTest.h"

namespace
{
    FHearthStructureComponentSpec Component(const TCHAR* Catalog, const TCHAR* Key, FVector2D Offset, int32 Cost, bool bSupport = false, const TCHAR* Supports = TEXT(""))
    {
        FHearthStructureComponentSpec Spec; Spec.CatalogId = Catalog; Spec.SemanticKey = Key;
        Spec.Offset = FVector(Offset, 0.f); Spec.Size = FVector2D(80.f, 20.f); Spec.CollisionRadius = 12.f;
        Spec.MaterialCost = Cost; Spec.bRequiresSupport = bSupport; Spec.SupportsComponentKey = Supports; return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthStructurePlanTest,"ThreeHearths.StructurePlan.StableExtensionsAndValidation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthStructurePlanTest::RunTest(const FString&)
{
    FHearthStructureFootprint Footprint; Footprint.Size = FVector2D(500.f, 500.f); Footprint.Orientation = FRotator(0.f, 37.f, 0.f);
    FHearthStructureReasonFields Reasons; Reasons.Need=TEXT("shelter"); Reasons.Occupation=TEXT("carpenter");
    Reasons.Budget=TEXT("seasonal allocation"); Reasons.Relationship=TEXT("shared courtyard"); Reasons.RoadAccess=TEXT("east lane");
    FHearthStructurePlan Shed = HearthStructurePlan::MakePlan(TEXT("plan-shed"), TEXT("seed-42"), Footprint, Reasons);
    FHearthStructurePlan Courtyard = HearthStructurePlan::MakePlan(TEXT("plan-yard"), TEXT("seed-42"), Footprint, Reasons);
    FHearthStructureMaterialRecipe WoodWall; WoodWall.RecipeId=TEXT("wall_wood"); WoodWall.CatalogId=TEXT("wall_2m");
    FHearthStructureMaterialQuantity Wood; Wood.MaterialId=TEXT("plank"); Wood.Quantity=2; WoodWall.Inputs.Add(Wood);
    FHearthStructureMaterialRecipe StoneWall; StoneWall.RecipeId=TEXT("wall_stone"); StoneWall.CatalogId=TEXT("wall_2m");
    FHearthStructureMaterialQuantity Stone; Stone.MaterialId=TEXT("stone"); Stone.Quantity=3; StoneWall.Inputs.Add(Stone);
    TestTrue(TEXT("Shed registers a fixed wood recipe"), HearthStructurePlan::RegisterRecipe(Shed,WoodWall));
    TestTrue(TEXT("Courtyard registers a fixed stone recipe"), HearthStructurePlan::RegisterRecipe(Courtyard,StoneWall));
    auto Base = Component(TEXT("wall_2m"), TEXT("north_wall"), FVector2D(0.f, 100.f), 3);
    Base.RecipeId=WoodWall.RecipeId; Base.Materials=WoodWall.Inputs;
    auto YardBase = Base; YardBase.RecipeId=StoneWall.RecipeId; YardBase.Materials=StoneWall.Inputs;
    TestTrue(TEXT("Shed accepts reusable catalog component"), HearthStructurePlan::AppendComponent(Shed, Base));
    TestTrue(TEXT("Courtyard accepts the same catalog component with a different fixed recipe"), HearthStructurePlan::AppendComponent(Courtyard, YardBase));
    TestEqual(TEXT("Catalog identity is reusable"), Shed.Components[0].CatalogId, Courtyard.Components[0].CatalogId);
    TestNotEqual(TEXT("Plan instance IDs remain scoped to their plan seed"), Shed.Components[0].Id, Courtyard.Components[0].Id);
    TestTrue(TEXT("Shed accepts a room"), HearthStructurePlan::AppendRoom(Shed, TEXT("main"), TEXT("Main room")));
    FHearthStructureOpening Door; Door.Offset=FVector2D(0.f,-100.f); Door.AccessDirection=FVector2D(0.f,-1.f); Door.bDoor=true;
    TestTrue(TEXT("Shed accepts a room opening"), HearthStructurePlan::AppendOpening(Shed, TEXT("front_door"), TEXT("main"), Door));
    const FString OldId=Shed.Components[0].Id;
    auto ExtensionSpec=Component(TEXT("wall_2m"),TEXT("south_wall"),FVector2D(0.f,-100.f),3); ExtensionSpec.RecipeId=WoodWall.RecipeId; ExtensionSpec.Materials=WoodWall.Inputs;
    const FHearthStructureComponentSpec Extension[]={ExtensionSpec};
    TestTrue(TEXT("Later extension appends components"), HearthStructurePlan::AppendExtension(Shed, TEXT("extension-1"), Extension));
    TestTrue(TEXT("Extension preserves the original instance ID"), Shed.Components.ContainsByPredicate([&](const FHearthStructureComponent& C){ return C.Id==OldId; }));
    TestTrue(TEXT("Extension preserves room and opening data"), Shed.Openings.Num()==1 && Shed.Rooms.Num()==1);
    FHearthStructureValidationContext ValidContext; ValidContext.AvailableBudget=20; ValidContext.bRoadAccessible=true;
    ValidContext.AvailableMaterials.Add(Wood); ValidContext.AvailableMaterials[0].Quantity=6;
    FHearthStructureMaterialRecipe RoofRecipe; RoofRecipe.RecipeId=TEXT("roof_wood"); RoofRecipe.CatalogId=TEXT("roof"); RoofRecipe.Inputs.Add(Wood);
    TestTrue(TEXT("Shed registers a roof recipe"), HearthStructurePlan::RegisterRecipe(Shed,RoofRecipe));
    auto RoofSpec=Component(TEXT("roof"),TEXT("roof"),FVector2D(0.f,0.f),2,true,TEXT("north_wall"));
    RoofSpec.RecipeId=RoofRecipe.RecipeId; RoofSpec.Materials=RoofRecipe.Inputs; RoofSpec.Offset=FVector(0.f,100.f,100.f); RoofSpec.Height=80.f;
    TestTrue(TEXT("Upper floor component is accepted above the lower wall"), HearthStructurePlan::AppendComponent(Shed,RoofSpec));
    auto Valid=HearthStructurePlan::Validate(Shed,ValidContext);
    TestTrue(TEXT("Valid plan passes pure validation"), Valid.bValid);
    TestEqual(TEXT("Upper component preserves local Z for persistence"), Shed.Components.Last().Offset.Z, 100.0);
    TestEqual(TEXT("Upper component preserves explicit height for persistence"), Shed.Components.Last().Height, 80.f);
    FHearthStructureAttachment RoofAttachment; RoofAttachment.Id=TEXT("roof_attachment"); RoofAttachment.ParentComponentId=OldId;
    RoofAttachment.CatalogId=TEXT("vent"); RoofAttachment.Offset=FVector(0.f,0.f,100.f); Shed.Attachments.Add(RoofAttachment);
    TestEqual(TEXT("Attachment preserves three-dimensional local offset"), Shed.Attachments.Last().Offset.Z, 100.0);
    FHearthStructurePlan Floating = HearthStructurePlan::MakePlan(TEXT("floating"),TEXT("seed-floating"),Footprint,Reasons);
    TestTrue(TEXT("Floating plan registers its roof recipe"), HearthStructurePlan::RegisterRecipe(Floating,RoofRecipe));
    auto FloatingSpec=Component(TEXT("roof"),TEXT("roof"),FVector2D(0.f,0.f),2,true,TEXT("missing")); FloatingSpec.RecipeId=RoofRecipe.RecipeId; FloatingSpec.Materials=RoofRecipe.Inputs;
    TestTrue(TEXT("Floating component can be represented"), HearthStructurePlan::AppendComponent(Floating, FloatingSpec));
    auto FloatingResult=HearthStructurePlan::Validate(Floating,ValidContext);
    TestFalse(TEXT("Unsupported component is rejected"), FloatingResult.bValid);
    FHearthStructureValidationContext BlockedContext=ValidContext;
    FHearthStructureOccupiedVolume Obstacle; Obstacle.Center=FVector2D(0.f,-150.f); Obstacle.Radius=40.f; BlockedContext.Occupied.Add(Obstacle);
    auto Blocked=HearthStructurePlan::Validate(Shed,BlockedContext);
    TestFalse(TEXT("Blocked door is rejected"), Blocked.bValid);
    FHearthStructureValidationContext CheapContext=ValidContext; CheapContext.AvailableBudget=1;
    TestFalse(TEXT("Budget overrun is rejected"), HearthStructurePlan::Validate(Shed,CheapContext).bValid);
    FHearthStructureValidationContext ShortContext=ValidContext; ShortContext.AvailableMaterials[0].Quantity=1;
    TestFalse(TEXT("Material shortage is rejected"), HearthStructurePlan::Validate(Shed,ShortContext).bValid);
    FHearthStructurePlan RepeatA=HearthStructurePlan::MakePlan(TEXT("repeat"),TEXT("same-seed"),Footprint,Reasons);
    FHearthStructurePlan RepeatB=HearthStructurePlan::MakePlan(TEXT("repeat"),TEXT("same-seed"),Footprint,Reasons);
    HearthStructurePlan::RegisterRecipe(RepeatA,WoodWall); HearthStructurePlan::RegisterRecipe(RepeatB,WoodWall);
    Base.RecipeId=WoodWall.RecipeId; Base.Materials=WoodWall.Inputs;
    TestTrue(TEXT("Repeated plan construction succeeds"), HearthStructurePlan::AppendComponent(RepeatA,Base) && HearthStructurePlan::AppendComponent(RepeatB,Base));
    TestEqual(TEXT("Same seed produces stable IDs"), RepeatA.Components[0].Id, RepeatB.Components[0].Id);
    const int32 RevisionBeforeRollback=RepeatA.Revision;
    auto GoodExtension=Base; GoodExtension.SemanticKey=TEXT("extension_wall"); GoodExtension.Offset=FVector(0.f,-100.f,0.f);
    const FHearthStructureComponentSpec BadExtension[]={GoodExtension,Base};
    TestFalse(TEXT("Partially invalid extension is rejected"), HearthStructurePlan::AppendExtension(RepeatA,TEXT("bad-extension"),BadExtension));
    TestEqual(TEXT("Failed extension restores Revision atomically"),RepeatA.Revision,RevisionBeforeRollback);
    TestEqual(TEXT("Failed extension restores component count atomically"),RepeatA.Components.Num(),1);
    return true;
}
#endif
