#include "HearthBuildingArchetypes.h"

namespace
{
    bool HasArchetypeTerm(const FString& Value, const TCHAR* Term)
    {
        return Value.Contains(Term, ESearchCase::IgnoreCase, ESearchDir::FromStart);
    }
}

namespace HearthBuildingArchetypes
{
    FString SelectArchetype(const FString& Role, const FString& Goal, bool bKing)
    {
        if (bKing)
        {
            return TEXT("keep");
        }

        const FString RoleValue = Role.ToLower();
        const FString GoalValue = Goal.ToLower();
        if (HasArchetypeTerm(RoleValue, TEXT("inn")) || HasArchetypeTerm(RoleValue, TEXT("host")) || HasArchetypeTerm(RoleValue, TEXT("tavern"))
            || HasArchetypeTerm(RoleValue, TEXT("旅店")) || HasArchetypeTerm(RoleValue, TEXT("客栈")) || HasArchetypeTerm(RoleValue, TEXT("酒馆")))
        {
            return TEXT("inn");
        }
        if (HasArchetypeTerm(GoalValue, TEXT("warehouse")) || HasArchetypeTerm(GoalValue, TEXT("storage")) || HasArchetypeTerm(GoalValue, TEXT("store"))
            || HasArchetypeTerm(GoalValue, TEXT("仓库")) || HasArchetypeTerm(GoalValue, TEXT("仓储")))
        {
            return TEXT("warehouse");
        }
        if (HasArchetypeTerm(RoleValue, TEXT("merchant")) || HasArchetypeTerm(RoleValue, TEXT("trader")) || HasArchetypeTerm(RoleValue, TEXT("shop"))
            || HasArchetypeTerm(GoalValue, TEXT("shop")) || HasArchetypeTerm(GoalValue, TEXT("commerce")) || HasArchetypeTerm(GoalValue, TEXT("market"))
            || HasArchetypeTerm(GoalValue, TEXT("商铺")) || HasArchetypeTerm(GoalValue, TEXT("店铺")))
        {
            return TEXT("shop_house");
        }
        if (HasArchetypeTerm(RoleValue, TEXT("craft")) || HasArchetypeTerm(RoleValue, TEXT("carpenter")) || HasArchetypeTerm(RoleValue, TEXT("smith"))
            || HasArchetypeTerm(RoleValue, TEXT("potter")) || HasArchetypeTerm(RoleValue, TEXT("weaver")) || HasArchetypeTerm(RoleValue, TEXT("木匠"))
            || HasArchetypeTerm(RoleValue, TEXT("铁匠")) || HasArchetypeTerm(RoleValue, TEXT("陶工")) || HasArchetypeTerm(RoleValue, TEXT("织工"))
            || HasArchetypeTerm(GoalValue, TEXT("workshop")) || HasArchetypeTerm(GoalValue, TEXT("courtyard")) || HasArchetypeTerm(GoalValue, TEXT("作坊"))
            || HasArchetypeTerm(GoalValue, TEXT("工坊")) || HasArchetypeTerm(GoalValue, TEXT("庭院")))
        {
            return TEXT("courtyard_workshop");
        }
        return TEXT("rowhouse");
    }
}
