#include "HearthHillNavigation.h"
#include "HearthRoyalHill.h"
#include "HearthTownLayout.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HearthHillNavigation
{
    namespace
    {
        const TArray<FVector>& Road()
        {
            static const TArray<FVector> Points=[]
            {
                auto Result=HearthRoyalHill::AscentRoute();
                const int32 End=Result.IndexOfByPredicate([](const FVector& P){return P.Equals(HearthRoyalHill::DeliveryApproach(),.01);});
                if(End!=INDEX_NONE) Result.SetNum(End+1);
                return Result;
            }();
            return Points;
        }
        double Radius(const FVector& P)
        { return FVector2D::Distance(FVector2D(P.X,P.Y),HearthRoyalHill::Center()); }
        FVector Project(const FVector& P,const FVector& A,const FVector& B,double& T)
        {
            const FVector D(B.X-A.X,B.Y-A.Y,0),V(P.X-A.X,P.Y-A.Y,0);
            T=D.SizeSquared()>0?FMath::Clamp(FVector::DotProduct(V,D)/D.SizeSquared(),0.0,1.0):0;
            return FMath::Lerp(A,B,T);
        }
        double Nearest(const FVector& P,int32& Segment,double& Fraction)
        {
            double Best=DBL_MAX; const auto& Points=Road();
            for(int32 I=1;I<Points.Num();++I)
            {
                double T; const FVector Q=Project(P,Points[I-1],Points[I],T);
                const double D=FVector::DistSquared2D(P,Q);
                if(D<Best) { Best=D;Segment=I-1;Fraction=T; }
            }
            return Best;
        }
        int32 Region(const FVector& P)
        {
            if(Radius(P)>=HearthRoyalHill::FootRadius || (!OnAscent(P) && P.Z<HearthRoyalHill::PlateauElevation*.5)) return 0;
            return Radius(P)<=HearthRoyalHill::PlateauRadius?2:1;
        }
    }

    bool InHill(const FVector& P,float Margin)
    { return Radius(P)<=HearthRoyalHill::FootRadius+Margin; }

    bool TouchesHill(const FVector& A,const FVector& B)
    {
        const auto C=HearthRoyalHill::Center();double T;
        return InHill(Project(FVector(C.X,C.Y,0),A,B,T),220.f);
    }

    bool OnAscent(const FVector& P)
    {
        if(!InHill(P)) return false;
        const auto& Points=Road();
        // Vehicles occupy the level graded shoulders too, especially their
        // horse/driver at the unloading bend. Actual slope checks remain
        // mandatory; the later preview road inside the site is not admitted.
        const double Half=HearthRoyalHill::RoadWidth*.5+HearthRoyalHill::RoadShoulder;
        for(int32 I=1;I<Points.Num();++I)
        {
            const auto& A=Points[I-1];const auto& B=Points[I];
            if(P.X<FMath::Min(A.X,B.X)-Half || P.X>FMath::Max(A.X,B.X)+Half
                || P.Y<FMath::Min(A.Y,B.Y)-Half || P.Y>FMath::Max(A.Y,B.Y)+Half) continue;
            double T;
            if(FVector::DistSquared2D(P,Project(P,A,B,T))<=Half*Half) return true;
        }
        return false;
    }

    bool AscentSegment(const FVector& A,const FVector& B)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/40.0));
        if(Steps>2048) return false;
        for(int32 I=0;I<=Steps;++I) if(!OnAscent(FMath::Lerp(A,B,double(I)/Steps))) return false;
        return true;
    }

    bool Accessible(const FVector& P)
    {
        if(!InHill(P) || Radius(P)<=HearthRoyalHill::PlateauRadius || OnAscent(P)) return true;
        // Ground-projected legacy roads and parking pads remain real lowland.
        // Callers must pass actual ground Z, never a planner's inherited Z.
        if(!Road().IsEmpty() && P.Z<=Road()[0].Z+150) return true;
        // Keep the authored southwest connection to the toe. This only admits
        // its XY corridor; callers still certify the real ground and slope.
        static const TArray<FHearthTownRoadSegment> Streets=HearthTownLayout::VillageRoads(true,4);
        if(Road().IsEmpty()) return false;
        for(const auto& Street:Streets)
        {
            if(FVector::Dist2D(Street.A,Road()[0])>1 && FVector::Dist2D(Street.B,Road()[0])>1) continue;
            double T;const FVector Q=Project(P,Street.A,Street.B,T);
            if(FVector::Dist2D(P,Q)<=Street.Width*.5) return true;
        }
        return false;
    }

    bool MakeGuide(const FVector& Start,const FVector& Goal,TArray<FVector>& Guide)
    {
        Guide.Reset(); const auto& Points=Road();
        if(Points.Num()<2 || Points.Num()>512) return false;
        const int32 ARegion=Region(Start),BRegion=Region(Goal);
        if(ARegion==BRegion && ARegion!=1) return false;
        int32 A=0,B=0;double TA=0,TB=0;
        Nearest(Start,A,TA);Nearest(Goal,B,TB);
        if(ARegion==0) { A=0;TA=0; } else if(ARegion==2) { A=Points.Num()-2;TA=1; }
        if(BRegion==0) { B=0;TB=0; } else if(BRegion==2) { B=Points.Num()-2;TB=1; }
        const double From=A+TA,To=B+TB;
        Guide.Add(FMath::Lerp(Points[A],Points[A+1],TA));
        if(To>=From)
        {
            for(int32 I=FMath::FloorToInt(From)+1;I<To;++I) Guide.Add(Points[I]);
        }
        else for(int32 I=FMath::CeilToInt(From)-1;I>To;--I) Guide.Add(Points[I]);
        const FVector Last=FMath::Lerp(Points[B],Points[B+1],TB);
        if(!Guide.Last().Equals(Last,.01)) Guide.Add(Last);
        return true;
    }

    bool PlanFreight(HearthFreightNavigation::FPose Start,const FVector& Goal,
        TFunctionRef<bool(const HearthFreightNavigation::FPose&)> Clear,
        TArray<HearthFreightNavigation::FPose>& Out,int32 MaxNodes)
    {
        using namespace HearthFreightNavigation;
        Out.Reset(); TArray<FVector> Guide;
        if(!MakeGuide(Start.Position,Goal,Guide)) return Plan(Start,Goal,Clear,Out,MaxNodes);
        if(Guide.IsEmpty() || MaxNodes<=0 || !Clear(Start)) return false;
        MaxNodes=FMath::Min(MaxNodes,6000);
        TArray<FPose> Route;
        if(FVector::Dist2D(Start.Position,Guide[0])>150)
        {
            if(!Plan(Start,Guide[0],Clear,Route,MaxNodes*2/3))
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("HearthFreightTrace")))
                    UE_LOG(LogTemp,Display,TEXT("HILL_FREIGHT_FAILED attach from=%s yaw=%.2f toe=%s nodes=%d"),*Start.Position.ToString(),Start.Yaw,*Guide[0].ToString(),MaxNodes*2/3);
                return false;
            }
        }
        else Route.Add(Start);
        FPose Current=Route.Last();int32 Segment=0;
        for(int32 Step=0;Step<2048 && Guide.Num()>1;++Step)
        {
            double Best=DBL_MAX;
            FVector Closest=Guide[Segment];
            const int32 LastSearch=FMath::Min(Segment+8,Guide.Num()-1);
            for(int32 I=Segment;I<LastSearch;++I)
            {
                double T; const FVector Q=Project(Current.Position,Guide[I],Guide[I+1],T);
                const double D=FVector::DistSquared2D(Current.Position,Q);
                if(D<Best) { Best=D;Closest=Q;Segment=I; }
            }
            if(Segment>=Guide.Num()-2 && FVector::Dist2D(Current.Position,Guide.Last())<=100) break;
            // Pure pursuit integrates an actual forward arc. Small steering on
            // the broad spiral avoids the normal planner's 350cm-radius zigzag.
            FVector Aim=Closest;double LookAhead=400;
            for(int32 I=Segment;I<Guide.Num()-1 && LookAhead>0;++I)
            {
                const FVector A=I==Segment?Closest:Guide[I],B=Guide[I+1];
                const double Length=FVector::Dist2D(A,B);
                if(Length>=LookAhead) { Aim=FMath::Lerp(A,B,LookAhead/Length);break; }
                LookAhead-=Length;Aim=B;
            }
            const FVector Local=FRotator(0,Current.Yaw,0).UnrotateVector(Aim-Current.Position);
            if(Local.SizeSquared2D()<.01) { Out.Reset();return false; }
            const double K=FMath::Clamp(2*Local.Y/Local.SizeSquared2D(),-1.0/350,1.0/350);
            constexpr double Travel=50;
            const double Angle=K*Travel;
            const FVector Delta=FMath::Abs(K)<1.e-8?FVector(Travel,0,0):FVector(FMath::Sin(Angle)/K,(1-FMath::Cos(Angle))/K,0);
            const FPose Next{Current.Position+FRotator(0,Current.Yaw,0).RotateVector(Delta),
                float(FMath::UnwindDegrees(Current.Yaw+FMath::RadiansToDegrees(Angle)))};
            if(!Clear(Next) || Route.Num()>=2049)
            {
                if(FParse::Param(FCommandLine::Get(),TEXT("HearthFreightTrace")))
                    UE_LOG(LogTemp,Display,TEXT("HILL_FREIGHT_FAILED ascent point=%s yaw=%.2f segment=%d"),*Next.Position.ToString(),Next.Yaw,Segment);
                return false;
            }
            Route.Add(Next);Current=Next;
        }
        if(Guide.Num()>1 && (Segment<Guide.Num()-2 || FVector::Dist2D(Current.Position,Guide.Last())>150)) return false;
        if(FVector::Dist2D(Current.Position,Goal)>150)
        {
            TArray<FPose> Tail;
            if(!Plan(Current,Goal,Clear,Tail,MaxNodes-MaxNodes*2/3) || Route.Num()+Tail.Num()-1>2049) return false;
            for(int32 I=1;I<Tail.Num();++I) Route.Add(Tail[I]);
        }
        Out=MoveTemp(Route);return true;
    }
}
