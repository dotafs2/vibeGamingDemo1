#include "HearthAincradForaging.h"
#include "HearthAincradSurvival.h"
#include "Dom/JsonObject.h"

namespace HearthAincradForaging
{
namespace
{
constexpr int32 Schema = 1;
constexpr double GrowthInterval = 1800.0;
constexpr int32 Capacity = 3;
const TCHAR* const SourceId = TEXT("starter_commons_berry_patch");
const TCHAR* const Source = TEXT("developer_ecosystem_bootstrap");

bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double& Out)
{
    return Object.IsValid() && Object->TryGetNumberField(Key, Out) && FMath::IsFinite(Out);
}

bool Integer(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Out, int64 Min, int64 Max)
{
    double Value = 0;
    if (!Number(Object, Key, Value) || Value != FMath::FloorToDouble(Value) || Value < Min || Value > Max) return false;
    Out = static_cast<int64>(Value);
    return true;
}

const TSharedPtr<FJsonObject> Resident(const TSharedRef<FJsonObject>& World, const FString& Id)
{
    const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
    if (!World->TryGetArrayField(TEXT("residents"), Residents)) return nullptr;
    for (const TSharedPtr<FJsonValue>& Value : *Residents)
    {
        const TSharedPtr<FJsonObject> Candidate = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        FString CandidateId;
        if (Candidate.IsValid() && Candidate->TryGetStringField(TEXT("stable_id"), CandidateId) && CandidateId == Id) return Candidate;
    }
    return nullptr;
}

bool Active(const TSharedPtr<FJsonObject>& ResidentObject)
{
    if (!ResidentObject.IsValid() || !ResidentObject->HasTypedField<EJson::Object>(TEXT("runtime"))) return false;
    bool Value = false;
    return ResidentObject->GetObjectField(TEXT("runtime"))->TryGetBoolField(TEXT("active"), Value) && Value;
}

bool SurvivalInstalled(const TSharedRef<FJsonObject>& World)
{
    return World->HasTypedField<EJson::Object>(TEXT("survival"));
}

TSharedPtr<FJsonObject> State(const TSharedRef<FJsonObject>& World)
{
    return World->HasTypedField<EJson::Object>(TEXT("foraging")) ? World->GetObjectField(TEXT("foraging")) : nullptr;
}

bool AtWorkPoint(const TSharedPtr<FJsonObject>& ResidentObject, double Radius)
{
    if (!ResidentObject.IsValid() || !ResidentObject->HasTypedField<EJson::Object>(TEXT("runtime"))) return false;
    const TSharedPtr<FJsonObject> Runtime = ResidentObject->GetObjectField(TEXT("runtime"));
    const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
    if (!Runtime->TryGetArrayField(TEXT("position_cm"), Position) || !Position || Position->Num() != 3) return false;
    double X = 0, Y = 0, Z = 0;
    if (!(*Position)[0].IsValid() || !(*Position)[1].IsValid() || !(*Position)[2].IsValid()
        || !(*Position)[0]->TryGetNumber(X) || !(*Position)[1]->TryGetNumber(Y) || !(*Position)[2]->TryGetNumber(Z)
        || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
    return FVector::Dist(FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)), WorkPoint()) <= Radius;
}
}

FVector WorkPoint()
{
    return FVector(0.0f, 465000.0f, 92.0f);
}

FVector VisiblePoint()
{
    return FVector(130.0f, 465000.0f, 0.0f);
}

bool Validate(const TSharedRef<FJsonObject>& World, FString& Error)
{
    Error.Empty();
    if (!World->HasField(TEXT("foraging"))) return true;
    if (!World->HasTypedField<EJson::Object>(TEXT("foraging"))) { Error = TEXT("foraging must be an object"); return false; }
    if (!World->HasTypedField<EJson::Object>(TEXT("survival"))) { Error = TEXT("survival must be installed first"); return false; }
    if (!HearthAincradSurvival::Validate(World, Error)) return false;
    if (!World->HasTypedField<EJson::Object>(TEXT("life"))) { Error = TEXT("life must be installed first"); return false; }
    const TSharedPtr<FJsonObject> Life = World->GetObjectField(TEXT("life"));
    int64 LifeSeq = 0;
    if (!Integer(Life, TEXT("seq"), LifeSeq, 0, 1000000000)) { Error = TEXT("life sequence invalid"); return false; }
    const TSharedPtr<FJsonObject> Foraging = State(World);
    double Remainder = 0;
    int64 SchemaValue = 0, Stock = 0, CapacityValue = 0, InitialStock = 0, Produced = 0, Harvested = 0, InstalledSeq = 0;
    FString ForagingSourceId, ForagingSource;
    if (!Integer(Foraging, TEXT("schema_version"), SchemaValue, Schema, Schema)
        || !Foraging->TryGetStringField(TEXT("source_id"), ForagingSourceId) || ForagingSourceId != SourceId
        || !Foraging->TryGetStringField(TEXT("source"), ForagingSource) || ForagingSource != Source
        || !Integer(Foraging, TEXT("stock"), Stock, 0, Capacity)
        || !Integer(Foraging, TEXT("capacity"), CapacityValue, Capacity, Capacity)
        || !Integer(Foraging, TEXT("initial_stock"), InitialStock, Capacity, Capacity)
        || !Integer(Foraging, TEXT("installed_at_life_seq"), InstalledSeq, 0, LifeSeq)
        || !Integer(Foraging, TEXT("produced_total"), Produced, 0, 1000000000)
        || !Integer(Foraging, TEXT("harvested_total"), Harvested, 0, 1000000000)
        || !Number(Foraging, TEXT("growth_remainder_seconds"), Remainder) || Remainder < 0 || Remainder >= GrowthInterval
        || Harvested > InitialStock + Produced || Stock != InitialStock + Produced - Harvested
        || (Stock == Capacity && Remainder != 0.0))
    { Error = TEXT("foraging state invalid"); return false; }
    return true;
}
bool Initialize(const TSharedRef<FJsonObject>& World, bool& bAdded, FString& Error)
{
    bAdded = false; Error.Empty();
    if (World->HasField(TEXT("foraging"))) return Validate(World, Error);
    if (!SurvivalInstalled(World) || !HearthAincradSurvival::Validate(World, Error)) { if (Error.IsEmpty()) Error = TEXT("survival must be installed first"); return false; }
    if (!World->HasTypedField<EJson::Object>(TEXT("life"))) { Error = TEXT("life must be installed first"); return false; }
    const TSharedPtr<FJsonObject> Life = World->GetObjectField(TEXT("life"));
    int64 LifeSeq = 0;
    if (!Integer(Life, TEXT("seq"), LifeSeq, 0, 1000000000)) { Error = TEXT("life sequence invalid"); return false; }
    auto Foraging = MakeShared<FJsonObject>();
    Foraging->SetNumberField(TEXT("schema_version"), Schema);
    Foraging->SetStringField(TEXT("source_id"), SourceId);
    Foraging->SetStringField(TEXT("source"), Source);
    Foraging->SetNumberField(TEXT("stock"), Capacity);
    Foraging->SetNumberField(TEXT("capacity"), Capacity);
    Foraging->SetNumberField(TEXT("initial_stock"), Capacity);
    Foraging->SetNumberField(TEXT("installed_at_life_seq"), LifeSeq);
    Foraging->SetNumberField(TEXT("produced_total"), 0);
    Foraging->SetNumberField(TEXT("harvested_total"), 0);
    Foraging->SetNumberField(TEXT("growth_remainder_seconds"), 0);
    World->SetObjectField(TEXT("foraging"), Foraging);
    if (!Validate(World, Error)) { World->RemoveField(TEXT("foraging")); return false; }
    bAdded = true;
    return true;
}

bool Tick(const TSharedRef<FJsonObject>& World, double DeltaSeconds, FString& Error)
{
    Error.Empty();
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0) { Error = TEXT("delta seconds invalid"); return false; }
    if (!Validate(World, Error)) return false;
    const TSharedPtr<FJsonObject> Foraging = State(World);
    if (!Foraging.IsValid()) return true;
    double Remainder = Foraging->GetNumberField(TEXT("growth_remainder_seconds"));
    const double Total = Remainder + DeltaSeconds;
    if (!FMath::IsFinite(Total)) { Error = TEXT("growth clock overflow"); return false; }
    int64 Stock = 0, Produced = 0;
    Integer(Foraging, TEXT("stock"), Stock, 0, Capacity);
    Integer(Foraging, TEXT("produced_total"), Produced, 0, 1000000000);
    const double StepsValue = FMath::FloorToDouble(Total / GrowthInterval);
    if (!FMath::IsFinite(StepsValue) || StepsValue > static_cast<double>(1000000000)) { Error = TEXT("growth step overflow"); return false; }
    const int64 Steps = static_cast<int64>(StepsValue);
    if (Stock < Capacity && Steps > 0)
    {
        const int64 Added = FMath::Min<int64>(Capacity - Stock, Steps);
        if (Produced > 1000000000 - Added) { Error = TEXT("growth counter overflow"); return false; }
        Foraging->SetNumberField(TEXT("stock"), Stock + Added);
        Foraging->SetNumberField(TEXT("produced_total"), Produced + Added);
        Remainder = (Stock + Added >= Capacity || Added < Steps) ? 0.0 : FMath::Fmod(Total, GrowthInterval);
    }
    else if (Stock >= Capacity)
    {
        Remainder = 0.0;
    }
    else
    {
        Remainder = Total;
    }
    Foraging->SetNumberField(TEXT("growth_remainder_seconds"), Remainder);
    return Validate(World, Error);
}

TArray<TSharedPtr<FJsonValue>> Options(const TSharedRef<FJsonObject>& World, const FString& ResidentId)
{
    TArray<TSharedPtr<FJsonValue>> Result; FString Error;
    if (!Validate(World, Error)) return Result;
    const TSharedPtr<FJsonObject> Foraging = State(World);
    const TSharedPtr<FJsonObject> ResidentObject = Resident(World, ResidentId);
    if (!Foraging.IsValid() || !ResidentObject.IsValid() || !Active(ResidentObject)) return Result;
    int64 Stock = 0, Food = 0; bool bFoundAccount = false;
    const TSharedPtr<FJsonObject> Survival = World->GetObjectField(TEXT("survival"));
    const TArray<TSharedPtr<FJsonValue>>* Accounts = nullptr;
    if (!Survival->TryGetArrayField(TEXT("accounts"), Accounts)) return Result;
    for (const TSharedPtr<FJsonValue>& Value : *Accounts)
    {
        const TSharedPtr<FJsonObject> Account = Value->AsObject();
        if (Account->GetStringField(TEXT("resident_id")) == ResidentId) bFoundAccount = Integer(Account, TEXT("food"), Food, 0, 2);
    }
    Integer(Foraging, TEXT("stock"), Stock, 0, Capacity);
    if (!bFoundAccount || Stock < 1 || Food > 1) return Result;
    auto Option = MakeShared<FJsonObject>();
    Option->SetStringField(TEXT("id"), FString(TEXT("harvest_ration:")) + SourceId);
    Option->SetStringField(TEXT("verb"), TEXT("harvest_ration"));
    Option->SetStringField(TEXT("label"), TEXT("前往公共浆果地采集1份口粮"));
    Option->SetNumberField(TEXT("duration_seconds"), 20);
    Option->SetStringField(TEXT("target_resource_id"), SourceId);
    Option->SetStringField(TEXT("effect"), TEXT("公共浆果地 stock -1，本人 food +1；不改变钱物身份"));
    Result.Add(MakeShared<FJsonValueObject>(Option));
    return Result;
}

bool Apply(const TSharedRef<FJsonObject>& World, const FString& ResidentId, const FString& OptionId, FString& Error)
{
    Error.Empty();
    if (!Validate(World, Error)) return false;
    const FString Expected = FString(TEXT("harvest_ration:")) + SourceId;
    if (OptionId != Expected) { Error = TEXT("foraging option invalid"); return false; }
    const TSharedPtr<FJsonObject> ResidentObject = Resident(World, ResidentId);
    const TSharedPtr<FJsonObject> Foraging = State(World);
    if (!ResidentObject.IsValid() || !Foraging.IsValid() || !Active(ResidentObject) || !AtWorkPoint(ResidentObject, 200.0)) { Error = TEXT("resident is not at the berry patch"); return false; }
    int64 Stock = 0, Harvested = 0, Food = 0;
    Integer(Foraging, TEXT("stock"), Stock, 0, Capacity);
    Integer(Foraging, TEXT("harvested_total"), Harvested, 0, 1000000000);
    const TSharedPtr<FJsonObject> Survival = World->GetObjectField(TEXT("survival"));
    const TArray<TSharedPtr<FJsonValue>>* Accounts = nullptr;
    Survival->TryGetArrayField(TEXT("accounts"), Accounts);
    TSharedPtr<FJsonObject> Account;
    for (const TSharedPtr<FJsonValue>& Value : *Accounts) if (Value->AsObject()->GetStringField(TEXT("resident_id")) == ResidentId) Account = Value->AsObject();
    if (!Account.IsValid() || !Integer(Account, TEXT("food"), Food, 0, 2) || Stock < 1 || Food >= 2 || Harvested == 1000000000) { Error = TEXT("ration unavailable"); return false; }
    Account->SetNumberField(TEXT("food"), Food + 1);
    Foraging->SetNumberField(TEXT("stock"), Stock - 1);
    Foraging->SetNumberField(TEXT("harvested_total"), Harvested + 1);
    return Validate(World, Error);
}

TSharedRef<FJsonObject> PersonalContext(const TSharedRef<FJsonObject>& World, const FString& ResidentId)
{
    auto Result = MakeShared<FJsonObject>(); FString Error;
    if (!Validate(World, Error) || !State(World).IsValid()) return Result;
    if (!Resident(World, ResidentId).IsValid() || !Active(Resident(World, ResidentId))) return Result;
    Result->SetNumberField(TEXT("schema_version"), Schema);
    Result->SetStringField(TEXT("source_id"), SourceId);
    Result->SetStringField(TEXT("source"), Source);
    Result->SetNumberField(TEXT("personal_food_capacity"), 2);
    Result->SetStringField(TEXT("eligibility"), TEXT("本人food最多2份，food为0或1且公共源有存量时才有采集选项；已满时不能再领取，选择由本人决定。"));
    Result->SetStringField(TEXT("rules"), TEXT("公共浆果地位于固定采集点；每1800秒实际运行时间最多恢复1份，容量3；前往采集可将1份公共stock转为本人food，库存实时值由世界侧另行核验"));
    Result->SetStringField(TEXT("work_point_cm"), TEXT("0,465000,92"));
    Result->SetStringField(TEXT("visible_point_cm"), TEXT("130,465000,0"));
    return Result;
}
}
