#include "HearthFreightNavigation.h"
#include "Algo/Reverse.h"

namespace HearthFreightNavigation
{
    namespace
    {
        constexpr float CellSize = 150.f;
        constexpr float PrimitiveLength = 250.f;
        constexpr float TurnRadius = 350.f;
        constexpr float SampleLength = 50.f;
        constexpr float GoalTolerance = 150.f;
        constexpr float YawBinDegrees = 22.5f;
        constexpr float TurnPenalty = 25.f;

        struct FNode
        {
            FPose Pose;
            float Cost = 0.f;
            float Heuristic = 0.f;
            int32 Parent = INDEX_NONE;
            int8 Steering = 0;
            bool bClosed = false;
        };

        float NormalizedYaw(float Yaw)
        {
            return FMath::UnwindDegrees(Yaw);
        }

        int32 HeadingBin(float Yaw)
        {
            constexpr int32 BinCount = 16;
            int32 Bin = FMath::RoundToInt(NormalizedYaw(Yaw) / YawBinDegrees) % BinCount;
            if (Bin < 0) Bin += BinCount;
            return Bin;
        }

        FIntVector StateKey(const FPose& Pose)
        {
            return FIntVector(
                FMath::RoundToInt(Pose.Position.X / CellSize),
                FMath::RoundToInt(Pose.Position.Y / CellSize),
                HeadingBin(Pose.Yaw));
        }

        /** Append samples after Start, including the primitive endpoint. */
        template<typename Allocator>
        void BuildPrimitive(const FPose& Start, int8 Steering, TArray<FPose, Allocator>& Samples)
        {
            Samples.Reset();
            const int32 Count = FMath::Max(1, FMath::CeilToInt(PrimitiveLength / SampleLength));
            const float StartYaw = NormalizedYaw(Start.Yaw);
            const float StartRadians = FMath::DegreesToRadians(StartYaw);
            const float Curvature = Steering == 0 ? 0.f : static_cast<float>(Steering) / TurnRadius;

            for (int32 I = 1; I <= Count; ++I)
            {
                const float Distance = PrimitiveLength * static_cast<float>(I) / static_cast<float>(Count);
                FVector Local;
                float Yaw = StartYaw;
                if (Steering == 0)
                {
                    Local = FVector(Distance, 0.f, 0.f);
                }
                else
                {
                    const float Angle = Curvature * Distance;
                    // Integrating a constant curvature in the vehicle's local
                    // frame keeps +X forward and makes +/- steering mirror
                    // exactly around the start heading.
                    Local = FVector(
                        FMath::Sin(Angle) / Curvature,
                        (1.f - FMath::Cos(Angle)) / Curvature,
                        0.f);
                    Yaw = StartYaw + FMath::RadiansToDegrees(Angle);
                }

                const float C = FMath::Cos(StartRadians), S = FMath::Sin(StartRadians);
                const FVector Offset(C * Local.X - S * Local.Y, S * Local.X + C * Local.Y, 0.f);
                FPose Sample;
                Sample.Position = Start.Position + Offset;
                // Planning is XY-only.  Ground-aware callers validate XY and
                // can reproject Z; preserving the start Z avoids inventing a
                // vertical segment for a curved primitive.
                Sample.Position.Z = Start.Position.Z;
                Sample.Yaw = NormalizedYaw(Yaw);
                Samples.Add(Sample);
            }
        }

        bool PrimitiveIsClear(const FPose& Start, int8 Steering,
            TFunctionRef<bool(const FPose&)> IsPoseClear, FPose& OutEnd)
        {
            TArray<FPose, TInlineAllocator<8>> Samples;
            BuildPrimitive(Start, Steering, Samples);
            for (const FPose& Sample : Samples)
            {
                if (!FMath::IsFinite(Sample.Position.X) || !FMath::IsFinite(Sample.Position.Y)
                    || !FMath::IsFinite(Sample.Position.Z) || !FMath::IsFinite(Sample.Yaw)
                    || !IsPoseClear(Sample)) return false;
            }
            if (Samples.IsEmpty()) return false;
            OutEnd = Samples.Last();
            return true;
        }

        bool Reconstruct(const TArray<FNode>& Nodes, int32 GoalIndex,
            TFunctionRef<bool(const FPose&)> IsPoseClear, TArray<FPose>& Out)
        {
            TArray<int32, TInlineAllocator<64>> Chain;
            for (int32 At = GoalIndex, Guard = 0; At != INDEX_NONE && Guard <= Nodes.Num(); At = Nodes[At].Parent, ++Guard)
                Chain.Add(At);
            if (Chain.IsEmpty() || Chain.Last() != 0) return false;
            Algo::Reverse(Chain);

            Out.Reset();
            Out.Add(Nodes[Chain[0]].Pose);
            if (!IsPoseClear(Out[0])) { Out.Reset(); return false; }
            for (int32 I = 1; I < Chain.Num(); ++I)
            {
                const FNode& Node = Nodes[Chain[I]];
                TArray<FPose, TInlineAllocator<8>> Samples;
                BuildPrimitive(Nodes[Node.Parent].Pose, Node.Steering, Samples);
                for (const FPose& Sample : Samples)
                {
                    if (!IsPoseClear(Sample)) { Out.Reset(); return false; }
                    Out.Add(Sample);
                }
            }
            return !Out.IsEmpty();
        }
    }

    bool Plan(FPose Start, FVector Goal,
        TFunctionRef<bool(const FPose&)> IsPoseClear, TArray<FPose>& Out, int32 MaxNodes)
    {
        Out.Reset();
        MaxNodes = FMath::Min(MaxNodes, 6000);
        if (MaxNodes <= 0 || Start.Position.ContainsNaN() || Goal.ContainsNaN()
            || !FMath::IsFinite(Start.Yaw) || !IsPoseClear(Start)) return false;
        if (FVector::Dist2D(Start.Position, Goal) <= GoalTolerance)
        {
            Out.Add(Start);
            return true;
        }

        TArray<FNode> Nodes;
        Nodes.Reserve(MaxNodes * 3);
        TArray<int32> Open;
        Open.Reserve(MaxNodes * 3);
        TMap<FIntVector, int32> BestByState;

        FNode First;
        First.Pose = Start;
        First.Cost = 0.f;
        First.Heuristic = FVector::Dist2D(Start.Position, Goal);
        const int32 FirstIndex = Nodes.Add(First);
        BestByState.Add(StateKey(Start), FirstIndex);
        Open.Add(FirstIndex);

        int32 Expansions = 0;
        while (!Open.IsEmpty() && Expansions < MaxNodes)
        {
            int32 BestOpen = INDEX_NONE;
            float BestScore = FLT_MAX;
            for (int32 I = 0; I < Open.Num(); ++I)
            {
                const int32 Index = Open[I];
                if (!Nodes.IsValidIndex(Index) || Nodes[Index].bClosed) continue;
                const float Score = Nodes[Index].Cost + Nodes[Index].Heuristic;
                if (Score < BestScore)
                {
                    BestScore = Score;
                    BestOpen = I;
                }
            }
            if (BestOpen == INDEX_NONE) break;

            const int32 CurrentIndex = Open[BestOpen];
            Open.RemoveAtSwap(BestOpen, 1, EAllowShrinking::No);
            Nodes[CurrentIndex].bClosed = true;
            // Nodes.Add below may reallocate the array; keep this expansion's
            // state by value so all three primitive candidates use the same
            // immutable parent pose and cost.
            const FNode Current = Nodes[CurrentIndex];
            ++Expansions;

            if (FVector::Dist2D(Current.Pose.Position, Goal) <= GoalTolerance)
                return Reconstruct(Nodes, CurrentIndex, IsPoseClear, Out);

            for (const int8 Steering : {int8(-1), int8(0), int8(1)})
            {
                FPose End;
                if (!PrimitiveIsClear(Current.Pose, Steering, IsPoseClear, End)) continue;
                const FIntVector Key = StateKey(End);
                const float NewCost = Current.Cost + PrimitiveLength + (Steering == 0 ? 0.f : TurnPenalty);
                int32* Existing = BestByState.Find(Key);
                if (Existing)
                {
                    const int32 ExistingIndex = *Existing;
                    const FNode& Candidate = Nodes[ExistingIndex];
                    if (Candidate.bClosed || NewCost >= Candidate.Cost) continue;
                    // Keep nodes immutable once descendants may refer to them;
                    // reopening in place would make an older child begin at a
                    // pose different from the endpoint it was generated from.
                    FNode Improved;
                    Improved.Pose = End;
                    Improved.Cost = NewCost;
                    Improved.Heuristic = FVector::Dist2D(End.Position, Goal);
                    Improved.Parent = CurrentIndex;
                    Improved.Steering = Steering;
                    const int32 ImprovedIndex = Nodes.Add(Improved);
                    BestByState.Add(Key, ImprovedIndex);
                    Open.Add(ImprovedIndex);
                    continue;
                }

                FNode Candidate;
                Candidate.Pose = End;
                Candidate.Cost = NewCost;
                Candidate.Heuristic = FVector::Dist2D(End.Position, Goal);
                Candidate.Parent = CurrentIndex;
                Candidate.Steering = Steering;
                const int32 NewIndex = Nodes.Add(Candidate);
                BestByState.Add(Key, NewIndex);
                Open.Add(NewIndex);
            }
        }
        return false;
    }
}
