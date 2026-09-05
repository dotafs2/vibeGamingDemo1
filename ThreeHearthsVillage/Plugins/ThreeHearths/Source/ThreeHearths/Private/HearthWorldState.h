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
    int64 Revision=0;
    float Elapsed=0, Speed=1;
    double Remainder=0;
    bool bIsland=false, bPaused=false, bAutonomy=true, bComplete=false;
    int32 Selected=0, LastLife=-1, Food=30, Stone=0;
    int32 Wood[3]={12,12,12}, Owners[3]={-1,-1,-1}, Costs[3]={12,9,6};
    int32 Produced[3]={0,0,0}, Spent[3]={0,0,0};
    FString PlotIds[3];
    FVector Plots[3], Stocks[3];
    TArray<FHearthSavedResident> People;
    TArray<FHearthSite> Sites;
    TMap<FString,int32> Totals;
    TArray<FHearthDecisionRecord> History;
};
namespace HearthWorld
{
    FString Encode(const FHearthWorldImage& Image);
    bool Decode(const FString& Text,FHearthWorldImage& Out,FString& Error);
    bool Read(const FString& Path,FString& Payload,FString& Error);
    bool Write(const FString& Path,const FString& Payload,FString& Error);
    bool Archive(const FString& Path,FString& Error);
}
