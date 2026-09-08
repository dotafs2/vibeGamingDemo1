#include "HearthSettlementPlan.h"
#include "HearthRoyalWorksPlan.h"

// No world, assets, ownership, inventory or build-progress access. Distinct
// namespace also avoids collisions when UBT combines private translation units.
namespace HearthSettlementPlanDetail
{
    constexpr double DistrictDatumZ = 8.0;
    constexpr float MaxDistrictPenalty = 8.f;

    bool FinitePosition(const FVector& P)
    {
        return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z);
    }

    TArray<FHearthSettlementDistrict> FixedDistricts()
    {
        // These are advisory envelopes around the supplied checkpoint anchors,
        // not land parcels. Other terrain heights are resolved by the caller.
        return {
            {TEXT("residential_east"), TEXT("东侧既有住作与草甸"),
                TEXT("原宅保留 · 生活往来"),
                FVector(3800,-1700,DistrictDatumZ),
                {FVector(1800,-1400,8),FVector(2600,-3500,8),FVector(4600,-3900,8),
                 FVector(5500,-2800,8),FVector(5600,300,8),FVector(4400,1100,8),FVector(2400,500,8)},
                FLinearColor(.33f,.55f,.42f,1)},
            {TEXT("market_life"), TEXT("起始集市与生活核心"),
                TEXT("起点 · 买卖 · 会面"),
                FVector(-850,-650,DistrictDatumZ),
                {FVector(-1950,-1700,8),FVector(-700,-1900,8),FVector(850,-1300,8),
                 FVector(1500,0,8),FVector(950,1300,8),FVector(-900,1400,8),FVector(-1900,750,8)},
                FLinearColor(.76f,.60f,.32f,1)},
            {TEXT("workshop_west"), TEXT("西侧林缘工坊"),
                TEXT("林缘木工 · 窑场运料"),
                FVector(-3250,-450,DistrictDatumZ),
                {FVector(-4900,-1700,8),FVector(-4400,-2300,8),FVector(-3100,-1950,8),
                 FVector(-1950,-1700,8),FVector(-1900,750,8),FVector(-2700,2150,8),
                 FVector(-4200,2500,8),FVector(-4900,1600,8)},
                FLinearColor(.65f,.43f,.29f,1)},
            {TEXT("residential_southwest"), TEXT("西南既有住作聚落"),
                TEXT("既有住作混合 · 原宅保留"),
                FVector(-4350,-3700,DistrictDatumZ),
                {FVector(-6800,-2400,8),FVector(-6600,-750,8),FVector(-5000,-1000,8),
                 FVector(-4600,-2200,8),FVector(-3000,-2500,8),FVector(-2100,-3500,8),
                 FVector(-1000,-3800,8),FVector(-850,-5000,8),FVector(-2700,-5900,8),
                 FVector(-5100,-5650,8),FVector(-6500,-4100,8)},
                FLinearColor(.43f,.53f,.55f,1)},
            {TEXT("production_north_quarry"), TEXT("北侧采石作业带"),
                TEXT("石料采集 · 交接运输"),
                FVector(-300,3250,DistrictDatumZ),
                {FVector(-1900,2300,8),FVector(-500,2150,8),FVector(950,2450,8),
                 FVector(1450,3300,8),FVector(650,4200,8),FVector(-1300,4300,8),FVector(-2250,3450,8)},
                FLinearColor(.49f,.52f,.61f,1)},
            {TEXT("production_south_farm"), TEXT("南侧田野生产带"),
                TEXT("耕作 · 收获 · 粮食供给"),
                FVector(-1050,-2800,DistrictDatumZ),
                {FVector(-2600,-2700,8),FVector(-2250,-2100,8),FVector(-850,-1850,8),
                 FVector(550,-2350,8),FVector(750,-3500,8),FVector(-700,-3700,8),FVector(-1950,-3300,8)},
                FLinearColor(.56f,.62f,.29f,1)},
            {TEXT("future_west_workshops"), TEXT("西侧林缘工坊预留"),
                TEXT("待发展 · 林缘 · 木工"),FVector(-4700,8800,8),
                {FVector(-6400,4000,8),FVector(-3000,4300,8),FVector(-2800,7900,8),
                 FVector(-1600,11600,8),FVector(-4200,14100,8),FVector(-6600,10200,8)},
                FLinearColor(.65f,.43f,.29f,1)},
            // The compatibility ID remains stable, but this is deliberately a
            // planned exploration reserve rather than a promise of new homes,
            // a dungeon or a new town.
            {TEXT("future_north_residential"), TEXT("北侧未探地预留"),
                TEXT("待发展 · 未探 · 勘察"),FVector(6500,17600,8),
                {FVector(-500,16800,8),FVector(1700,14400,8),FVector(5800,16200,8),
                 FVector(10900,15000,8),FVector(13800,17400,8),FVector(10800,18900,8),
                 FVector(3500,18900,8)},FLinearColor(.33f,.55f,.42f,1)},
            {TEXT("future_east_life"), TEXT("东侧水畔草甸预留"),
                TEXT("待发展 · 生活 · 牧草"),FVector(17800,6500,8),
                {FVector(15300,1000,8),FVector(18500,-800,8),FVector(19800,3700,8),
                 FVector(19500,10700,8),FVector(16800,13800,8),FVector(14500,11700,8),
                 FVector(16300,6800,8)},FLinearColor(.76f,.60f,.32f,1)}
        };
    }

    FString StructuralGroup(const FString& Id)
    {
        if (Id.StartsWith(TEXT("tower_")) || Id == TEXT("v2_tower_foundation") || Id.StartsWith(TEXT("keep_")))
            return TEXT("main_hall");
        for (const TCHAR* Side : {TEXT("west"),TEXT("east")})
        {
            const FString S(Side);
            if (Id.StartsWith(S + TEXT("_wing_")) || Id == TEXT("v2_") + S + TEXT("_wing_foundation")
                || Id.StartsWith(TEXT("side_wing_") + S)) return S + TEXT("_wing");
            if (Id.StartsWith(TEXT("gate_tower_") + S + TEXT("_")) || Id.StartsWith(TEXT("gate_roof_") + S + TEXT("_"))
                || Id == TEXT("v2_gate_foundation_") + S || Id == TEXT("gate_foundation_") + S
                || Id == TEXT("gatehouse_") + S || Id == TEXT("gatehouse_roof_") + S) return TEXT("gate_") + S;
        }
        for (const TCHAR* Side : {TEXT("south_west"),TEXT("south_east"),TEXT("north"),TEXT("west"),TEXT("east")})
        {
            const FString S(Side);
            if (Id.StartsWith(TEXT("curtain_wall_") + S + TEXT("_")) || Id == TEXT("curtain_wall_") + S
                || Id.StartsWith(TEXT("curtain_parapet_") + S + TEXT("_"))
                || Id == TEXT("v2_curtain_foundation_") + S || Id == TEXT("curtain_foundation_") + S)
                return TEXT("curtain_") + S;
        }
        if (Id.StartsWith(TEXT("gate_lintel"))) return TEXT("gate_lintel");
        if (Id.StartsWith(TEXT("gate_floor_")) || Id.StartsWith(TEXT("gate_step_"))) return TEXT("gate_approach");
        // In particular: courtyard paths, planting and service-yard landscape
        // are not building silhouettes. Unknown new modules need classification.
        return FString();
    }

    FBox ManifestBounds(const FHearthRoyalModule& Module)
    {
        FVector Size = Module.BoundsSizeCm;
        if (Size.IsNearlyZero() && (Module.MeshPath == TEXT("/Engine/BasicShapes/Cube")
            || Module.MeshPath == TEXT("/Engine/BasicShapes/Cone")))
        {
            // Legacy v1 uses centered engine primitives (100 cm native size).
            const FVector Half = Module.Scale.GetAbs() * 50;
            Size = FBox(-Half,Half).TransformBy(FTransform(FRotator(0,Module.Yaw,0))).GetSize();
        }
        if (!FinitePosition(Size) || !FinitePosition(Module.Offset) || Size.GetMin() <= 0) return FBox(ForceInit);
        // v2 already records the scaled, rotated full bounds. Applying yaw or
        // Scale again would distort reflected native walls and roof slopes.
        return FBox(Module.Offset - Size * .5, Module.Offset + Size * .5);
    }

    void AddOutline(FHearthSettlementPlan& Out, const FString& Id, TArray<FVector> Points, bool bClosed)
    {
        for (const FVector& P : Points) Out.Bounds += P;
        Out.CastleLines.Add({Id,MoveTemp(Points),bClosed});
    }

    TArray<FVector> RectangleAt(const FBox& Box, double Z)
    {
        return {FVector(Box.Min.X,Box.Min.Y,Z),FVector(Box.Max.X,Box.Min.Y,Z),
            FVector(Box.Max.X,Box.Max.Y,Z),FVector(Box.Min.X,Box.Max.Y,Z)};
    }

    void BodyOutline(FHearthSettlementPlan& Out, const FString& Id, const FBox& Box)
    {
        const TArray<FVector> Bottom = RectangleAt(Box,Box.Min.Z);
        const TArray<FVector> Top = RectangleAt(Box,Box.Max.Z);
        AddOutline(Out,Id + TEXT("/base"),Bottom,true);
        AddOutline(Out,Id + TEXT("/eave"),Top,true);
        for (int32 I=0; I<4; ++I)
            AddOutline(Out,Id + FString::Printf(TEXT("/corner_%d"),I),{Bottom[I],Top[I]},false);
    }

    void RoofOutline(FHearthSettlementPlan& Out, const FString& Id, const FBox& Box)
    {
        // One section per 4m authored roof bay, not per strip/tile/module.
        // Extrema are conservative envelope lines, not replacement geometry.
        const double X = Box.GetCenter().X;
        const FVector Front(X,Box.Min.Y,Box.Max.Z), Back(X,Box.Max.Y,Box.Max.Z);
        AddOutline(Out,Id + TEXT("/ridge"),{Front,Back},false);
        AddOutline(Out,Id + TEXT("/front"),
            {FVector(Box.Min.X,Box.Min.Y,Box.Min.Z),Front,FVector(Box.Max.X,Box.Min.Y,Box.Min.Z)},false);
        AddOutline(Out,Id + TEXT("/back"),
            {FVector(Box.Min.X,Box.Max.Y,Box.Min.Z),Back,FVector(Box.Max.X,Box.Max.Y,Box.Min.Z)},false);
    }

    double CrossXY(const FVector& A, const FVector& B, const FVector& C)
    {
        return (B.X-A.X)*(C.Y-A.Y) - (B.Y-A.Y)*(C.X-A.X);
    }

    TArray<FVector> CastleEnvelope(TArray<FVector> Points)
    {
        Points.Sort([](const FVector& A,const FVector& B){ return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
        TArray<FVector> Unique;
        for (const FVector& P : Points)
            if (Unique.IsEmpty() || !P.Equals(Unique.Last(),.001)) Unique.Add(P);
        if (Unique.Num() < 3) return {};
        TArray<FVector> Hull;
        for (const FVector& P : Unique)
        {
            while (Hull.Num() >= 2 && CrossXY(Hull[Hull.Num()-2],Hull.Last(),P) <= 0) Hull.Pop(EAllowShrinking::No);
            Hull.Add(P);
        }
        const int32 LowerCount = Hull.Num();
        for (int32 I=Unique.Num()-2; I>=0; --I)
        {
            const FVector& P = Unique[I];
            while (Hull.Num() > LowerCount && CrossXY(Hull[Hull.Num()-2],Hull.Last(),P) <= 0) Hull.Pop(EAllowShrinking::No);
            Hull.Add(P);
        }
        Hull.Pop(EAllowShrinking::No); // closed by the caller; no duplicate end
        return Hull;
    }

    double DistanceToDistrict(const TArray<FVector>& Polygon, const FVector& P)
    {
        bool bInside = false;
        double BestSquared = TNumericLimits<double>::Max();
        for (int32 I=0, J=Polygon.Num()-1; I<Polygon.Num(); J=I++)
        {
            const FVector& A = Polygon[J]; const FVector& B = Polygon[I];
            const double DX=B.X-A.X, DY=B.Y-A.Y;
            const double Denominator=DX*DX+DY*DY;
            const double T=Denominator > 0 ? FMath::Clamp(((P.X-A.X)*DX+(P.Y-A.Y)*DY)/Denominator,0.0,1.0) : 0.0;
            BestSquared=FMath::Min(BestSquared,FMath::Square(P.X-A.X-T*DX)+FMath::Square(P.Y-A.Y-T*DY));
            if ((A.Y>P.Y)!=(B.Y>P.Y) && P.X < A.X+(P.Y-A.Y)*DX/DY) bInside=!bInside;
        }
        return bInside ? 0.0 : FMath::Sqrt(BestSquared);
    }
}

FHearthSettlementPlan HearthSettlementPlan::Build(const FVector& CastleSite, const FString& TemplateId)
{
    using namespace HearthSettlementPlanDetail;
    FHearthSettlementPlan Out;
    Out.Districts = FixedDistricts();
    if (FinitePosition(CastleSite))
    {
        const auto Royal = HearthRoyalWorksPlan::BuildForTemplate(TemplateId);
        TMap<FString,FBox> Bodies, Roofs;
        TMap<FString,double> FloorTops;
        for (const auto& Module : Royal.Modules)
        {
            if (!Module.PlantId.IsEmpty() || Module.MeshPath.IsEmpty()) continue;
            const FString Group = StructuralGroup(Module.Id);
            if (Group.IsEmpty()) continue;
            const FBox Local = ManifestBounds(Module);
            if (!Local.IsValid) continue;
            const FBox Box(Local.Min+CastleSite,Local.Max+CastleSite);
            const int32 Bay = Module.Id.Find(TEXT("_bay_"));
            if (Bay != INDEX_NONE && (Module.MeshPath.Contains(TEXT("roof_")) || Module.MeshPath.Contains(TEXT("gable_"))))
            {
                const int32 End = Module.Id.Find(TEXT("_"),ESearchCase::CaseSensitive,ESearchDir::FromStart,Bay+5);
                const FString RoofId = Module.Id.Left(End == INDEX_NONE ? Module.Id.Len() : End);
                Roofs.FindOrAdd(RoofId,FBox(ForceInit)) += Box;
            }
            else Bodies.FindOrAdd(Group,FBox(ForceInit)) += Box;
            if (Module.MeshPath.Contains(TEXT("floor_timber_")))
            {
                double* Top = FloorTops.Find(Group);
                if (Top) *Top = FMath::Min(*Top,Box.Max.Z);
                else FloorTops.Add(Group,Box.Max.Z);
            }
        }
        TArray<FVector> EnvelopePoints;
        FBox CastleBounds(ForceInit);
        const auto ExtendEnvelope = [&](const FBox& Box)
        {
            CastleBounds += Box;
            EnvelopePoints.Append(RectangleAt(Box.ExpandBy(200),CastleSite.Z));
        };
        TArray<FString> BodyIds, RoofIds;
        Bodies.GetKeys(BodyIds); BodyIds.Sort();
        Roofs.GetKeys(RoofIds); RoofIds.Sort();
        for (const FString& Id : BodyIds)
        {
            const FBox& Box = Bodies[Id];
            if (Id == TEXT("gate_approach"))
                AddOutline(Out,Id + TEXT("/footprint"),RectangleAt(Box,Box.Max.Z),true);
            else BodyOutline(Out,Id,Box);
            ExtendEnvelope(Box);
        }
        for (const FString& Id : RoofIds)
        {
            RoofOutline(Out,Id,Roofs[Id]);
            ExtendEnvelope(Roofs[Id]);
        }
        // Only the three primary occupied room footprints; courtyard planting
        // and the archive's schematic upper-storey datum are intentionally absent.
        for (const auto& Room : Royal.Rooms)
        {
            const FString Group = Room.Id == TEXT("throne_hall") ? TEXT("main_hall")
                : Room.Id == TEXT("west_side_wing") ? TEXT("west_wing")
                : Room.Id == TEXT("east_side_wing") ? TEXT("east_wing") : TEXT("");
            const double* FloorTop = FloorTops.Find(Group);
            if (Group.IsEmpty() || Room.bCourtyard || !FloorTop) continue;
            const FVector Center = CastleSite + Room.Center;
            const FVector Half(Room.Size.X*.5,Room.Size.Y*.5,0);
            AddOutline(Out,TEXT("room/") + Room.Id,RectangleAt(FBox(Center-Half,Center+Half),*FloorTop),true);
        }
        if (CastleBounds.IsValid)
        {
            FHearthSettlementDistrict Castle;
            // Keep the persisted/advisory ID stable; its displayed geography
            // now describes the central highland instead of the old SW town.
            Castle.Id=TEXT("castle_northeast"); Castle.Label=TEXT("中央高地城堡");
            Castle.Purpose=TEXT("盘山入城 · 主堡与庭院");
            Castle.LabelPosition=FVector(CastleBounds.GetCenter().X,CastleBounds.GetCenter().Y,CastleSite.Z);
            Castle.Boundary=CastleEnvelope(MoveTemp(EnvelopePoints));
            Castle.Color=FLinearColor(.64f,.50f,.66f,1);
            Out.Districts.Add(MoveTemp(Castle));
        }
    }
    for (const auto& District : Out.Districts)
    {
        Out.Bounds += District.LabelPosition;
        for (const FVector& P : District.Boundary) Out.Bounds += P;
    }
    return Out;
}

float HearthSettlementPlan::SitingPenalty(const FString& Role, const FVector& Position)
{
    using namespace HearthSettlementPlanDetail;
    if (!FinitePosition(Position)) return MaxDistrictPenalty;
    // Fixed checkpoint geography, cached once; candidate evaluation does not
    // rebuild the module manifest or query mutable world/construction state.
    static const auto Plan = Build(FVector(6500,6500,355),TEXT("royal_keep_garden_v2"));
    TArray<FString> Preferred;
    if (Role.Contains(TEXT("国王")) || Role.Contains(TEXT("门卫")) || Role.Contains(TEXT("护卫")))
        Preferred={TEXT("castle_northeast")};
    else if (Role.Contains(TEXT("农"))) Preferred={TEXT("production_south_farm")};
    else if (Role.Contains(TEXT("石"))) Preferred={TEXT("production_north_quarry")};
    else if (Role.Contains(TEXT("木")) || Role.Contains(TEXT("陶")) || Role.Contains(TEXT("铁")))
        Preferred={TEXT("workshop_west"),TEXT("future_west_workshops")};
    else if (Role.Contains(TEXT("商")) || Role.Contains(TEXT("车夫")) || Role.Contains(TEXT("旅店")))
        Preferred={TEXT("market_life"),TEXT("future_east_life")};
    else if (Role.Contains(TEXT("探索")) || Role.Contains(TEXT("勘察")))
        Preferred={TEXT("future_north_residential")};
    else Preferred={TEXT("residential_east"),TEXT("residential_southwest"),TEXT("market_life"),TEXT("future_east_life")};
    double Distance = TNumericLimits<double>::Max();
    for (const auto& District : Plan.Districts)
        if (Preferred.Contains(District.Id)) Distance=FMath::Min(Distance,DistanceToDistrict(District.Boundary,Position));
    // Continuous at boundaries: 0 inside, 4 at 24m beyond the envelope,
    // saturating at 8. This cannot change ownership or require relocation.
    return float(FMath::Clamp(Distance/4800.0,0.0,1.0))*MaxDistrictPenalty;
}
