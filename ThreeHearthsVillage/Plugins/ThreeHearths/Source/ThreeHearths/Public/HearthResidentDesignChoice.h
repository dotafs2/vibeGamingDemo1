#pragma once

#include "CoreMinimal.h"

/**
 * The small, serializable input surface used by the organic-village resident
 * design chooser.  This deliberately contains facts about one household and
 * its site, rather than an actor index, so a save/load or a reordered resident
 * array cannot change the result.
 */
struct THREEHEARTHS_API FHearthResidentDesignInput
{
    FString StableId;
    FString Name;
    FString Role;
    FString Occupation;
    FString Personality;
    FString HouseholdDescription;
    FString RelationshipSummary;
    FString PersistentGoals;

    int32 HouseholdSize = 1;
    int32 Dependents = 0;
    int32 CloseRelationships = 0;
    int32 NeighborCount = 0;

    int32 Coins = 0;
    int32 Planks = 0;
    int32 Beams = 0;
    int32 Stone = 0;
    int32 Tiles = 0;
    float SiteSlope = 0.f;

    uint32 WorldSeed = 0;
    int32 Revision = 0;
};

struct THREEHEARTHS_API FHearthResidentDesignAlternative
{
    int32 LayoutId = -1;
    FString Id;
    FString Label;
    // One of the three built visual master families. LayoutId is an
    // expansion intent; it is not a claim that six distinct recipes exist.
    FString Family;
    FString Reason;
    TArray<FString> Constraints;

    int32 Coins = 0;
    int32 Planks = 0;
    int32 Beams = 0;
    int32 Stone = 0;
    int32 Tiles = 0;
    float MaxSlope = 0.f;
    float Score = -BIG_NUMBER;
    bool bFeasible = false;
};

/** A resident's selected intent and the complete alternatives shown to them. */
struct THREEHEARTHS_API FHearthResidentDesignChoice
{
    int32 LayoutId = -1;
    uint32 Seed = 0;
    FString Reason;
    FString Source = TEXT("local_rules");
    bool bLocked = false;
    int32 Revision = 0;
    TArray<FHearthResidentDesignAlternative> Alternatives;
};

namespace HearthResidentDesignChoice
{
    constexpr int32 CompactCluster = 0;
    constexpr int32 LCourt = 1;
    constexpr int32 SteppedWings = 2;
    constexpr int32 UCourt = 3;
    constexpr int32 OffsetWorkshop = 4;
    constexpr int32 TowerAnnex = 5;
    constexpr int32 LayoutCount = 6;

    /** Stable identity seed. It does not depend on resident-array position. */
    THREEHEARTHS_API uint32 StableSeed(const FHearthResidentDesignInput& Input);

    /** Build all six intents and their current feasibility/score evidence. */
    THREEHEARTHS_API TArray<FHearthResidentDesignAlternative> RankAlternatives(const FHearthResidentDesignInput& Input);

    /** Choose the highest-ranked feasible intent, with deterministic deficit fallback. */
    THREEHEARTHS_API FHearthResidentDesignChoice ChooseLocal(const FHearthResidentDesignInput& Input);

    /**
     * Validate a structured model answer. The answer must contain exactly
     * layout_id, seed and reason; the layout must be one of this input's
     * feasible alternatives and seed must echo StableSeed(Input).
     */
    THREEHEARTHS_API bool ParseKimiChoice(const FString& JsonText, const FHearthResidentDesignInput& Input,
        FHearthResidentDesignChoice& OutChoice, FString& OutError);

    /** Prompt text for a later Kimi adapter. It enumerates the real alternatives. */
    THREEHEARTHS_API FString BuildKimiPrompt(const FHearthResidentDesignInput& Input,
        const TArray<FHearthResidentDesignAlternative>& Alternatives);

    THREEHEARTHS_API const TCHAR* LayoutIdName(int32 LayoutId);
}
