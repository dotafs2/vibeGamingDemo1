#include "HearthStructurePlan.h"

namespace HearthStructurePlan
{
    namespace
    {
        FString KeyId(const FHearthStructurePlan& Plan, const FString& Namespace, const FString& Key)
        {
            return FString::Printf(TEXT("%s:%s:%s:%s"), *Plan.PlanId, *Plan.StableSeed, *Namespace, *Key);
        }

        bool HasId(const FHearthStructurePlan& Plan, const FString& Id)
        {
            if (Id.IsEmpty()) return false;
            for (const auto& C : Plan.Components) if (C.Id == Id) return true;
            for (const auto& R : Plan.Rooms) if (R.Id == Id) return true;
            for (const auto& O : Plan.Openings) if (O.Id == Id) return true;
            for (const auto& A : Plan.Attachments) if (A.Id == Id) return true;
            for (const auto& C : Plan.Connections) if (C.Id == Id) return true;
            return false;
        }

        const FHearthStructureComponent* FindComponent(const FHearthStructurePlan& Plan, const FString& Id)
        {
            return Plan.Components.FindByPredicate([&](const FHearthStructureComponent& Component) { return Component.Id == Id; });
        }

        const FHearthStructureMaterialRecipe* FindRecipe(const FHearthStructurePlan& Plan, const FString& RecipeId)
        {
            return Plan.MaterialRecipes.FindByPredicate([&](const FHearthStructureMaterialRecipe& Recipe) { return Recipe.RecipeId == RecipeId; });
        }

        bool SameMaterials(const TArray<FHearthStructureMaterialQuantity>& A, const TArray<FHearthStructureMaterialQuantity>& B)
        {
            if (A.Num() != B.Num()) return false;
            for (const auto& Entry : A)
            {
                const auto* Other = B.FindByPredicate([&](const FHearthStructureMaterialQuantity& Candidate)
                { return Candidate.MaterialId == Entry.MaterialId; });
                if (!Other || Other->Quantity != Entry.Quantity) return false;
            }
            return true;
        }

        void Issue(FHearthStructureValidationResult& Result, const FString& Text)
        {
            Result.bValid = false;
            Result.Issues.AddUnique(Text);
        }

        FVector2D Rotate2D(const FVector2D& Value, float Degrees)
        {
            const float Radians = FMath::DegreesToRadians(Degrees);
            return FVector2D(Value.X * FMath::Cos(Radians) - Value.Y * FMath::Sin(Radians),
                Value.X * FMath::Sin(Radians) + Value.Y * FMath::Cos(Radians));
        }
    }

    FHearthStructurePlan MakePlan(const FString& PlanId, const FString& StableSeed,
        const FHearthStructureFootprint& Footprint, const FHearthStructureReasonFields& Reasons)
    {
        FHearthStructurePlan Plan;
        Plan.PlanId = PlanId;
        Plan.StableSeed = StableSeed;
        Plan.Footprint = Footprint;
        Plan.Reasons = Reasons;
        return Plan;
    }

    bool RegisterRecipe(FHearthStructurePlan& Plan, const FHearthStructureMaterialRecipe& Recipe)
    {
        if (Recipe.RecipeId.IsEmpty() || Recipe.CatalogId.IsEmpty() || Recipe.Inputs.IsEmpty()
            || FindRecipe(Plan, Recipe.RecipeId)) return false;
        for (const auto& Input : Recipe.Inputs) if (Input.MaterialId.IsEmpty() || Input.Quantity <= 0) return false;
        Plan.MaterialRecipes.Add(Recipe); ++Plan.Revision; return true;
    }

    FString StableId(const FHearthStructurePlan& Plan, const FString& Namespace, const FString& Key)
    {
        return KeyId(Plan, Namespace, Key);
    }

    bool AppendComponent(FHearthStructurePlan& Plan, const FHearthStructureComponentSpec& Spec,
        const FString& ExtensionId)
    {
        const FHearthStructureMaterialRecipe* Recipe = FindRecipe(Plan, Spec.RecipeId);
        if (Spec.CatalogId.IsEmpty() || Spec.SemanticKey.IsEmpty() || !Recipe || Recipe->CatalogId != Spec.CatalogId
            || !SameMaterials(Recipe->Inputs, Spec.Materials)) return false;
        const FString Id = KeyId(Plan, TEXT("component"), Spec.SemanticKey);
        if (HasId(Plan, Id) || Plan.Components.ContainsByPredicate([&](const FHearthStructureComponent& C)
            { return C.CatalogId == Spec.CatalogId && C.ExtensionId == ExtensionId && C.Offset == Spec.Offset; })) return false;
        FHearthStructureComponent Component;
        Component.Id = Id; Component.CatalogId = Spec.CatalogId; Component.ExtensionId = ExtensionId;
        Component.Offset = Spec.Offset; Component.Orientation = Spec.Orientation; Component.Size = Spec.Size;
        Component.Height = FMath::Max(0.f, Spec.Height);
        Component.MaterialCost = FMath::Max(0, Spec.MaterialCost); Component.CollisionRadius = FMath::Max(0.f, Spec.CollisionRadius);
        Component.RecipeId = Spec.RecipeId; Component.Materials = Spec.Materials;
        Component.bRequiresSupport = Spec.bRequiresSupport;
        if (!Spec.SupportsComponentKey.IsEmpty()) Component.SupportsComponentId = KeyId(Plan, TEXT("component"), Spec.SupportsComponentKey);
        Plan.Components.Add(MoveTemp(Component));
        ++Plan.Revision;
        return true;
    }

    bool AppendRoom(FHearthStructurePlan& Plan, const FString& Key, const FString& Label,
        const FString& ExtensionId)
    {
        if (Key.IsEmpty()) return false;
        const FString Id = KeyId(Plan, TEXT("room"), Key);
        if (HasId(Plan, Id)) return false;
        FHearthStructureRoom Room; Room.Id = Id; Room.Label = Label;
        Plan.Rooms.Add(MoveTemp(Room)); ++Plan.Revision;
        return true;
    }

    bool AppendOpening(FHearthStructurePlan& Plan, const FString& Key, const FString& RoomKey,
        const FHearthStructureOpening& Opening, const FString& ExtensionId)
    {
        if (Key.IsEmpty() || RoomKey.IsEmpty()) return false;
        const FString Id = KeyId(Plan, TEXT("opening"), Key);
        const FString RoomId = KeyId(Plan, TEXT("room"), RoomKey);
        if (HasId(Plan, Id) || !Plan.Rooms.ContainsByPredicate([&](const FHearthStructureRoom& R) { return R.Id == RoomId; })) return false;
        FHearthStructureOpening Copy = Opening; Copy.Id = Id; Copy.RoomId = RoomId;
        Plan.Openings.Add(MoveTemp(Copy));
        Plan.Rooms.FindByPredicate([&](FHearthStructureRoom& R) { return R.Id == RoomId; })->OpeningIds.Add(Id);
        ++Plan.Revision;
        return true;
    }

    bool AppendConnection(FHearthStructurePlan& Plan, const FString& Key,
        const FString& FromComponentKey, const FString& ToComponentKey, bool bLoadBearing,
        const FString& ExtensionId)
    {
        if (Key.IsEmpty() || FromComponentKey.IsEmpty() || ToComponentKey.IsEmpty()) return false;
        const FString Id = KeyId(Plan, TEXT("connection"), Key);
        const FString FromId = KeyId(Plan, TEXT("component"), FromComponentKey);
        const FString ToId = KeyId(Plan, TEXT("component"), ToComponentKey);
        if (HasId(Plan, Id) || !FindComponent(Plan, FromId) || !FindComponent(Plan, ToId)) return false;
        FHearthStructureConnection Connection; Connection.Id = Id; Connection.FromComponentId = FromId;
        Connection.ToComponentId = ToId; Connection.bLoadBearing = bLoadBearing;
        Plan.Connections.Add(MoveTemp(Connection)); ++Plan.Revision;
        return true;
    }

    bool AppendExtension(FHearthStructurePlan& Plan, const FString& ExtensionKey,
        TConstArrayView<FHearthStructureComponentSpec> Components)
    {
        if (ExtensionKey.IsEmpty() || Components.IsEmpty()) return false;
        const int32 Before = Plan.Components.Num();
        const int32 BeforeRevision = Plan.Revision;
        for (const FHearthStructureComponentSpec& Spec : Components)
        {
            if (!AppendComponent(Plan, Spec, ExtensionKey))
            {
                Plan.Components.SetNum(Before);
                Plan.Revision = BeforeRevision;
                return false;
            }
        }
        return true;
    }

    FHearthStructureValidationResult Validate(const FHearthStructurePlan& Plan,
        const FHearthStructureValidationContext& Context)
    {
        FHearthStructureValidationResult Result;
        if (Plan.PlanId.IsEmpty() || Plan.StableSeed.IsEmpty()) Issue(Result, TEXT("missing_plan_identity"));
        if (Plan.Footprint.Size.X <= 0 || Plan.Footprint.Size.Y <= 0) Issue(Result, TEXT("invalid_footprint"));

        int64 Cost = 0;
        TMap<FString, int32> RequiredMaterials;
        for (const FHearthStructureComponent& Component : Plan.Components)
        {
            if (Component.Id.IsEmpty() || Component.CatalogId.IsEmpty()) Issue(Result, TEXT("component_missing_identity"));
            Cost += FMath::Max(0, Component.MaterialCost);
            const FHearthStructureMaterialRecipe* Recipe = FindRecipe(Plan, Component.RecipeId);
            if (!Recipe || Recipe->CatalogId != Component.CatalogId || !SameMaterials(Recipe->Inputs, Component.Materials))
                Issue(Result, FString::Printf(TEXT("recipe_mismatch:%s"), *Component.Id));
            for (const auto& Material : Component.Materials) RequiredMaterials.FindOrAdd(Material.MaterialId) += Material.Quantity;
            const FVector2D Half = Component.Size * .5f;
            if (FMath::Abs(Component.Offset.X) + Half.X > Plan.Footprint.Size.X * .5f
                || FMath::Abs(Component.Offset.Y) + Half.Y > Plan.Footprint.Size.Y * .5f)
                Issue(Result, FString::Printf(TEXT("component_outside_footprint:%s"), *Component.Id));
            if (Component.bRequiresSupport && !FindComponent(Plan, Component.SupportsComponentId))
                Issue(Result, FString::Printf(TEXT("unsupported_component:%s"), *Component.Id));
            for (const FHearthStructureOccupiedVolume& Existing : Context.Occupied)
            {
                const bool bSameFloor = Component.Offset.Z < Existing.Z + Existing.Height && Existing.Z < Component.Offset.Z + Component.Height;
                if (bSameFloor && FVector2D::Distance(FVector2D(Component.Offset.X, Component.Offset.Y), Existing.Center) < Component.CollisionRadius + Existing.Radius)
                    Issue(Result, FString::Printf(TEXT("occupied_collision:%s"), *Component.Id));
            }
        }
        for (int32 I = 0; I < Plan.Components.Num(); ++I) for (int32 J = I + 1; J < Plan.Components.Num(); ++J)
        {
            const auto& A = Plan.Components[I]; const auto& B = Plan.Components[J];
            const bool bSameFloor = A.Offset.Z < B.Offset.Z + B.Height && B.Offset.Z < A.Offset.Z + A.Height;
            if (bSameFloor && FVector2D::Distance(FVector2D(A.Offset.X, A.Offset.Y), FVector2D(B.Offset.X, B.Offset.Y)) < A.CollisionRadius + B.CollisionRadius)
                Issue(Result, FString::Printf(TEXT("component_collision:%s:%s"), *A.Id, *B.Id));
        }
        for (const FHearthStructureConnection& Connection : Plan.Connections)
        {
            if (!FindComponent(Plan, Connection.FromComponentId) || !FindComponent(Plan, Connection.ToComponentId))
                Issue(Result, FString::Printf(TEXT("invalid_connection:%s"), *Connection.Id));
        }
        for (const FHearthStructureAttachment& Attachment : Plan.Attachments)
            if (!FindComponent(Plan, Attachment.ParentComponentId)) Issue(Result, FString::Printf(TEXT("invalid_attachment:%s"), *Attachment.Id));
        for (const FHearthStructureOpening& Opening : Plan.Openings)
        {
            if (!Plan.Rooms.ContainsByPredicate([&](const FHearthStructureRoom& R) { return R.Id == Opening.RoomId; }))
                Issue(Result, FString::Printf(TEXT("opening_missing_room:%s"), *Opening.Id));
            if (!Opening.bDoor) continue;
            if (!Context.bRoadAccessible)
            {
                Issue(Result, FString::Printf(TEXT("road_access_missing:%s"), *Opening.Id));
                continue;
            }
            const FVector2D Direction = Opening.AccessDirection.GetSafeNormal();
            const FVector2D Door = Opening.Offset;
            for (const FHearthStructureOccupiedVolume& Existing : Context.Occupied)
            {
                const FVector2D Delta = Existing.Center - Door;
                const float Along = FVector2D::DotProduct(Delta, Direction);
                const float Across = FMath::Abs(Direction.X * Delta.Y - Direction.Y * Delta.X);
                if (Along >= 0 && Along <= 200.f && Across < Existing.Radius + Opening.Width * .5f)
                    Issue(Result, FString::Printf(TEXT("blocked_door:%s"), *Opening.Id));
            }
        }
        if (Cost > Context.AvailableBudget) Issue(Result, TEXT("budget_exceeded"));
        for (const auto& Pair : RequiredMaterials)
        {
            const auto* Available = Context.AvailableMaterials.FindByPredicate([&](const FHearthStructureMaterialQuantity& Entry)
            { return Entry.MaterialId == Pair.Key; });
            if (!Available || Available->Quantity < Pair.Value)
                Issue(Result, FString::Printf(TEXT("material_shortage:%s"), *Pair.Key));
        }
        return Result;
    }
}
