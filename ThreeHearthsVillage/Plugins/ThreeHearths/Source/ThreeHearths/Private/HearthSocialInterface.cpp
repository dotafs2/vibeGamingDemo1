#include "HearthVillage.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace HearthSocialUI
{
    const FLinearColor Ink(.09f,.12f,.095f),Muted(.28f,.30f,.25f),Paper(.98f,.95f,.85f,.98f);
    FSlateFontInfo Font(int32 Size,bool Bold=false) { return FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size); }
}

class SHearthSocial : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHearthSocial) {} SLATE_ARGUMENT(AHearthVillage*,Village) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Village=Args._Village;
        FSlateBrush Paper=*FCoreStyle::Get().GetBrush("WhiteBrush");
        Paper.TintColor=FSlateColor(FLinearColor(.94f,.89f,.77f)); ListStyle.SetBackgroundBrush(Paper);
        RowStyle=FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
        RowStyle.SetEvenRowBackgroundBrush(Paper).SetOddRowBackgroundBrush(Paper)
            .SetEvenRowBackgroundHoveredBrush(Paper).SetOddRowBackgroundHoveredBrush(Paper);
        ChildSlot[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this] { auto* V=Village.Get(); return FText::FromString(V && V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].Name+TEXT("的对话与关系"):TEXT("对话与关系")); }).Font(HearthSocialUI::Font(20,true)).ColorAndOpacity(HearthSocialUI::Ink)]
                +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ContentPadding(FMargin(12,8)).OnClicked_Lambda([this] { if(auto* V=Village.Get()) V->bSocialOpen=false; return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("关闭")))]]
            ]
            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,12)[SNew(STextBlock).Text(FText::FromString(TEXT("他们各自决定怎么回应。答应之后，还要把事情做完。"))).Font(HearthSocialUI::Font(11)).ColorAndOpacity(HearthSocialUI::Muted).AutoWrapText(true)]
            +SVerticalBox::Slot().FillHeight(1)[SAssignNew(List,SListView<TSharedPtr<FString>>).ListViewStyle(&ListStyle).ListItemsSource(&Rows).SelectionMode(ESelectionMode::None)
                .OnGenerateRow_Lambda([this](TSharedPtr<FString> Text,const TSharedRef<STableViewBase>& Owner) {
                    return SNew(STableRow<TSharedPtr<FString>>,Owner).Style(&RowStyle).Padding(FMargin(5,7))[
                        SNew(STextBlock).Text(FText::FromString(*Text)).Font(HearthSocialUI::Font(13)).ColorAndOpacity(HearthSocialUI::Ink).AutoWrapText(true)];
                })]
        ];
        Refresh();
    }
    virtual void Tick(const FGeometry& Geometry,const double Time,const float Dt) override
    {
        SCompoundWidget::Tick(Geometry,Time,Dt);
        auto* V=Village.Get(); if(V && (Revision!=V->SocialRevision || Selected!=V->SelectedResident)) Refresh();
    }
private:
    TWeakObjectPtr<AHearthVillage> Village;
    TArray<TSharedPtr<FString>> Rows;
    TSharedPtr<SListView<TSharedPtr<FString>>> List;
    FTableViewStyle ListStyle;
    FTableRowStyle RowStyle;
    int32 Revision=-1,Selected=-1;
    void Refresh()
    {
        auto* V=Village.Get(); if(!V) return; Revision=V->SocialRevision; Selected=V->SelectedResident;
        Rows.Reset(); Rows.Add(MakeShared<FString>(V->RelationshipSummary(Selected)));
        for(int32 I=V->Commitments.Num()-1;I>=0;--I)
        {
            const auto& P=V->Commitments[I]; if(P.Worker!=Selected && P.Beneficiary!=Selected) continue;
            const FString Status=P.Status==TEXT("fulfilled")?TEXT("已履行"):P.Status==TEXT("broken")?TEXT("未履行"):P.Status==TEXT("active")?TEXT("正在履行"):TEXT("已约定");
            Rows.Add(MakeShared<FString>(TEXT("约定 · ")+Status+TEXT("\n")+V->Residents[P.Worker].Name+TEXT(" → ")+V->Residents[P.Beneficiary].Name+TEXT("：")+V->LifeActionName(P.Worker,P.Action)+TEXT("\n")+P.Result));
        }
        bool Found=false;
        for(int32 I=V->Conversations.Num()-1;I>=0;--I)
        {
            const auto& S=V->Conversations[I]; if(S.First!=Selected && S.Second!=Selected) continue; Found=true;
            Rows.Add(MakeShared<FString>(TEXT("—— ")+V->Residents[S.First].Name+TEXT(" 与 ")+V->Residents[S.Second].Name+TEXT(" · ")+(S.bClosed?TEXT("已结束"):S.bMet?TEXT("正在交谈"):TEXT("正在会面"))+TEXT(" ——")));
            for(const auto& L:S.Lines) Rows.Add(MakeShared<FString>(V->Residents[L.Speaker].Name+TEXT("：")+L.Text));
            if(!S.Outcome.IsEmpty()) Rows.Add(MakeShared<FString>(S.Outcome));
        }
        if(!Found) Rows.Add(MakeShared<FString>(TEXT("还没有交谈记录。可以安排这位居民拜访一位空闲邻居。")));
        if(List.IsValid()) List->RequestListRefresh();
    }
};

class SHearthSpeech : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHearthSpeech) {} SLATE_ARGUMENT(AHearthVillage*,Village) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Village=Args._Village; ChildSlot[SAssignNew(Canvas,SCanvas)];
        if(!Village.IsValid()) return;
        for(int32 I=0;I<Village->Residents.Num();++I)
            Canvas->AddSlot().Position_Lambda([this,I] { return ScreenPosition(I); }).Size(FVector2D(280,105))[
                SNew(SBox).VAlign(VAlign_Bottom).Visibility_Lambda([this,I] {
                    auto* V=Village.Get(); return V && V->Residents.IsValidIndex(I) && V->Residents[I].SpeechRemaining>0?EVisibility::HitTestInvisible:EVisibility::Collapsed;
                })[
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HearthSocialUI::Paper).Padding(FMargin(12,9))[
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this,I] { auto* V=Village.Get(); return FText::FromString(V && V->Residents.IsValidIndex(I)?V->Residents[I].Name:TEXT("")); }).Font(HearthSocialUI::Font(11,true)).ColorAndOpacity(HearthSocialUI::Muted)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[SNew(STextBlock).Text_Lambda([this,I] { auto* V=Village.Get(); return FText::FromString(V && V->Residents.IsValidIndex(I)?V->Residents[I].Speech:TEXT("")); }).Font(HearthSocialUI::Font(14,true)).ColorAndOpacity(HearthSocialUI::Ink).AutoWrapText(true)]
                    ]
                ]
            ];
        SetVisibility(EVisibility::HitTestInvisible);
    }
private:
    TWeakObjectPtr<AHearthVillage> Village; TSharedPtr<SCanvas> Canvas;
    FVector2D ScreenPosition(int32 Index) const
    {
        auto* V=Village.Get(); if(!V || !V->Residents.IsValidIndex(Index) || !IsValid(V->Residents[Index].Actor)) return FVector2D(-10000,-10000);
        auto* PC=V->GetWorld()->GetFirstPlayerController(); FVector2D Screen;
        if(!PC || !PC->ProjectWorldLocationToScreen(V->Residents[Index].Actor->GetActorLocation()+FVector(0,0,205),Screen)) return FVector2D(-10000,-10000);
        FVector2D Pixels(1920,1080); if(GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Pixels);
        const FVector2D Size=Canvas->GetCachedGeometry().GetLocalSize();
        if(Size.X>0 && Size.Y>0) Screen*=Size/Pixels;
        return Screen-FVector2D(140,105);
    }
};

TSharedRef<SWidget> MakeHearthSocialWidget(AHearthVillage* V) { return SNew(SHearthSocial).Village(V); }
TSharedRef<SWidget> MakeHearthSpeechWidget(AHearthVillage* V) { return SNew(SHearthSpeech).Village(V); }
