#include "HearthAincradWorldStore.h"
#include "HearthAincradLife.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <cstdio>
#endif

namespace HearthAincradWorldStore
{
    namespace
    {
        constexpr int64 MaxFileBytes = 1024 * 1024;
        constexpr int32 ResidentCount = 13;
        constexpr int32 MaxCapabilities = 32;
        constexpr int32 MaxKnownPlaces = 32;
        constexpr int32 MaxMemoryEntries = 32;
        constexpr int32 MaxMemoryEntryLength = 2000;
        constexpr int32 MaxKnownPlaceLength = 128;

        using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

        const TArray<FString>& ResidentRoles()
        {
            static const TArray<FString> Roles = { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("merchant"), TEXT("baker"), TEXT("herbalist"), TEXT("farmer"), TEXT("tailor"), TEXT("carpenter"), TEXT("porter"), TEXT("cook"), TEXT("stablehand"), TEXT("toolvendor"), TEXT("apprentice") };
            return Roles;
        }

        bool IsEnabledCapability(const FString& Value)
        {
            return Value == TEXT("map_blockout") || Value == TEXT("persistent_identity");
        }

        bool IsPendingCapability(const FString& Value)
        {
            return Value == TEXT("combat") || Value == TEXT("swordskill") || Value == TEXT("quests") || Value == TEXT("teleport") || Value == TEXT("shops_ai");
        }

        bool Guid(const FString& Value)
        {
            FGuid Parsed;
            return FGuid::Parse(Value, Parsed) && Parsed.IsValid();
        }

        bool RequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& OutValue, int32 MaxLength = 4096)
        {
            return Object.IsValid() && Object->TryGetStringField(Key, OutValue) && !OutValue.IsEmpty() && OutValue.Len() <= MaxLength;
        }

        bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double& OutValue, double Minimum, double Maximum)
        {
            return Object.IsValid() && Object->TryGetNumberField(Key, OutValue) && FMath::IsFinite(OutValue) && OutValue >= Minimum && OutValue <= Maximum;
        }

        bool IntegerNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& OutValue, int64 Minimum, int64 Maximum)
        {
            double Value = 0.0;
            if (!Number(Object, Key, Value, static_cast<double>(Minimum), static_cast<double>(Maximum)) || Value != FMath::FloorToDouble(Value))
            {
                return false;
            }
            OutValue = static_cast<int64>(Value);
            return true;
        }

        bool ExactStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const TArray<FString>& Expected)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || Values->Num() != Expected.Num())
            {
                return false;
            }
            for (int32 Index = 0; Index < Expected.Num(); ++Index)
            {
                FString Value;
                if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetString(Value) || Value != Expected[Index])
                {
                    return false;
                }
            }
            return true;
        }

        bool ValidateCapabilityArray(const TSharedPtr<FJsonObject>& Root, const TCHAR* Key, bool bEnabled, TSet<FString>& Seen, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Root->TryGetArrayField(Key, Values) || Values->Num() > MaxCapabilities)
            {
                Error = FString::Printf(TEXT("%s is missing or too large"), Key);
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString Capability;
                if (!Value.IsValid() || !Value->TryGetString(Capability) || Capability.IsEmpty() || Capability.Len() > 128 || Seen.Contains(Capability))
                {
                    Error = FString::Printf(TEXT("%s contains an invalid or duplicate capability"), Key);
                    return false;
                }
                if ((bEnabled && !IsEnabledCapability(Capability)) || (!bEnabled && !IsPendingCapability(Capability)))
                {
                    Error = FString::Printf(TEXT("%s contains an unsupported capability"), Key);
                    return false;
                }
                Seen.Add(Capability);
            }
            return true;
        }

        bool ValidateKnownPlaces(const TSharedPtr<FJsonObject>& Resident, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Resident->TryGetArrayField(TEXT("known_places"), Values) || Values->Num() > MaxKnownPlaces)
            {
                Error = TEXT("resident known_places is missing or too large");
                return false;
            }
            TSet<FString> Seen;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString Place;
                if (!Value.IsValid() || !Value->TryGetString(Place) || Place.IsEmpty() || Place.Len() > MaxKnownPlaceLength || Seen.Contains(Place))
                {
                    Error = TEXT("resident known_places contains an invalid or duplicate place");
                    return false;
                }
                Seen.Add(Place);
            }
            return true;
        }

        bool NumberArray3(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Object->TryGetArrayField(Key, Values) || Values->Num() != 3)
            {
                Error = FString::Printf(TEXT("%s must contain three numbers"), Key);
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                double NumberValue = 0.0;
                if (!Value.IsValid() || !Value->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue) || FMath::Abs(NumberValue) > 1000000000.0)
                {
                    Error = FString::Printf(TEXT("%s contains an invalid coordinate"), Key);
                    return false;
                }
            }
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> NumberArray(const double X, const double Y, const double Z)
        {
            return { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Z) };
        }

        bool IsActiveRole(const FString& Role)
        {
            return Role == TEXT("innkeeper") || Role == TEXT("blacksmith") || Role == TEXT("carpenter");
        }

        FString ExpectedBuildingId(const FString& Role)
        {
            if (Role == TEXT("innkeeper")) return TEXT("sao_inn_01");
            if (Role == TEXT("blacksmith")) return TEXT("sao_smithy_01");
            if (Role == TEXT("carpenter")) return TEXT("sao_carpentry_01");
            return FString();
        }

        bool ExpectedSpawnPosition(const FString& Role, double& X, double& Y, double& Z)
        {
            Z = 92.0;
            if (Role == TEXT("innkeeper")) { X = -400.0; Y = 468700.0; return true; }
            if (Role == TEXT("blacksmith")) { X = 400.0; Y = 466700.0; return true; }
            if (Role == TEXT("carpenter")) { X = -400.0; Y = 463700.0; return true; }
            X = 0.0; Y = 0.0; Z = 0.0;
            return false;
        }

        bool ValidateRuntime(const TSharedPtr<FJsonObject>& Resident, const FString& Role, FString& Error)
        {
            const TSharedPtr<FJsonObject> Runtime = Resident->HasTypedField<EJson::Object>(TEXT("runtime")) ? Resident->GetObjectField(TEXT("runtime")) : nullptr;
            if (!Runtime.IsValid()) return false;

            bool Active = false;
            if (!Runtime->TryGetBoolField(TEXT("active"), Active) || Active != IsActiveRole(Role))
            {
                Error = TEXT("resident runtime active flag is invalid");
                return false;
            }
            FString BuildingId;
            if (!Runtime->TryGetStringField(TEXT("building_id"), BuildingId) || BuildingId.Len() > 128 || BuildingId != ExpectedBuildingId(Role))
            {
                Error = TEXT("resident runtime building_id is invalid");
                return false;
            }
            if (!NumberArray3(Runtime, TEXT("position_cm"), Error)) return false;

            FString Phase;
            if (!Runtime->TryGetStringField(TEXT("phase"), Phase) || !(Phase == TEXT("idle") || Phase == TEXT("moving") || Phase == TEXT("observing") || Phase == TEXT("awaiting_decision") || Phase == TEXT("blocked")))
            {
                Error = TEXT("resident runtime phase is invalid");
                return false;
            }
            int64 ObservationSeq = 0;
            if (!IntegerNumber(Runtime, TEXT("observation_seq"), ObservationSeq, 0, TNumericLimits<int64>::Max()))
            {
                Error = TEXT("resident runtime observation_seq is invalid");
                return false;
            }
            double LastThinkUtc = 0.0;
            if (!Number(Runtime, TEXT("last_think_utc"), LastThinkUtc, 0.0, TNumericLimits<double>::Max()))
            {
                Error = TEXT("resident runtime last_think_utc is invalid");
                return false;
            }
            FString PendingOperation;
            if (!Runtime->TryGetStringField(TEXT("pending_operation"), PendingOperation) || PendingOperation.Len() > 256)
            {
                Error = TEXT("resident runtime pending_operation is invalid");
                return false;
            }
            const TArray<TSharedPtr<FJsonValue>>* Memory = nullptr;
            if (!Runtime->TryGetArrayField(TEXT("memory"), Memory) || Memory->Num() > MaxMemoryEntries)
            {
                Error = TEXT("resident runtime memory is missing or too large");
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Entry : *Memory)
            {
                FString MemoryText;
                if (!Entry.IsValid() || !Entry->TryGetString(MemoryText) || MemoryText.Len() > MaxMemoryEntryLength)
                {
                    Error = TEXT("resident runtime memory contains an invalid entry");
                    return false;
                }
            }
            return true;
        }

        bool ValidateResidents(const TSharedPtr<FJsonObject>& Root, bool bV2, TSet<FString>& ResidentIds, FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
            if (!Root->TryGetArrayField(TEXT("residents"), Residents) || Residents->Num() != ResidentCount)
            {
                Error = TEXT("residents must contain exactly 13 entries");
                return false;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Residents)
            {
                const TSharedPtr<FJsonObject> Resident = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
                FString Id, Name, Personality, Story, Role, Faction, HomeId;
                if (!RequiredString(Resident, TEXT("stable_id"), Id, 64) || !Guid(Id) || ResidentIds.Contains(Id)) { Error = TEXT("resident stable_id must be a unique GUID"); return false; }
                if (!RequiredString(Resident, TEXT("name"), Name, 128) || !RequiredString(Resident, TEXT("personality"), Personality, 512) || !RequiredString(Resident, TEXT("story"), Story, 200)) { Error = TEXT("resident identity/story is invalid"); return false; }
                if (!Resident->TryGetStringField(TEXT("role"), Role) || !ResidentRoles().Contains(Role)) { Error = TEXT("resident role is invalid"); return false; }
                if (!Resident->TryGetStringField(TEXT("faction"), Faction) || Faction != TEXT("npc_local")) { Error = TEXT("resident faction is invalid"); return false; }
                if (bV2)
                {
                    if (!ValidateKnownPlaces(Resident, Error)) return false;
                }
                else if (!ExactStringArray(Resident, TEXT("known_places"), { TEXT("town_of_beginnings") }))
                {
                    Error = TEXT("resident known_places is not the initial profile");
                    return false;
                }
                if (!RequiredString(Resident, TEXT("home_id"), HomeId, 128)) { Error = TEXT("resident home_id is invalid"); return false; }
                double Hp = 0.0, Coins = 0.0;
                if (!Number(Resident, TEXT("hp"), Hp, 0.0, 100.0) || (!bV2 && Hp != 100.0)) { Error = bV2 ? TEXT("resident hp is invalid") : TEXT("resident hp must be 100"); return false; }
                if (!Number(Resident, TEXT("coins_col"), Coins, 0.0, 1000000.0) || Coins != FMath::FloorToDouble(Coins)) { Error = TEXT("resident coins_col is invalid"); return false; }
                const TSharedPtr<FJsonObject> Needs = Resident->HasTypedField<EJson::Object>(TEXT("needs")) ? Resident->GetObjectField(TEXT("needs")) : nullptr;
                double Hunger = 0.0, Warmth = 0.0, Safety = 0.0;
                if (!Number(Needs, TEXT("hunger"), Hunger, 0.0, 100.0) || !Number(Needs, TEXT("warmth"), Warmth, 0.0, 100.0) || !Number(Needs, TEXT("safety"), Safety, 0.0, 100.0)) { Error = TEXT("resident needs are invalid"); return false; }
                if (bV2 && !ValidateRuntime(Resident, Role, Error)) { if (Error.IsEmpty()) Error = TEXT("resident runtime is invalid"); return false; }
                ResidentIds.Add(Id);
            }
            return true;
        }

        bool ValidateV1(const TSharedPtr<FJsonObject>& Root, FString& Error)
        {
            auto Fail = [&Error](const TCHAR* Message)
            {
                Error = Message;
                return false;
            };
            FString WorldId, SettingId, ProjectCodename;
            double SchemaVersion = 0.0, LayoutRevision = 0.0, ElapsedSeconds = 0.0;
            if (!RequiredString(Root, TEXT("world_id"), WorldId, 64) || !Guid(WorldId)) return Fail(TEXT("invalid world_id"));
            if (!Number(Root, TEXT("schema_version"), SchemaVersion, 1.0, 1.0) || SchemaVersion != 1.0) return Fail(TEXT("schema_version must be 1"));
            if (!Root->TryGetStringField(TEXT("setting_id"), SettingId) || SettingId != TEXT("sao_aincrad_floor_1")) return Fail(TEXT("wrong setting_id"));
            if (!Root->TryGetStringField(TEXT("project_codename"), ProjectCodename) || ProjectCodename != TEXT("Level0")) return Fail(TEXT("wrong project_codename"));
            if (!Number(Root, TEXT("layout_revision"), LayoutRevision, 1.0, 1.0) || LayoutRevision != 1.0) return Fail(TEXT("layout_revision must be 1"));
            if (!Number(Root, TEXT("elapsed_seconds"), ElapsedSeconds, 0.0, TNumericLimits<double>::Max())) return Fail(TEXT("elapsed_seconds must be finite and non-negative"));

            if (!ExactStringArray(Root, TEXT("capabilities_enabled"), { TEXT("map_blockout"), TEXT("persistent_identity") })) return Fail(TEXT("capabilities_enabled is not the initial profile"));
            if (!ExactStringArray(Root, TEXT("capabilities_pending"), { TEXT("combat"), TEXT("swordskill"), TEXT("quests"), TEXT("teleport"), TEXT("shops_ai") })) return Fail(TEXT("capabilities_pending is not the initial profile"));
            TSet<FString> Ids;
            if (!ValidateResidents(Root, false, Ids, Error)) return false;
            Error.Empty();
            return true;
        }

        bool ValidateV2(const TSharedPtr<FJsonObject>& Root, FString& Error)
        {
            FString WorldId, SettingId, ProjectCodename;
            double SchemaVersion = 0.0, LayoutRevision = 0.0, TownLayoutRevision = 0.0, ElapsedSeconds = 0.0;
            if (!RequiredString(Root, TEXT("world_id"), WorldId, 64) || !Guid(WorldId)) { Error = TEXT("invalid world_id"); return false; }
            if (!Number(Root, TEXT("schema_version"), SchemaVersion, 2.0, 2.0) || SchemaVersion != 2.0) { Error = TEXT("schema_version must be 2"); return false; }
            if (!Root->TryGetStringField(TEXT("setting_id"), SettingId) || SettingId != TEXT("sao_aincrad_floor_1")) { Error = TEXT("wrong setting_id"); return false; }
            if (!Root->TryGetStringField(TEXT("project_codename"), ProjectCodename) || ProjectCodename != TEXT("Level0")) { Error = TEXT("wrong project_codename"); return false; }
            if (!Number(Root, TEXT("layout_revision"), LayoutRevision, 1.0, 1.0) || LayoutRevision != 1.0) { Error = TEXT("layout_revision must be 1"); return false; }
            if (!Number(Root, TEXT("town_layout_revision"), TownLayoutRevision, 1.0, 1.0) || TownLayoutRevision != 1.0) { Error = TEXT("town_layout_revision must be 1"); return false; }
            if (!Number(Root, TEXT("elapsed_seconds"), ElapsedSeconds, 0.0, TNumericLimits<double>::Max())) { Error = TEXT("elapsed_seconds must be finite and non-negative"); return false; }

            TSet<FString> Capabilities;
            if (!ValidateCapabilityArray(Root, TEXT("capabilities_enabled"), true, Capabilities, Error) || !ValidateCapabilityArray(Root, TEXT("capabilities_pending"), false, Capabilities, Error)) return false;

            TSet<FString> ResidentIds;
            if (!ValidateResidents(Root, true, ResidentIds, Error)) return false;

            const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
            if (!Root->TryGetArrayField(TEXT("buildings"), Buildings) || Buildings->Num() != 3) { Error = TEXT("buildings must contain the three Level0 bindings"); return false; }
            const TArray<FString> BuildingIds = { TEXT("sao_inn_01"), TEXT("sao_smithy_01"), TEXT("sao_carpentry_01") };
            const TArray<FString> BuildingRoles = { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("carpenter") };
            const TArray<double> BuildingX = { -1800.0, 1800.0, -1800.0 };
            const TArray<double> BuildingY = { 467000.0, 465000.0, 462000.0 };
            TSet<FString> BuildingSet;
            TSet<FString> OwnerSet;
            TMap<FString, FString> BuildingOwners;
            for (int32 Index = 0; Index < Buildings->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Building = (*Buildings)[Index].IsValid() && (*Buildings)[Index]->Type == EJson::Object ? (*Buildings)[Index]->AsObject() : nullptr;
                FString BuildingId, Role, OwnerId;
                if (!RequiredString(Building, TEXT("building_id"), BuildingId, 128) || BuildingId != BuildingIds[Index] || BuildingSet.Contains(BuildingId)) { Error = TEXT("building_id is invalid"); return false; }
                if (!Building->TryGetStringField(TEXT("role"), Role) || Role != BuildingRoles[Index]) { Error = TEXT("building role is invalid"); return false; }
                if (!RequiredString(Building, TEXT("owner_id"), OwnerId, 64) || !ResidentIds.Contains(OwnerId) || OwnerSet.Contains(OwnerId)) { Error = TEXT("building owner_id is invalid"); return false; }
                bool RoleMatchesOwner = false;
                const TArray<TSharedPtr<FJsonValue>>* ResidentsForBuilding = nullptr;
                Root->TryGetArrayField(TEXT("residents"), ResidentsForBuilding);
                for (const TSharedPtr<FJsonValue>& ResidentValue : *ResidentsForBuilding)
                {
                    const TSharedPtr<FJsonObject> Resident = ResidentValue->AsObject();
                    if (Resident->GetStringField(TEXT("stable_id")) == OwnerId) { RoleMatchesOwner = Resident->GetStringField(TEXT("role")) == Role; break; }
                }
                if (!RoleMatchesOwner) { Error = TEXT("building owner role does not match resident"); return false; }
                if (!NumberArray3(Building, TEXT("position_cm"), Error)) return false;
                const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
                Building->TryGetArrayField(TEXT("position_cm"), Position);
                double X = 0.0, Y = 0.0, Z = 0.0;
                (*Position)[0]->TryGetNumber(X); (*Position)[1]->TryGetNumber(Y); (*Position)[2]->TryGetNumber(Z);
                if (X != BuildingX[Index] || Y != BuildingY[Index] || Z != 0.0) { Error = TEXT("building position does not match the fixed Level0 layout"); return false; }
                BuildingSet.Add(BuildingId); OwnerSet.Add(OwnerId); BuildingOwners.Add(BuildingId, OwnerId);
            }

            const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
            if (!Root->TryGetArrayField(TEXT("building_bindings"), Bindings) || Bindings->Num() != 3) { Error = TEXT("building_bindings must contain three entries"); return false; }
            TSet<FString> BoundBuildings;
            TSet<FString> BoundResidents;
            for (const TSharedPtr<FJsonValue>& Value : *Bindings)
            {
                const TSharedPtr<FJsonObject> Binding = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
                FString BuildingId, ResidentId, HomeId;
                if (!RequiredString(Binding, TEXT("building_id"), BuildingId, 128) || !BuildingSet.Contains(BuildingId) || BoundBuildings.Contains(BuildingId)) { Error = TEXT("building binding building_id is invalid"); return false; }
                if (!RequiredString(Binding, TEXT("resident_id"), ResidentId, 64) || !OwnerSet.Contains(ResidentId) || BoundResidents.Contains(ResidentId)) { Error = TEXT("building binding resident_id is invalid"); return false; }
                if (!BuildingOwners.Contains(BuildingId) || BuildingOwners[BuildingId] != ResidentId) { Error = TEXT("building binding resident_id does not match building owner"); return false; }
                if (!RequiredString(Binding, TEXT("home_id"), HomeId, 128)) { Error = TEXT("building binding home_id is invalid"); return false; }
                bool FoundMatchingResident = false;
                const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
                Root->TryGetArrayField(TEXT("residents"), Residents);
                for (const TSharedPtr<FJsonValue>& ResidentValue : *Residents)
                {
                    const TSharedPtr<FJsonObject> Resident = ResidentValue->AsObject();
                    if (Resident->GetStringField(TEXT("stable_id")) == ResidentId) { FoundMatchingResident = Resident->GetStringField(TEXT("home_id")) == HomeId; break; }
                }
                if (!FoundMatchingResident) { Error = TEXT("building binding home_id does not match resident"); return false; }
                BoundBuildings.Add(BuildingId); BoundResidents.Add(ResidentId);
            }
            if (!HearthAincradLife::Validate(Root.ToSharedRef(), Error)) return false;
            Error.Empty();
            return true;
        }

        bool ReadSchemaVersion(const TSharedPtr<FJsonObject>& Root, int32& OutVersion, FString& Error)
        {
            double Version = 0.0;
            if (!Root.IsValid() || !Root->TryGetNumberField(TEXT("schema_version"), Version) || !FMath::IsFinite(Version) || Version != FMath::FloorToDouble(Version) || Version < 1.0 || Version > TNumericLimits<int32>::Max()) { Error = TEXT("schema_version is missing or invalid"); return false; }
            OutVersion = static_cast<int32>(Version);
            if (OutVersion != 1 && OutVersion != 2) { Error = TEXT("unsupported future schema_version"); return false; }
            return true;
        }

        TSharedPtr<FJsonObject> NewObject()
        {
            auto Root = MakeShared<FJsonObject>();
            Root->SetStringField(TEXT("world_id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
            Root->SetNumberField(TEXT("schema_version"), 2);
            Root->SetStringField(TEXT("setting_id"), TEXT("sao_aincrad_floor_1"));
            Root->SetStringField(TEXT("project_codename"), TEXT("Level0"));
            Root->SetNumberField(TEXT("layout_revision"), 1);
            Root->SetNumberField(TEXT("town_layout_revision"), 1);
            Root->SetNumberField(TEXT("elapsed_seconds"), 0);
            Root->SetArrayField(TEXT("capabilities_enabled"), { MakeShared<FJsonValueString>(TEXT("map_blockout")), MakeShared<FJsonValueString>(TEXT("persistent_identity")) });
            Root->SetArrayField(TEXT("capabilities_pending"), { MakeShared<FJsonValueString>(TEXT("combat")), MakeShared<FJsonValueString>(TEXT("swordskill")), MakeShared<FJsonValueString>(TEXT("quests")), MakeShared<FJsonValueString>(TEXT("teleport")), MakeShared<FJsonValueString>(TEXT("shops_ai")) });

            const TArray<FString> Names = { TEXT("艾琳"), TEXT("拓真"), TEXT("米娅"), TEXT("洛恩"), TEXT("莎耶"), TEXT("朔"), TEXT("宁音"), TEXT("柏木"), TEXT("阿律"), TEXT("绫"), TEXT("冬马"), TEXT("澪"), TEXT("小岚") };
            const TArray<FString> Roles = { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("merchant"), TEXT("baker"), TEXT("herbalist"), TEXT("farmer"), TEXT("tailor"), TEXT("carpenter"), TEXT("porter"), TEXT("cook"), TEXT("stablehand"), TEXT("toolvendor"), TEXT("apprentice") };
            const TArray<FString> Personalities = { TEXT("谨慎而善于倾听"), TEXT("沉静，做事精确"), TEXT("健谈但会记账"), TEXT("耐心，珍惜新鲜食材"), TEXT("温和，观察细致"), TEXT("务实，愿意等季节"), TEXT("爱整洁，重视体面"), TEXT("慢热，喜欢解决结构问题"), TEXT("可靠，先确认路线"), TEXT("爽朗，怕浪费"), TEXT("稳重，和动物相处自然"), TEXT("好奇，喜欢拆解工具"), TEXT("勤奋但缺少经验") };
            const TArray<FString> Stories = { TEXT("我想让旅店成为大家能安心交换消息的地方，但木料和客房都很有限。"), TEXT("我想修好镇上每一把常用的剑，却只能先用手边的普通铁料。"), TEXT("我想让货物在镇里流转得更公平，但我还不知道远处会带来什么。"), TEXT("我想每天都烤出不让人挨饿的面包，但炉火和面粉必须精打细算。"), TEXT("我想种出能缓解小伤的草药，但药圃的土还没有准备好。"), TEXT("我想收成稳定一些，让邻居冬天也有粮，可我手上的种子不多。"), TEXT("我想为镇民做出耐穿的衣服，但布料和时间总是不够。"), TEXT("我想把新住处修得结实，让人敢于留下，可木材需要慢慢积攒。"), TEXT("我想让搬运少走弯路，使每个人都能早点回家，但道路还很陌生。"), TEXT("我想让忙碌的人吃上一顿热饭，可厨房的锅具和柴火有限。"), TEXT("我想照料好镇里的坐骑，让出行更可靠，但马厩还需要整理。"), TEXT("我想做出不容易坏的工具，可我还得先学会辨认合适的材料。"), TEXT("我想成为一名真正有用的工匠，眼下只能从记录库存和跑腿开始。") };
            FJsonArray Values;
            for (int32 Index = 0; Index < ResidentCount; ++Index)
            {
                auto Resident = MakeShared<FJsonObject>();
                Resident->SetStringField(TEXT("stable_id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
                Resident->SetStringField(TEXT("name"), Names[Index]); Resident->SetStringField(TEXT("personality"), Personalities[Index]); Resident->SetStringField(TEXT("story"), Stories[Index]); Resident->SetStringField(TEXT("role"), Roles[Index]); Resident->SetStringField(TEXT("faction"), TEXT("npc_local"));
                Resident->SetArrayField(TEXT("known_places"), { MakeShared<FJsonValueString>(TEXT("town_of_beginnings")) }); Resident->SetStringField(TEXT("home_id"), FString::Printf(TEXT("home_%02d"), Index + 1));
                Resident->SetNumberField(TEXT("hp"), 100); Resident->SetNumberField(TEXT("coins_col"), (Index == 1 || Index == 2 || Index == 11) ? 200 : 100);
                auto Needs = MakeShared<FJsonObject>(); Needs->SetNumberField(TEXT("hunger"), 100); Needs->SetNumberField(TEXT("warmth"), 100); Needs->SetNumberField(TEXT("safety"), 100); Resident->SetObjectField(TEXT("needs"), Needs);
                auto Runtime = MakeShared<FJsonObject>();
                const bool bActive = IsActiveRole(Roles[Index]);
                Runtime->SetBoolField(TEXT("active"), bActive);
                Runtime->SetStringField(TEXT("building_id"), ExpectedBuildingId(Roles[Index]));
                double SpawnX = 0.0, SpawnY = 0.0, SpawnZ = 0.0;
                ExpectedSpawnPosition(Roles[Index], SpawnX, SpawnY, SpawnZ);
                Runtime->SetArrayField(TEXT("position_cm"), NumberArray(SpawnX, SpawnY, SpawnZ));
                FJsonArray EmptyMemory;
                Runtime->SetStringField(TEXT("phase"), TEXT("idle")); Runtime->SetNumberField(TEXT("observation_seq"), 0); Runtime->SetNumberField(TEXT("last_think_utc"), 0); Runtime->SetStringField(TEXT("pending_operation"), TEXT("")); Runtime->SetArrayField(TEXT("memory"), EmptyMemory);
                Resident->SetObjectField(TEXT("runtime"), Runtime);
                Values.Add(MakeShared<FJsonValueObject>(Resident));
            }
            Root->SetArrayField(TEXT("residents"), Values);

            const TArray<FString> BuildingIds = { TEXT("sao_inn_01"), TEXT("sao_smithy_01"), TEXT("sao_carpentry_01") };
            const TArray<FString> BuildingRoles = { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("carpenter") };
            const TArray<double> BuildingX = { -1800.0, 1800.0, -1800.0 };
            const TArray<double> BuildingY = { 467000.0, 465000.0, 462000.0 };
            FJsonArray Buildings, Bindings;
            for (int32 Index = 0; Index < BuildingIds.Num(); ++Index)
            {
                const int32 ResidentIndex = Index == 2 ? 7 : Index;
                const TSharedPtr<FJsonObject> Resident = Values[ResidentIndex]->AsObject();
                const FString ResidentId = Resident->GetStringField(TEXT("stable_id"));
                const FString HomeId = Resident->GetStringField(TEXT("home_id"));
                auto Building = MakeShared<FJsonObject>(); Building->SetStringField(TEXT("building_id"), BuildingIds[Index]); Building->SetStringField(TEXT("role"), BuildingRoles[Index]); Building->SetStringField(TEXT("owner_id"), ResidentId); Building->SetArrayField(TEXT("position_cm"), NumberArray(BuildingX[Index], BuildingY[Index], 0)); Buildings.Add(MakeShared<FJsonValueObject>(Building));
                auto Binding = MakeShared<FJsonObject>(); Binding->SetStringField(TEXT("building_id"), BuildingIds[Index]); Binding->SetStringField(TEXT("resident_id"), ResidentId); Binding->SetStringField(TEXT("home_id"), HomeId); Bindings.Add(MakeShared<FJsonValueObject>(Binding));
            }
            Root->SetArrayField(TEXT("buildings"), Buildings); Root->SetArrayField(TEXT("building_bindings"), Bindings);
            return Root;
        }

        bool ParseText(const FString& Text, TSharedPtr<FJsonObject>& Out, FString& Error)
        {
            TSharedPtr<FJsonObject> Candidate;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Candidate) || !Candidate.IsValid()) { Error = TEXT("invalid JSON"); return false; }
            int32 Version = 0;
            if (!ReadSchemaVersion(Candidate, Version, Error)) return false;
            if (Version == 1 ? !ValidateV1(Candidate, Error) : !ValidateV2(Candidate, Error)) return false;
            Out = MoveTemp(Candidate); return true;
        }

        bool AtomicUtf8(const FString& Path, const FString& Text, FString& Error);

        bool BackupOnce(const FString& Path, const FString& Text, FString& Error)
        {
            IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
            return Files.FileExists(*Path) || AtomicUtf8(Path, Text, Error);
        }

        bool AtomicUtf8(const FString& Path, const FString& Text, FString& Error)
        {
            IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
            Files.CreateDirectoryTree(*FPaths::GetPath(Path));
            const FString Temp = Path + TEXT(".tmp-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
            bool Written = false;
            { TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Temp)); FTCHARToUTF8 Bytes(*Text); Written = Handle.IsValid() && Handle->Write(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length()) && Handle->Flush(true); }
            if (Written)
            {
#if PLATFORM_WINDOWS
                Written = !!MoveFileExW(*FPaths::ConvertRelativePathToFull(Temp), *FPaths::ConvertRelativePathToFull(Path), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
                Written = std::rename(TCHAR_TO_UTF8(*Temp), TCHAR_TO_UTF8(*Path)) == 0;
#endif
            }
            if (!Written) { Files.DeleteFile(*Temp); Error = TEXT("atomic UTF-8 replacement failed"); }
            return Written;
        }

        FString Serialize(const TSharedRef<FJsonObject>& State)
        {
            FString Text;
            FJsonSerializer::Serialize(State, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
            return Text;
        }
    }

    bool LoadOrCreate(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error)
    {
        Out.Reset(); Error.Empty();
        if (Path.IsEmpty()) { Error = TEXT("path is empty"); return false; }
        IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
        if (!Files.FileExists(*Path))
        {
            TSharedPtr<FJsonObject> Initial = NewObject();
            if (!Save(Path, Initial.ToSharedRef(), Error)) return false;
            Out = MoveTemp(Initial); return true;
        }
        if (Files.FileSize(*Path) < 1 || Files.FileSize(*Path) > MaxFileBytes) { Error = TEXT("existing file size is outside the 1 MiB limit"); return false; }
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) { Error = TEXT("existing file could not be read"); return false; }
        TSharedPtr<FJsonObject> Existing;
        if (!ParseText(Text, Existing, Error)) return false;
        int32 Version = 0;
        if (!ReadSchemaVersion(Existing, Version, Error)) return false;
        if (Version == 1) return MigrateV1ToV2(Path, Out, Error);
        Out = MoveTemp(Existing);
        return true;
    }

    bool MigrateV1ToV2(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& Error)
    {
        Out.Reset(); Error.Empty();
        if (Path.IsEmpty()) { Error = TEXT("path is empty"); return false; }
        IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
        if (!Files.FileExists(*Path) || Files.FileSize(*Path) < 1 || Files.FileSize(*Path) > MaxFileBytes) { Error = TEXT("migration source is missing or outside the 1 MiB limit"); return false; }
        FString ExistingText;
        if (!FFileHelper::LoadFileToString(ExistingText, *Path)) { Error = TEXT("migration source could not be read"); return false; }
        TSharedPtr<FJsonObject> State;
        if (!ParseText(ExistingText, State, Error)) return false;
        int32 Version = 0;
        if (!ReadSchemaVersion(State, Version, Error)) return false;
        if (Version == 2) { Out = MoveTemp(State); return true; }

        State->SetNumberField(TEXT("schema_version"), 2);
        State->SetNumberField(TEXT("town_layout_revision"), 1);
        const TArray<TSharedPtr<FJsonValue>>* Residents = nullptr;
        State->TryGetArrayField(TEXT("residents"), Residents);
        const TArray<FString> BuildingIds = { TEXT("sao_inn_01"), TEXT("sao_smithy_01"), TEXT("sao_carpentry_01") };
        const TArray<FString> BuildingRoles = { TEXT("innkeeper"), TEXT("blacksmith"), TEXT("carpenter") };
        const TArray<double> BuildingX = { -1800.0, 1800.0, -1800.0 };
        const TArray<double> BuildingY = { 467000.0, 465000.0, 462000.0 };
        FJsonArray Buildings, Bindings;
        for (int32 Index = 0; Index < Residents->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Resident = (*Residents)[Index]->AsObject();
            const FString Role = Resident->GetStringField(TEXT("role"));
            auto Runtime = MakeShared<FJsonObject>();
            Runtime->SetBoolField(TEXT("active"), IsActiveRole(Role)); Runtime->SetStringField(TEXT("building_id"), ExpectedBuildingId(Role));
            double SpawnX = 0.0, SpawnY = 0.0, SpawnZ = 0.0; ExpectedSpawnPosition(Role, SpawnX, SpawnY, SpawnZ); Runtime->SetArrayField(TEXT("position_cm"), NumberArray(SpawnX, SpawnY, SpawnZ));
            FJsonArray EmptyMemory;
            Runtime->SetStringField(TEXT("phase"), TEXT("idle")); Runtime->SetNumberField(TEXT("observation_seq"), 0); Runtime->SetNumberField(TEXT("last_think_utc"), 0); Runtime->SetStringField(TEXT("pending_operation"), TEXT("")); Runtime->SetArrayField(TEXT("memory"), EmptyMemory); Resident->SetObjectField(TEXT("runtime"), Runtime);
        }
        for (int32 Index = 0; Index < BuildingIds.Num(); ++Index)
        {
            const TSharedPtr<FJsonValue>* ResidentValue = Residents->FindByPredicate([&](const TSharedPtr<FJsonValue>& Value)
            { return Value->AsObject()->GetStringField(TEXT("role")) == BuildingRoles[Index]; });
            if(!ResidentValue){Error=TEXT("migration role binding is missing");return false;}
            const TSharedPtr<FJsonObject> Resident = (*ResidentValue)->AsObject();
            const FString ResidentId = Resident->GetStringField(TEXT("stable_id")); const FString HomeId = Resident->GetStringField(TEXT("home_id"));
            auto Building = MakeShared<FJsonObject>(); Building->SetStringField(TEXT("building_id"), BuildingIds[Index]); Building->SetStringField(TEXT("role"), BuildingRoles[Index]); Building->SetStringField(TEXT("owner_id"), ResidentId); Building->SetArrayField(TEXT("position_cm"), NumberArray(BuildingX[Index], BuildingY[Index], 0)); Buildings.Add(MakeShared<FJsonValueObject>(Building));
            auto Binding = MakeShared<FJsonObject>(); Binding->SetStringField(TEXT("building_id"), BuildingIds[Index]); Binding->SetStringField(TEXT("resident_id"), ResidentId); Binding->SetStringField(TEXT("home_id"), HomeId); Bindings.Add(MakeShared<FJsonValueObject>(Binding));
        }
        State->SetArrayField(TEXT("buildings"), Buildings); State->SetArrayField(TEXT("building_bindings"), Bindings);
        if (!ValidateV2(State, Error)) return false;
        const FString PreV2Path = Path + TEXT(".pre-v2");
        if (!BackupOnce(PreV2Path, ExistingText, Error)) return false;
        const FString MigratedText = Serialize(State.ToSharedRef());
        FTCHARToUTF8 Bytes(*MigratedText);
        if (Bytes.Length() <= 0 || Bytes.Length() > MaxFileBytes) { Error = TEXT("migrated state exceeds the 1 MiB limit"); return false; }
        if (!AtomicUtf8(Path, MigratedText, Error)) return false;
        Out = MoveTemp(State);
        return true;
    }

    bool Save(const FString& Path, const TSharedRef<FJsonObject>& State, FString& Error)
    {
        Error.Empty();
        if (Path.IsEmpty()) { Error = TEXT("path is empty"); return false; }
        if (!ValidateV2(State, Error)) return false;
        const FString Text = Serialize(State);
        FTCHARToUTF8 Bytes(*Text);
        if (Bytes.Length() <= 0 || Bytes.Length() > MaxFileBytes) { Error = TEXT("serialized state exceeds the 1 MiB limit"); return false; }
        IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
        if (Files.FileExists(*Path))
        {
            if (Files.FileSize(*Path) < 1 || Files.FileSize(*Path) > MaxFileBytes) { Error = TEXT("existing file is outside the 1 MiB limit"); return false; }
            FString Existing;
            TSharedPtr<FJsonObject> ExistingState;
            if (!FFileHelper::LoadFileToString(Existing, *Path) || !ParseText(Existing, ExistingState, Error)) { Error = TEXT("existing file is invalid; refusing to overwrite it"); return false; }
            int32 ExistingVersion = 0;
            if (!ReadSchemaVersion(ExistingState, ExistingVersion, Error) || ExistingVersion != 2) { Error = TEXT("existing v1 file requires explicit MigrateV1ToV2; refusing to overwrite it"); return false; }
            if(ExistingState->GetStringField(TEXT("world_id"))!=State->GetStringField(TEXT("world_id"))) { Error=TEXT("refusing to replace an existing world identity"); return false; }
            if (ExistingState->HasField(TEXT("life")) && !State->HasField(TEXT("life"))) { Error = TEXT("refusing to overwrite a life-enabled save with a life-less state"); return false; }
            if (!AtomicUtf8(Path + TEXT(".bak"), Existing, Error)) return false;
        }
        return AtomicUtf8(Path, Text, Error);
    }
}
