#include "HearthSiteCandidates.h"

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    float DistanceToSegment2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        const FVector2D P(Point.X, Point.Y);
        const FVector2D Start(A.X, A.Y);
        const FVector2D Delta(B.X - A.X, B.Y - A.Y);
        const float LengthSquared = Delta.SizeSquared();
        const float Alpha = LengthSquared > KINDA_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(P - Start, Delta) / LengthSquared, 0.f, 1.f) : 0.f;
        return FVector2D::Distance(P, Start + Delta * Alpha);
    }

    bool IsOccupied(const FVector& Center, const FHearthReplacementCandidateInput& Input)
    {
        const int32 BlockerCount = FMath::Min(Input.Occupied.Num(), 256);
        for (int32 BlockerIndex = 0; BlockerIndex < BlockerCount; ++BlockerIndex)
        {
            const FHearthSiteCandidateBlocker& Blocker = Input.Occupied[BlockerIndex];
            if (!IsFiniteVector(Blocker.Center) || !FMath::IsFinite(Blocker.Radius) || Blocker.Radius < 0.f) continue;
            if (FMath::Abs(Center.X - Blocker.Center.X) < Input.PlotRadius + Blocker.Radius + Input.RoadClearance
                && FMath::Abs(Center.Y - Blocker.Center.Y) < Input.PlotRadius + Blocker.Radius + Input.RoadClearance)
            {
                return true;
            }
        }
        return false;
    }

    bool IsRoadBlocked(const FVector& Center, const FHearthReplacementCandidateInput& Input)
    {
        for (int32 RoadIndex = 0; RoadIndex < Input.Roads.Num() && RoadIndex < 32; ++RoadIndex)
        {
            const FHearthTownRoadSegment& Road = Input.Roads[RoadIndex];
            if (!IsFiniteVector(Road.A) || !IsFiniteVector(Road.B) || !FMath::IsFinite(Road.Width) || Road.Width <= 0.f) continue;
            const float Required = Input.PlotRadius + Road.Width * .5f + Input.RoadClearance;
            if (DistanceToSegment2D(Center, Road.A, Road.B) < Required) return true;
        }
        return false;
    }
}

TArray<FHearthReplacementCandidate> HearthSiteCandidates::Generate(const FHearthReplacementCandidateInput& Input)
{
    TArray<FHearthReplacementCandidate> Result;
    const int32 Limit = FMath::Clamp(Input.MaxCandidates, 0, 64);
    const int32 Samples = FMath::Clamp(Input.SamplesPerRoad, 1, 8);
    if (Limit == 0 || Input.Roads.IsEmpty() || Input.Roads.Num() > 32 || Input.Occupied.Num() > 256
        || !IsFiniteVector(Input.ResidentAnchor) || !IsFiniteVector(Input.PublicSite)
        || !FMath::IsFinite(Input.PlotRadius) || !FMath::IsFinite(Input.RoadClearance)
        || Input.PlotRadius <= 0.f || Input.RoadClearance < 0.f) return Result;

    for (const FHearthTownRoadSegment& Road : Input.Roads)
        if (!IsFiniteVector(Road.A) || !IsFiniteVector(Road.B) || !FMath::IsFinite(Road.Width) || Road.Width <= 0.f || Road.Width > 4000.f)
            return Result;
    for (const FHearthSiteCandidateBlocker& Blocker : Input.Occupied)
        if (!IsFiniteVector(Blocker.Center) || !FMath::IsFinite(Blocker.Radius) || Blocker.Radius < 0.f)
            return Result;

    const auto Quantize = [](float Value)->uint32
    {
        const float Bounded = FMath::Clamp(Value * .01f, -1000000.f, 1000000.f);
        return static_cast<uint32>(FMath::RoundToInt(Bounded));
    };
    const auto Mix = [](uint32 Value)->uint32
    {
        Value ^= Value >> 16; Value *= 0x7feb352dU;
        Value ^= Value >> 15; Value *= 0x846ca68bU;
        return Value ^ (Value >> 16);
    };
    uint32 BaseSeed = Mix(static_cast<uint32>(Input.Seed));
    BaseSeed = Mix(BaseSeed ^ Quantize(Input.ResidentAnchor.X));
    BaseSeed = Mix(BaseSeed ^ (Quantize(Input.ResidentAnchor.Y) * 0x9e3779b9U));
    BaseSeed = Mix(BaseSeed ^ (Quantize(Input.PublicSite.X) * 0x85ebca6bU));
    BaseSeed = Mix(BaseSeed ^ (Quantize(Input.PublicSite.Y) * 0xc2b2ae35U));
    const int32 RoadCount = Input.Roads.Num();
    for (int32 RoadIndex = 0; RoadIndex < RoadCount && Result.Num() < Limit; ++RoadIndex)
    {
        const FHearthTownRoadSegment& Road = Input.Roads[RoadIndex];
        const FVector2D Delta(Road.B.X - Road.A.X, Road.B.Y - Road.A.Y);
        const float Length = Delta.Size();
        if (Length < 2.f) continue;
        const FVector2D Tangent = Delta / Length;
        const FVector2D Normal(-Tangent.Y, Tangent.X);
        const float RoadYaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));

        for (int32 Sample = 0; Sample < Samples && Result.Num() < Limit; ++Sample)
        {
            const float Alpha = (Sample + 1.f) / (Samples + 1.f);
            const FVector2D Street = FVector2D(Road.A.X, Road.A.Y) + Delta * Alpha;
            for (int32 Side = 0; Side < 2 && Result.Num() < Limit; ++Side)
            {
                const uint32 VariationHash = Mix(BaseSeed ^ static_cast<uint32>(RoadIndex * 977 + Sample * 37 + Side * 13));
                FRandomStream Variation(static_cast<int32>(VariationHash));
                const float Along = Variation.FRandRange(-85.f, 85.f);
                const float Setback = Variation.FRandRange(Input.RoadClearance, Input.RoadClearance + 110.f);
                const FVector2D Outward = Side == 0 ? Normal : -Normal;
                const FVector2D Center2D = Street + Tangent * Along
                    + Outward * (Road.Width * .5f + Input.PlotRadius + Setback);
                const FVector Center(Center2D.X, Center2D.Y, Road.A.Z);
                if (IsOccupied(Center, Input) || IsRoadBlocked(Center, Input)) continue;
                if (Result.ContainsByPredicate([&](const FHearthReplacementCandidate& Existing)
                    { return FVector::DistSquared2D(Existing.Position, Center) < FMath::Square(Input.PlotRadius * 1.8f); })) continue;

                FHearthReplacementCandidate Candidate;
                Candidate.Position = Center;
                const FVector2D Approach2D = Street + Tangent * Along + Outward * (Road.Width * .5f + 35.f);
                Candidate.Approach = FVector(Approach2D.X, Approach2D.Y, Road.A.Z);
                Candidate.Yaw = RoadYaw + (Side == 0 ? 0.f : 180.f) + Variation.FRandRange(-8.f, 8.f);
                Candidate.RoadIndex = RoadIndex;
                Candidate.SampleIndex = Sample;
                Candidate.Side = Side;
                Candidate.StableKey = FString::Printf(TEXT("road_%d_sample_%d_side_%d"), RoadIndex, Sample, Side);
                Result.Add(MoveTemp(Candidate));
            }
        }
    }
    return Result;
}
