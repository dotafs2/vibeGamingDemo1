#include "HearthOrganicTerrain.h"

namespace HearthOrganicTerrain
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float MinElevation = 0.f;
        constexpr float MaxElevation = 600.f;

        FBounds EffectiveBounds(const FSettings& Settings)
        {
            return Settings.Bounds.IsValid() ? Settings.Bounds : DefaultBounds();
        }

        FVector2D ClampToBounds(const FVector2D& P, const FBounds& Bounds)
        {
            return FVector2D(FMath::Clamp(P.X, Bounds.Min.X, Bounds.Max.X),
                FMath::Clamp(P.Y, Bounds.Min.Y, Bounds.Max.Y));
        }

        float SeedPhase(int32 Seed, int32 Salt)
        {
            // A bounded trigonometric hash is stable across repeated world loads
            // and does not consume a mutable random stream.
            const float Input = static_cast<float>(Seed) * (0.00137f + Salt * 0.000071f) + Salt * 1.61803f;
            return FMath::Frac(FMath::Abs(FMath::Sin(Input) * 43758.5453f));
        }

        float SmoothStep(float Edge0, float Edge1, float X)
        {
            if (Edge1 <= Edge0) return X >= Edge1 ? 1.f : 0.f;
            const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
            return T * T * (3.f - 2.f * T);
        }

        float RawHeight(const FVector2D& P, int32 Seed)
        {
            const float PhaseA = SeedPhase(Seed, 3) * 2.f * Pi;
            const float PhaseB = SeedPhase(Seed, 11) * 2.f * Pi;
            const FVector2D Royal(6500.f, 6500.f);
            const FVector2D Delta = P - Royal;
            const float RoyalDistanceSquared = Delta.SizeSquared();

            // Broad, low-frequency terms keep the 300 m map walkable while the
            // concentric term gives the elevated royal district visible terraces.
            const float Broad = 42.f * FMath::Sin(P.X / 2300.f + PhaseA)
                + 31.f * FMath::Cos(P.Y / 1900.f + PhaseB)
                + 18.f * FMath::Sin((P.X + P.Y) / 1350.f + PhaseA * .37f);
            const float RoyalRise = 155.f * FMath::Exp(-RoyalDistanceSquared / (2.f * 3000.f * 3000.f));
            const float Terrace = 9.f * FMath::Sin(FMath::Sqrt(RoyalDistanceSquared) / 520.f + PhaseB);
            const float ApproachRamp = 64.f * SmoothStep(1000.f, 7600.f, P.Y)
                * FMath::Exp(-FMath::Square((P.X - 6500.f) / 9000.f));
            return FMath::Clamp(145.f + Broad + RoyalRise + Terrace + ApproachRamp, MinElevation, MaxElevation);
        }

        FVector2D ToLocal(const FVector2D& P, const FVector2D& Center, float YawDegrees)
        {
            const float Radians = FMath::DegreesToRadians(-YawDegrees);
            const float C = FMath::Cos(Radians), S = FMath::Sin(Radians);
            const FVector2D D = P - Center;
            return FVector2D(D.X * C - D.Y * S, D.X * S + D.Y * C);
        }

        float FlattenBlend(const FVector2D& P, const FFlattenZone& Zone)
        {
            const FVector2D Local = ToLocal(P, Zone.Center, Zone.YawDegrees);
            const FVector2D Half(FMath::Abs(Zone.HalfSize.X), FMath::Abs(Zone.HalfSize.Y));
            if (Half.X <= 0.f || Half.Y <= 0.f) return 0.f;
            const float OutsideX = FMath::Max(FMath::Abs(Local.X) - Half.X, 0.f);
            const float OutsideY = FMath::Max(FMath::Abs(Local.Y) - Half.Y, 0.f);
            const float OutsideDistance = FMath::Sqrt(OutsideX * OutsideX + OutsideY * OutsideY);
            const float InsideMargin = FMath::Min(Half.X - FMath::Abs(Local.X), Half.Y - FMath::Abs(Local.Y));
            const float SignedDistance = OutsideDistance > 0.f ? OutsideDistance : -InsideMargin;
            const float Transition = FMath::Max(1.f, Zone.Transition);
            return 1.f - SmoothStep(0.f, Transition, SignedDistance);
        }

        bool ClosestRoadPoint(const FVector2D& P, const FRoadCenterline& Road, FVector2D& OutPoint, float& OutElevation, float& OutDistance)
        {
            if (Road.Nodes.Num() < 2) return false;
            bool bFound = false;
            OutDistance = FLT_MAX;
            for (int32 I = 1; I < Road.Nodes.Num(); ++I)
            {
                const FRoadNode& A = Road.Nodes[I - 1];
                const FRoadNode& B = Road.Nodes[I];
                const FVector2D D = B.Position - A.Position;
                const float LengthSquared = D.SizeSquared();
                if (LengthSquared <= KINDA_SMALL_NUMBER) continue;
                const float Alpha = FMath::Clamp(FVector2D::DotProduct(P - A.Position, D) / LengthSquared, 0.f, 1.f);
                const FVector2D Candidate = A.Position + D * Alpha;
                const float Distance = FVector2D::Distance(P, Candidate);
                if (Distance < OutDistance)
                {
                    bFound = true; OutDistance = Distance; OutPoint = Candidate;
                    OutElevation = FMath::Lerp(A.Elevation, B.Elevation, Alpha);
                }
            }
            return bFound;
        }

        float NaturalAndFeatures(const FVector2D& P, const FSettings& Settings)
        {
            float Height = RawHeight(P, Settings.Seed);
            // Roads are graded first. Pads are the final authority so a civic
            // road crossing a foundation cannot reintroduce a slope.
            for (const FRoadCenterline& Road : Settings.Roads)
            {
                FVector2D Closest; float RoadHeight = 0.f, Distance = 0.f;
                if (!ClosestRoadPoint(P, Road, Closest, RoadHeight, Distance)) continue;
                const float Width = FMath::Max(0.f, Road.Width);
                const float Transition = FMath::Max(1.f, Road.Transition);
                const float Blend = 1.f - SmoothStep(Width, Width + Transition, Distance);
                Height = FMath::Lerp(Height, FMath::Clamp(RoadHeight, MinElevation, MaxElevation), Blend);
            }
            for (const FFlattenZone& Zone : Settings.FlattenZones)
            {
                const float Blend = FlattenBlend(P, Zone);
                Height = FMath::Lerp(Height, FMath::Clamp(Zone.Elevation, MinElevation, MaxElevation), Blend);
            }
            return FMath::Clamp(Height, MinElevation, MaxElevation);
        }
    }

    FBounds DefaultBounds()
    {
        return FBounds();
    }

    bool IsWithinBounds(const FVector2D& XY, const FSettings& Settings)
    {
        return EffectiveBounds(Settings).Contains(XY);
    }

    float HeightAt(const FVector2D& XY, const FSettings& Settings)
    {
        const FBounds Bounds = EffectiveBounds(Settings);
        return NaturalAndFeatures(ClampToBounds(XY, Bounds), Settings);
    }

    FHeightSample NormalOrSlopeAt(const FVector2D& XY, const FSettings& Settings)
    {
        const FBounds Bounds = EffectiveBounds(Settings);
        const FVector2D P = ClampToBounds(XY, Bounds);
        const float Span = FMath::Min(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y);
        const float Epsilon = FMath::Clamp(25.f, 1.f, Span / 100.f);
        const FVector2D X0(FMath::Max(Bounds.Min.X, P.X - Epsilon), P.Y);
        const FVector2D X1(FMath::Min(Bounds.Max.X, P.X + Epsilon), P.Y);
        const FVector2D Y0(P.X, FMath::Max(Bounds.Min.Y, P.Y - Epsilon));
        const FVector2D Y1(P.X, FMath::Min(Bounds.Max.Y, P.Y + Epsilon));
        const float DX = (HeightAt(X1, Settings) - HeightAt(X0, Settings)) / FMath::Max(1.f, X1.X - X0.X);
        const float DY = (HeightAt(Y1, Settings) - HeightAt(Y0, Settings)) / FMath::Max(1.f, Y1.Y - Y0.Y);
        FHeightSample Result;
        Result.Height = HeightAt(P, Settings);
        Result.Normal = FVector(-DX, -DY, 1.f).GetSafeNormal();
        Result.SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY)));
        Result.bInsideBounds = Bounds.Contains(XY);
        return Result;
    }

    bool GenerateGrid(const FSettings& Settings, FGrid& OutGrid)
    {
        OutGrid.Reset();
        // Height queries can safely use the documented default bounds, but a
        // mesh request must reject an invalid finite domain rather than silently
        // producing geometry in a different world region.
        const FBounds Bounds = Settings.Bounds;
        if (!Bounds.IsValid() || Settings.GridQuadsX < 1 || Settings.GridQuadsY < 1
            || Settings.GridQuadsX > 512 || Settings.GridQuadsY > 512) return false;
        OutGrid.VertexColumns = Settings.GridQuadsX + 1;
        OutGrid.VertexRows = Settings.GridQuadsY + 1;
        const int32 VertexCount = OutGrid.VertexColumns * OutGrid.VertexRows;
        OutGrid.Vertices.Reserve(VertexCount); OutGrid.Normals.Reserve(VertexCount);
        OutGrid.UV0.Reserve(VertexCount); OutGrid.VertexColors.Reserve(VertexCount);
        for (int32 Y = 0; Y <= Settings.GridQuadsY; ++Y)
        {
            const float V = static_cast<float>(Y) / Settings.GridQuadsY;
            for (int32 X = 0; X <= Settings.GridQuadsX; ++X)
            {
                const float U = static_cast<float>(X) / Settings.GridQuadsX;
                const FVector2D XY(FMath::Lerp(Bounds.Min.X, Bounds.Max.X, U), FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, V));
                const FHeightSample Sample = NormalOrSlopeAt(XY, Settings);
                OutGrid.Vertices.Add(FVector(XY.X, XY.Y, Sample.Height));
                OutGrid.Normals.Add(Sample.Normal);
                OutGrid.UV0.Add(FVector2D(U, V));
                const float HeightT = FMath::Clamp(Sample.Height / MaxElevation, 0.f, 1.f);
                const float SlopeT = FMath::Clamp(Sample.SlopeDegrees / 20.f, 0.f, 1.f);
                OutGrid.VertexColors.Add(FLinearColor(FMath::Lerp(.18f, .42f, HeightT), FMath::Lerp(.31f, .48f, HeightT), FMath::Lerp(.12f, .18f, SlopeT), 1.f));
            }
        }
        OutGrid.Indices.Reserve(Settings.GridQuadsX * Settings.GridQuadsY * 6);
        for (int32 Y = 0; Y < Settings.GridQuadsY; ++Y) for (int32 X = 0; X < Settings.GridQuadsX; ++X)
        {
            const int32 A = Y * OutGrid.VertexColumns + X, B = A + 1, C = A + OutGrid.VertexColumns, D = C + 1;
            OutGrid.Indices.Append({A, C, B, B, C, D});
        }
        return true;
    }
}
