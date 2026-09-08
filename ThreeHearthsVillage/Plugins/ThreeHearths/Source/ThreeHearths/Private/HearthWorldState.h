#pragma once
#include "HearthVillage.h"
#include "HearthStructurePlan.h"
#include "HearthTavernRuntime.h"
#include "HearthOrganicConstruction.h"

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
    FHearthWorldImage()
    {
        // FVector's default constructor does not initialize its coordinates.
        // Legacy saves omit entries, including the seven added by migration.
        for(FVector& Entry:PlotEntrances) Entry=FVector::ZeroVector;
    }
    FString Id, Run, Event;
    int32 Schema=11, PlotCount=3, PopulationCount=3;
    int64 Revision=0;
    float Elapsed=0, Speed=1;
    double Remainder=0;
    bool bOrganicTownLayout=false;
    int32 TownLayoutVersion=0;
    float PlotYaws[HearthVillageLimits::MaxPopulation]={0};
    FVector PlotEntrances[HearthVillageLimits::MaxPopulation];
    bool bIsland=false, bPaused=false, bAutonomy=true, bComplete=false;
    int32 Selected=0, LastLife=-1, Food=30, Stone=0, Planks=0, Beams=0, Clay=0, Tiles=0, TreasuryCoins=500, TaxProjectCoins=0, TaxReleasedCoins=0, TaxRatePercent=25;
    int32 TaxRemainders[HearthVillageLimits::MaxPopulation]={};
    int32 Wood[3]={12,12,12}, Owners[HearthVillageLimits::MaxPopulation]={}, Costs[HearthVillageLimits::MaxPopulation]={};
    int32 Produced[3]={0,0,0}, Spent[3]={0,0,0};
    int32 Manufactured[2]={0,0}, ManufacturedSpent[2]={0,0};
    int32 ProducedClay=0, SpentClay=0, ProducedTiles=0, SpentTiles=0;
    FString PlotIds[HearthVillageLimits::MaxPopulation];
    FVector Plots[HearthVillageLimits::MaxPopulation], Stocks[3];
    TArray<FHearthSavedResident> People;
    TArray<FHearthSite> Sites;
    TMap<FString,int32> Totals;
    TArray<FHearthDecisionRecord> History;
    TArray<FHearthConversation> Conversations;
    TArray<FHearthCommitment> Commitments;
    TArray<FHearthTransaction> Transactions;
    TArray<FHearthTaxAssessment> TaxAssessments;
    FHearthPublicProject PublicProject;
    TArray<FHearthWagePayable> WagePayables;
    TArray<FHearthServiceDutyRecord> ServiceDuties;
    TArray<FHearthFreightOrder> FreightOrders;
    TArray<FHearthTradeOffer> TradeOffers;
    TArray<FHearthTileOrder> TileOrders;
    TArray<FHearthStructurePlan> StructurePlans;
    TArray<FHearthWorldRequest> WorldRequests;
    FHearthTavernRuntimeState TavernRuntime;
    TMap<FString,FOrganicConstructionHomeState> OrganicHomes;
    int32 OrganicWorldSeed=7919;
};
namespace HearthWorld
{
    FString Encode(const FHearthWorldImage& Image);
    bool Decode(const FString& Text,FHearthWorldImage& Out,FString& Error);
    bool Read(const FString& Path,FString& Payload,FString& Error);
    bool Write(const FString& Path,const FString& Payload,FString& Error);
    bool Archive(const FString& Path,FString& Error);
}
