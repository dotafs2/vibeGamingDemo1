#include "HearthStructureCatalog.h"

#include "Engine/StaticMesh.h"

namespace
{
    void AddSocket(FHearthStructureCatalogEntry& Entry, const TCHAR* Id, const FVector& Position, const TCHAR* Role)
    {
        FHearthStructureCatalogSocket Socket;
        Socket.Id = Id; Socket.LocalPosition = Position; Socket.Role = Role;
        Entry.Sockets.Add(MoveTemp(Socket));
    }

    void AddSupport(FHearthStructureCatalogEntry& Entry, const TCHAR* Parent, const TCHAR* ParentSocket, const TCHAR* ChildSocket)
    {
        FHearthStructureSupportContact Contact;
        Contact.ParentCatalogId = Parent; Contact.ParentSocket = ParentSocket; Contact.ChildSocket = ChildSocket;
        Entry.SupportContacts.Add(MoveTemp(Contact));
    }

    FHearthStructureCatalogEntry Entry(const TCHAR* Id, const TCHAR* Path, const FVector& Min, const FVector& Max, const TCHAR* Origin)
    {
        FHearthStructureCatalogEntry Result;
        Result.CatalogId = Id; Result.AssetPath = Path; Result.BoundsMin = Min; Result.BoundsMax = Max; Result.OriginConvention = Origin;
        return Result;
    }

    FHearthStructureCatalogEntry WallEntry(const TCHAR* Id, const FVector& Min, const FVector& Max)
    {
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"),Id,Id,Id);
        auto Result=Entry(Id,*Path,Min,Max,TEXT("Bay edge centre; measured infill bounds; rotate around Z for other edges."));
        AddSocket(Result,TEXT("base"),FVector(0,0,Min.Z),TEXT("rests_on_floor"));
        AddSocket(Result,TEXT("top"),FVector(0,0,Max.Z),TEXT("meets_beam"));
        AddSupport(Result,TEXT("floor_timber_2m"),TEXT("support_top"),TEXT("base"));
        return Result;
    }

    FHearthStructureCatalogEntry DoorEntry(const TCHAR* Id, const FVector& Min, const FVector& Max)
    {
        auto Result=WallEntry(Id,Min,Max);
        Result.OriginConvention=TEXT("Bay edge centre; measured wall bounds; doorway remains open between side jambs.");
        if(FHearthStructureCatalogSocket* Base=Result.Sockets.FindByPredicate([](const auto& Socket){return Socket.Id==TEXT("base");})) Base->LocalPosition.Z=.16f;
        Result.bHasDoorClearance=true; Result.DoorClearanceMin=FVector2D(-.47f,.16f); Result.DoorClearanceMax=FVector2D(.47f,2.06f);
        return Result;
    }

    FHearthStructureCatalogEntry RoofEntry(const TCHAR* Id)
    {
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"),Id,Id,Id);
        auto Result=Entry(Id,*Path,FVector(-.04340045f,-1.f,-.13000008f),FVector(2.25f,1.f,1.2841004f),TEXT("Ridge projection on wall-top datum; ridge along Y, slope toward +X."));
        AddSocket(Result,TEXT("ridge"),FVector(0,0,1.2f),TEXT("meets_ridge"));
        AddSocket(Result,TEXT("eave"),FVector(2.18f,0,0),TEXT("overhangs_wall"));
        AddSupport(Result,TEXT("beam_timber_2m"),TEXT("end_x_plus"),TEXT("ridge"));
        return Result;
    }

    FHearthStructureCatalogEntry CanopyEntry(const TCHAR* Id)
    {
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"),Id,Id,Id);
        // Raw authoring bounds (m): (-1.1,-1.31000006,-.05500004) to
        // (1.1,.02540396,.58715278), wall datum at (0,0,0), eave toward -Y.
        // The UE import flips Y, without rebasing that datum. These are the
        // loaded UStaticMesh bounds from generic-needs/loaded_mesh_bounds.json.
        const FVector NativeOriginCm(0.0,64.22980499267578,26.607637405395508);
        const FVector NativeExtentCm(110.0,66.77019500732422,32.107643127441406);
        auto Result=Entry(Id,*Path,(NativeOriginCm-NativeExtentCm)/100.0,(NativeOriginCm+NativeExtentCm)/100.0,
            TEXT("Source wall datum (0,0,0) preserved; source eave -Y imports as native +Y. Default yaw 180 restores the planned -Y front; bounds and sockets are native metres."));
        Result.DefaultRotation=FRotator(0.f,180.f,0.f);
        AddSocket(Result,TEXT("support_bottom"),FVector(0,1.25,Result.BoundsMin.Z),TEXT("rests_on_front_beam"));
        AddSocket(Result,TEXT("ridge"),FVector(0,0,Result.BoundsMax.Z),TEXT("meets_roof_ridge"));
        AddSupport(Result,TEXT("beam_timber_2m"),TEXT("support_top"),TEXT("support_bottom"));
        return Result;
    }

    FHearthStructureCatalogEntry RidgeEntry(const TCHAR* Id)
    {
        const FString Path=FString::Printf(TEXT("/Game/ThreeHearths/Generated/VillageKit/%s/%s.%s"),Id,Id,Id);
        auto Result=Entry(Id,*Path,FVector(-.15f,-1.f,1.22000003f),FVector(.15f,1.f,1.40900004f),TEXT("Roof datum; ridge runs along Y, underside at local Z=1.22m."));
        AddSocket(Result,TEXT("support_bottom"),FVector(0,0,1.22000003f),TEXT("rests_on_canopy"));
        AddSocket(Result,TEXT("top"),FVector(0,0,1.40900004f),TEXT("ridge_top"));
        AddSupport(Result,TEXT("canopy_terracotta_2m"),TEXT("ridge"),TEXT("support_bottom"));
        AddSupport(Result,TEXT("canopy_slateblue_2m"),TEXT("ridge"),TEXT("support_bottom"));
        return Result;
    }

    FHearthStructureCatalogEntry BenchEntry()
    {
        auto Result=Entry(TEXT("bench_timber"),TEXT("/Game/ThreeHearths/Generated/VillageKit/bench_timber/bench_timber.bench_timber"),
            FVector(-.9f,-.1965f,0.f),FVector(.9f,.1965f,.46f),TEXT("Ground-level seating datum; 1.8m seat span, +Z up."));
        AddSocket(Result,TEXT("feet"),FVector(0,0,0),TEXT("rests_on_floor"));
        AddSocket(Result,TEXT("seat"),FVector(0,0,.46f),TEXT("usable_seat"));
        AddSupport(Result,TEXT("floor_timber_2m"),TEXT("support_top"),TEXT("feet"));
        return Result;
    }

    TArray<FHearthStructureCatalogEntry> BuildEntries()
    {
        TArray<FHearthStructureCatalogEntry> Result;

        auto Foundation = Entry(TEXT("foundation_stone_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/foundation_stone_2m/foundation_stone_2m.foundation_stone_2m"), FVector(-1.f,-1.f,-.24f), FVector(1.f,1.f,0.f), TEXT("Centre of top surface at Z=0; extends downward .24m."));
        AddSocket(Foundation, TEXT("support_top"), FVector(0,0,0), TEXT("supports_floor"));
        Result.Add(MoveTemp(Foundation));

        auto Floor = Entry(TEXT("floor_timber_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/floor_timber_2m/floor_timber_2m.floor_timber_2m"), FVector(-1.f,-1.f,0.f), FVector(1.f,1.f,.16f), TEXT("Floor structural datum at Z=0; deck ends at Z=.16."));
        AddSocket(Floor, TEXT("support_bottom"), FVector(0,0,0), TEXT("rests_on_foundation"));
        AddSocket(Floor, TEXT("support_top"), FVector(0,0,.16f), TEXT("supports_frame"));
        AddSupport(Floor, TEXT("foundation_stone_2m"), TEXT("support_top"), TEXT("support_bottom"));
        Result.Add(MoveTemp(Floor));

        auto Post = Entry(TEXT("post_timber_2_4m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/post_timber_2_4m/post_timber_2_4m.post_timber_2_4m"), FVector(-.09f,-.09f,0.f), FVector(.09f,.09f,2.4f), TEXT("Centre bottom; .18m square section, Z=0..2.4."));
        AddSocket(Post, TEXT("base"), FVector(0,0,0), TEXT("rests_on_floor"));
        AddSocket(Post, TEXT("top"), FVector(0,0,2.4f), TEXT("supports_beam"));
        AddSupport(Post, TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("base"));
        Result.Add(MoveTemp(Post));

        auto Beam = Entry(TEXT("beam_timber_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/beam_timber_2m/beam_timber_2m.beam_timber_2m"), FVector(-.91f,-.09f,0.f), FVector(.91f,.09f,.2f), TEXT("Centre underside; span fits between .18m posts; underside at storey datum+2.2."));
        AddSocket(Beam, TEXT("end_x_minus"), FVector(-.91f,0,0), TEXT("rests_on_post"));
        AddSocket(Beam, TEXT("end_x_plus"), FVector(.91f,0,0), TEXT("rests_on_post"));
        AddSocket(Beam, TEXT("support_top"), FVector(0,0,.2f), TEXT("supports_roof"));
        AddSupport(Beam, TEXT("post_timber_2_4m"), TEXT("top"), TEXT("end_x_minus"));
        Result.Add(MoveTemp(Beam));

        auto Wall = Entry(TEXT("wall_timber_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/wall_timber_2m/wall_timber_2m.wall_timber_2m"), FVector(-.906f,-.08f,.16f), FVector(.906f,.08f,2.24f), TEXT("Bay edge centre; infill Z=.16..2.24; rotate around Z for other edges."));
        AddSocket(Wall, TEXT("base"), FVector(0,0,.16f), TEXT("rests_on_floor"));
        AddSocket(Wall, TEXT("top"), FVector(0,0,2.24f), TEXT("meets_beam"));
        AddSupport(Wall, TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("base"));
        Result.Add(MoveTemp(Wall));
        Result.Add(WallEntry(TEXT("wall_plaster_2m"),FVector(-.91000003f,-.08f,.16000009f),FVector(.91000003f,.08f,2.24f)));
        Result.Add(WallEntry(TEXT("wall_stone_2m"),FVector(-.90400004f,-.08f,.166f),FVector(.90400010f,.08f,2.23399997f)));

        auto Door = Entry(TEXT("wall_door_timber_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/wall_door_timber_2m/wall_door_timber_2m.wall_door_timber_2m"), FVector(-.906f,-.128f,.125f), FVector(.906f,.092f,2.24f), TEXT("Bay edge centre; doorway remains open between side jambs."));
        AddSocket(Door, TEXT("base"), FVector(0,0,.16f), TEXT("rests_on_floor"));
        AddSocket(Door, TEXT("top"), FVector(0,0,2.24f), TEXT("meets_beam"));
        Door.bHasDoorClearance = true; Door.DoorClearanceMin = FVector2D(-.47f,.16f); Door.DoorClearanceMax = FVector2D(.47f,2.06f);
        AddSupport(Door, TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("base"));
        Result.Add(MoveTemp(Door));
        Result.Add(DoorEntry(TEXT("wall_door_plaster_2m"),FVector(-.90999997f,-.12799999f,.125f),FVector(.91000003f,.092f,2.24000025f)));
        Result.Add(DoorEntry(TEXT("wall_door_stone_2m"),FVector(-.90400004f,-.12799999f,.12499997f),FVector(.90400010f,.092f,2.23399997f)));

        auto Roof = Entry(TEXT("roof_slope_timber_2m"), TEXT("/Game/ThreeHearths/Generated/VillageKit/roof_slope_timber_2m/roof_slope_timber_2m.roof_slope_timber_2m"), FVector(-.04340045f,-1.f,-.13000008f), FVector(2.25f,1.f,1.2841004f), TEXT("Ridge projection on wall-top datum; ridge along Y, slope toward +X."));
        AddSocket(Roof, TEXT("ridge"), FVector(0,0,1.2f), TEXT("meets_ridge"));
        AddSocket(Roof, TEXT("eave"), FVector(2.18f,0,0), TEXT("overhangs_wall"));
        AddSupport(Roof, TEXT("beam_timber_2m"), TEXT("end_x_plus"), TEXT("ridge"));
        Result.Add(MoveTemp(Roof));
        Result.Add(RoofEntry(TEXT("roof_slope_terracotta_2m")));
        Result.Add(RoofEntry(TEXT("roof_slope_slateblue_2m")));

        Result.Add(WallEntry(TEXT("gable_timber_4m"),FVector(-2.0262088f,-.12750004f,0.f),FVector(2.0262088f,.12750004f,1.297008f)));
        Result.Add(CanopyEntry(TEXT("canopy_terracotta_2m")));
        Result.Add(CanopyEntry(TEXT("canopy_slateblue_2m")));
        Result.Add(RidgeEntry(TEXT("roof_ridge_terracotta_2m")));
        Result.Add(RidgeEntry(TEXT("roof_ridge_slateblue_2m")));
        Result.Add(RidgeEntry(TEXT("roof_ridge_timber_2m")));
        Result.Add(BenchEntry());

        return Result;
    }
}

namespace HearthStructureCatalog
{
    const TArray<FHearthStructureCatalogEntry>& Entries()
    {
        static const TArray<FHearthStructureCatalogEntry> Catalog = BuildEntries();
        return Catalog;
    }

    const FHearthStructureCatalogEntry* Find(const FString& CatalogId)
    {
        return Entries().FindByPredicate([&](const FHearthStructureCatalogEntry& Candidate) { return Candidate.CatalogId == CatalogId; });
    }

    bool Validate(FString* OutError)
    {
        auto Fail = [&](const FString& Error) { if (OutError) *OutError = Error; return false; };
        if (Entries().Num() < 7) return Fail(TEXT("catalog_missing_required_entries"));
        for (const auto& E : Entries())
        {
            if (E.CatalogId.IsEmpty() || !E.AssetPath.StartsWith(TEXT("/Game/ThreeHearths/Generated/VillageKit/"))) return Fail(TEXT("invalid_asset_path"));
            for (int32 Axis = 0; Axis < 3; ++Axis)
                if (!FMath::IsFinite(E.BoundsMin[Axis]) || !FMath::IsFinite(E.BoundsMax[Axis]) || E.BoundsMax[Axis] <= E.BoundsMin[Axis]) return Fail(TEXT("invalid_bounds"));
            for (const auto& Socket : E.Sockets)
                for (int32 Axis = 0; Axis < 3; ++Axis) if (!FMath::IsFinite(Socket.LocalPosition[Axis])) return Fail(TEXT("invalid_socket"));
            if (E.bHasDoorClearance && (E.DoorClearanceMax.X <= E.DoorClearanceMin.X || E.DoorClearanceMax.Y <= E.DoorClearanceMin.Y)) return Fail(TEXT("invalid_door_clearance"));
        }
        return true;
    }

    bool HasFoundationToRoofSupportChain(FString* OutError)
    {
        auto Fail = [&](const FString& Error) { if (OutError) *OutError = Error; return false; };
        const TCHAR* Required[] = { TEXT("foundation_stone_2m"), TEXT("floor_timber_2m"), TEXT("post_timber_2_4m"), TEXT("beam_timber_2m"), TEXT("wall_timber_2m"), TEXT("wall_door_timber_2m"), TEXT("roof_slope_timber_2m"), TEXT("roof_slope_terracotta_2m"), TEXT("canopy_terracotta_2m"), TEXT("canopy_slateblue_2m"), TEXT("roof_ridge_terracotta_2m"), TEXT("roof_ridge_slateblue_2m"), TEXT("bench_timber") };
        for (const TCHAR* Id : Required) if (!Find(Id)) return Fail(FString::Printf(TEXT("missing_chain_entry:%s"), Id));
        auto HasContact = [&](const TCHAR* Child, const TCHAR* Parent, const TCHAR* ParentSocket, const TCHAR* ChildSocket)
        {
            const auto* Entry = Find(Child);
            return Entry && Entry->SupportContacts.ContainsByPredicate([&](const FHearthStructureSupportContact& Contact)
            { return Contact.ParentCatalogId == Parent && Contact.ParentSocket == ParentSocket && Contact.ChildSocket == ChildSocket; });
        };
        if (!HasContact(TEXT("floor_timber_2m"), TEXT("foundation_stone_2m"), TEXT("support_top"), TEXT("support_bottom"))) return Fail(TEXT("foundation_floor_contact_missing"));
        if (!HasContact(TEXT("post_timber_2_4m"), TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("base"))) return Fail(TEXT("floor_post_contact_missing"));
        if (!HasContact(TEXT("beam_timber_2m"), TEXT("post_timber_2_4m"), TEXT("top"), TEXT("end_x_minus"))) return Fail(TEXT("post_beam_contact_missing"));
        if (!HasContact(TEXT("wall_timber_2m"), TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("base"))) return Fail(TEXT("floor_wall_contact_missing"));
        if (!HasContact(TEXT("roof_slope_timber_2m"), TEXT("beam_timber_2m"), TEXT("end_x_plus"), TEXT("ridge"))) return Fail(TEXT("beam_roof_contact_missing"));
        if (!HasContact(TEXT("roof_slope_terracotta_2m"), TEXT("beam_timber_2m"), TEXT("end_x_plus"), TEXT("ridge"))) return Fail(TEXT("beam_terracotta_roof_contact_missing"));
        if (!Find(TEXT("gable_timber_4m"))) return Fail(TEXT("gable_missing"));
        if (!HasContact(TEXT("canopy_terracotta_2m"), TEXT("beam_timber_2m"), TEXT("support_top"), TEXT("support_bottom"))) return Fail(TEXT("beam_canopy_contact_missing"));
        if (!HasContact(TEXT("canopy_slateblue_2m"), TEXT("beam_timber_2m"), TEXT("support_top"), TEXT("support_bottom"))) return Fail(TEXT("beam_slateblue_canopy_contact_missing"));
        if (!HasContact(TEXT("roof_ridge_terracotta_2m"), TEXT("canopy_terracotta_2m"), TEXT("ridge"), TEXT("support_bottom"))) return Fail(TEXT("terracotta_ridge_contact_missing"));
        if (!HasContact(TEXT("roof_ridge_slateblue_2m"), TEXT("canopy_slateblue_2m"), TEXT("ridge"), TEXT("support_bottom"))) return Fail(TEXT("slateblue_ridge_contact_missing"));
        if (!HasContact(TEXT("bench_timber"), TEXT("floor_timber_2m"), TEXT("support_top"), TEXT("feet"))) return Fail(TEXT("floor_bench_contact_missing"));
        const auto* Door = Find(TEXT("wall_door_timber_2m"));
        if (!Door || !Door->bHasDoorClearance) return Fail(TEXT("door_clearance_missing"));
        return true;
    }
}
