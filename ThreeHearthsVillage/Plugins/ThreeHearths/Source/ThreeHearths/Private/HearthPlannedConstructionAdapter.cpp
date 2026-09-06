#include "HearthPlannedConstructionAdapter.h"

namespace
{
    struct FCatalogRule
    {
        int32 Stage;
        int32 MaterialType;
        const TCHAR* MaterialId;
    };

    bool RuleFor(const FString& CatalogId, FCatalogRule& OutRule)
    {
        if (CatalogId == TEXT("foundation_stone_2m")) { OutRule = {1, 2, TEXT("stone")}; return true; }
        if (CatalogId == TEXT("floor_timber_2m")) { OutRule = {1, 3, TEXT("plank")}; return true; }
        if (CatalogId == TEXT("post_timber_2_4m") || CatalogId == TEXT("beam_timber_2m")) { OutRule = {2, 4, TEXT("beam")}; return true; }
        if (CatalogId == TEXT("wall_window_timber_2m") || CatalogId == TEXT("wall_timber_2m") ||
            CatalogId == TEXT("wall_door_timber_2m") || CatalogId == TEXT("gable_timber_4m") ||
            CatalogId == TEXT("roof_slope_timber_2m") || CatalogId == TEXT("roof_ridge_timber_2m"))
        {
            OutRule = {3, 3, TEXT("plank")};
            if (CatalogId.StartsWith(TEXT("roof_"))) OutRule.Stage = 4;
            return true;
        }
        return false;
    }

    bool MatchesImmutablePlanFields(const FHearthCottageComponent& Existing, const FHearthCottageComponent& Expected)
    {
        return Existing.Id == Expected.Id && Existing.AssetId == Expected.AssetId &&
            Existing.Offset == Expected.Offset && Existing.Yaw == Expected.Yaw &&
            Existing.Stage == Expected.Stage && Existing.MaterialType == Expected.MaterialType &&
            Existing.MaterialAmount == Expected.MaterialAmount && Existing.Owner == Expected.Owner;
    }

    FHearthCottageComponent MakeComponent(const FHearthStructureComponent& Spec, const FCatalogRule& Rule,
        const FHearthStructureMaterialQuantity& Material, int32 Owner)
    {
        FHearthCottageComponent Result;
        Result.Id = Spec.Id;
        Result.AssetId = Spec.CatalogId;
        Result.Offset = Spec.Offset;
        Result.Yaw = Spec.Orientation.Yaw;
        Result.Stage = Rule.Stage;
        Result.MaterialType = Rule.MaterialType;
        Result.MaterialAmount = Material.Quantity;
        Result.Owner = Owner;
        Result.ReservedBy = -1;
        return Result;
    }
}

FHearthPlannedConstructionResult HearthPlannedConstructionAdapter::Convert(const FHearthStructurePlan& Plan,
    int32 Owner, const TArray<FHearthCottageComponent>& Existing)
{
    FHearthPlannedConstructionResult Result;
    Result.Components = Existing;

    TMap<FString, int32> ExistingById;
    for (const FHearthCottageComponent& ExistingComponent : Existing)
    {
        if (ExistingComponent.Id.IsEmpty())
        {
            Result.Components = Existing;
            Result.Reason = TEXT("existing cottage component has no stable id");
            return Result;
        }
        if (ExistingById.Contains(ExistingComponent.Id))
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("duplicate existing component id: %s"), *ExistingComponent.Id);
            return Result;
        }
        ExistingById.Add(ExistingComponent.Id, ExistingById.Num());
    }

    TSet<FString> PlanIds;
    TArray<FHearthCottageComponent> Appends;
    for (const FHearthStructureComponent& Spec : Plan.Components)
    {
        if (Spec.Id.IsEmpty())
        {
            Result.Components = Existing;
            Result.Reason = TEXT("plan component has no stable id");
            return Result;
        }
        if (PlanIds.Contains(Spec.Id))
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("duplicate plan component id: %s"), *Spec.Id);
            return Result;
        }
        PlanIds.Add(Spec.Id);

        FCatalogRule Rule;
        if (!RuleFor(Spec.CatalogId, Rule))
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("unsupported cottage catalog: %s"), *Spec.CatalogId);
            return Result;
        }
        if (Spec.Materials.Num() != 1)
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("component %s requires exactly one material input"), *Spec.Id);
            return Result;
        }
        const FHearthStructureMaterialQuantity& Material = Spec.Materials[0];
        if (Material.Quantity <= 0)
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("component %s has invalid material quantity"), *Spec.Id);
            return Result;
        }
        if (Material.MaterialId != Rule.MaterialId)
        {
            Result.Components = Existing;
            Result.Reason = FString::Printf(TEXT("component %s material %s cannot map to its catalog"), *Spec.Id, *Material.MaterialId);
            return Result;
        }

        const FHearthCottageComponent Expected = MakeComponent(Spec, Rule, Material, Owner);
        if (const int32* ExistingIndex = ExistingById.Find(Spec.Id))
        {
            if (!MatchesImmutablePlanFields(Existing[*ExistingIndex], Expected))
            {
                Result.Components = Existing;
                Result.Reason = FString::Printf(TEXT("existing component %s conflicts with the plan"), *Spec.Id);
                return Result;
            }
        }
        else
        {
            Appends.Add(Expected);
        }
    }

    Result.Components.Append(Appends);
    Result.bAccepted = true;
    Result.Reason = TEXT("accepted");
    return Result;
}
