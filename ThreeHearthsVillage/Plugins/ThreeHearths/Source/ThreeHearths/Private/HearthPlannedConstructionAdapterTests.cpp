#if WITH_DEV_AUTOMATION_TESTS
#include "HearthPlannedConstructionAdapter.h"
#include "Misc/AutomationTest.h"

namespace
{
    FHearthStructureComponent Component(const TCHAR* Id, const TCHAR* Catalog, const TCHAR* Material,
        int32 Quantity, FVector Offset, float Yaw)
    {
        FHearthStructureComponent Result;
        Result.Id = Id; Result.CatalogId = Catalog; Result.Offset = Offset;
        Result.Orientation = FRotator(0.f, Yaw, 0.f);
        FHearthStructureMaterialQuantity Input; Input.MaterialId = Material; Input.Quantity = Quantity;
        Result.Materials.Add(Input);
        return Result;
    }

    bool Same(const FHearthCottageComponent& A, const FHearthCottageComponent& B)
    {
        return A.Id == B.Id && A.AssetId == B.AssetId && A.Status == B.Status && A.Source == B.Source &&
            A.SupplyPolicy == B.SupplyPolicy && A.Offset == B.Offset && A.Yaw == B.Yaw &&
            A.Stage == B.Stage && A.MaterialType == B.MaterialType && A.MaterialAmount == B.MaterialAmount &&
            A.Owner == B.Owner && A.ReservedBy == B.ReservedBy;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearthPlannedConstructionAdapterTest,
    "ThreeHearths.Structure.PlannedConstructionAdapter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHearthPlannedConstructionAdapterTest::RunTest(const FString&)
{
    FHearthStructurePlan Base;
    Base.PlanId = TEXT("cottage");
    Base.Components.Add(Component(TEXT("foundation-a"), TEXT("foundation_stone_2m"), TEXT("stone"), 2, FVector(10, 20, 8), 15.f));
    Base.Components.Add(Component(TEXT("wall-a"), TEXT("wall_timber_2m"), TEXT("plank"), 3, FVector(10, 40, 108), 90.f));

    const auto First = HearthPlannedConstructionAdapter::Convert(Base, 7, TArray<FHearthCottageComponent>());
    TestTrue(TEXT("first conversion is accepted"), First.bAccepted);
    TestEqual(TEXT("first conversion creates both components"), First.Components.Num(), 2);
    TestEqual(TEXT("stable id comes from plan"), First.Components[0].Id, FString(TEXT("foundation-a")));
    TestEqual(TEXT("catalog is preserved as asset"), First.Components[0].AssetId, FString(TEXT("foundation_stone_2m")));
    TestTrue(TEXT("3D offset is preserved"), First.Components[0].Offset == FVector(10, 20, 8));
    TestEqual(TEXT("yaw is preserved"), First.Components[1].Yaw, 90.f);
    TestEqual(TEXT("stage and material type map to cottage executor"), First.Components[1].Stage, 3);
    TestEqual(TEXT("material amount maps without consumption"), First.Components[1].MaterialAmount, 3);
    TestEqual(TEXT("owner is assigned"), First.Components[1].Owner, 7);

    const auto Replay = HearthPlannedConstructionAdapter::Convert(Base, 7, First.Components);
    TestTrue(TEXT("same plan replay is accepted"), Replay.bAccepted);
    TestEqual(TEXT("replay is idempotent"), Replay.Components.Num(), First.Components.Num());
    for (int32 Index = 0; Index < First.Components.Num(); ++Index)
        TestTrue(TEXT("replay leaves every old field unchanged"), Same(Replay.Components[Index], First.Components[Index]));

    TArray<FHearthCottageComponent> InProgress = First.Components;
    InProgress[0].Status = TEXT("completed");
    InProgress[0].ReservedBy = 3;
    InProgress[0].Source = TEXT("installed_structure");
    InProgress[0].SupplyPolicy = TEXT("already_delivered");
    InProgress[1].Status = TEXT("transporting");
    InProgress[1].ReservedBy = 4;
    const auto ProgressReplay = HearthPlannedConstructionAdapter::Convert(Base, 7, InProgress);
    TestTrue(TEXT("replay accepts completed and transporting records"), ProgressReplay.bAccepted);
    for (int32 Index = 0; Index < InProgress.Num(); ++Index)
        TestTrue(TEXT("replay preserves all executor progress fields"), Same(ProgressReplay.Components[Index], InProgress[Index]));

    FHearthStructurePlan ExtensionOne = Base;
    ExtensionOne.Components.Add(Component(TEXT("post-ext-1"), TEXT("post_timber_2_4m"), TEXT("beam"), 1, FVector(80, 20, 8), 0.f));
    const auto Once = HearthPlannedConstructionAdapter::Convert(ExtensionOne, 7, First.Components);
    TestTrue(TEXT("first extension is accepted"), Once.bAccepted);
    TestEqual(TEXT("first extension appends one stable id"), Once.Components.Num(), 3);
    TestTrue(TEXT("base records remain unchanged after first extension"), Same(Once.Components[0], First.Components[0]) && Same(Once.Components[1], First.Components[1]));

    FHearthStructurePlan ExtensionTwo = ExtensionOne;
    ExtensionTwo.Components.Add(Component(TEXT("roof-ext-2"), TEXT("roof_slope_timber_2m"), TEXT("plank"), 2, FVector(80, 20, 248), 180.f));
    const auto Twice = HearthPlannedConstructionAdapter::Convert(ExtensionTwo, 7, Once.Components);
    TestTrue(TEXT("second extension is accepted"), Twice.bAccepted);
    TestEqual(TEXT("second extension appends only its new id"), Twice.Components.Num(), 4);
    for (int32 Index = 0; Index < Once.Components.Num(); ++Index)
        TestTrue(TEXT("old fields survive later extension"), Same(Twice.Components[Index], Once.Components[Index]));

    FHearthStructurePlan Unknown = Base;
    Unknown.Components[0].Materials[0].MaterialId = TEXT("tiles");
    const auto UnknownResult = HearthPlannedConstructionAdapter::Convert(Unknown, 7, First.Components);
    TestFalse(TEXT("unknown material is rejected"), UnknownResult.bAccepted);
    TestEqual(TEXT("unknown material rejection is atomic"), UnknownResult.Components.Num(), First.Components.Num());
    for (int32 Index = 0; Index < First.Components.Num(); ++Index)
        TestTrue(TEXT("unknown material does not mutate existing records"), Same(UnknownResult.Components[Index], First.Components[Index]));

    FHearthStructurePlan Multi = Base;
    FHearthStructureMaterialQuantity Second; Second.MaterialId = TEXT("beam"); Second.Quantity = 1;
    Multi.Components[1].Materials.Add(Second);
    const auto MultiResult = HearthPlannedConstructionAdapter::Convert(Multi, 7, First.Components);
    TestFalse(TEXT("multi-material component is rejected"), MultiResult.bAccepted);
    TestEqual(TEXT("multi-material rejection is atomic"), MultiResult.Components.Num(), First.Components.Num());
    return true;
}
#endif
