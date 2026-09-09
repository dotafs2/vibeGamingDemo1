#include "HearthAincradTownLayout.h"

namespace HearthAincradTownLayout
{
    namespace
    {
        constexpr float MainStreetSouthMinCm = 455000.f;
        constexpr float MainStreetSouthMaxCm = 470000.f;
        constexpr float ReplacementWestCm = -3200.f;
        constexpr float ReplacementEastCm = 3200.f;
        constexpr float ReplacementFloorCm = 900.f;
        constexpr float WorkbenchStandOffCm = 155.f;

        FVector RotateLocalXY(const FVector2D& LocalCm, const FVector& CenterCm, float YawDegrees, float ZCm)
        {
            const float Radians = FMath::DegreesToRadians(YawDegrees);
            const float Cosine = FMath::Cos(Radians);
            const float Sine = FMath::Sin(Radians);
            return CenterCm + FVector(
                Cosine * LocalCm.X - Sine * LocalCm.Y,
                Sine * LocalCm.X + Cosine * LocalCm.Y,
                ZCm);
        }

        FBuilding MakeBuilding(
            const TCHAR* Id,
            const TCHAR* Role,
            const FVector& CenterCm,
            const FVector2D& FootprintCm,
            float YawDegrees,
            int32 Floors,
            const FVector* SpawnOverride = nullptr,
            bool bHasWorkbench = false)
        {
            FBuilding Building;
            Building.Id = Id;
            Building.Role = Role;
            Building.CenterCm = CenterCm;
            Building.FootprintCm = FootprintCm;
            Building.YawDegrees = YawDegrees;
            Building.Floors = Floors;

            const float HalfDepthCm = FootprintCm.Y * 0.5f;
            Building.EntranceCm = RotateLocalXY(
                FVector2D(0.f, -HalfDepthCm - 90.f), CenterCm, YawDegrees, 92.f);
            Building.LegacyWorkCm = RotateLocalXY(
                FVector2D(0.f, -HalfDepthCm + 350.f), CenterCm, YawDegrees, 92.f);
            Building.WorkCm = Building.LegacyWorkCm;
            Building.bHasWorkbench = bHasWorkbench;
            if (bHasWorkbench)
            {
                Building.WorkbenchCm = RotateLocalXY(
                    FVector2D(FootprintCm.X * 0.28f, -FootprintCm.Y * 0.15f), CenterCm, YawDegrees, 88.f);
                const FVector TowardLegacyWork = (Building.LegacyWorkCm - Building.WorkbenchCm).GetSafeNormal2D();
                Building.WorkCm = Building.WorkbenchCm + TowardLegacyWork * WorkbenchStandOffCm;
                Building.WorkCm.Z = 92.f;
            }
            Building.ObserveCm = RotateLocalXY(
                FVector2D(0.f, -HalfDepthCm - 550.f), CenterCm, YawDegrees, 92.f);
            Building.SpawnCm = SpawnOverride != nullptr ? *SpawnOverride : Building.EntranceCm;
            return Building;
        }
    }

    FPlan Build()
    {
        FPlan Plan;
        Plan.Id = TEXT("sao_starter_street_s0");
        Plan.Revision = 1;

        // MainStreet is a 150 m centreline. All values are UE centimetres;
        // UE positive Y points south on this street.
        Plan.MainStreet.Add(FVector(0.f, MainStreetSouthMaxCm, 0.f));
        Plan.MainStreet.Add(FVector(0.f, MainStreetSouthMinCm, 0.f));
        Plan.ReplacementBounds = FBox(
            FVector(ReplacementWestCm, MainStreetSouthMinCm, 0.f),
            FVector(ReplacementEastCm, MainStreetSouthMaxCm, ReplacementFloorCm));

        const FVector InnSpawn(-400.f, 468700.f, 92.f);
        const FVector SmithySpawn(400.f, 466700.f, 92.f);
        const FVector CarpentrySpawn(-400.f, 463700.f, 92.f);

        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_inn_01"), TEXT("innkeeper"),
            FVector(-1800.f, 467000.f, 0.f), FVector2D(1200.f, 1600.f), 90.f, 2, &InnSpawn, true));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_smithy_01"), TEXT("blacksmith"),
            FVector(1800.f, 465000.f, 0.f), FVector2D(1000.f, 1400.f), -90.f, 2, &SmithySpawn, true));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_carpentry_01"), TEXT("carpenter"),
            FVector(-1800.f, 462000.f, 0.f), FVector2D(1000.f, 1400.f), 90.f, 2, &CarpentrySpawn, true));

        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_01"), TEXT("residential"),
            FVector(2200.f, 469000.f, 0.f), FVector2D(1000.f, 1200.f), -90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_02"), TEXT("residential"),
            FVector(-2400.f, 464500.f, 0.f), FVector2D(1000.f, 1200.f), 90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_03"), TEXT("residential"),
            FVector(2400.f, 467000.f, 0.f), FVector2D(900.f, 1100.f), -90.f, 3));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_04"), TEXT("residential"),
            FVector(-2300.f, 459500.f, 0.f), FVector2D(1000.f, 1300.f), 90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_05"), TEXT("residential"),
            FVector(2200.f, 463000.f, 0.f), FVector2D(1000.f, 1200.f), -90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_06"), TEXT("residential"),
            FVector(-2200.f, 457000.f, 0.f), FVector2D(1000.f, 1200.f), 90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_07"), TEXT("residential"),
            FVector(2400.f, 460500.f, 0.f), FVector2D(900.f, 1100.f), -90.f, 2));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_08"), TEXT("residential"),
            FVector(-2400.f, 455600.f, 0.f), FVector2D(1000.f, 1200.f), 90.f, 3));
        Plan.Buildings.Add(MakeBuilding(
            TEXT("sao_residential_09"), TEXT("residential"),
            FVector(2200.f, 458000.f, 0.f), FVector2D(1000.f, 1200.f), -90.f, 2));

        return Plan;
    }

    bool IsReplacementArea(const FVector2D& GeographicEastNorthCm)
    {
        const FVector UEPoint(
            GeographicEastNorthCm.X,
            -GeographicEastNorthCm.Y,
            0);
        return UEPoint.X >= ReplacementWestCm
            && UEPoint.X <= ReplacementEastCm
            && UEPoint.Y >= MainStreetSouthMinCm
            && UEPoint.Y <= MainStreetSouthMaxCm;
    }

    const FBuilding* Find(const FPlan& Plan, const FString& Id)
    {
        return Plan.Buildings.FindByPredicate([&Id](const FBuilding& Building)
        {
            return Building.Id == Id;
        });
    }
}
