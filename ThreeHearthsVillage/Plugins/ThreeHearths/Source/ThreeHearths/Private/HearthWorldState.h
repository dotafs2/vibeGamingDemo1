#pragma once
#include "HearthVillage.h"

// Logical world image. UObject pointers and wall-clock epochs never enter the file.
struct FHearthSavedResident
{
    FHearthResident Person;
    FVector Position=FVector::ZeroVector;
    double Yaw=0, DecisionDelay=0;
    bool bPending=false;
    FString PendingOperation;
};
struct FHearthWorldImage
{
    FString Id, Run, Event;
    int32 Schema=7, PlotCount=3;
    int64 Revision=0;
    float Elapsed=0, Speed=1;
    double Remainder=0;
    bool bIsland=false, bPaused=false, bAutonomy=true, bComplete=false;
    int32 Selected=0, LastLife=-1, Food=30, Stone=0, Planks=0, Beams=0, TreasuryCoins=500, TaxProjectCoins=0, TaxRatePercent=25;
    int32 TaxRemainders[10]={0,0,0,0,0,0,0,0,0,0};
    int32 Wood[3]={12,12,12}, Owners[10]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, Costs[10]={12,9,6,6,6,6,6,6,6,6};
    int32 Produced[3]={0,0,0}, Spent[3]={0,0,0};
    int32 Manufactured[2]={0,0}, ManufacturedSpent[2]={0,0};
    FString PlotIds[10];
    FVector Plots[10], Stocks[3];
    TArray<FHearthSavedResident> People;
    TArray<FHearthSite> Sites;
    TMap<FString,int32> Totals;
    TArray<FHearthDecisionRecord> History;
    TArray<FHearthConversation> Conversations;
    TArray<FHearthCommitment> Commitments;
    TArray<FHearthTransaction> Transactions;
    TArray<FHearthTaxAssessment> TaxAssessments;
    TArray<FHearthWagePayable> WagePayables;
    TArray<FHearthTradeOffer> TradeOffers;
};
namespace HearthWorld
{
    FString Encode(const FHearthWorldImage& Image);
    bool Decode(const FString& Text,FHearthWorldImage& Out,FString& Error);
    bool Read(const FString& Path,FString& Payload,FString& Error);
    bool Write(const FString& Path,const FString& Payload,FString& Error);
    bool Archive(const FString& Path,FString& Error);
}
