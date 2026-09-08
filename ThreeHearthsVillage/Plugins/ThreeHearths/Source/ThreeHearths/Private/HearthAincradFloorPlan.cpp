#include "HearthAincradFloorPlan.h"
#include <initializer_list>

namespace HearthAincradFloorPlan
{
    namespace
    {
        constexpr float CentimetresPerMetre = 100.f;
        constexpr float FloorRadiusCmValue = 500000.f;
        constexpr float TownWallRadiusCmValue = 50000.f;

        FVector2D M(float X, float Y)
        {
            return FVector2D(X * CentimetresPerMetre, Y * CentimetresPerMetre);
        }

        FRegion Region(const TCHAR* Id, const TCHAR* Label, const TCHAR* Kind,
            const FVector2D& CenterCm, float RadiusMetres, bool bSafe, const TCHAR* Note)
        {
            FRegion Result;
            Result.Id = Id;
            Result.ChineseLabel = Label;
            Result.Kind = Kind;
            Result.CenterCm = CenterCm;
            Result.RadiusCm = RadiusMetres * CentimetresPerMetre;
            Result.bSafeZone = bSafe;
            Result.CanonicalNote = Note;
            return Result;
        }

        void AddRoute(FPlan& Plan, const TCHAR* From, const TCHAR* To, std::initializer_list<FVector2D> Points)
        {
            FRoute Route;
            Route.FromRegionId = From;
            Route.ToRegionId = To;
            for (const FVector2D& Point : Points) Route.WaypointsCm.Add(Point);
            Plan.Routes.Add(MoveTemp(Route));
        }

        float Smooth01(float Value)
        {
            const float T = FMath::Clamp(Value, 0.f, 1.f);
            return T * T * (3.f - 2.f * T);
        }

        float PadBlend(const FVector2D& XYCm, const FVector2D& CenterCm, float RadiusCm)
        {
            return 1.f - Smooth01((XYCm - CenterCm).Size() / RadiusCm);
        }
    }

    FPlan Build()
    {
        FPlan Plan;
        Plan.FloorRadiusCm = FloorRadiusCmValue;
        Plan.TownWallCenterCm = M(0.f, -4970.f);
        Plan.TownWallRadiusCm = TownWallRadiusCmValue;
        Plan.SourcePolicy = TEXT("SAO 1st-floor geography; approximate implementation coordinates only. Sources: https://w.atwiki.jp/saop/pages/430.html and https://swordartonline.fandom.com/wiki/1st_Floor_(Aincrad). No canonical survey is implied. Level0 is the project codename for Aincrad floor 1, a 10 km diameter circular floor with a southern first-city semicircle and northern city wall. Superseded medieval sovereignty, levy, and central hill nodes are excluded.");

        Plan.Regions = {
            Region(TEXT("beginnings_plaza"), TEXT("起始之城广场"), TEXT("southern_city"), M(0.f, -4770.f), 500.f, true, TEXT("Southern first city implementation anchor; approximate.")),
            Region(TEXT("black_iron_palace"), TEXT("黑铁宫"), TEXT("southern_city_landmark"), M(0.f, -4900.f), 140.f, true, TEXT("Southern city landmark; approximate.")),
            Region(TEXT("beginnings_northgate"), TEXT("起始之城北门"), TEXT("city_gate"), M(0.f, -4470.f), 80.f, true, TEXT("Northern city wall gate; approximate.")),
            Region(TEXT("western_gate"), TEXT("西门"), TEXT("city_gate"), M(-433.f, -4720.f), 70.f, true, TEXT("Western city gate; approximate.")),
            Region(TEXT("meadow"), TEXT("草原"), TEXT("meadow"), M(0.f, -3400.f), 650.f, false, TEXT("Southern grassland transition; approximate.")),
            Region(TEXT("horunka"), TEXT("霍伦卡"), TEXT("settlement"), M(-2050.f, -2550.f), 120.f, true, TEXT("Western village and safe settlement; exact extent is an implementation choice.")),
            Region(TEXT("nepenthes_forest"), TEXT("食人花森林"), TEXT("forest"), M(-1900.f, -1700.f), 650.f, false, TEXT("Western forest anchor; approximate.")),
            Region(TEXT("lake_region"), TEXT("东侧湖区"), TEXT("lake_region"), M(1500.f, -1600.f), 700.f, false, TEXT("Eastern lake region shore access anchor; approximate, lake basin lies farther east.")),
            Region(TEXT("ruins"), TEXT("遗迹"), TEXT("ruins"), M(0.f, 750.f), 600.f, false, TEXT("Central ruins anchor; approximate.")),
            Region(TEXT("labyrinth_forest"), TEXT("迷宫森林"), TEXT("forest"), M(-1850.f, 2300.f), 700.f, false, TEXT("Northern western forest edge; approximate.")),
            Region(TEXT("tolbana"), TEXT("托尔巴纳"), TEXT("settlement"), M(0.f, 2900.f), 100.f, true, TEXT("Northern valley town, approximately 200m across in Progressive; map coordinate approximate.")),
            Region(TEXT("labyrinth"), TEXT("迷宫"), TEXT("labyrinth"), M(0.f, 4200.f), 800.f, false, TEXT("Northern labyrinth anchor; approximate."))
        };

        // Each route starts and ends at the exact CenterCm of its named regions.
        AddRoute(Plan, TEXT("beginnings_plaza"), TEXT("beginnings_northgate"), {M(0.f, -4770.f), M(0.f, -4470.f)});
        AddRoute(Plan, TEXT("beginnings_plaza"), TEXT("western_gate"), {M(0.f, -4770.f), M(-433.f, -4720.f)});
        AddRoute(Plan, TEXT("beginnings_plaza"), TEXT("black_iron_palace"), {M(0.f, -4770.f), M(0.f, -4900.f)});
        AddRoute(Plan, TEXT("western_gate"), TEXT("horunka"), {M(-433.f, -4720.f), M(-1200.f, -3400.f), M(-2050.f, -2550.f)});
        AddRoute(Plan, TEXT("beginnings_northgate"), TEXT("meadow"), {M(0.f, -4470.f), M(0.f, -3400.f)});
        AddRoute(Plan, TEXT("meadow"), TEXT("horunka"), {M(0.f, -3400.f), M(-1050.f, -3000.f), M(-2050.f, -2550.f)});
        AddRoute(Plan, TEXT("horunka"), TEXT("nepenthes_forest"), {M(-2050.f, -2550.f), M(-1900.f, -1700.f)});
        AddRoute(Plan, TEXT("meadow"), TEXT("lake_region"), {M(0.f, -3400.f), M(1200.f, -2700.f), M(1500.f, -1600.f)});
        AddRoute(Plan, TEXT("lake_region"), TEXT("ruins"), {M(1500.f, -1600.f), M(1200.f, -250.f), M(0.f, 750.f)});
        AddRoute(Plan, TEXT("nepenthes_forest"), TEXT("ruins"), {M(-1900.f, -1700.f), M(-900.f, -450.f), M(0.f, 750.f)});
        AddRoute(Plan, TEXT("ruins"), TEXT("labyrinth_forest"), {M(0.f, 750.f), M(-950.f, 1500.f), M(-1850.f, 2300.f)});
        AddRoute(Plan, TEXT("ruins"), TEXT("tolbana"), {M(0.f, 750.f), M(0.f, 2900.f)});
        AddRoute(Plan, TEXT("tolbana"), TEXT("labyrinth"), {M(0.f, 2900.f), M(0.f, 4200.f)});
        return Plan;
    }

    float HeightAt(const FVector2D& XYCm)
    {
        if(XYCm.Y<=-430000.f) return 0.f;
        const float DistanceFromCenter = XYCm.Size();
        const float North01 = Smooth01((XYCm.Y + 340000.f) / 700000.f);
        const float East01 = Smooth01((XYCm.X + 50000.f) / 350000.f);
        float Height = 0.f;

        // The southern city is flat, while the grassland rises gradually into
        // the central ruins and northern rolling hills.
        if (XYCm.Y <= -430000.f) Height = 0.f;
        else Height = 900.f * North01 + 450.f * FMath::Sin(XYCm.X / 42000.f) * North01;
        if (XYCm.Y >= 65000.f) Height += 2800.f * Smooth01((XYCm.Y - 65000.f) / 355000.f);
        if (DistanceFromCenter > 1000.f) Height += 700.f * East01 * Smooth01((DistanceFromCenter - 1000.f) / 300000.f);

        const float LakeBlend = 1.f - Smooth01(((XYCm - M(2350.f, -1200.f)).Size()-65000.f) / 25000.f);
        Height = FMath::Lerp(Height, -200.f, LakeBlend);

        const FVector2D PadCenters[] = {M(0.f, -4770.f), M(0.f, -4900.f), M(0.f, 2900.f)};
        const float PadRadii[] = {45000.f, 16000.f, 30000.f};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(PadCenters); ++Index)
            Height = FMath::Lerp(Height, 0.f, PadBlend(XYCm, PadCenters[Index], PadRadii[Index]));
        return FMath::Clamp(Height, -200.f, 10000.f);
    }

    bool IsInsideFloor(const FVector2D& XYCm)
    {
        return XYCm.SizeSquared() <= FMath::Square(FloorRadiusCmValue);
    }
}
