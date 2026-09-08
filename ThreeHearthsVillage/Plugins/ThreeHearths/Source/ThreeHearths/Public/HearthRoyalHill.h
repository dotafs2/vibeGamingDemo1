#pragma once

#include "CoreMinimal.h"

namespace HearthOrganicTerrain { struct FSettings; }

// Pure terrain/road manifest. All elevations and distances are centimetres.
namespace HearthRoyalHill
{
    constexpr float PlateauElevation = 3500.f;
    constexpr float PlateauRadius = 4800.f;
    constexpr float FootRadius = 9000.f;
    constexpr float RoadWidth = 600.f;
    constexpr float RoadShoulder = 450.f;
    constexpr float RoadTransition = 600.f;

    THREEHEARTHS_API FVector2D Center();
    // Outside the +/-5000cm public-site AABB including its 25cm clearance.
    // Use for workers/deliveries; the later gate road is only planning geometry.
    THREEHEARTHS_API FVector DeliveryApproach();
    // One ascent turn, -90 to +270 degrees, then the level southern approach.
    // The first node samples the original terrain with bRoyalHill disabled.
    THREEHEARTHS_API TArray<FVector> AscentRoute();
    // Same geometry, using the supplied seed/bounds for the legacy toe datum.
    THREEHEARTHS_API TArray<FVector> AscentRoute(const HearthOrganicTerrain::FSettings& Settings);
    // Opt in and append the complete protected ascent profile once.
    THREEHEARTHS_API void AddToTerrain(HearthOrganicTerrain::FSettings& Settings);
}
