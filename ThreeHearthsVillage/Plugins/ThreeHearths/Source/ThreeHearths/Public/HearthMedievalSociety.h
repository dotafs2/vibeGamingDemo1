#pragma once

#include "CoreMinimal.h"

/** Data-only guard, gate and freight model for the medieval village layer. */
enum class EMedievalSocietyRole : uint8
{
    Gatekeeper,
    RoyalGuard,
    Carter
};

enum class EMedievalDutyAction : uint8
{
    Patrol,
    Stand,
    Salute,
    Rest
};

enum class EMedievalFreightStage : uint8
{
    Reserved,
    PickedUp,
    Delivered,
    Cancelled
};

struct THREEHEARTHS_API FMedievalEmotionState
{
    float Morale = 60.f;
    float Fatigue = 0.f;
    float Alertness = 50.f;
    float SocialNeed = 0.f;
};

struct THREEHEARTHS_API FMedievalSocietyMember
{
    FString StableId;
    FString DisplayName;
    EMedievalSocietyRole Role = EMedievalSocietyRole::Gatekeeper;
    FMedievalEmotionState Emotion;
    EMedievalDutyAction CurrentDuty = EMedievalDutyAction::Stand;
    FString DutyReason;
    int32 DutyRevision = 0;
};

struct THREEHEARTHS_API FMedievalDutyContext
{
    bool bVisitorWaiting = false;
    bool bThreatDetected = false;
    bool bAtGate = true;
    float MinutesOnDuty = 0.f;
};

struct THREEHEARTHS_API FMedievalDutyDecision
{
    EMedievalDutyAction Action = EMedievalDutyAction::Stand;
    FString Description;
    FString Reason;
};

struct THREEHEARTHS_API FMedievalVisitorRecord
{
    FString StableId;
    FString DisplayName;
    bool bKnownResident = false;
    bool bCheckedIn = false;
    bool bAllowed = false;
    FString LastEventId;
    FString LastReason;
};

struct THREEHEARTHS_API FMedievalSocietyEvent
{
    FString EventId;
    FString Kind;
    FString SubjectId;
    FString GuardId;
    bool bAllowed = false;
    FString Reason;
    int32 Revision = 0;
};

struct THREEHEARTHS_API FMedievalStorage
{
    FString StableId;
    TMap<FString, int32> Inventory;
    TMap<FString, int32> Reserved;
};

struct THREEHEARTHS_API FMedievalCart
{
    FString StableId;
    FString OperatorStableId;
    FString HorseStableId;
    int32 CapacityUnits = 0;
    TMap<FString, int32> Cargo;
    FString ActiveReservationId;
};

struct THREEHEARTHS_API FMedievalHorse
{
    FString StableId;
    FString HarnessedCartId;
};

struct THREEHEARTHS_API FMedievalFreightReservation
{
    FString ReservationId;
    FString CartId;
    FString OperatorStableId;
    FString SourceStorageId;
    FString DestinationStorageId;
    TMap<FString, int32> Cargo;
    EMedievalFreightStage Stage = EMedievalFreightStage::Reserved;
};

struct THREEHEARTHS_API FMedievalVisitorResult
{
    bool bSuccess = false;
    bool bAlreadyApplied = false;
    bool bAllowed = false;
    FString Reason;
};

struct THREEHEARTHS_API FMedievalFreightResult
{
    bool bSuccess = false;
    bool bAlreadyApplied = false;
    int32 MovedUnits = 0;
    FString Reason;
};

struct THREEHEARTHS_API FMedievalSocietyState
{
    TArray<FMedievalSocietyMember> Members;
    TArray<FMedievalVisitorRecord> Visitors;
    TArray<FMedievalSocietyEvent> Events;
    TArray<FMedievalStorage> Storages;
    TArray<FMedievalHorse> Horses;
    TArray<FMedievalCart> Carts;
    TArray<FMedievalFreightReservation> FreightReservations;
    int32 Revision = 0;
};

namespace HearthMedievalSociety
{
    THREEHEARTHS_API bool ValidateMember(const FMedievalSocietyMember& Member, FString* OutError = nullptr);
    THREEHEARTHS_API bool ValidateState(const FMedievalSocietyState& State, FString* OutError = nullptr);

    THREEHEARTHS_API bool AddMember(FMedievalSocietyState& State, const FMedievalSocietyMember& Member, FString* OutError = nullptr);
    THREEHEARTHS_API bool AddStorage(FMedievalSocietyState& State, const FMedievalStorage& Storage, FString* OutError = nullptr);
    THREEHEARTHS_API bool AddHorse(FMedievalSocietyState& State, const FMedievalHorse& Horse, FString* OutError = nullptr);
    THREEHEARTHS_API bool AddCart(FMedievalSocietyState& State, const FMedievalCart& Cart, FString* OutError = nullptr);
    THREEHEARTHS_API const FMedievalSocietyMember* FindMember(const FMedievalSocietyState& State, const FString& StableId);
    THREEHEARTHS_API FMedievalSocietyMember* FindMember(FMedievalSocietyState& State, const FString& StableId);

    THREEHEARTHS_API FMedievalDutyDecision DecideLocalDuty(const FMedievalSocietyMember& Member, const FMedievalDutyContext& Context);

    /** Check in a visitor once. Reusing EventId returns the original decision without another log entry. */
    THREEHEARTHS_API bool CheckInVisitor(FMedievalSocietyState& State, const FString& VisitorId, const FString& DisplayName,
        bool bKnownResident, bool bHasPass, const FString& GuardId, const FString& EventId, FMedievalVisitorResult& Out);

    THREEHEARTHS_API int32 InventoryCount(const FMedievalStorage& Storage, const FString& ItemId);
    THREEHEARTHS_API bool ReservePickup(FMedievalSocietyState& State, const FString& ReservationId, const FString& CartId,
        const FString& OperatorStableId, const FString& SourceStorageId, const FString& DestinationStorageId,
        const TMap<FString, int32>& Cargo, FMedievalFreightResult& Out);
    THREEHEARTHS_API bool CompletePickup(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out);
    THREEHEARTHS_API bool CompleteDelivery(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out);
    THREEHEARTHS_API bool CancelFreight(FMedievalSocietyState& State, const FString& ReservationId, FMedievalFreightResult& Out);
}
