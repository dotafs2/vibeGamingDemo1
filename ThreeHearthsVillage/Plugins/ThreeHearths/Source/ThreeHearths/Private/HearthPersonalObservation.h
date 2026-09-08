#pragma once

#include "HearthVillage.h"
#include "Dom/JsonObject.h"

namespace HearthPersonalObservation
{
    constexpr float HorizontalFov=85.f;
    constexpr const TCHAR* Provenance=TEXT("resident_first_person_v1");

    // The existing villagers face mesh +Y (Body has a -90 degree yaw). Locate
    // the eyes within the native bare-body bounds, then express that offset in
    // the head bone's reference frame so the live head animation carries it.
    inline FVector EyeInHeadSpace(const FBox& NativeBodyBounds,const FTransform& ReferenceHead)
    {
        const double Height=NativeBodyBounds.GetSize().Z;
        FVector Eye=ReferenceHead.GetLocation();
        Eye.Y+=Height*.04;
        Eye.Z=NativeBodyBounds.Min.Z+Height*.92;
        return ReferenceHead.InverseTransformPosition(Eye);
    }

    inline TSharedRef<FJsonObject> Context(const FHearthResident& R,const FString& Signature,
        const FString& ImageFile,const FString& TargetId,bool bRoyal,bool bModular)
    {
        auto Result=MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("observation_scope"),TEXT("resident_first_person"));
        Result->SetStringField(TEXT("resident_id"),R.StableId);Result->SetStringField(TEXT("name"),R.Name);
        Result->SetStringField(TEXT("role"),R.Role);Result->SetStringField(TEXT("personality"),R.Personality);
        Result->SetStringField(TEXT("persistent_story"),R.InnerStory);Result->SetStringField(TEXT("goal"),R.DesignGoal);
        Result->SetStringField(TEXT("observation_id"),ImageFile.EndsWith(TEXT(".png"))?ImageFile.LeftChop(4):ImageFile);
        Result->SetStringField(TEXT("design_revision"),Signature);Result->SetStringField(TEXT("image_file"),ImageFile);
        Result->SetStringField(TEXT("view"),TEXT("Actual first-person UE image from my current eyes and actor facing, horizontal FOV 85 degrees. Occluders and neighbors remain. My house or commissioned project may be behind me, outside this view, or hidden. A target identity is personal knowledge, not proof that it appears in the image."));
        Result->SetStringField(TEXT("target_identity_if_known"),TargetId);
        Result->SetStringField(TEXT("target_relationship"),bRoyal?TEXT("my commissioned project; visibility unknown"):TEXT("my home or personal design intention; visibility unknown"));
        if(R.bVisualInspectionArrived && !R.VisualInspectionId.IsEmpty() && !TargetId.IsEmpty() && R.VisualInspectionTargetId==TargetId)
        {
            Result->SetStringField(TEXT("physical_inspection_memory"),TEXT("我现在位于自己已知住所或工程的可进入前沿，实际身体已朝向它的中心。正前方的目标是我记得的自家房屋或委托工程，归属来自我自己的长期记忆，无须画面中的门牌、文字或产权证明。若正前方房屋外观可见，可以评估它；这不证明它在当前图像中可见，若遮挡或出画则暂缓；室内及隐藏条件仍未知。"));
            Result->SetStringField(TEXT("view"),TEXT("Actual first-person UE image from my eyes, horizontal FOV 85 degrees. I have physically reached my known target frontage and turned toward its center. My arrival memory identifies the target; use this image to assess visible exterior details. Occluders and neighbors remain. Do not assume that arrival proves an unobstructed view."));
            Result->SetStringField(TEXT("target_relationship"),bRoyal?TEXT("my known commissioned project, identified by my arrival memory"):TEXT("my known home, identified by my arrival memory"));
        }
        Result->SetNumberField(TEXT("my_coins"),R.Coins);
        Result->SetNumberField(TEXT("my_owned_planks"),R.PersonalPlanks);
        Result->SetNumberField(TEXT("my_owned_tiles"),R.PersonalTiles);
        if(R.LastVisualSignature.Contains(TEXT(":fp1:"))) Result->SetStringField(TEXT("previous_unverified_visual_interpretation"),R.DesignFeedback);
        Result->SetStringField(TEXT("knowledge_boundary"),TEXT("Only the supplied image is current visual evidence. Story, identity, goal and my own wallet/materials are personal memory or possessions, not visual proof. Unseen interiors, room counts, other people's locations, communal stock, construction completion and future floor plans are unknown here. An unfinished object cannot be called finished, and an absent image detail cannot prove an absent world object."));
        Result->SetStringField(TEXT("shared_cultural_preference"),TEXT("共同审美愿望（不是眼前事实）：偏爱温暖灰泥墙、陶土或蓝灰板岩屋顶、清楚可读的屋檐、庭院绿植，以及适合居民步行和马车通行的街道比例。逐步靠真实劳动与材料实现。"));
        FString Options=TEXT("0: satisfied with the visible target, pause further extensions for now; 6: cannot identify or assess my target in this FOV, defer without changing my existing satisfaction or growth intention; 4: ask one specific unimplemented capability or rule question; 5: propose one physical object or space as my pending personal request. When my target cannot be identified in this FOV, choose 6 and explicitly defer without inventing a defect or a seen building. All choices are intentions only; existing ownership, materials, wages, land and route checks remain required.");
        if(bModular && !bRoyal) Options+=TEXT(" When the relevant target is actually visible, 1: prefer right wing (+local X); 2: prefer left wing (-local X); 3: prefer rear wing (+local Y). These are plan-relative intentions, not screen directions or instant construction.");
        Result->SetStringField(TEXT("options"),Options);
        return Result;
    }
}
