#include "HearthVillage.h"

bool AHearthVillage::PrepareIncomeTax(int32 Resident,int32 Gross,const FString& SourceId,bool bIncomeRecorded,FHearthTaxAssessment& Out) const
{
    // This first policy is fixed at 25%. There is deliberately no rate-changing action.
    if(!Residents.IsValidIndex(Resident) || Resident>=10 || Gross<=0 || Gross>100000000 || TaxRatePercent!=25
        || TaxRemainders[Resident]<0 || TaxRemainders[Resident]>99 || TaxAssessments.Num()>=100000
        || TaxAssessments.ContainsByPredicate([&](const auto& A){ return A.SourceTransactionId==SourceId; })) return false;
    FGuid Id; if(!FGuid::Parse(SourceId,Id) || !Id.IsValid()) return false;
    const int64 Accrued=static_cast<int64>(Gross)*25+TaxRemainders[Resident];
    const int32 Tax=static_cast<int32>(Accrued/100);
    const int32 Required=(bIncomeRecorded?0:1)+(Tax>0?1:0);
    if(Transactions.Num()>100000-Required || TreasuryCoins<0 || TaxProjectCoins<0
        || static_cast<int64>(TreasuryCoins)+Tax>100000000 || static_cast<int64>(TaxProjectCoins)+Tax>100000000
        || static_cast<int64>(Residents[Resident].Coins)+(bIncomeRecorded?0:Gross)>100000000
        || (bIncomeRecorded && Residents[Resident].Coins<Tax)) return false;
    Out.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); Out.SourceTransactionId=SourceId;
    Out.Resident=Resident; Out.Gross=Gross; Out.Tax=Tax; Out.Net=Gross-Tax;
    Out.RemainderBefore=TaxRemainders[Resident]; Out.RemainderAfter=static_cast<int32>(Accrued%100); Out.At=Elapsed;
    return true;
}

void AHearthVillage::CommitIncomeTax(const FHearthTaxAssessment& A)
{
    Residents[A.Resident].Coins-=A.Tax; TreasuryCoins+=A.Tax; TaxProjectCoins+=A.Tax;
    TaxRemainders[A.Resident]=A.RemainderAfter; TaxAssessments.Add(A);
    if(A.Tax>0)
    {
        FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=TEXT("income_tax"); T.TaskId=A.SourceTransactionId;
        T.From=A.Resident; T.To=-1; T.Amount=A.Tax; T.Item=TEXT("income_tax"); T.Quantity=1; T.At=Elapsed; Transactions.Add(MoveTemp(T));
    }
}

bool AHearthVillage::AssessIncomeTax(int32 Resident,const FString& SourceId)
{
    const auto* Source=Transactions.FindByPredicate([&](const auto& T){ return T.Id==SourceId; });
    if(!Source || Source->To!=Resident || (Source->Kind!=TEXT("wage") && Source->Kind!=TEXT("plank_trade") && Source->Kind!=TEXT("public_purchase"))) return false;
    FHearthTaxAssessment A; if(!PrepareIncomeTax(Resident,Source->Amount,SourceId,true,A)) return false;
    CommitIncomeTax(A); return true;
}

bool AHearthVillage::TransferCoins(const FString& Kind,const FString& TaskId,int32 From,int32 To,int32 Amount,const FString& Item,int32 Quantity)
{
    FGuid Id;
    const bool Purchase=Kind==TEXT("food_purchase") && From>=0 && To==-1 && Item==TEXT("food") && Quantity==1 && Amount==1;
    const bool Trade=Kind==TEXT("plank_trade") && From>=0 && To>=0 && Item==TEXT("plank") && Quantity==1 && Amount==2;
    if(!FGuid::Parse(TaskId,Id) || !Id.IsValid() || From<-1 || To<-1 || From==To || From>=Residents.Num() || To>=Residents.Num()
        || (!Purchase && !Trade) || Transactions.Num()>=100000
        || Transactions.ContainsByPredicate([&](const auto& T){ return T.Kind==Kind && T.TaskId==TaskId; })) return false;
    if((From<0?GeneralFunds():Residents[From].Coins)<Amount || (To<0?TreasuryCoins:Residents[To].Coins)>100000000-Amount) return false;
    FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=Kind; T.TaskId=TaskId;
    T.From=From; T.To=To; T.Amount=Amount; T.Item=Item; T.Quantity=Quantity; T.At=Elapsed;
    FHearthTaxAssessment Tax;
    if(Trade && !PrepareIncomeTax(To,Amount,T.Id,false,Tax)) return false;
    // All validation precedes this ordered, non-failing game-thread commit.
    if(From<0) TreasuryCoins-=Amount; else Residents[From].Coins-=Amount;
    if(To<0) TreasuryCoins+=Amount; else Residents[To].Coins+=Amount;
    Transactions.Add(MoveTemp(T)); if(Trade) CommitIncomeTax(Tax); return true;
}

int32 AHearthVillage::WageForOperation(int32 Operation) const { return Operation>=13?3:2; }

bool AHearthVillage::ReserveWage(int32 Worker,const FString& TaskId,int32 Amount,bool bTaxFunded)
{
    FGuid Id;
    if(!Residents.IsValidIndex(Worker) || !FGuid::Parse(TaskId,Id) || !Id.IsValid() || (Amount!=2 && Amount!=3)
        || TreasuryCoins<Amount || (bTaxFunded?TaxProjectCoins:GeneralFunds())<Amount || WagePayables.Num()>=100000
        || WagePayables.ContainsByPredicate([&](const auto& P){ return P.TaskId==TaskId; })) return false;
    TreasuryCoins-=Amount; if(bTaxFunded) TaxProjectCoins-=Amount;
    FHearthWagePayable P; P.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    P.TaskId=TaskId; P.Worker=Worker; P.Amount=Amount; P.bTaxFunded=bTaxFunded; WagePayables.Add(MoveTemp(P)); return true;
}

bool AHearthVillage::CancelWage(const FString& TaskId)
{
    auto* P=WagePayables.FindByPredicate([&](const auto& X){ return X.TaskId==TaskId; });
    if(!P || P->Status==TEXT("paid") || P->Status==TEXT("cancelled")) return false;
    if(P->Status==TEXT("reserved")) { TreasuryCoins+=P->Amount; if(P->bTaxFunded) TaxProjectCoins+=P->Amount; }
    if(P->bTaxFunded) P->Status=TEXT("cancelled");
    else WagePayables.RemoveAll([&](const auto& X){ return X.TaskId==TaskId; });
    return true;
}

bool AHearthVillage::SettleWage(int32 Worker,const FString& TaskId)
{
    auto* P=WagePayables.FindByPredicate([&](const auto& X){ return X.TaskId==TaskId; });
    if(!P || P->Worker!=Worker || !Residents.IsValidIndex(Worker) || (P->Status!=TEXT("reserved") && P->Status!=TEXT("owed") && P->Status!=TEXT("unfunded"))
        || Transactions.ContainsByPredicate([&](const auto& T){ return T.Kind==TEXT("wage") && T.TaskId==TaskId; })) return false;
    const bool NeedsFunding=P->Status!=TEXT("reserved");
    if(NeedsFunding && (TreasuryCoins<P->Amount || (P->bTaxFunded?TaxProjectCoins:GeneralFunds())<P->Amount)) return false;
    FHearthTransaction T; T.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens); T.Kind=TEXT("wage"); T.TaskId=TaskId;
    T.From=-1; T.To=Worker; T.Amount=P->Amount; T.Item=TEXT("labor"); T.Quantity=1; T.At=Elapsed;
    FHearthTaxAssessment Tax; if(!PrepareIncomeTax(Worker,P->Amount,T.Id,false,Tax)) return false;
    if(NeedsFunding) { TreasuryCoins-=P->Amount; if(P->bTaxFunded) TaxProjectCoins-=P->Amount; }
    Residents[Worker].Coins+=P->Amount; P->Status=TEXT("paid");
    Transactions.Add(MoveTemp(T)); CommitIncomeTax(Tax); return true;
}
