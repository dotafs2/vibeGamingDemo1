#include "HearthFreightNavigation.h"
#include "Algo/Reverse.h"

namespace HearthFreightNavigation
{
    TConstArrayView<FFootprintBox> Footprint()
    {
        static const FFootprintBox Boxes[]={{5,0,125,115},{305,0,175,50},{330,115,50,40}};
        return MakeArrayView(Boxes);
    }

    namespace
    {
        constexpr float CellSize = 150.f;
        constexpr float PrimitiveLength = 250.f;
        constexpr float TurnRadius = 350.f;
        constexpr float SampleLength = 50.f;
        constexpr float GoalTolerance = 150.f;
        constexpr float YawBinDegrees = 22.5f;
        constexpr float TurnPenalty = 25.f;

        // Clip a vehicle rectangle against an obstacle in the obstacle's
        // local frame. Area, unlike a boolean hit or minimum SAT depth,
        // measures progress when backing out of a shallow corner overlap.
        double OverlapArea(const FVector& Center, float Yaw, double HX, double HY,
            const FRecoveryObstacle& Obstacle)
        {
            const FRotator Frame(0,Obstacle.Yaw,0), Vehicle(0,Yaw,0);
            const FVector D=Frame.UnrotateVector(Center-Obstacle.Center);
            const FVector F=Frame.UnrotateVector(Vehicle.Vector());
            const FVector S(-F.Y,F.X,0);
            if(FMath::Abs(D.X)>HX*FMath::Abs(F.X)+HY*FMath::Abs(S.X)+Obstacle.HalfSize.X
                || FMath::Abs(D.Y)>HX*FMath::Abs(F.Y)+HY*FMath::Abs(S.Y)+Obstacle.HalfSize.Y) return 0;
            TArray<FVector2D,TInlineAllocator<12>> Polygon;
            for(const FVector2D Corner:{FVector2D(-HX,-HY),FVector2D(HX,-HY),FVector2D(HX,HY),FVector2D(-HX,HY)})
            {
                const FVector P=D+F*Corner.X+S*Corner.Y;
                Polygon.Add(FVector2D(P.X,P.Y));
            }
            for(int32 Axis=0;Axis<2;++Axis) for(double Sign:{-1.0,1.0})
            {
                if(Polygon.IsEmpty()) return 0;
                TArray<FVector2D,TInlineAllocator<12>> Clipped;
                FVector2D A=Polygon.Last();
                double DA=Sign*A[Axis]-Obstacle.HalfSize[Axis];
                for(const FVector2D& B:Polygon)
                {
                    const double DB=Sign*B[Axis]-Obstacle.HalfSize[Axis];
                    if((DA<=0)!=(DB<=0)) Clipped.Add(FMath::Lerp(A,B,DA/(DA-DB)));
                    if(DB<=0) Clipped.Add(B);
                    A=B;DA=DB;
                }
                Polygon=MoveTemp(Clipped);
            }
            double Area=0;
            for(int32 I=0;I<Polygon.Num();++I)
            {
                const FVector2D& A=Polygon[I];const FVector2D& B=Polygon[(I+1)%Polygon.Num()];
                Area+=A.X*B.Y-A.Y*B.X;
            }
            return FMath::Abs(Area)*.5;
        }

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

    bool CanReverseStep(const FPose& From, const FPose& To, TConstArrayView<FRecoveryObstacle> Obstacles)
    {
        if(From.Position.ContainsNaN() || To.Position.ContainsNaN() || !FMath::IsFinite(From.Yaw)
            || !FMath::IsFinite(To.Yaw) || FMath::Abs(FMath::FindDeltaAngleDegrees(From.Yaw,To.Yaw))>.001f) return false;
        const FVector Delta=To.Position-From.Position, F=FRotator(0,From.Yaw,0).Vector(), S(-F.Y,F.X,0);
        const double Distance=-FVector::DotProduct(Delta,F);
        if(Distance<=.00001 || Distance>10.001 || FMath::Abs(FVector::DotProduct(Delta,S))>.001) return false;
        double BeforeTotal=0,AfterTotal=0;
        for(const FRecoveryObstacle& Obstacle:Obstacles)
        {
            if(Obstacle.Center.ContainsNaN() || Obstacle.HalfSize.ContainsNaN() || !FMath::IsFinite(Obstacle.Yaw)
                || !FMath::IsFinite(Obstacle.ExtraMargin) || Obstacle.HalfSize.X<=0 || Obstacle.HalfSize.Y<=0 || Obstacle.ExtraMargin<0) return false;
            for(const FFootprintBox& Box:Footprint())
            {
                const FVector Center=From.Position+F*Box.X+S*Box.Y;
                const double HX=Box.HalfX+40.0+Obstacle.ExtraMargin,HY=Box.HalfY+40.0+Obstacle.ExtraMargin;
                const double Before=OverlapArea(Center,From.Yaw,HX,HY,Obstacle);
                const double After=OverlapArea(Center+Delta,From.Yaw,HX,HY,Obstacle);
                // For fixed-heading translation along -X this enlarged box
                // is the EXACT union over the whole segment, not endpoint
                // sampling. Its intersection must be contained in the old
                // intersection. This forbids crossing even a thin rear wall
                // and forbids pushing deeper through an initial wall.
                const double Swept=OverlapArea(Center+Delta*.5,From.Yaw,HX+Distance*.5,HY,Obstacle);
                constexpr double AreaRoundoff=1.e-6;
                if(Swept>Before+AreaRoundoff || After>Before+AreaRoundoff) return false;
                BeforeTotal+=Before;AfterTotal+=After;
            }
        }
        return BeforeTotal<=1.e-6 || AfterTotal<BeforeTotal-1.e-6;
    }

    bool PlanRecovery(FPose Start,
        TFunctionRef<bool(const FPose&,const FPose&)> CanReverse,
        TFunctionRef<bool(const FPose&,TArray<FPose>&)> ConnectForward,
        TArray<FPose>& Out,float MaxDistance)
    {
        Out.Reset();
        if(Start.Position.ContainsNaN() || !FMath::IsFinite(Start.Yaw) || !FMath::IsFinite(MaxDistance)) return false;
        const int32 Steps=FMath::FloorToInt(FMath::Clamp(MaxDistance,0.f,900.f)/10.f);
        const FVector Back=-FRotator(0,Start.Yaw,0).Vector();
        TArray<FPose> Retreat;Retreat.Reserve(Steps+1);Retreat.Add(Start);
        for(int32 Step=1;Step<=Steps;++Step)
        {
            const FPose Next{Start.Position+Back*(Step*10.f),Start.Yaw};
            if(!CanReverse(Retreat.Last(),Next)) return false;
            Retreat.Add(Next);
            if(Step%15!=0) continue;
            TArray<FPose> Forward;
            if(!ConnectForward(Next,Forward) || Forward.IsEmpty()
                || !Forward[0].Position.Equals(Next.Position,.01f)
                || FMath::Abs(FMath::FindDeltaAngleDegrees(Next.Yaw,Forward[0].Yaw))>.001f) continue;
            Out=MoveTemp(Retreat);
            for(int32 I=1;I<Forward.Num();++I) Out.Add(Forward[I]);
            return true;
        }
        return false;
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
