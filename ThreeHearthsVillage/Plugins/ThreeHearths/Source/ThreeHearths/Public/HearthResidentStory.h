#pragma once

#include "CoreMinimal.h"

struct FHearthResidentStoryInput
{
    FString StableId;
    FString Name;
    FString Role;
    FString Personality;
    bool bKing = false;
};

namespace HearthResidentStory
{
    FString Create(const FHearthResidentStoryInput& Input);
    FString Prompt(const FString& PersistentStory);
}
