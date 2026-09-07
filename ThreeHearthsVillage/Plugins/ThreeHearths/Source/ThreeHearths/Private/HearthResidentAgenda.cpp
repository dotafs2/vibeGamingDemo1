#include "HearthResidentAgenda.h"

namespace
{
    FString Clip(const FString& Text, int32 Limit)
    {
        return Text.Left(FMath::Max(0, Limit));
    }

    bool HasAction(const FHearthResidentAgendaInput& Input, int32 Id)
    {
        return Input.AvailableActions.ContainsByPredicate([Id](const FHearthResidentAgendaAction& Action) { return Action.Id == Id; });
    }

    void AddPriority(FHearthResidentAgenda& Out, const FHearthResidentAgendaInput& Input, int32 Id)
    {
        if(HasAction(Input, Id) && !Out.GroundedPriorityActions.Contains(Id)) Out.GroundedPriorityActions.Add(Id);
    }

    bool IsRoleAction(const FString& Role,const FHearthResidentAgendaAction& Action)
    {
        if(Action.Id<100) return false;
        if(Role.Contains(TEXT("陶"))) return Action.Label.Contains(TEXT("瓦"));
        if(Role.Contains(TEXT("农"))) return Action.Label.Contains(TEXT("播种")) || Action.Label.Contains(TEXT("收获"));
        if(Role.Contains(TEXT("木"))) return Action.Label.Contains(TEXT("木板")) || Action.Label.Contains(TEXT("木工"));
        if(Role.Contains(TEXT("石匠"))) return Action.Label.Contains(TEXT("石"));
        if(Role.Contains(TEXT("铁匠"))) return Action.Label.Contains(TEXT("铁"));
        return false;
    }
}

namespace HearthResidentAgenda
{
    FHearthResidentAgenda Build(const FHearthResidentAgendaInput& Input)
    {
        FHearthResidentAgenda Out;
        Out.AuthoritativeFacts=FString::Printf(TEXT("事实：饥饿 %.0f/100，精力 %.0f/100，心情 %.0f/100，社交需求 %.0f/100；个人钱 %d；村庄库存食物 %d、原木 %d、石材 %d、黏土 %d、屋瓦 %d；村庄金库 %d，税率 %d%%；已建房屋 %d/%d。"),
            Input.Hunger,Input.Energy,Input.Mood,Input.SocialNeed,Input.Coins,Input.FoodStock,Input.WoodStock,Input.StoneStock,Input.ClayStock,Input.TileStock,Input.TreasuryCoins,Input.TaxRatePercent,Input.CompletedHomes,Input.ResidentCount);
        Out.AuthoritativeFacts+=FString::Printf(TEXT("当前可执行行动 %d 项；可用行动清单是唯一行动边界。"),Input.AvailableActions.Num());
        if(!Input.RelationshipSummary.IsEmpty()) Out.AuthoritativeFacts+=TEXT("关系记忆：")+Clip(Input.RelationshipSummary,320)+TEXT("。");
        Out.PrivateAspirations=FString::Printf(TEXT("私人愿望：角色“%s”，性格“%s”；短期希望靠真实工作改善收入、税后余钱和家庭安稳，长期希望获得被邻里认可的生活成果。"),*Clip(Input.Role,32),*Clip(Input.Personality,48));
        if(Input.bKing)
        {
            Out.PrivateAspirations+=TEXT("作为国王，我想用税收扩大住所、追求奢华，又依赖村民继续生产来提供收入。排场须由真实财力支持。");
        }
        else if(Input.Role.Contains(TEXT("商人")))
        {
            Out.PrivateAspirations+=TEXT("作为商人，愿望是靠真实交易、信誉和可结算的收入维持家庭余钱；不新增交易规则。");
        }
        else if(Input.Role.Contains(TEXT("石匠")))
        {
            Out.PrivateAspirations+=TEXT("作为石匠，愿望是用现有采石或建设行动交付可靠的石材成果，并逐步改善居所。");
        }
        else if(Input.Role.Contains(TEXT("铁匠")))
        {
            Out.PrivateAspirations+=TEXT("作为铁匠，愿望是靠现有生产与建设行动获得收入和地位；不虚构锻造系统或质量效果。");
        }
        else if(Input.Role.Contains(TEXT("陶")))
        {
            Out.PrivateAspirations+=TEXT("作为陶工，愿望是通过现有制瓦行动和真实订单交付建立信誉；不虚构质量效果。");
            if(Input.ClayStock<=0) Out.MissingInventory=TEXT("当前黏土库存为0");
            else if(Input.WoodStock<=0) Out.MissingInventory=TEXT("当前原木库存为0");
        }
        else if(Input.Role.Contains(TEXT("农")) || Input.Role.Contains(TEXT("田")))
        {
            Out.PrivateAspirations+=TEXT("作为农人，愿望是通过现有播种或收获行动维持粮食和家庭余粮；不假定未验证的配方成本。");
            if(Input.FoodStock<=0) Out.MissingInventory=TEXT("当前食物库存为0");
        }
        else if(Input.Role.Contains(TEXT("木")))
        {
            Out.PrivateAspirations+=TEXT("作为木工，短期倾向把现有木料转成可交付的建材，长期希望以工艺质量和可靠交付赢得地位。");
            if(Input.WoodStock<=0) Out.MissingInventory=TEXT("当前原木库存为0");
        }
        else
        {
            Out.PrivateAspirations+=TEXT("当前愿望可落在一项能带来收入或邻里信任的真实工作上，并逐步改善家庭居所。");
        }
        if(!Input.DesignGoal.IsEmpty()) Out.PrivateAspirations+=TEXT("已有居所愿望：")+Clip(Input.DesignGoal,96)+TEXT("。");
        if(!Input.DesignRequest.IsEmpty()) Out.PrivateAspirations+=TEXT("本人补充请求：")+Clip(Input.DesignRequest,72)+TEXT("。");
        if(!Input.DesignFeedback.IsEmpty()) Out.PrivateAspirations+=TEXT("近期反馈偏好：")+Clip(Input.DesignFeedback,72)+TEXT("。");
        bool bRoleAction=false;
        for(const auto& Action:Input.AvailableActions)
            if(IsRoleAction(Input.Role,Action)) { bRoleAction=true; break; }
        if(Out.MissingInventory.IsEmpty()) Out.MissingInventory=TEXT("未发现已证实的库存缺口");
        Out.MissingCapability=bRoleAction?TEXT("未识别到额外未实现能力"):TEXT("角色愿望对应的专门行动未在当前可执行清单中；不请求新增资源或资产");

        // Survival is always first, but only when the corresponding real action exists.
        if(Input.Hunger>=60.f && Input.FoodStock>0 && Input.Coins>0) AddPriority(Out,Input,50);
        if(Input.Energy<45.f) AddPriority(Out,Input,0);
        if(Input.SocialNeed>=60.f)
            for(const auto& Action:Input.AvailableActions) if(Action.Id>=3 && Action.Id<50) { AddPriority(Out,Input,Action.Id); break; }
        for(const auto& Action:Input.AvailableActions)
        {
            if(IsRoleAction(Input.Role,Action))
                AddPriority(Out,Input,Action.Id);
        }
        Out.Summary=FString::Printf(TEXT("%s %s 库存提示：%s 能力边界：%s"),*Out.AuthoritativeFacts,*Out.PrivateAspirations,*Out.MissingInventory,*Out.MissingCapability);
        return Out;
    }

    FString ToPromptText(const FHearthResidentAgenda& Agenda)
    {
        FString Text=TEXT("【决策议程】\n")+Agenda.AuthoritativeFacts+TEXT("\n")+Agenda.PrivateAspirations+
            TEXT("\n库存提示：")+Agenda.MissingInventory+TEXT("。\n能力边界：")+Agenda.MissingCapability+TEXT("。\n只能选择下方 supplied available_actions 中的 id；愿望不是事实，也不能创造库存、人物、订单或新命令。");
        return Text.Left(2400);
    }
}
