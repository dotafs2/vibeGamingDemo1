#include "HearthSitePresentation.h"

namespace HearthSitePresentation
{
    TArray<FHearthGroundPatch> CropGround(const FString& StableId, float Radius)
    {
        TArray<FHearthGroundPatch> Result;
        if (!FMath::IsFinite(Radius) || Radius <= 0.f) return Result;

        // The cylinder is 100 uu in diameter. Keep the smallest valid field
        // useful instead of allowing a patch to escape its logical radius.
        const float BaseScale = FMath::Min(1.25f, Radius / 70.f);
        if (BaseScale < 0.08f) return Result;

        FRandomStream Random(static_cast<int32>(FCrc::StrCrc32(*StableId)));
        const float Phase = Random.FRandRange(0.f, 90.f);
        const float CenterX = Radius * 0.22f;
        const float CenterY = Radius * 0.15f;

        Result.Reserve(4);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float Angle = FMath::DegreesToRadians(Phase + Index * 90.f);
            const float ScaleX = BaseScale * Random.FRandRange(0.88f, 1.08f);
            const float ScaleY = BaseScale * Random.FRandRange(0.82f, 1.04f);
            const float Yaw = Random.FRandRange(-12.f, 12.f);

            FHearthGroundPatch Patch;
            Patch.Offset = FVector(CenterX * FMath::Cos(Angle), CenterY * FMath::Sin(Angle), 0.f);
            Patch.Scale = FVector(ScaleX, ScaleY, Random.FRandRange(0.012f, 0.018f));
            Patch.Offset.Z = -50.f * Patch.Scale.Z;
            Patch.Yaw = Yaw;
            Result.Add(Patch);
        }
        return Result;
    }
}
