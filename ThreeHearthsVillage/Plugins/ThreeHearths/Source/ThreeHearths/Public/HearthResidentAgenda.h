#pragma once

#include "CoreMinimal.h"

struct FHearthResidentAgendaAction
{
    int32 Id = INDEX_NONE;
    FString Label;
};

struct FHearthResidentAgendaInput
{
    FString Role;
    FString Personality;
    FString DesignGoal;
    FString DesignFeedback;
    FString DesignRequest;
    FString RelationshipSummary;
    bool bKing = false;
    float Hunger = 0.f;
    float Energy = 0.f;
    float Mood = 0.f;
    float SocialNeed = 0.f;
    int32 Coins = 0;
    int32 FoodStock = 0;
    int32 WoodStock = 0;
    int32 StoneStock = 0;
    int32 ClayStock = 0;
    int32 TileStock = 0;
    int32 TreasuryCoins = 0;
    int32 TaxRatePercent = 0;
    int32 CompletedHomes = 0;
    int32 ResidentCount = 0;
    TArray<FHearthResidentAgendaAction> AvailableActions;
};

struct FHearthResidentAgenda
{
    FString AuthoritativeFacts;
    FString PrivateAspirations;
    FString MissingInventory;
    FString MissingCapability;
    FString Summary;
    TArray<int32> GroundedPriorityActions;
};

namespace HearthResidentAgenda
{
    FHearthResidentAgenda Build(const FHearthResidentAgendaInput& Input);
    FString ToPromptText(const FHearthResidentAgenda& Agenda);
}
