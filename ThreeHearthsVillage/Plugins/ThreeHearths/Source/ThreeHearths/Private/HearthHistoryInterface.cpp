#include "HearthVillage.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace HearthHistoryUI
{
    const FLinearColor Ink(0.09f,0.12f,0.095f);
    const FLinearColor Muted(0.28f,0.30f,0.25f);
    FSlateFontInfo Font(int32 Size,bool Bold=false) { return FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size); }
    FString Status(const FHearthDecisionRecord& R)
    {
        if(R.Status==TEXT("thinking")) return TEXT("等待回复");
        if(R.Status==TEXT("executing")) return TEXT("执行中");
        if(R.Status==TEXT("completed")) return TEXT("已完成");
        if(R.Status==TEXT("cancelled")) return TEXT("已中断");
        return TEXT("旧版恢复");
    }
}

class SHearthHistory : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHearthHistory) {} SLATE_ARGUMENT(AHearthVillage*,Village) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Village=Args._Village;
        FSlateBrush Paper=*FCoreStyle::Get().GetBrush("WhiteBrush");
        Paper.TintColor=FSlateColor(FLinearColor(0.94f,0.89f,0.77f)); ListStyle.SetBackgroundBrush(Paper);
        AreaStyle=FCoreStyle::Get().GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea");
        FSlateBrush Closed=AreaStyle.CollapsedImage; Closed.TintColor=FSlateColor(HearthHistoryUI::Ink); AreaStyle.SetCollapsedImage(Closed);
        FSlateBrush Opened=AreaStyle.ExpandedImage; Opened.TintColor=FSlateColor(HearthHistoryUI::Ink); AreaStyle.SetExpandedImage(Opened);
        ChildSlot[
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[
                    SNew(STextBlock).Text_Lambda([this] {
                        auto* V=Village.Get(); return FText::FromString(V&&V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].Name+TEXT("的全部抉择"):TEXT("决策历史"));
                    }).Font(HearthHistoryUI::Font(20,true)).ColorAndOpacity(HearthHistoryUI::Ink)
                ]
                +SHorizontalBox::Slot().AutoWidth()[
                    SNew(SButton).ContentPadding(FMargin(12,8)).OnClicked_Lambda([this] { if(auto* V=Village.Get()) V->bHistoryOpen=false; return FReply::Handled(); })[
                        SNew(STextBlock).Text(FText::FromString(TEXT("关闭"))).Font(HearthHistoryUI::Font(12))
                    ]
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,12)[
                SNew(STextBlock).Text_Lambda([this] {
                    auto* V=Village.Get(); return FText::FromString(V?FString::Printf(TEXT("共 %d 次 · 最新在前 · 滚动查看旧记录\n%s"),V->HistoryCount(V->SelectedResident),*V->HistorySaveStatus):TEXT(""));
                }).Font(HearthHistoryUI::Font(11)).ColorAndOpacity(HearthHistoryUI::Muted).AutoWrapText(true)
            ]
            +SVerticalBox::Slot().AutoHeight()[
                SNew(STextBlock).Visibility_Lambda([this] { return Items.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed; })
                .Text(FText::FromString(TEXT("还没有做出选择。模型开始思考后，这里会出现第一条记录。")))
                .Font(HearthHistoryUI::Font(13)).ColorAndOpacity(HearthHistoryUI::Ink).AutoWrapText(true)
            ]
            +SVerticalBox::Slot().FillHeight(1)[
                SAssignNew(List,SListView<TSharedPtr<int32>>).ListViewStyle(&ListStyle).ListItemsSource(&Items).SelectionMode(ESelectionMode::None)
                .OnGenerateRow_Lambda([this](TSharedPtr<int32> Item,const TSharedRef<STableViewBase>& Owner) { return MakeRow(Item,Owner); })
            ]
        ];
    }
    virtual void Tick(const FGeometry& Geometry,double Time,float Dt) override
    {
        SCompoundWidget::Tick(Geometry,Time,Dt);
        auto* V=Village.Get(); if(!V || !V->bHistoryOpen) return;
        const bool ChangedPerson=Person!=V->SelectedResident;
        if(!ChangedPerson && Revision==V->HistoryRevision) return;
        Revision=V->HistoryRevision;
        const int32 Count=V->HistoryCount(V->SelectedResident);
        if(!ChangedPerson && Count==Items.Num()) return; // Bound row text updates without collapsing open details.
        TMap<int32,TSharedPtr<int32>> Existing;
        if(!ChangedPerson) for(const auto& Item:Items) Existing.Add(*Item,Item);
        const int32 Added=Count-Items.Num(); const float Offset=List->GetScrollOffset();
        Person=V->SelectedResident; Items.Reset();
        for(int32 I=V->DecisionHistory.Num()-1;I>=0;--I) if(V->DecisionHistory[I].Resident==Person)
            Items.Add(Existing.Contains(I)?Existing[I]:MakeShared<int32>(I));
        List->RequestListRefresh();
        if(ChangedPerson) List->ScrollToTop();
        else if(Offset>0.1f && Added>0) List->SetScrollOffset(Offset+Added);
    }
private:
    TWeakObjectPtr<AHearthVillage> Village;
    TArray<TSharedPtr<int32>> Items;
    TSharedPtr<SListView<TSharedPtr<int32>>> List;
    FTableViewStyle ListStyle;
    FExpandableAreaStyle AreaStyle;
    int32 Person=-1,Revision=-1;
    FString Field(int32 Index,int32 Part) const
    {
        auto* V=Village.Get(); if(!V || !V->DecisionHistory.IsValidIndex(Index)) return FString();
        const auto& R=V->DecisionHistory[Index];
        if(Part==0) return R.Choice+TEXT(" · ")+HearthHistoryUI::Status(R);
        if(Part==1) return R.Timestamp+TEXT(" · ")+(R.Run==V->CurrentRun?TEXT("本轮"):TEXT("历史轮次"))+
            (R.Kind==TEXT("legacy")?FString():FString::Printf(TEXT(" · 游戏 %02d:%02d"),static_cast<int32>(R.At)/60,static_cast<int32>(R.At)%60));
        if(Part==2) return R.Reason.IsEmpty()?(R.Status==TEXT("cancelled")?TEXT("尚未收到完整选择，本轮就已结束。"):TEXT("正在等待模型给出选择和理由……")):TEXT("理由：")+R.Reason;
        if(Part==3) return R.Result.IsEmpty()?TEXT("结果：尚未开始执行"):TEXT("结果：")+R.Result;
        if(Part==4) return R.Context;
        const FString Source=R.Source==TEXT("player")?TEXT("你安排的任务"):R.Source==TEXT("api")?R.Model:R.Source==TEXT("local_fallback")?TEXT("本地备用规则"):TEXT("本地人设规则");
        if(R.Kind==TEXT("legacy")) return Source+TEXT(" · 旧版未记录耗时与 Token");
        if(R.Model.IsEmpty()) return Source+TEXT(" · 未调用模型 · 0 tokens");
        return Source+FString::Printf(TEXT(" · %.1f 秒 · "),R.Latency)+(R.bHasUsage?FString::Printf(TEXT("%d tokens"),R.Tokens):TEXT("token 未报告"));
    }
    TSharedRef<ITableRow> MakeRow(TSharedPtr<int32> Item,const TSharedRef<STableViewBase>& Owner)
    {
        const int32 Index=*Item;
        auto Text=[this,Index](int32 Part,int32 Size,bool Bold=false)
        {
            return SNew(STextBlock).Text_Lambda([this,Index,Part] { return FText::FromString(Field(Index,Part)); })
                .Font(HearthHistoryUI::Font(Size,Bold)).ColorAndOpacity(Part==1||Part==5?HearthHistoryUI::Muted:HearthHistoryUI::Ink).AutoWrapText(true);
        };
        return SNew(STableRow<TSharedPtr<int32>>,Owner).Padding(FMargin(0,0,8,10))[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0.98f,0.95f,0.85f)).Padding(14)[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Text(0,14,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,7)[Text(1,10)]
                +SVerticalBox::Slot().AutoHeight()[Text(2,13)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,7)[Text(3,12)]
                +SVerticalBox::Slot().AutoHeight()[
                    SNew(SExpandableArea).Style(&AreaStyle).InitiallyCollapsed(true)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.91f,0.89f,0.79f))
                    .BodyBorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BodyBorderBackgroundColor(FLinearColor(0.98f,0.95f,0.85f))
                    .Padding(FMargin(6)).HeaderPadding(FMargin(6))
                    .HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("查看当时状态与可选行动"))).Font(HearthHistoryUI::Font(11)).ColorAndOpacity(HearthHistoryUI::Muted)]
                    .BodyContent()[Text(4,12)]
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Text(5,10)]
            ]
        ];
    }
};

TSharedRef<SWidget> MakeHearthHistoryWidget(AHearthVillage* Village) { return SNew(SHearthHistory).Village(Village); }
