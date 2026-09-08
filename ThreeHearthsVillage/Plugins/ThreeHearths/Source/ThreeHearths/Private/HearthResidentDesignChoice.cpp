#include "HearthResidentDesignChoice.h"

#include "Dom/JsonObject.h"
#include "Misc/Crc.h"
#include "Serialization/JsonSerializer.h"

#include <initializer_list>

namespace
{
    struct FLayoutSpec
    {
        const TCHAR* Id;
        const TCHAR* Label;
        const TCHAR* Family;
        int32 Coins;
        int32 Planks;
        int32 Beams;
        int32 Stone;
        int32 Tiles;
        float MaxSlope;
    };

    static const FLayoutSpec Specs[HearthResidentDesignChoice::LayoutCount] =
    {
        {TEXT("compact_cluster"), TEXT("Compact cluster"), TEXT("family_cluster"), 18, 10, 4, 6, 6, .24f},
        {TEXT("L_court"), TEXT("L court"), TEXT("carpenter_court"), 27, 16, 7, 10, 10, .14f},
        {TEXT("stepped_wings"), TEXT("Stepped wings"), TEXT("merchant_steps"), 33, 20, 8, 14, 12, .36f},
        {TEXT("U_court"), TEXT("U court"), TEXT("family_cluster"), 43, 26, 11, 20, 16, .11f},
        {TEXT("offset_workshop"), TEXT("Offset workshop"), TEXT("carpenter_court"), 37, 30, 13, 11, 13, .28f},
        {TEXT("tower_annex"), TEXT("Tower annex"), TEXT("merchant_steps"), 55, 24, 16, 22, 20, .20f},
    };

    FString Normalize(const FString& In)
    {
        FString Result = In;
        Result.TrimStartAndEndInline();
        return Result;
    }

    bool HasAny(const FString& Text, std::initializer_list<const TCHAR*> Terms)
    {
        for (const TCHAR* Term : Terms)
        {
            if (Text.Contains(Term, ESearchCase::IgnoreCase)) return true;
        }
        return false;
    }

    float PreferenceScore(int32 LayoutId, const FHearthResidentDesignInput& Input)
    {
        const FString Signals = Input.Role + TEXT(" ") + Input.Occupation + TEXT(" ") + Input.Personality + TEXT(" ")
            + Input.HouseholdDescription + TEXT(" ") + Input.RelationshipSummary + TEXT(" ") + Input.PersistentGoals;
        // HouseholdSize is the total household count; Dependents is a
        // dependency detail inside that count, not an additional headcount.
        const int32 Household = FMath::Max(1, Input.HouseholdSize);
        const bool bWorkshop = HasAny(Signals, {TEXT("carpenter"), TEXT("merchant"), TEXT("workshop"), TEXT("forge"), TEXT("作坊"), TEXT("木匠"), TEXT("商")});
        const bool bFamily = Household >= 4 || HasAny(Signals, {TEXT("family"), TEXT("children"), TEXT("家人"), TEXT("孩子"), TEXT("家庭")});
        const bool bSocial = Input.CloseRelationships + Input.NeighborCount >= 4 || HasAny(Signals, {TEXT("neighbor"), TEXT("social"), TEXT("welcome"), TEXT("邻居"), TEXT("招待"), TEXT("院子")});
        const bool bQuiet = HasAny(Signals, {TEXT("quiet"), TEXT("private"), TEXT("安静"), TEXT("独处")});
        const bool bStorage = HasAny(Signals, {TEXT("storage"), TEXT("goods"), TEXT("材料"), TEXT("储藏"), TEXT("货物")});
        const float Slope = FMath::Abs(Input.SiteSlope);
        float Score = 0.f;
        switch (LayoutId)
        {
            case HearthResidentDesignChoice::CompactCluster:
                Score += bFamily ? 4.f : 1.f;
                Score += bQuiet ? 2.f : 0.f;
                Score += Household <= 2 ? 2.f : 0.f;
                Score += Slope > .25f ? 3.f : 0.f;
                break;
            case HearthResidentDesignChoice::LCourt:
                Score += bSocial ? 6.f : 1.f;
                Score += bWorkshop ? 2.f : 0.f;
                Score += bFamily ? 2.f : 0.f;
                break;
            case HearthResidentDesignChoice::SteppedWings:
                Score += Slope > .18f ? 9.f : 1.f;
                Score += bWorkshop ? 3.f : 0.f;
                Score += bStorage ? 2.f : 0.f;
                break;
            case HearthResidentDesignChoice::UCourt:
                Score += bFamily ? 7.f : 1.f;
                Score += bSocial ? 5.f : 0.f;
                Score += Input.CloseRelationships >= 2 ? 2.f : 0.f;
                break;
            case HearthResidentDesignChoice::OffsetWorkshop:
                Score += bWorkshop ? 10.f : 0.f;
                Score += bStorage ? 4.f : 0.f;
                Score += bQuiet ? 2.f : 0.f;
                Score += Input.Planks + Input.Beams >= 40 ? 2.f : 0.f;
                break;
            case HearthResidentDesignChoice::TowerAnnex:
                Score += bStorage ? 6.f : 0.f;
                Score += HasAny(Signals, {TEXT("status"), TEXT("watch"), TEXT("view"), TEXT("体面"), TEXT("瞭望")}) ? 4.f : 0.f;
                Score += Input.Coins >= 80 ? 5.f : 0.f;
                Score += Household >= 3 ? 2.f : 0.f;
                break;
            default: break;
        }
        return Score;
    }

    FString PreferenceReason(int32 LayoutId, const FHearthResidentDesignInput& Input)
    {
        const FString Signals = Input.Role + TEXT(" ") + Input.Occupation + TEXT(" ") + Input.Personality + TEXT(" ")
            + Input.HouseholdDescription + TEXT(" ") + Input.RelationshipSummary + TEXT(" ") + Input.PersistentGoals;
        const int32 Household = FMath::Max(1, Input.HouseholdSize);
        const bool bWorkshop = HasAny(Signals, {TEXT("carpenter"), TEXT("merchant"), TEXT("workshop"), TEXT("forge"), TEXT("作坊"), TEXT("木匠"), TEXT("商")});
        const bool bFamily = Household >= 4 || HasAny(Signals, {TEXT("family"), TEXT("children"), TEXT("家人"), TEXT("孩子"), TEXT("家庭")});
        const bool bSocial = Input.CloseRelationships + Input.NeighborCount >= 4 || HasAny(Signals, {TEXT("neighbor"), TEXT("social"), TEXT("welcome"), TEXT("邻居"), TEXT("招待"), TEXT("院子")});
        const bool bQuiet = HasAny(Signals, {TEXT("quiet"), TEXT("private"), TEXT("安静"), TEXT("独处")});
        const bool bStorage = HasAny(Signals, {TEXT("storage"), TEXT("goods"), TEXT("材料"), TEXT("储藏"), TEXT("货物")});
        TArray<FString> SignalsForReason;
        switch (LayoutId)
        {
            case HearthResidentDesignChoice::CompactCluster:
                if (bFamily) SignalsForReason.Add(TEXT("家庭"));
                if (bQuiet) SignalsForReason.Add(TEXT("安静"));
                if (Household <= 2) SignalsForReason.Add(TEXT("小家庭"));
                if (FMath::Abs(Input.SiteSlope) > .25f) SignalsForReason.Add(TEXT("坡地"));
                break;
            case HearthResidentDesignChoice::LCourt:
                if (bSocial) SignalsForReason.Add(TEXT("社交"));
                if (bWorkshop) SignalsForReason.Add(TEXT("作坊"));
                if (bFamily) SignalsForReason.Add(TEXT("家庭"));
                break;
            case HearthResidentDesignChoice::SteppedWings:
                if (FMath::Abs(Input.SiteSlope) > .18f) SignalsForReason.Add(TEXT("坡地"));
                if (bWorkshop) SignalsForReason.Add(TEXT("作坊"));
                if (bStorage) SignalsForReason.Add(TEXT("储藏"));
                break;
            case HearthResidentDesignChoice::UCourt:
                if (bFamily) SignalsForReason.Add(TEXT("家庭"));
                if (bSocial) SignalsForReason.Add(TEXT("社交"));
                if (Input.CloseRelationships >= 2) SignalsForReason.Add(TEXT("亲密关系"));
                break;
            case HearthResidentDesignChoice::OffsetWorkshop:
                if (bWorkshop) SignalsForReason.Add(TEXT("作坊"));
                if (bStorage) SignalsForReason.Add(TEXT("储藏"));
                if (bQuiet) SignalsForReason.Add(TEXT("安静"));
                if (Input.Planks + Input.Beams >= 40) SignalsForReason.Add(TEXT("材料储备"));
                break;
            case HearthResidentDesignChoice::TowerAnnex:
                if (bStorage) SignalsForReason.Add(TEXT("储藏"));
                if (HasAny(Signals, {TEXT("status"), TEXT("watch"), TEXT("view"), TEXT("体面"), TEXT("瞭望")})) SignalsForReason.Add(TEXT("体面/瞭望"));
                if (Input.Coins >= 80) SignalsForReason.Add(TEXT("富余资金"));
                if (Household >= 3) SignalsForReason.Add(TEXT("家庭"));
                break;
            default: break;
        }
        return SignalsForReason.IsEmpty() ? TEXT("居民的当前需要") : FString::Join(SignalsForReason, TEXT("、"));
    }

    FString ConstraintReason(const FHearthResidentDesignAlternative& Alt, const FHearthResidentDesignInput& Input)
    {
        TArray<FString> Missing;
        if (Input.Coins < Alt.Coins) Missing.Add(FString::Printf(TEXT("coins %d/%d"), Input.Coins, Alt.Coins));
        if (Input.Planks < Alt.Planks) Missing.Add(FString::Printf(TEXT("planks %d/%d"), Input.Planks, Alt.Planks));
        if (Input.Beams < Alt.Beams) Missing.Add(FString::Printf(TEXT("beams %d/%d"), Input.Beams, Alt.Beams));
        if (Input.Stone < Alt.Stone) Missing.Add(FString::Printf(TEXT("stone %d/%d"), Input.Stone, Alt.Stone));
        if (Input.Tiles < Alt.Tiles) Missing.Add(FString::Printf(TEXT("tiles %d/%d"), Input.Tiles, Alt.Tiles));
        if (FMath::Abs(Input.SiteSlope) > Alt.MaxSlope)
            Missing.Add(FString::Printf(TEXT("slope %.2f>%.2f"), FMath::Abs(Input.SiteSlope), Alt.MaxSlope));
        if (Missing.IsEmpty()) return TEXT("当前材料与地形约束可行");
        return FString::Join(Missing, TEXT(", "));
    }

    bool IsIntegerNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, uint32& Out)
    {
        double Number = 0;
        if (!Object->HasTypedField<EJson::Number>(Field) || !Object->TryGetNumberField(Field, Number)
            || !FMath::IsFinite(Number) || Number < 0 || Number > static_cast<double>(MAX_uint32)
            || Number != FMath::FloorToDouble(Number)) return false;
        Out = static_cast<uint32>(Number);
        return true;
    }
}

namespace HearthResidentDesignChoice
{
    const TCHAR* LayoutIdName(int32 LayoutId)
    {
        return LayoutId >= 0 && LayoutId < LayoutCount ? Specs[LayoutId].Id : TEXT("unknown");
    }

    uint32 StableSeed(const FHearthResidentDesignInput& Input)
    {
        const FString Identity = Input.StableId + TEXT("|") + FString::FromInt(Input.WorldSeed)
            + TEXT("|") + FString::FromInt(Input.Revision);
        return FCrc::StrCrc32(*Identity);
    }

    TArray<FHearthResidentDesignAlternative> RankAlternatives(const FHearthResidentDesignInput& Input)
    {
        TArray<FHearthResidentDesignAlternative> Result;
        Result.Reserve(LayoutCount);
        for (int32 Id = 0; Id < LayoutCount; ++Id)
        {
            const FLayoutSpec& Spec = Specs[Id];
            FHearthResidentDesignAlternative Alt;
            Alt.LayoutId = Id;
            Alt.Id = Spec.Id;
            Alt.Label = Spec.Label;
            Alt.Family = Spec.Family;
            Alt.Coins = Spec.Coins;
            Alt.Planks = Spec.Planks;
            Alt.Beams = Spec.Beams;
            Alt.Stone = Spec.Stone;
            Alt.Tiles = Spec.Tiles;
            Alt.MaxSlope = Spec.MaxSlope;
            Alt.Score = PreferenceScore(Id, Input);
            Alt.Constraints = {
                FString::Printf(TEXT("coins >= %d"), Alt.Coins),
                FString::Printf(TEXT("planks >= %d"), Alt.Planks),
                FString::Printf(TEXT("beams >= %d"), Alt.Beams),
                FString::Printf(TEXT("stone >= %d"), Alt.Stone),
                FString::Printf(TEXT("tiles >= %d"), Alt.Tiles),
                FString::Printf(TEXT("abs(site_slope) <= %.2f"), Alt.MaxSlope)
            };
            Alt.bFeasible = Input.Coins >= Alt.Coins && Input.Planks >= Alt.Planks && Input.Beams >= Alt.Beams
                && Input.Stone >= Alt.Stone && Input.Tiles >= Alt.Tiles && FMath::Abs(Input.SiteSlope) <= Alt.MaxSlope;
            Alt.Reason = PreferenceReason(Id, Input) + TEXT("；") + ConstraintReason(Alt, Input);
            if (!Alt.bFeasible)
            {
                const float Deficit = FMath::Max(0, Alt.Coins - Input.Coins) + FMath::Max(0, Alt.Planks - Input.Planks)
                    + 1.5f * FMath::Max(0, Alt.Beams - Input.Beams) + FMath::Max(0, Alt.Stone - Input.Stone)
                    + FMath::Max(0, Alt.Tiles - Input.Tiles) + 30.f * FMath::Max(0.f, FMath::Abs(Input.SiteSlope) - Alt.MaxSlope);
                Alt.Score -= Deficit;
            }
            Result.Add(MoveTemp(Alt));
        }
        const uint32 TieSeed = StableSeed(Input);
        Result.Sort([TieSeed](const FHearthResidentDesignAlternative& A, const FHearthResidentDesignAlternative& B)
        {
            if (!FMath::IsNearlyEqual(A.Score, B.Score)) return A.Score > B.Score;
            const uint32 AKey = HashCombineFast(TieSeed, static_cast<uint32>(A.LayoutId));
            const uint32 BKey = HashCombineFast(TieSeed, static_cast<uint32>(B.LayoutId));
            return AKey < BKey;
        });
        return Result;
    }

    FHearthResidentDesignChoice ChooseLocal(const FHearthResidentDesignInput& Input)
    {
        FHearthResidentDesignChoice Choice;
        Choice.Seed = StableSeed(Input);
        Choice.Revision = Input.Revision;
        Choice.Source = TEXT("local_rules");
        Choice.Alternatives = RankAlternatives(Input);
        const FHearthResidentDesignAlternative* Selected = Choice.Alternatives.FindByPredicate(
            [](const FHearthResidentDesignAlternative& Alt) { return Alt.bFeasible; });
        if (!Selected) Selected = Choice.Alternatives.Num() ? &Choice.Alternatives[0] : nullptr;
        if (!Selected)
        {
            Choice.Reason = TEXT("没有可评估的住所方案");
            return Choice;
        }
        Choice.LayoutId = Selected->LayoutId;
        Choice.bLocked = Selected->bFeasible;
        Choice.Reason = FString::Printf(TEXT("我是%s，选择%s：%s。"), Input.Name.IsEmpty() ? TEXT("这户人家") : *Input.Name,
            *Selected->Label, *Selected->Reason);
        return Choice;
    }

    FString BuildKimiPrompt(const FHearthResidentDesignInput& Input, const TArray<FHearthResidentDesignAlternative>& Alternatives)
    {
        FString Prompt = FString::Printf(TEXT("你是%s的居民设计选择者。只能从下面六个布局扩建意图中选择一个；它们复用三个已制作的视觉母版，不代表六个独立视觉recipe。根据家庭、关系、目标、库存与坡度作出有约束的选择。\n"
            "稳定seed必须原样返回为%u。输出严格JSON：{\"layout_id\":整数,\"seed\":整数,\"reason\":\"第一人称、说明约束的短理由\"}。\n"
            "居民 role=%s occupation=%s personality=%s household_description=%s relationship_summary=%s household=%d dependents=%d close_relationships=%d neighbors=%d slope=%.3f coins=%d planks=%d beams=%d stone=%d tiles=%d goals=%s\n"
            "不要创造资源、空间、人物或规则；不可行方案不能选择。\n方案："),
            Input.Name.IsEmpty() ? TEXT("居民") : *Input.Name, StableSeed(Input), *Input.Role, *Input.Occupation,
            *Input.Personality, *Input.HouseholdDescription, *Input.RelationshipSummary, Input.HouseholdSize, Input.Dependents, Input.CloseRelationships, Input.NeighborCount, Input.SiteSlope,
            Input.Coins, Input.Planks, Input.Beams, Input.Stone, Input.Tiles, *Input.PersistentGoals);
        for (const FHearthResidentDesignAlternative& Alt : Alternatives)
        {
            Prompt += FString::Printf(TEXT("\n%d %s family=%s cost(coins=%d,planks=%d,beams=%d,stone=%d,tiles=%d) max_slope=%.2f feasible=%s constraints=%s"),
                Alt.LayoutId, *Alt.Id, *Alt.Family, Alt.Coins, Alt.Planks, Alt.Beams, Alt.Stone, Alt.Tiles, Alt.MaxSlope,
                Alt.bFeasible ? TEXT("true") : TEXT("false"), *FString::Join(Alt.Constraints, TEXT(";")));
        }
        return Prompt;
    }

    bool ParseKimiChoice(const FString& JsonText, const FHearthResidentDesignInput& Input,
        FHearthResidentDesignChoice& OutChoice, FString& OutError)
    {
        OutChoice = FHearthResidentDesignChoice();
        OutError.Empty();
        FString Text = Normalize(JsonText);
        if (Text.StartsWith(TEXT("```json")) && Text.EndsWith(TEXT("```"))) Text = Text.Mid(7, Text.Len() - 10).TrimStartAndEnd();
        TSharedPtr<FJsonObject> Object;
        if (Text.Len() > 4096 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) || !Object.IsValid()
            || Object->Values.Num() != 3)
        {
            OutError = TEXT("choice must be exactly JSON layout_id, seed, reason");
            return false;
        }
        uint32 LayoutNumber = 0, Seed = 0;
        if (!IsIntegerNumber(Object, TEXT("layout_id"), LayoutNumber) || LayoutNumber >= LayoutCount)
        {
            OutError = TEXT("layout_id is outside 0..5");
            return false;
        }
        if (!IsIntegerNumber(Object, TEXT("seed"), Seed) || Seed != StableSeed(Input))
        {
            OutError = TEXT("seed does not match resident identity");
            return false;
        }
        FString Reason;
        if (!Object->HasTypedField<EJson::String>(TEXT("reason")) || !Object->TryGetStringField(TEXT("reason"), Reason))
        {
            OutError = TEXT("reason is required");
            return false;
        }
        Reason.TrimStartAndEndInline();
        Reason.ReplaceInline(TEXT("\r"), TEXT(" "));
        Reason.ReplaceInline(TEXT("\n"), TEXT(" "));
        if (Reason.IsEmpty() || Reason.Len() > 240)
        {
            OutError = TEXT("reason is empty or too long");
            return false;
        }
        const TArray<FHearthResidentDesignAlternative> Alternatives = RankAlternatives(Input);
        const FHearthResidentDesignAlternative* Selected = Alternatives.FindByPredicate([LayoutNumber](const FHearthResidentDesignAlternative& Alt)
        { return Alt.LayoutId == static_cast<int32>(LayoutNumber); });
        if (!Selected || !Selected->bFeasible)
        {
            OutError = TEXT("selected layout violates current constraints");
            return false;
        }
        OutChoice.LayoutId = static_cast<int32>(LayoutNumber);
        OutChoice.Seed = Seed;
        OutChoice.Reason = Reason;
        OutChoice.Source = TEXT("kimi");
        OutChoice.bLocked = true;
        OutChoice.Revision = Input.Revision;
        OutChoice.Alternatives = Alternatives;
        return true;
    }
}
