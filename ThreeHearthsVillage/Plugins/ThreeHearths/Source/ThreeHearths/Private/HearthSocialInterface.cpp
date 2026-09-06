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
        auto* V=Village.Get(); if(V && (Revision!=V->SocialRevision || Selected!=V->SelectedResident || TileRevision!=CurrentTileRevision(*V))) Refresh();
    }
private:
    TWeakObjectPtr<AHearthVillage> Village;
    TArray<TSharedPtr<FString>> Rows;
    TSharedPtr<SListView<TSharedPtr<FString>>> List;
    FTableViewStyle ListStyle;
    FTableRowStyle RowStyle;
    int32 Revision=-1,Selected=-1;
    uint32 TileRevision=MAX_uint32;
    uint32 CurrentTileRevision(const AHearthVillage& V) const
    {
        uint32 Hash=GetTypeHash(V.TileOrders.Num());
        for(const auto& O:V.TileOrders)
        {
            Hash=HashCombine(Hash,GetTypeHash(O.Id)); Hash=HashCombine(Hash,GetTypeHash(O.Status)); Hash=HashCombine(Hash,GetTypeHash(O.Result));
            Hash=HashCombine(Hash,GetTypeHash(O.ReservedClay)); Hash=HashCombine(Hash,GetTypeHash(O.ReservedTiles)); Hash=HashCombine(Hash,GetTypeHash(O.Escrow));
        }
        return Hash;
    }
    void Refresh()
    {
        auto* V=Village.Get(); if(!V) return; Revision=V->SocialRevision; Selected=V->SelectedResident; TileRevision=CurrentTileRevision(*V);
        Rows.Reset(); Rows.Add(MakeShared<FString>(V->RelationshipSummary(Selected)));
        for(int32 I=V->Commitments.Num()-1;I>=0;--I)
        {
            const auto& P=V->Commitments[I]; if(P.Worker!=Selected && P.Beneficiary!=Selected) continue;
            const FString Status=P.Status==TEXT("fulfilled")?TEXT("已履行"):P.Status==TEXT("broken")?TEXT("未履行"):P.Status==TEXT("active")?TEXT("正在履行"):TEXT("已约定");
            Rows.Add(MakeShared<FString>(TEXT("约定 · ")+Status+TEXT("\n")+V->Residents[P.Worker].Name+TEXT(" → ")+V->Residents[P.Beneficiary].Name+TEXT("：")+V->LifeActionName(P.Worker,P.Action)+TEXT("\n")+P.Result));
        }
        for(int32 I=V->TileOrders.Num()-1;I>=0;--I)
        {
            const auto& O=V->TileOrders[I]; if(O.Customer!=Selected && O.Potter!=Selected) continue;
            const FString Status=O.Status==TEXT("completed")?TEXT("已交付结算"):O.Status==TEXT("delivering")?TEXT("陶瓦待交付")
                :O.Status==TEXT("producing") || O.Status==TEXT("active")?TEXT("正在制瓦"):O.Status==TEXT("accepted")?TEXT("已接受并预留")
                :O.Status==TEXT("rejected")?TEXT("已拒绝"):O.Status==TEXT("cancelled")?TEXT("已取消"):O.Status==TEXT("broken")?TEXT("未履行"):TEXT("已提出");
            const FString Customer=V->Residents.IsValidIndex(O.Customer)?V->Residents[O.Customer].Name:TEXT("未知客户");
            const FString Potter=V->Residents.IsValidIndex(O.Potter)?V->Residents[O.Potter].Name:TEXT("未知陶工");
            Rows.Add(MakeShared<FString>(FString::Printf(TEXT("制瓦订单 · %s\n%s 委托 %s：村庄陶土 %d 份 → 个人陶瓦 %d 片，价格 %d 枚钱\n当前预留：陶土 %d、陶瓦 %d、托管款 %d。%s"),
                *Status,*Customer,*Potter,O.ClayQuantity,O.TileQuantity,O.Price,O.ReservedClay,O.ReservedTiles,O.Escrow,*O.Result)));
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
