#include "HearthVillage.h"
#include "HearthSettlementPlan.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

// Project labels from the same positions as the scene contours; no second map.
class SHearthPlanningOverlay : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHearthPlanningOverlay) {} SLATE_ARGUMENT(AHearthVillage*,Village) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Village=Args._Village; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100,100); }
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
        FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
    {
        const auto* V=Village.Get();
        if(!V || !V->IsOrganicVillage() || !V->bTownPlanningVisible) return Layer;
        const auto* Plan=V->GetSettlementPlan();
        auto* PC=V->GetWorld()->GetFirstPlayerController();
        if(!Plan || !PC) return Layer;
        const FVector2D Size=Geometry.GetLocalSize();
        const FLinearColor Paper(.97f,.91f,.76f), Ink(.07f,.09f,.085f,.94f);
        const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
        auto Box=[&](const FVector2D& Pos,const FVector2D& Extent,const FLinearColor& Color)
        {
            FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),Brush,ESlateDrawEffect::None,Color);
        };
        auto Text=[&](const FVector2D& Pos,const FString& Value,int32 FontSize,const FLinearColor& Color)
        {
            FSlateDrawElement::MakeText(Out,Layer+1,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(Pos)),Value,
                FCoreStyle::GetDefaultFontStyle("Bold",FontSize),ESlateDrawEffect::None,Color);
        };
        Box(FVector2D(22,20),FVector2D(FMath::Min(620.,Size.X-44),112),Ink);
        Text(FVector2D(40,31),TEXT("炉火与王国  /  中央高地城"),22,Paper);
        Text(FVector2D(40,68),FString::Printf(TEXT("城堡已建 %d / %d 构件   ·   金色线框为待建轮廓"),V->PublicProject.Completed,V->PublicProject.Parts.Num()),13,Paper);
        const FHearthPublicPart* Next=nullptr;
        for(const auto& Part:V->PublicProject.Parts)
            if(Part.Status!=TEXT("completed") && (!Next || Part.Stage<Next->Stage)) Next=&Part;
        Text(FVector2D(40,96),Next?FString::Printf(TEXT("下一构件需 石 %d / 板 %d / 梁 %d    工地现有 %d / %d / %d"),
            Next->Required[0],Next->Required[1],Next->Required[2],V->PublicProject.Stock[0],V->PublicProject.Stock[1],V->PublicProject.Stock[2]):TEXT("城堡已完工"),11,Paper*.85f);
        FVector2D Viewport(1440,900);
        if(GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Viewport);
        TArray<FSlateRect> Labels;
        Labels.Add(FSlateRect(15,15,FMath::Min(650.,Size.X-20),138));
        for(const auto& District:Plan->Districts)
        {
            FVector2D Screen;
            FVector Anchor=District.LabelPosition; Anchor.Z=V->GroundHeightAt(Anchor)+240.f;
            if(!PC->ProjectWorldLocationToScreen(Anchor,Screen,true)) continue;
            Screen=Screen/Viewport*Size;
            if(Screen.X<0 || Screen.Y<0 || Screen.X>Size.X || Screen.Y>Size.Y) continue;
            const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const double TitleWidth=Measure->Measure(District.Label,FCoreStyle::GetDefaultFontStyle("Bold",17)).X;
            const double PurposeWidth=Measure->Measure(District.Purpose,FCoreStyle::GetDefaultFontStyle("Bold",10)).X;
            const float Width=float(FMath::Max(TitleWidth,PurposeWidth)+28.0);
            FVector2D Pos(24,145);
            double Best=TNumericLimits<double>::Max();
            for(int32 Row=-4;Row<=4;++Row) for(int32 Col=-4;Col<=4;++Col)
            {
                const FVector2D Candidate(FMath::Clamp(Screen.X-Width*.5+Col*(Width+16),24.,FMath::Max(24.,Size.X-Width-24)),
                    FMath::Clamp(Screen.Y+Row*72.,145.,FMath::Max(145.,Size.Y-140)));
                const FSlateRect Rect(Candidate.X-7,Candidate.Y-7,Candidate.X+Width+7,Candidate.Y+64);
                double Cost=(Candidate+FVector2D(Width*.5,28)-Screen).SizeSquared();
                for(const auto& Other:Labels)
                    if(Rect.Left<Other.Right && Rect.Right>Other.Left && Rect.Top<Other.Bottom && Rect.Bottom>Other.Top) Cost+=1.e9;
                if(Cost<Best) { Best=Cost; Pos=Candidate; }
            }
            Labels.Add(FSlateRect(Pos.X-7,Pos.Y-7,Pos.X+Width+7,Pos.Y+64));
            const TArray<FVector2D> Leader={Screen,Pos+FVector2D(Width*.5,28)};
            FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),Leader,ESlateDrawEffect::None,District.Color,true,1.5f);
            Box(Pos,FVector2D(Width,57),Ink);
            Box(Pos,FVector2D(4,57),District.Color);
            Text(Pos+FVector2D(13,5),District.Label,17,Paper);
            Text(Pos+FVector2D(13,32),District.Purpose,10,Paper*.84f);
        }
        Box(FVector2D(22,Size.Y-69),FVector2D(Size.X-44,47),Ink);
        Text(FVector2D(39,Size.Y-57),TEXT("P 规划 / 居民   Home 全城   WASD 移动   滚轮缩放   虚线引导用途，线框不计入施工"),12,Paper);
        return Layer+2;
    }
private:
    TWeakObjectPtr<AHearthVillage> Village;
};

TSharedRef<SWidget> MakeHearthPlanningWidget(AHearthVillage* Village)
{
    return SNew(SHearthPlanningOverlay).Village(Village);
}
