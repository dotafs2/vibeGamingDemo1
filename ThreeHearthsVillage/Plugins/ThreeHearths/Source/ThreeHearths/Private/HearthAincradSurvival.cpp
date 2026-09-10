#include "HearthAincradSurvival.h"
#include "HearthAincradTownLayout.h"
#include "Dom/JsonObject.h"

namespace HearthAincradSurvival
{
namespace
{
constexpr int32 Schema = 1;
constexpr double Interval = 120.0;
constexpr double InitialFood = 2.0;
const TCHAR* const Source = TEXT("developer_survival_bootstrap");
const TCHAR* const InitialFact = TEXT("initial survival ration=2");

TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonValue>& V)
{ return V.IsValid() && V->Type == EJson::Object ? V->AsObject() : nullptr; }

const TArray<TSharedPtr<FJsonValue>>* Arr(const TSharedRef<FJsonObject>& O, const TCHAR* K)
{ const TArray<TSharedPtr<FJsonValue>>* A = nullptr; return O->TryGetArrayField(K, A) && A ? A : nullptr; }

bool Number(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double& Out)
{ return O.IsValid() && O->TryGetNumberField(K, Out) && FMath::IsFinite(Out); }

TSharedPtr<FJsonObject> Resident(const TSharedRef<FJsonObject>& W, const FString& Id)
{
    const auto* Residents = Arr(W, TEXT("residents")); if (!Residents) return nullptr;
    for (const auto& V : *Residents) { auto R = Obj(V); FString Candidate; if (R.IsValid() && R->TryGetStringField(TEXT("stable_id"), Candidate) && Candidate == Id) return R; }
    return nullptr;
}

TSharedPtr<FJsonObject> Runtime(const TSharedPtr<FJsonObject>& R)
{ return R.IsValid() && R->HasTypedField<EJson::Object>(TEXT("runtime")) ? R->GetObjectField(TEXT("runtime")) : nullptr; }

TSharedPtr<FJsonObject> Needs(const TSharedPtr<FJsonObject>& R)
{ return R.IsValid() && R->HasTypedField<EJson::Object>(TEXT("needs")) ? R->GetObjectField(TEXT("needs")) : nullptr; }

bool Active(const TSharedPtr<FJsonObject>& R)
{ bool b = false; auto Rt = Runtime(R); return Rt.IsValid() && Rt->TryGetBoolField(TEXT("active"), b) && b; }

TSharedPtr<FJsonObject> Account(const TSharedRef<FJsonObject>& S, const FString& Id)
{
    const auto* Accounts = Arr(S, TEXT("accounts")); if (!Accounts) return nullptr;
    for (const auto& V : *Accounts) { auto A = Obj(V); FString Candidate; if (A.IsValid() && A->TryGetStringField(TEXT("resident_id"), Candidate) && Candidate == Id) return A; }
    return nullptr;
}

bool Building(const TSharedRef<FJsonObject>& W, const FString& Id, FString& Out)
{
    Out.Empty(); const auto* Bindings = Arr(W, TEXT("building_bindings")); if (!Bindings) return false;
    for (const auto& V : *Bindings) { auto B = Obj(V); FString Candidate; if (B.IsValid() && B->TryGetStringField(TEXT("resident_id"), Candidate) && Candidate == Id) return B->TryGetStringField(TEXT("building_id"), Out) && !Out.IsEmpty(); }
    return false;
}

bool AtBuilding(const TSharedRef<FJsonObject>& W, const TSharedPtr<FJsonObject>& R, const FString& BuildingId)
{
    FString Id, Bound; if (!R.IsValid() || !R->TryGetStringField(TEXT("stable_id"), Id) || !Building(W, Id, Bound) || Bound != BuildingId) return false;
    const auto Plan = HearthAincradTownLayout::Build(); const auto* Site = HearthAincradTownLayout::Find(Plan, BuildingId); auto Rt = Runtime(R);
    const auto* P = Rt.IsValid() ? Arr(Rt.ToSharedRef(), TEXT("position_cm")) : nullptr;
    if (!Site || !P || P->Num() != 3) return false;
    double X = 0, Y = 0, Z = 0;
    if (!(*P)[0].IsValid() || !(*P)[1].IsValid() || !(*P)[2].IsValid() || !(*P)[0]->TryGetNumber(X) || !(*P)[1]->TryGetNumber(Y) || !(*P)[2]->TryGetNumber(Z)
        || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
    return FVector::Dist(FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)), Site->WorkCm) <= 250.0f;
}
}

bool Validate(const TSharedRef<FJsonObject>& W, FString& E)
{
    E.Empty(); if (!W->HasField(TEXT("survival"))) return true;
    if (!W->HasTypedField<EJson::Object>(TEXT("survival"))) { E = TEXT("survival must be an object"); return false; }
    auto S = W->GetObjectField(TEXT("survival")); double Version = 0, Remainder = 0;
    if (!Number(S, TEXT("schema_version"), Version) || Version != Schema) { E = TEXT("survival schema invalid"); return false; }
    if (!Number(S, TEXT("tick_remainder_seconds"), Remainder) || Remainder < 0 || Remainder >= Interval) { E = TEXT("survival remainder invalid"); return false; }
    const auto* Residents = Arr(W, TEXT("residents")); const auto* Accounts = Arr(S.ToSharedRef(), TEXT("accounts")); const auto* Conditions = Arr(S.ToSharedRef(), TEXT("initial_conditions"));
    if (!Residents || !Accounts || !Conditions || Accounts->Num() == 0 || Accounts->Num() != Conditions->Num()) { E = TEXT("survival account and initial condition arrays invalid"); return false; }
    TMap<FString, TSharedPtr<FJsonObject>> Known; TSet<FString> ActiveIds;
    for (const auto& V : *Residents) { auto R = Obj(V); FString Id; if (R.IsValid() && R->TryGetStringField(TEXT("stable_id"), Id) && !Id.IsEmpty()) { Known.Add(Id, R); if (Active(R)) ActiveIds.Add(Id); } }
    TSet<FString> AccountIds;
    for (const auto& V : *Accounts)
    {
        auto A = Obj(V); FString Id, AccountSource; double Food = 0, Energy = 0, Hunger = 0;
        if (!A.IsValid() || !A->TryGetStringField(TEXT("resident_id"), Id) || Id.IsEmpty() || Id.Contains(TEXT(":")) || !Known.Contains(Id) || AccountIds.Contains(Id)
            || !A->TryGetStringField(TEXT("source"), AccountSource) || AccountSource != Source
            || !Number(A, TEXT("food"), Food) || Food != FMath::FloorToDouble(Food) || Food < 0 || Food > InitialFood
            || !Number(A, TEXT("energy"), Energy) || Energy != FMath::FloorToDouble(Energy) || Energy < 0 || Energy > 100
            || !Number(Needs(Known[Id]), TEXT("hunger"), Hunger) || Hunger < 0 || Hunger > 100)
        { E = TEXT("survival account fields invalid"); return false; }
        AccountIds.Add(Id);
    }
    if (AccountIds.Num() != ActiveIds.Num()) { E = TEXT("survival accounts do not match active residents"); return false; }
    for (const FString& Id : ActiveIds) if (!AccountIds.Contains(Id)) { E = TEXT("survival account missing for active resident"); return false; }
    TSet<FString> ConditionIds;
    for (const auto& V : *Conditions)
    {
        auto C = Obj(V); FString Id, ConditionSource, Fact;
        if (!C.IsValid() || !C->TryGetStringField(TEXT("resident_id"), Id) || !AccountIds.Contains(Id) || ConditionIds.Contains(Id)
            || !C->TryGetStringField(TEXT("source"), ConditionSource) || ConditionSource != Source || !C->TryGetStringField(TEXT("fact"), Fact) || Fact != InitialFact)
        { E = TEXT("survival initial condition invalid"); return false; }
        ConditionIds.Add(Id);
    }
    return ConditionIds.Num() == AccountIds.Num() ? true : (E = TEXT("survival initial conditions do not match accounts"), false);
}

bool Initialize(const TSharedRef<FJsonObject>& W, bool& Added, FString& E)
{
    Added = false; E.Empty(); if (!Validate(W, E)) return false; if (W->HasField(TEXT("survival"))) return true;
    const auto* Residents = Arr(W, TEXT("residents")); if (!Residents) { E = TEXT("residents array missing"); return false; }
    TArray<TSharedPtr<FJsonValue>> Accounts, Conditions; TSet<FString> Ids;
    for (const auto& V : *Residents)
    {
        auto R = Obj(V); if (!Active(R)) continue; FString Id; double Hunger = 0;
        if (!R->TryGetStringField(TEXT("stable_id"), Id) || Id.IsEmpty() || Id.Contains(TEXT(":")) || Ids.Contains(Id) || !Number(Needs(R), TEXT("hunger"), Hunger) || Hunger < 0 || Hunger > 100)
        { E = TEXT("active resident cannot receive survival state"); return false; }
        Ids.Add(Id); auto A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("resident_id"), Id); A->SetNumberField(TEXT("food"), InitialFood); A->SetNumberField(TEXT("energy"), 100); A->SetStringField(TEXT("source"), Source); Accounts.Add(MakeShared<FJsonValueObject>(A));
        auto C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("resident_id"), Id); C->SetStringField(TEXT("fact"), InitialFact); C->SetStringField(TEXT("source"), Source); Conditions.Add(MakeShared<FJsonValueObject>(C));
    }
    if (Accounts.Num() == 0) { E = TEXT("no active residents available for survival initialization"); return false; }
    auto S = MakeShared<FJsonObject>(); S->SetNumberField(TEXT("schema_version"), Schema); S->SetNumberField(TEXT("tick_remainder_seconds"), 0); S->SetArrayField(TEXT("accounts"), Accounts); S->SetArrayField(TEXT("initial_conditions"), Conditions);
    W->SetObjectField(TEXT("survival"), S); if (!Validate(W, E)) { W->RemoveField(TEXT("survival")); return false; } Added = true; return true;
}

bool Tick(const TSharedRef<FJsonObject>& W, double Dt, FString& E)
{
    E.Empty(); if (!FMath::IsFinite(Dt) || Dt < 0) { E = TEXT("delta seconds invalid"); return false; } if (!Validate(W, E)) return false; if (!W->HasField(TEXT("survival"))) return true;
    auto S = W->GetObjectField(TEXT("survival")); const double Total = S->GetNumberField(TEXT("tick_remainder_seconds")) + Dt;
    if (!FMath::IsFinite(Total)) { E = TEXT("delta seconds overflows survival clock"); return false; }
    const double Steps = FMath::FloorToDouble(Total / Interval); double Remainder = FMath::Fmod(Total, Interval); if (Remainder < 0) Remainder = 0; const double Decrease = FMath::Min(100.0, Steps);
    S->SetNumberField(TEXT("tick_remainder_seconds"), Remainder); if (Decrease < 1) return true;
    for (const auto& V : *Arr(S.ToSharedRef(), TEXT("accounts"))) { auto A = V->AsObject(); auto R = Resident(W, A->GetStringField(TEXT("resident_id"))); if (!Active(R)) continue; auto N = Needs(R); A->SetNumberField(TEXT("energy"), FMath::Max(0.0, A->GetNumberField(TEXT("energy")) - Decrease)); N->SetNumberField(TEXT("hunger"), FMath::Max(0.0, N->GetNumberField(TEXT("hunger")) - Decrease)); }
    return true;
}

TArray<TSharedPtr<FJsonValue>> Options(const TSharedRef<FJsonObject>& W, const FString& Id)
{
    TArray<TSharedPtr<FJsonValue>> Out; FString E; if (!Validate(W, E) || !W->HasField(TEXT("survival"))) return Out;
    auto R = Resident(W, Id); auto S = W->GetObjectField(TEXT("survival")); auto A = Account(S.ToSharedRef(), Id); FString B;
    if (!Active(R) || !A.IsValid() || !Building(W, Id, B)) return Out; const double Food = A->GetNumberField(TEXT("food")), Energy = A->GetNumberField(TEXT("energy")), Hunger = Needs(R)->GetNumberField(TEXT("hunger"));
    auto Add = [&](const FString& OptionId, const FString& Verb, double Duration, const FString& Label, const FString& Effect) { auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"), OptionId); O->SetStringField(TEXT("verb"), Verb); O->SetStringField(TEXT("label"), Label); O->SetStringField(TEXT("effect"), Effect); O->SetStringField(TEXT("target_building_id"), B); O->SetNumberField(TEXT("duration_seconds"), Duration); Out.Add(MakeShared<FJsonValueObject>(O)); };
    if (Food >= 1 && Hunger <= 80) Add(TEXT("eat_ration:") + Id, TEXT("eat_ration"), 30, TEXT("吃一份口粮"), TEXT("消耗1 food，hunger +40，上限100"));
    if (Energy <= 70) Add(TEXT("rest:") + Id, TEXT("rest"), 60, TEXT("回工作点休息"), TEXT("energy +35，上限100")); return Out;
}

bool Apply(const TSharedRef<FJsonObject>& W, const FString& Id, const FString& Option, FString& E)
{
    E.Empty(); if (!Validate(W, E)) return false; if (!W->HasField(TEXT("survival"))) { E = TEXT("survival is not installed"); return false; }
    const FString Eat = TEXT("eat_ration:") + Id, Rest = TEXT("rest:") + Id; if (Id.IsEmpty() || (Option != Eat && Option != Rest)) { E = TEXT("survival option invalid"); return false; }
    auto R = Resident(W, Id); auto S = W->GetObjectField(TEXT("survival")); auto A = Account(S.ToSharedRef(), Id); FString B;
    if (!Active(R) || !A.IsValid() || !Building(W, Id, B) || !AtBuilding(W, R, B)) { E = TEXT("resident is not at registered station"); return false; }
    auto N = Needs(R); if (Option == Eat) { const double Food = A->GetNumberField(TEXT("food")), Hunger = N->GetNumberField(TEXT("hunger")); if (Food < 1 || Hunger > 80) { E = TEXT("ration unavailable"); return false; } A->SetNumberField(TEXT("food"), Food - 1); N->SetNumberField(TEXT("hunger"), FMath::Min(100.0, Hunger + 40)); return true; }
    const double Energy = A->GetNumberField(TEXT("energy")); if (Energy > 70) { E = TEXT("rest is not currently needed"); return false; } A->SetNumberField(TEXT("energy"), FMath::Min(100.0, Energy + 35)); return true;
}

TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& W, const FString& Id)
{
    auto Out = MakeShared<FJsonObject>(); FString E; if (!Validate(W, E) || !W->HasField(TEXT("survival"))) return Out;
    auto R = Resident(W, Id); auto S = W->GetObjectField(TEXT("survival")); auto A = Account(S.ToSharedRef(), Id); if (!R.IsValid() || !A.IsValid()) return Out;
    Out->SetNumberField(TEXT("schema_version"), Schema); Out->SetNumberField(TEXT("food"), A->GetNumberField(TEXT("food"))); Out->SetNumberField(TEXT("energy"), A->GetNumberField(TEXT("energy"))); Out->SetNumberField(TEXT("hunger_satisfaction"), Needs(R)->GetNumberField(TEXT("hunger"))); Out->SetStringField(TEXT("source"), TEXT("own_survival_account_and_needs")); Out->SetStringField(TEXT("rules"), TEXT("hunger is satisfaction: 100 full, 0 empty; energy is 0-100; live time only; eat at own work station uses 1 ration and adds 40 hunger; rest there adds 35 energy")); return Out;
}
}
