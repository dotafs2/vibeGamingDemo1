#include "HearthCityPlan.h"

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

FHearthCityPlan HearthCityPlan::BuildForVersion(int32 LayoutVersion)
{
    return LayoutVersion >= 3 ? BuildVersion3() : Build();
}
