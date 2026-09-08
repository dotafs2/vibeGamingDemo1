#include "HearthCityPlan.h"
#include "HearthRoyalHill.h"
#include "HearthOrganicTerrain.h"

namespace
{
    constexpr float GroundZ = 8.f;

    FVector Point(float X, float Y)
    {
        return FVector(X, Y, GroundZ);
    }

    void AddRoad(FHearthCityPlan& Plan, const FVector& A, const FVector& B, float Width)
    {
        Plan.Roads.Add({A, B, Width});
    }

    void AddLandmark(FHearthCityPlan& Plan, const TCHAR* Id, const TCHAR* Kind, const FVector& Position, const FVector& Approach, float Radius)
    {
        Plan.Landmarks.Add({Id, Kind, Position, Approach, Radius});
    }

    void AddDistrict(FHearthCityPlan& Plan, const TCHAR* Id, const TCHAR* Kind, const FVector& Center, float Radius)
    {
        Plan.Districts.Add({Id, Kind, Center, Radius});
    }
}

FHearthCityPlan HearthCityPlan::Build()
{
    FHearthCityPlan Plan;
    Plan.LayoutVersion = 2;
    Plan.RecommendedPopulation = 10;
    Plan.MapMin = FVector2D(-6000.f, -6000.f);
    Plan.MapMax = FVector2D(5700.f, 5700.f);

    // The old north-south street remains the compatibility spine. Its two old
    // endpoints are retained while the lower work yard is skirted by a bend.
    AddRoad(Plan, Point(-2130.f, -5100.f), Point(-2270.f, -3700.f), 220.f);
    AddRoad(Plan, Point(-2270.f, -3700.f), Point(-2130.f, -1450.f), 220.f);
    AddRoad(Plan, Point(-2130.f, -1450.f), Point(-1950.f, -1270.f), 200.f);
    AddRoad(Plan, Point(-1950.f, -1270.f), Point(-1950.f, -1050.f), 200.f);
    AddRoad(Plan, Point(-1950.f, -1050.f), Point(-1950.f, -830.f), 200.f);
    AddRoad(Plan, Point(-1950.f, -830.f), Point(-2130.f, -650.f), 200.f);
    AddRoad(Plan, Point(-2130.f, -650.f), Point(-2130.f, 500.f), 220.f);
    AddRoad(Plan, Point(-2130.f, 500.f), Point(-2130.f, 650.f), 220.f);
    AddRoad(Plan, Point(-2130.f, 650.f), Point(-2310.f, 2600.f), 220.f);
    AddRoad(Plan, Point(-2310.f, 2600.f), Point(-2130.f, 5100.f), 220.f);
    AddRoad(Plan, Point(-2130.f, -1450.f), Point(-2800.f, -1450.f), 140.f);
    AddRoad(Plan, Point(-2800.f, -1450.f), Point(-2800.f, -1350.f), 140.f);

    // A straight civic approach reaches the castle gate, then stops before the
    // castle footprint. The gate reserve is deliberately below the castle.
    AddRoad(Plan, Point(-2130.f, 650.f), Point(-1050.f, 1200.f), 220.f);

    // Market access passes north of the shared pickup point before turning into
    // the market edge, so neither service point is cut through by the road.
    AddRoad(Plan, Point(-1950.f, -830.f), Point(-1500.f, -650.f), 160.f);
    AddRoad(Plan, Point(-1500.f, -650.f), Point(-1100.f, -600.f), 160.f);
    AddRoad(Plan, Point(-1950.f, -1050.f), Point(-1840.f, -1050.f), 140.f);


    // A closed residential block gives the plan a shared street frontage and a
    // protected green court instead of leaving all growth on tree branches.
    AddRoad(Plan, Point(-2130.f, 500.f), Point(-1890.f, 430.f), 160.f);
    AddRoad(Plan, Point(-1890.f, 430.f), Point(-1870.f, -270.f), 160.f);
    AddRoad(Plan, Point(-1870.f, -270.f), Point(-1220.f, -210.f), 160.f);
    AddRoad(Plan, Point(-1220.f, -210.f), Point(-1120.f, 470.f), 160.f);
    AddRoad(Plan, Point(-1120.f, 470.f), Point(-1890.f, 430.f), 160.f);

    // Two irregular residential lanes reserve room for work yards and later
    // growth west of the civic core, clear of the existing resource groves.
    AddRoad(Plan, Point(-2130.f, 650.f), Point(-3300.f, 900.f), 180.f);
    AddRoad(Plan, Point(-3300.f, 900.f), Point(-4400.f, 900.f), 180.f);
    AddRoad(Plan, Point(-2310.f, 2600.f), Point(-3300.f, 2550.f), 180.f);
    AddRoad(Plan, Point(-3300.f, 2550.f), Point(-4200.f, 2700.f), 180.f);

    AddLandmark(Plan, TEXT("main_keep"), TEXT("castle"), Point(-1050.f, 2200.f), Point(-1050.f, 1200.f), 850.f);
    AddLandmark(Plan, TEXT("public_market"), TEXT("market"), Point(-1100.f, -1050.f), Point(-1100.f, -600.f), 330.f);
    AddLandmark(Plan, TEXT("shared_pickup"), TEXT("pickup"), Point(-1650.f, -1050.f), Point(-1840.f, -1050.f), 120.f);
    AddLandmark(Plan, TEXT("woodworking_bench"), TEXT("workshop"), Point(-2250.f, -1050.f), Point(-1950.f, -1050.f), 190.f);
    AddLandmark(Plan, TEXT("kiln"), TEXT("kiln"), Point(-2800.f, -1050.f), Point(-2800.f, -1350.f), 210.f);

    AddDistrict(Plan, TEXT("castle_core"), TEXT("civic_core"), Point(-1050.f, 2200.f), 800.f);
    AddDistrict(Plan, TEXT("gatehouse_reserve"), TEXT("gatehouse_reserve"), Point(-1050.f, 1200.f), 130.f);
    AddDistrict(Plan, TEXT("dense_residential_block"), TEXT("residential_high_density"), Point(-1525.f, 125.f), 500.f);
    AddDistrict(Plan, TEXT("green_court"), TEXT("residential_green_court"), Point(-1525.f, 125.f), 220.f);
    AddDistrict(Plan, TEXT("low_density_homes"), TEXT("residential_low_density"), Point(-3400.f, 1700.f), 850.f);
    AddDistrict(Plan, TEXT("market_square"), TEXT("civic_market"), Point(-1100.f, -1050.f), 500.f);
    AddDistrict(Plan, TEXT("work_yard"), TEXT("work_low_density"), Point(-2450.f, -1050.f), 500.f);
    AddDistrict(Plan, TEXT("farm_reserve"), TEXT("farm_low_density"), Point(-1135.f, -2825.f), 470.f);

    return Plan;
}

FHearthCityPlan HearthCityPlan::BuildVersion3()
{
    FHearthCityPlan Plan;
    Plan.LayoutVersion = 3;
    Plan.RecommendedPopulation = 30;
    // A 300 m square centered on the generated Town3 terrain. The castle is
    // intentionally north-east of the old Town2 sample so an old save never
    // gets silently reinterpreted as this plan.
    Plan.MapMin = FVector2D(-8500.f, -8500.f);
    Plan.MapMax = FVector2D(21500.f, 21500.f);

    // The civic approach terminates at the south gate reserve. It stays clear
    // of the 100 m castle reserve while tying the old service yard to the new
    // neighborhoods.
    AddRoad(Plan, Point(-8000.f, -7600.f), Point(-1650.f, -1050.f), 300.f);
    AddRoad(Plan, Point(-1650.f, -1050.f), Point(3000.f, 450.f), 300.f);
    AddRoad(Plan, Point(3000.f, 450.f), Point(6500.f, 1500.f), 300.f);

    // West dense block: a finite polyline loop gives the houses a continuous
    // street frontage without making a square grid. The market/work yard
    // remains attached to the old service junction below it.
    AddRoad(Plan, Point(-5200.f, 1200.f), Point(-3000.f, 1450.f), 220.f);
    AddRoad(Plan, Point(-3000.f, 1450.f), Point(-850.f, 900.f), 220.f);
    AddRoad(Plan, Point(-850.f, 900.f), Point(-700.f, -2050.f), 220.f);
    AddRoad(Plan, Point(-700.f, -2050.f), Point(-2850.f, -2500.f), 220.f);
    AddRoad(Plan, Point(-2850.f, -2500.f), Point(-5200.f, -1900.f), 220.f);
    AddRoad(Plan, Point(-5200.f, -1900.f), Point(-5200.f, 1200.f), 220.f);
    AddRoad(Plan, Point(-850.f, 900.f), Point(3000.f, 450.f), 180.f);

    // East dense block: another shallow, non-orthogonal loop, kept south of
    // the 5000 cm castle reserve and joined to the civic approach.
    AddRoad(Plan, Point(3700.f, -2100.f), Point(5200.f, -2280.f), 220.f);
    AddRoad(Plan, Point(5200.f, -2280.f), Point(7600.f, -1500.f), 220.f);
    AddRoad(Plan, Point(7600.f, -1500.f), Point(7350.f, 250.f), 220.f);
    AddRoad(Plan, Point(7350.f, 250.f), Point(5550.f, 650.f), 220.f);
    AddRoad(Plan, Point(5550.f, 650.f), Point(3700.f, 500.f), 220.f);
    AddRoad(Plan, Point(3700.f, 500.f), Point(3700.f, -2100.f), 220.f);
    AddRoad(Plan, Point(7350.f, 250.f), Point(9000.f, -800.f), 180.f);
    AddRoad(Plan, Point(9000.f, -800.f), Point(9000.f, -3600.f), 180.f);

    // Keep the market and workshop approaches explicit after bending the
    // residential streets; these are the stable service interfaces.
    AddRoad(Plan, Point(-1650.f, -1050.f), Point(-1500.f, -820.f), 160.f);
    AddRoad(Plan, Point(-1500.f, -820.f), Point(-1100.f, -600.f), 160.f);
    AddRoad(Plan, Point(-1650.f, -1050.f), Point(-1950.f, -1050.f), 140.f);
    AddRoad(Plan, Point(-1950.f, -1050.f), Point(-2250.f, -1050.f), 140.f);
    AddRoad(Plan, Point(-2250.f, -1050.f), Point(-2800.f, -1350.f), 140.f);

    // A long southern connection keeps the two blocks and the existing
    // resource yard on one walkable graph without sending homes into the keep.
    AddRoad(Plan, Point(-5200.f, -2300.f), Point(-5200.f, -5200.f), 200.f);
    AddRoad(Plan, Point(-5200.f, -5200.f), Point(9000.f, -5200.f), 200.f);
    AddRoad(Plan, Point(9000.f, -5200.f), Point(9000.f, -3600.f), 200.f);

    AddLandmark(Plan, TEXT("main_keep"), TEXT("castle"), Point(6500.f, 6500.f), Point(6500.f, 1500.f), 5000.f);
    AddLandmark(Plan, TEXT("public_market"), TEXT("market"), Point(-1100.f, -1050.f), Point(-1100.f, -600.f), 330.f);
    AddLandmark(Plan, TEXT("shared_pickup"), TEXT("pickup"), Point(-1650.f, -1050.f), Point(-1840.f, -1050.f), 120.f);
    AddLandmark(Plan, TEXT("woodworking_bench"), TEXT("workshop"), Point(-2250.f, -1050.f), Point(-1950.f, -1050.f), 190.f);
    AddLandmark(Plan, TEXT("kiln"), TEXT("kiln"), Point(-2800.f, -1050.f), Point(-2800.f, -1350.f), 210.f);

    AddDistrict(Plan, TEXT("castle_core"), TEXT("civic_core"), Point(6500.f, 6500.f), 5000.f);
    AddDistrict(Plan, TEXT("castle_gate_reserve"), TEXT("gatehouse_reserve"), Point(6500.f, 1500.f), 260.f);
    AddDistrict(Plan, TEXT("west_dense_block"), TEXT("residential_high_density"), Point(-2950.f, -550.f), 2500.f);
    AddDistrict(Plan, TEXT("west_green_court"), TEXT("residential_green_court"), Point(-2950.f, -550.f), 950.f);
    AddDistrict(Plan, TEXT("east_dense_block"), TEXT("residential_high_density"), Point(5650.f, -800.f), 2100.f);
    AddDistrict(Plan, TEXT("market_square"), TEXT("civic_market"), Point(-1100.f, -1050.f), 500.f);
    AddDistrict(Plan, TEXT("work_yard"), TEXT("work_low_density"), Point(-2450.f, -1050.f), 500.f);
    AddDistrict(Plan, TEXT("farm_reserve"), TEXT("farm_low_density"), Point(-1135.f, -2825.f), 470.f);
    return Plan;
}

namespace HearthCityPlanRoyalHillDetail
{
    constexpr float RingWidth=480.f; // ordinary road, below reserved royal 600

    FVector NaturalPoint(const FVector& Point)
    {
        return FVector(Point.X,Point.Y,HearthOrganicTerrain::HeightAt(FVector2D(Point.X,Point.Y)));
    }

    void ConnectBySamples(FHearthCityPlan& Plan, const FVector& A, const FVector& B, float Width)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/250.0));
        FVector Previous=A;
        for (int32 I=1; I<=Steps; ++I)
        {
            const FVector Next=I==Steps?B:NaturalPoint(FMath::Lerp(A,B,double(I)/Steps));
            AddRoad(Plan,Previous,Next,Width);
            Previous=Next;
        }
    }

    TArray<FVector2D> OuterCivicWaypoints()
    {
        // Authored around the royal hill rather than generated from one radius.
        // The first point stays at the old south toe so the royal load/ascent
        // connection keeps its exact legacy XY contract. The other points make
        // a bounded, gently changing civic edge for fields, forest work and
        // the east meadow approach without entering the protected hill.
        const FVector2D C=HearthRoyalHill::Center();
        return {
            C+FVector2D(0,-10500), C+FVector2D(3200,-10400), C+FVector2D(6500,-9500),
            C+FVector2D(9100,-7000), C+FVector2D(10800,-3600), C+FVector2D(11200,0),
            C+FVector2D(10600,3500), C+FVector2D(9000,6500), C+FVector2D(6500,9000),
            C+FVector2D(3000,10800), C+FVector2D(-500,11000), C+FVector2D(-3800,10500),
            C+FVector2D(-7000,9000), C+FVector2D(-9800,5800), C+FVector2D(-11100,2100),
            C+FVector2D(-10800,-1800), C+FVector2D(-9400,-5100), C+FVector2D(-6900,-7600),
            C+FVector2D(-3400,-10000)
        };
    }

    void AddOuterCivicRoad(FHearthCityPlan& Plan)
    {
        const TArray<FVector2D> Waypoints=OuterCivicWaypoints();
        for (int32 I=0; I<Waypoints.Num(); ++I)
        {
            const FVector A(Waypoints[I].X,Waypoints[I].Y,0);
            const FVector B(Waypoints[(I+1)%Waypoints.Num()].X,Waypoints[(I+1)%Waypoints.Num()].Y,0);
            ConnectBySamples(Plan,NaturalPoint(A),NaturalPoint(B),RingWidth);
        }
    }
}

FHearthCityPlan HearthCityPlan::BuildVersion4()
{
    using namespace HearthCityPlanRoyalHillDetail;
    FHearthCityPlan Plan=BuildVersion3();
    Plan.LayoutVersion=4;
    // First-floor inspiration is topology only: a starting hub, southern
    // fields, a western forest/work edge, an eastern meadow/waterside edge and
    // a northern unknown reserve. The reserve is planning space, not a promise
    // of a dungeon, a new town or generated inhabitants.
    // Keep every old residential/service street at its original low grade.
    // Replace only the previous direct castle approach with the winding road.
    Plan.Roads.RemoveAll([](const FHearthTownRoadSegment& Road)
    {
        return FVector::Dist2D(Road.A,FVector(3000,450,0))<1
            && FVector::Dist2D(Road.B,FVector(6500,1500,0))<1;
    });

    const FVector2D C=HearthRoyalHill::Center();
    const TArray<FVector2D> OuterWaypoints=OuterCivicWaypoints();
    TArray<FVector> Ring;
    Ring.Reserve(OuterWaypoints.Num());
    for (const FVector2D& Waypoint : OuterWaypoints)
        Ring.Add(NaturalPoint(FVector(Waypoint.X,Waypoint.Y,0)));
    AddOuterCivicRoad(Plan);
    const auto ConnectToRing=[&](const FVector& P)
    {
        int32 Closest=0;
        for (int32 I=1; I<Ring.Num(); ++I)
            if (FVector::DistSquared2D(P,Ring[I])<FVector::DistSquared2D(P,Ring[Closest])) Closest=I;
        ConnectBySamples(Plan,NaturalPoint(P),Ring[Closest],320.f);
    };
    // Attach the old western lane and southern spine outside the hillside.
    ConnectToRing(Point(-850,900));
    ConnectToRing(Point(9000,-3600));
    ConnectBySamples(Plan,NaturalPoint(Point(-5200,-1900)),NaturalPoint(Point(-5200,-2300)),220.f);
    // Exact service-spine junction into the existing western loop.
    ConnectBySamples(Plan,NaturalPoint(Point(-1650,-1050)),NaturalPoint(Point(-700,-2050)),220.f);
    // A short southern field lane makes the hub-to-fields relationship legible
    // while stopping at the advisory field edge, not a new farm building.
    ConnectBySamples(Plan,NaturalPoint(Point(-700,-2050)),NaturalPoint(Point(-850,-2300)),180.f);
    ConnectBySamples(Plan,NaturalPoint(Point(-850,-2300)),NaturalPoint(Point(-1600,-2350)),180.f);
    // The east loop opens toward the meadow/waterside planning edge without
    // inventing a water asset or promising a built eastern settlement.
    ConnectBySamples(Plan,NaturalPoint(Point(7350,250)),NaturalPoint(Point(9000,650)),180.f);
    // Future west/north/east district approaches remain natural at the foot.
    for (const FVector& P : {Point(-4700,8800),Point(6500,17600),Point(17800,6500)}) ConnectToRing(P);

    const TArray<FVector> Ascent=HearthRoyalHill::AscentRoute();
    ConnectBySamples(Plan,NaturalPoint(Point(3000,450)),Ascent[0],400.f);
    // The southern toe replaces the former straight elevated-gate shortcut.
    ConnectBySamples(Plan,Ring[0],Ascent[0],400.f);
    for (int32 I=1; I<Ascent.Num(); ++I) AddRoad(Plan,Ascent[I-1],Ascent[I],HearthRoyalHill::RoadWidth);
    for (auto& Landmark : Plan.Landmarks) if (Landmark.Id==TEXT("main_keep"))
    {
        Landmark.Position=FVector(C.X,C.Y,HearthRoyalHill::PlateauElevation);
        Landmark.Approach=HearthRoyalHill::DeliveryApproach();
    }
    for (auto& District : Plan.Districts)
    {
        if (District.Id==TEXT("castle_core")) District.Center=FVector(C.X,C.Y,HearthRoyalHill::PlateauElevation);
        if (District.Id==TEXT("castle_gate_reserve")) District.Center=HearthRoyalHill::DeliveryApproach();
    }
    return Plan;
}

FHearthCityPlan HearthCityPlan::BuildForVersion(int32 LayoutVersion)
{
    if (LayoutVersion>=4) return BuildVersion4();
    return LayoutVersion >= 3 ? BuildVersion3() : Build();
}
