#include "HearthRoyalHill.h"
#include "HearthOrganicTerrain.h"

namespace HearthRoyalHillDetail
{
    constexpr int32 TurnSteps = 256;
    constexpr double StartRadius = 8400.0;
    // Stop the navigable turn 100cm outside the site AABB; the level preview
    // subsequently passes the original 5000cm southern gate datum.
    constexpr double EndRadius = 5100.0;
    // Reach the top before any road/shoulder blend can touch the 48m plateau.
    // A final level portion also makes the turn into the south gate harmless.
    constexpr double ClimbEndRadius = 6200.0;
    constexpr double GradeEaseLength = 1800.0;

    double RiseFraction(double Distance, double ClimbLength)
    {
        if (Distance >= ClimbLength) return 1.0;
        const double Ease = FMath::Min(GradeEaseLength,ClimbLength*.2);
        double Area;
        if (Distance < Ease) Area=Distance*Distance/(2*Ease);
        else if (Distance <= ClimbLength-Ease) Area=Distance-Ease*.5;
        else Area=ClimbLength-Ease-FMath::Square(ClimbLength-Distance)/(2*Ease);
        return FMath::Clamp(Area/(ClimbLength-Ease),0.0,1.0);
    }
}

FVector2D HearthRoyalHill::Center()
{
    return FVector2D(6500,6500);
}

FVector HearthRoyalHill::DeliveryApproach()
{
    return FVector(6500,1400,PlateauElevation);
}

TArray<FVector> HearthRoyalHill::AscentRoute()
{
    return AscentRoute(HearthOrganicTerrain::FSettings());
}

TArray<FVector> HearthRoyalHill::AscentRoute(const HearthOrganicTerrain::FSettings& Settings)
{
    using namespace HearthRoyalHillDetail;
    // Do not sample the new hill or inherited pads/roads for its toe. That would
    // introduce feedback or a discontinuous start on a rebuilt heightfield.
    HearthOrganicTerrain::FSettings Legacy;
    Legacy.Seed=Settings.Seed; Legacy.Bounds=Settings.Bounds; Legacy.bRoyalHill=false;
    const FVector2D C=Center();
    const FVector2D Toe(C.X,C.Y-StartRadius);
    const double StartZ=HearthOrganicTerrain::HeightAt(Toe,Legacy);
    TArray<FVector> Route;
    TArray<double> Distances;
    Route.Reserve(TurnSteps+9); Distances.Reserve(TurnSteps+1);
    double Length=0;
    for (int32 I=0; I<=TurnSteps; ++I)
    {
        const double T=double(I)/TurnSteps;
        const double Angle=2.0*UE_PI*T-UE_PI*.5;
        const double Radius=FMath::Lerp(StartRadius,EndRadius,T);
        FVector P(C.X+Radius*FMath::Cos(Angle),C.Y+Radius*FMath::Sin(Angle),0);
        if (I==0) P=FVector(Toe.X,Toe.Y,0);
        if (I==TurnSteps) P=FVector(C.X,C.Y-EndRadius,0);
        if (I>0) Length+=FVector::Dist2D(Route.Last(),P);
        Route.Add(P); Distances.Add(Length);
    }
    const double ClimbIndex=(StartRadius-ClimbEndRadius)/(StartRadius-EndRadius)*TurnSteps;
    const int32 Before=FMath::FloorToInt(ClimbIndex);
    const double ClimbLength=FMath::Lerp(Distances[Before],Distances[Before+1],ClimbIndex-Before);
    for (int32 I=0; I<Route.Num(); ++I)
        Route[I].Z=FMath::Lerp(StartZ,double(PlateauElevation),RiseFraction(Distances[I],ClimbLength));
    // DeliveryApproach is the last node outside the public-work obstruction.
    // Keep the following level gate geometry, but consumers must not demand
    // that workers or loaded carts enter the still-blocked construction site.
    Route.Add(FVector(C.X,1500,PlateauElevation));
    for (int32 I=1; I<=7; ++I) Route.Add(FVector(C.X,1500+200*I,PlateauElevation));
    return Route;
}

void HearthRoyalHill::AddToTerrain(HearthOrganicTerrain::FSettings& Settings)
{
    // Replace only our own derived profile on repeat calls. Caller road/pad
    // arrays, ownership and saved world positions are otherwise untouched.
    Settings.Roads.RemoveAll([](const HearthOrganicTerrain::FRoadCenterline& Road){return Road.bRoyalHillRoad;});
    HearthOrganicTerrain::FRoadCenterline Road;
    Road.Width=RoadWidth; Road.ShoulderWidth=RoadShoulder;
    Road.Transition=RoadTransition; Road.bRoyalHillRoad=true;
    for (const FVector& P : AscentRoute(Settings))
        Road.Nodes.Add({FVector2D(P.X,P.Y),float(P.Z)});
    Settings.Roads.Add(MoveTemp(Road));
    Settings.bRoyalHill=true;
}
