#include "HearthTownLayout.h"

namespace
{
    constexpr float HomeLong = 120.f;
    constexpr float HomeShort = 100.f;
    constexpr float Gap = 70.f;

    FVector2D XY(const FVector& P) { return FVector2D(P.X, P.Y); }
    FVector MakePoint(const FVector2D& P, float Z = 8.f) { return FVector(P.X, P.Y, Z); }
    float WrapAngle(float A)
    {
        while (A > 180.f) A -= 360.f;
        while (A < -180.f) A += 360.f;
        return A;
    }

    FVector2D RotatedHalf(const FVector2D& Half, float Yaw)
    {
        const float R = FMath::DegreesToRadians(Yaw);
        const float C = FMath::Abs(FMath::Cos(R));
        const float S = FMath::Abs(FMath::Sin(R));
        return FVector2D(C * Half.X + S * Half.Y, S * Half.X + C * Half.Y);
    }

    bool Inside(const FVector2D& C, const FVector2D& Half, const FHearthTownLayoutInput& I)
    {
        return C.X - Half.X >= I.IslandMin.X && C.X + Half.X <= I.IslandMax.X
            && C.Y - Half.Y >= I.IslandMin.Y && C.Y + Half.Y <= I.IslandMax.Y;
    }

    bool Overlap(const FVector2D& A, const FVector2D& AH, const FVector2D& B, const FVector2D& BH, float Extra)
    {
        return FMath::Abs(A.X - B.X) < AH.X + BH.X + Extra
            && FMath::Abs(A.Y - B.Y) < AH.Y + BH.Y + Extra;
    }

    float PointSegmentDistance(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D Delta = B - A;
        const float Denom = Delta.SizeSquared();
        const float T = Denom > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Point - A, Delta) / Denom, 0.f, 1.f) : 0.f;
        return (Point - (A + Delta * T)).Length();
    }

    bool ClearRect(const FHearthTownFootprint& Candidate, const FHearthTownLayoutInput& I, const TArray<FHearthTownFootprint>& Homes)
    {
        const FVector2D H = RotatedHalf(Candidate.HalfExtent, Candidate.Yaw);
        if (!Inside(XY(Candidate.Center), H, I)) return false;
        for (const FHearthTownRect& B : I.TerrainBlockers)
            if (Overlap(XY(Candidate.Center), H, XY(B.Center), B.HalfExtent, B.Clearance)) return false;
        for (const FHearthTownFootprint& Existing : I.ExistingBuildings)
        {
            const FVector2D EH = RotatedHalf(Existing.HalfExtent, Existing.Yaw);
            if (Overlap(XY(Candidate.Center), H, XY(Existing.Center), EH, Gap)) return false;
        }
        for (const FHearthTownFootprint& Home : Homes)
        {
            const FVector2D HH = RotatedHalf(Home.HalfExtent, Home.Yaw);
            if (Overlap(XY(Candidate.Center), H, XY(Home.Center), HH, Gap)) return false;
        }
        return true;
    }

    float AnchorScore(const FVector2D& C, const FHearthTownLayoutInput& I)
    {
        float Score = 0.f;
        auto Add = [&](const TArray<FVector>& Points, float Weight)
        {
            float Best = FLT_MAX;
            for (const FVector& P : Points) Best = FMath::Min(Best, (C - XY(P)).Length());
            if (Best < FLT_MAX) Score += Best * Weight;
        };
        Add(I.Markets, .12f); Add(I.Friends, .035f); Add(I.Workpoints, .07f);
        return Score;
    }

    struct FCandidate
    {
        FHearthTownFootprint Footprint;
        FVector2D StreetPoint = FVector2D::ZeroVector;
        float Score = 0.f;
        int32 Serial = 0;
        int32 RoadIndex = -1;
        FString StableKey;
    };
}

FHearthTownLayoutPlan HearthTownLayout::Build(const FHearthTownLayoutInput& Input)
{
    FHearthTownLayoutPlan Plan;
    Plan.Homes = Input.ExistingBuildings;
    for (FHearthTownFootprint& Existing : Plan.Homes) Existing.bExisting = true;

    TArray<FCandidate> Candidates;
    int32 Serial = 0;
    for (int32 RoadIndex = 0; RoadIndex < Input.Roads.Num(); ++RoadIndex)
    {
        const FHearthTownRoadSegment& Road = Input.Roads[RoadIndex];
        FVector2D A = XY(Road.A), B = XY(Road.B), Delta = B - A;
        const float Length = Delta.Size(); if (Length < 1.f) continue;
        const FVector2D Tangent = Delta / Length;
        const FVector2D Normal(-Tangent.Y, Tangent.X);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
        const int32 Count = FMath::Max(1, FMath::FloorToInt((Length - 2.f * HomeLong) / (2.f * HomeLong + Gap)) + 1);
        for (int32 K = 0; K < Count; ++K)
        {
            const float Alpha = (K + .5f) / Count;
            const FVector2D Street = FMath::Lerp(A, B, Alpha);
            const int32 SideFirst = ((Input.Seed + RoadIndex) & 1) ? 1 : 0;
            for (int32 SidePass = 0; SidePass < 2; ++SidePass)
            {
                const int32 Side = SidePass == 0 ? SideFirst : 1 - SideFirst;
                const FVector2D Outward = Side == 0 ? Normal : -Normal;
                const FVector2D Center = Street + Outward * (Road.Width * .5f + HomeShort + 30.f);
                FCandidate Candidate;
                Candidate.Footprint.Center = MakePoint(Center); Candidate.Footprint.HalfExtent = FVector2D(HomeLong, HomeShort);
                Candidate.Footprint.Yaw = Yaw; Candidate.Footprint.Door = MakePoint(Street + Outward * (Road.Width * .5f + 20.f));
                Candidate.StreetPoint = Street; Candidate.Serial = Serial++;
                Candidate.RoadIndex = RoadIndex;
                Candidate.StableKey = FString::Printf(TEXT("r%d_k%d_side%d"), RoadIndex, K, Side);
                Candidate.Score = AnchorScore(Center, Input) + (Center - Street).Length() * .01f + SidePass * .001f;
                Candidates.Add(MoveTemp(Candidate));
            }
        }
        Plan.StreetNodes.Add(Road.A); Plan.StreetNodes.Add(Road.B);
    }
    Candidates.Sort([](const FCandidate& L, const FCandidate& R)
    { return L.Score == R.Score ? L.Serial < R.Serial : L.Score < R.Score; });

    const int32 Wanted = FMath::Max(0, Input.RequestedHomes);
    TSet<int32> UsedRoads;
    // Seed each connected street direction once before filling the remaining homes.
    // This keeps a small plan legible as a block instead of clustering on the nearest road.
    for (const FCandidate& Candidate : Candidates)
    {
        if (Plan.Homes.Num() >= Input.ExistingBuildings.Num() + Wanted) break;
        if (UsedRoads.Contains(Candidate.RoadIndex) || !ClearRect(Candidate.Footprint, Input, Plan.Homes)) continue;
        FHearthTownFootprint Home = Candidate.Footprint;
        Home.Id = TEXT("street_home_") + Candidate.StableKey;
        Plan.Homes.Add(MoveTemp(Home)); UsedRoads.Add(Candidate.RoadIndex);
    }
    for (const FCandidate& Candidate : Candidates)
    {
        if (Plan.Homes.Num() >= Input.ExistingBuildings.Num() + Wanted) break;
        if (!ClearRect(Candidate.Footprint, Input, Plan.Homes)) continue;
        FHearthTownFootprint Home = Candidate.Footprint;
        Home.Id = TEXT("street_home_") + Candidate.StableKey;
        Plan.Homes.Add(MoveTemp(Home));
    }

    if (Plan.Homes.Num() >= 3)
    {
        FVector2D Center = FVector2D::ZeroVector; int32 Count = 0;
        for (const FHearthTownFootprint& Home : Plan.Homes) if (!Home.bExisting) { Center += XY(Home.Center); ++Count; }
        if (Count > 0) { Plan.CourtyardCenter = MakePoint(Center / Count); Plan.bHasCourtyard = true; }
    }

    for (int32 I = Input.ExistingBuildings.Num(); I < Plan.Homes.Num(); ++I)
    {
        const FHearthTownFootprint& Parent = Plan.Homes[I];
        const float R = FMath::DegreesToRadians(Parent.Yaw);
        const FVector2D Back(-FMath::Sin(R), FMath::Cos(R));
        FHearthTownExpansion Expansion;
        Expansion.Id = Parent.Id + TEXT(":expansion:rear");
        Expansion.ParentIndex = I; Expansion.Yaw = Parent.Yaw; Expansion.HalfExtent = Parent.HalfExtent;
        const FVector2D BackOffset = Back * (Parent.HalfExtent.Y * 2.f + Gap);
        const FVector2D DoorOffset = Back * Parent.HalfExtent.Y;
        Expansion.Center = Parent.Center + FVector(BackOffset.X, BackOffset.Y, 0.f);
        Expansion.Door = Parent.Center + FVector(DoorOffset.X, DoorOffset.Y, 0.f);
        FHearthTownFootprint Probe; Probe.Center = Expansion.Center; Probe.Door = Expansion.Door; Probe.HalfExtent = Expansion.HalfExtent; Probe.Yaw = Expansion.Yaw;
        if (ClearRect(Probe, Input, Plan.Homes)) Plan.Expansions.Add(MoveTemp(Expansion));
    }
    return Plan;
}

bool HearthTownLayout::IsValid(const FHearthTownLayoutPlan& Plan, const FHearthTownLayoutInput& Input)
{
    if (Plan.Homes.Num() < Input.ExistingBuildings.Num()) return false;
    for (const FVector& Node : Plan.StreetNodes)
        if (Node.X < Input.IslandMin.X || Node.X > Input.IslandMax.X || Node.Y < Input.IslandMin.Y || Node.Y > Input.IslandMax.Y) return false;
    for (int32 I = 0; I < Input.ExistingBuildings.Num(); ++I)
    {
        const auto& A = Input.ExistingBuildings[I]; const auto& B = Plan.Homes[I];
        if (A.Id != B.Id || !A.Center.Equals(B.Center, .01f) || !A.Door.Equals(B.Door, .01f) || !FMath::IsNearlyEqual(WrapAngle(A.Yaw - B.Yaw), 0.f, .01f)) return false;
    }
    FHearthTownLayoutInput CheckInput = Input;
    CheckInput.ExistingBuildings.Reset();
    TArray<FHearthTownFootprint> Empty;
    for (const FHearthTownFootprint& Home : Plan.Homes)
    {
        if (!ClearRect(Home, CheckInput, Empty)) return false;
        if (Home.Door.X < Input.IslandMin.X || Home.Door.X > Input.IslandMax.X || Home.Door.Y < Input.IslandMin.Y || Home.Door.Y > Input.IslandMax.Y) return false;
        if (!Home.bExisting)
        {
            bool NearRoad = false;
            for (const FHearthTownRoadSegment& Road : Input.Roads)
                if (PointSegmentDistance(XY(Home.Door), XY(Road.A), XY(Road.B)) <= Road.Width * .5f + 100.f) { NearRoad = true; break; }
            if (!NearRoad) return false;
        }
        Empty.Add(Home);
    }
    for (const FHearthTownExpansion& Expansion : Plan.Expansions)
    {
        FHearthTownFootprint Probe; Probe.Center = Expansion.Center; Probe.Door = Expansion.Door; Probe.HalfExtent = Expansion.HalfExtent; Probe.Yaw = Expansion.Yaw;
        if (!ClearRect(Probe, CheckInput, Plan.Homes)) return false;
        if (Expansion.Door.X < Input.IslandMin.X || Expansion.Door.X > Input.IslandMax.X || Expansion.Door.Y < Input.IslandMin.Y || Expansion.Door.Y > Input.IslandMax.Y) return false;
    }
    return true;
}
