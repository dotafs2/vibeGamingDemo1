#include "HearthVillage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

TSharedRef<SWidget> MakeHearthHistoryWidget(AHearthVillage* Village);

namespace HearthUI
{
    const FLinearColor Paper(0.94f,0.89f,0.77f,0.97f);
    const FLinearColor Ink(0.09f,0.12f,0.095f);
    const FLinearColor Muted(0.28f,0.30f,0.25f);
    const FLinearColor Green(0.16f,0.29f,0.20f);
    const FLinearColor Light(0.98f,0.95f,0.85f);
    FSlateFontInfo Font(int32 Size,bool Bold=false) { return FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size); }
}

class SHearthOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHearthOverlay) {} SLATE_ARGUMENT(AHearthVillage*, Village) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Village=Args._Village;
        const FSlateBrush* White=FCoreStyle::Get().GetBrush("WhiteBrush");
        ButtonStyle=FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        FSlateBrush Normal=*White; Normal.TintColor=FSlateColor(FLinearColor(0.86f,0.83f,0.73f));
        FSlateBrush Hover=*White; Hover.TintColor=FSlateColor(FLinearColor(0.77f,0.81f,0.68f));
        FSlateBrush Press=*White; Press.TintColor=FSlateColor(FLinearColor(0.65f,0.73f,0.60f));
        ButtonStyle.SetNormal(Normal).SetHovered(Hover).SetPressed(Press);
        auto Panel=SNew(SVerticalBox);
        auto Footer=SNew(SVerticalBox);
        Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("三座小屋"))).Font(HearthUI::Font(25,true)).ColorAndOpacity(HearthUI::Ink)];
        Panel->AddSlot().AutoHeight().Padding(0,3,0,14)[SNew(STextBlock).Text(FText::FromString(TEXT("一个村庄，三种生活。"))).Font(HearthUI::Font(12)).ColorAndOpacity(HearthUI::Muted)];
        Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V?V->ProductionSummary():TEXT(""));
        }).Font(HearthUI::Font(12,true)).ColorAndOpacity(HearthUI::Ink).AutoWrapText(true)];
        Panel->AddSlot().AutoHeight().Padding(0,12,0,12)[SNew(SSeparator).ColorAndOpacity(FLinearColor(0.45f,0.44f,0.35f,0.35f))];
        Panel->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Text(FText::FromString(TEXT("村民"))).Font(HearthUI::Font(11,true)).ColorAndOpacity(HearthUI::Muted)];
        for(int32 I=0;I<3;++I) Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[ResidentButton(I)];
        Panel->AddSlot().AutoHeight().Padding(0,12,0,10)[SNew(SSeparator).ColorAndOpacity(FLinearColor(0.45f,0.44f,0.35f,0.35f))];
        Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V&&V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].Name+TEXT("的打算"):TEXT(""));
        }).Font(HearthUI::Font(16,true)).ColorAndOpacity(HearthUI::Ink)];
        Panel->AddSlot().AutoHeight().Padding(0,4,0,0)[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V?V->DecisionLabel(V->SelectedResident):TEXT(""));
        }).Font(HearthUI::Font(10)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];

        Panel->AddSlot().AutoHeight().Padding(0,10,0,6)[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); if(!V || !V->Residents.IsValidIndex(V->SelectedResident)) return FText::GetEmpty();
            const auto& R=V->Residents[V->SelectedResident];
            return FText::FromString(FString::Printf(TEXT("精力 %.0f / 100 · 社交需求 %.0f / 100"),R.Energy,R.SocialNeed));
        }).Font(HearthUI::Font(11)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];
        Panel->AddSlot().AutoHeight().Padding(0,8,0,10)[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V&&V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].Reason:TEXT(""));
        }).Font(HearthUI::Font(13)).ColorAndOpacity(HearthUI::Ink).AutoWrapText(true)];
        Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); if(!V||!V->Residents.IsValidIndex(V->SelectedResident)) return FText::GetEmpty();
            const auto& R=V->Residents[V->SelectedResident];
            return FText::FromString(V->CargoSummary(V->SelectedResident));
        }).Font(HearthUI::Font(11)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];
        Panel->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBox).HeightOverride(6)[SNew(SProgressBar).Percent_Lambda([this] {
            auto* V=Village.Get(); return TOptional<float>(V&&V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].BuildProgress:0.f);
        }).FillColorAndOpacity(HearthUI::Green)]];
        Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V&&V->Residents.IsValidIndex(V->SelectedResident)?V->Residents[V->SelectedResident].LatestEvent:TEXT(""));
        }).Font(HearthUI::Font(11)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];
        Footer->AddSlot().AutoHeight().Padding(0,8,0,6)[SNew(SSeparator)];
        Footer->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V?V->ApiSummary():TEXT(""));
        }).Font(HearthUI::Font(10)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];
        Footer->AddSlot().AutoHeight().Padding(0,7,0,5)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[ControlButton([this] {
                auto* V=Village.Get(); return FText::FromString(V&&V->bAutonomousLifeEnabled?TEXT("自主：开"):TEXT("自主：关"));
            },[this] { if(auto* V=Village.Get()) V->ToggleAutonomy(); })]
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SComboButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(8,9))
                .IsEnabled_Lambda([this] { auto* V=Village.Get(); return V&&V->CanAssignActivity(V->SelectedResident); })
                .OnGetMenuContent_Lambda([this] { return ActivityMenu(); })
                .ToolTipText(FText::FromString(TEXT("所有村民都有全部技能。列表只显示当前材料充足、有空地或资源且目标未被占用的任务。需等待当前任务或模型回复完成。")))
                .ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("安排工作"))).Font(HearthUI::Font(12,true)).ColorAndOpacity(HearthUI::Ink)]
            ]
        ];
        Footer->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
            auto* V=Village.Get(); return FText::FromString(V?V->LifeSummary():TEXT(""));
        }).Font(HearthUI::Font(10)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)];

        auto Controls=SNew(SHorizontalBox);
        Controls->AddSlot().AutoWidth().Padding(0,0,8,0)[ControlButton([] { return FText::FromString(TEXT("全景")); },
            [this] { if(auto* V=Village.Get()) if(auto* PC=Cast<AHearthPlayerController>(V->GetWorld()->GetFirstPlayerController())) PC->ShowIsland(); })];
        Controls->AddSlot().AutoWidth().Padding(0,0,8,0)[ControlButton([] { return FText::FromString(TEXT("看村民")); },
            [this] { if(auto* V=Village.Get()) if(auto* PC=Cast<AHearthPlayerController>(V->GetWorld()->GetFirstPlayerController())) PC->FocusResident(); })];
        Controls->AddSlot().AutoWidth().Padding(0,0,8,0)[ControlButton(
            [this] { auto* V=Village.Get(); return FText::FromString(V&&V->bSimulationPaused?TEXT("继续"):TEXT("暂停")); },
            [this] { if(auto* V=Village.Get()) V->TogglePause(); })];
        Controls->AddSlot().AutoWidth().Padding(0,0,8,0)[ControlButton(
            [] { return FText::FromString(TEXT("换挡")); },
            [this] { if(auto* V=Village.Get()) V->CycleSpeed(); })];
        Controls->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[
            SNew(SBorder).BorderImage(White).BorderBackgroundColor(HearthUI::Paper).Padding(10,8)[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[
                    SNew(SNumericEntryBox<float>).AllowSpin(false).MinValue(1.f).MaxValue(1000.f)
                    .MinDesiredValueWidth(65.f).Font(HearthUI::Font(12,true))
                    .Value_Lambda([this] { auto* V=Village.Get(); return TOptional<float>(V?V->SimulationSpeed:1.f); })
                    .OnValueCommitted_Lambda([this](float Value,ETextCommit::Type) { if(auto* V=Village.Get()) V->SetSimulationSpeed(Value); })
                    .ToolTipText(FText::FromString(TEXT("输入 1～1000，按回车生效。模型回复按实际时间等待。")))
                ]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)[
                    SNew(STextBlock).Text(FText::FromString(TEXT("倍速"))).Font(HearthUI::Font(12,true)).ColorAndOpacity(HearthUI::Ink)
                ]
            ]
        ];
        Controls->AddSlot().AutoWidth()[ControlButton([] { return FText::FromString(TEXT("重新开始")); },
            [this] { if(auto* V=Village.Get()) V->RestartVillage(); })];

        ChildSlot[
            SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(22)[
                SNew(SBox).WidthOverride(312)[SNew(SBorder).BorderImage(White).BorderBackgroundColor(HearthUI::Paper).Padding(20)[SNew(SVerticalBox)+SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Panel]]+SVerticalBox::Slot().AutoHeight()[Footer]]]
            ]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(356,82,22,112)[
                SNew(SBox).WidthOverride(680).Visibility_Lambda([this] { auto* V=Village.Get(); return V&&V->bHistoryOpen?EVisibility::Visible:EVisibility::Collapsed; })[
                    SNew(SBorder).BorderImage(White).BorderBackgroundColor(HearthUI::Paper).Padding(18)[MakeHearthHistoryWidget(Village.Get())]
                ]
            ]
            +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(22)[Controls]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(350,0,24,24)[
                SNew(SBox).MaxDesiredWidth(740)[SNew(SBorder).BorderImage(White).BorderBackgroundColor(HearthUI::Paper).Padding(16,12)[
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this] {
                        auto* V=Village.Get(); return FText::FromString(V?V->VillageEvent:TEXT(""));
                    }).Font(HearthUI::Font(13,true)).ColorAndOpacity(HearthUI::Ink).AutoWrapText(true)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(STextBlock).Text(FText::FromString(TEXT("WASD / 方向键 平移 · 滚轮 缩放 · F 看村民 · Home 全景 · 空格 暂停 · R 重来"))).Font(HearthUI::Font(10)).ColorAndOpacity(HearthUI::Muted).AutoWrapText(true)]
                ]]
            ]
        ];
    }
private:
    TWeakObjectPtr<AHearthVillage> Village;
    FButtonStyle ButtonStyle;
    TSharedRef<SWidget> ActivityMenu()
    {
        FMenuBuilder Menu(true,nullptr);
        auto* V=Village.Get(); if(!V) return SNew(STextBlock);
        const int32 Person=V->SelectedResident;
        for(int32 Action:V->AvailableLifeActions(Person))
        {
            Menu.AddMenuEntry(FText::FromString(V->LifeActionName(Person,Action)),FText::GetEmpty(),FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([this,Person,Action] { if(auto* Current=Village.Get()) Current->AssignActivity(Person,Action); })));
        }
        return SNew(SBox).MaxDesiredHeight(420).MaxDesiredWidth(560)[SNew(SScrollBox)+SScrollBox::Slot()[Menu.MakeWidget()]];
    }
    TSharedRef<SWidget> ResidentButton(int32 Index)
    {
        return SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(10,9))
            .OnClicked_Lambda([this,Index] { if(auto* V=Village.Get()) V->SelectResident(Index); return FReply::Handled(); })[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,9,0)[
                    SNew(SBox).WidthOverride(5).HeightOverride(35)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this,Index] {
                        auto* V=Village.Get(); return V?V->ResidentColor(Index):FLinearColor::White;
                    })]
                ]
                +SHorizontalBox::Slot().FillWidth(1)[
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this,Index] {
                        auto* V=Village.Get(); if(!V||!V->Residents.IsValidIndex(Index)) return FText::GetEmpty();
                        return FText::FromString((V->SelectedResident==Index?TEXT("› "):TEXT(""))+V->Residents[Index].Name+TEXT("   ")+V->StatusFor(Index));
                    }).Font(HearthUI::Font(13,true)).ColorAndOpacity(HearthUI::Ink).AutoWrapText(true)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(STextBlock).Text_Lambda([this,Index] {
                        auto* V=Village.Get(); return FText::FromString(V&&V->Residents.IsValidIndex(Index)?V->Residents[Index].Personality:TEXT(""));
                    }).Font(HearthUI::Font(10)).ColorAndOpacity(HearthUI::Muted)]
                ]
            ];
    }
    TSharedRef<SWidget> ControlButton(TFunction<FText()> Label,TFunction<void()> Action)
    {
        return SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(16,12))
            .OnClicked_Lambda([Action] { Action(); return FReply::Handled(); })[
                SNew(STextBlock).Text_Lambda([Label] { return Label(); }).Font(HearthUI::Font(12,true)).ColorAndOpacity(HearthUI::Ink)
            ];
    }
};

AHearthPlayerController::AHearthPlayerController()
{
    bShowMouseCursor=true;
    bEnableClickEvents=true;
    DefaultMouseCursor=EMouseCursor::Default;
}
void AHearthPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if(!IsLocalController()) return;
    for(TActorIterator<AHearthVillage> It(GetWorld());It;++It) { Village=*It; break; }
    bIslandCamera=Village.IsValid() && Village->bUseCropoutMap;
    CameraOffset=bIslandCamera?FVector(-9000,-11000,14500):FVector(-2300,-2800,3300);
    CameraCenter=bIslandCamera?FVector(-2200,-200,0):FVector::ZeroVector;
    CameraZoom=bIslandCamera?0.48f:1.f;
    const FRotator Rotation=(-CameraOffset).Rotation();
    auto* Camera=GetWorld()->SpawnActor<ACameraActor>(CameraCenter+CameraOffset*CameraZoom,Rotation);
    Camera->GetCameraComponent()->SetFieldOfView(52.f);
    Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
    SetViewTarget(Camera);
    UpdateCamera();
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    if(Village.IsValid() && GEngine && GEngine->GameViewport)
    {
        // Cropout's DPI curve targets its own HUD. Keep this compact panel readable in PIE.
        VillageUI=SNew(SDPIScaler).DPIScale_Lambda([] {
            FVector2D Size(1440,900);
            if(GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
            const float Inherited=GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(Size.X),FMath::RoundToInt(Size.Y)));
            const float Desired=FMath::Clamp(static_cast<float>(Size.Y)/850.f,0.85f,1.15f);
            return Desired/FMath::Max(Inherited,0.1f);
        })[SNew(SHearthOverlay).Village(Village.Get())];
        GEngine->GameViewport->AddViewportWidgetContent(VillageUI.ToSharedRef(),10);
    }
}
void AHearthPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if(!Village.IsValid()) return;
    // Typing a speed must not also trigger resident selection or restart shortcuts.
    const auto Focused=FSlateApplication::Get().GetKeyboardFocusedWidget();
    if(Focused.IsValid() && Focused->GetType()==FName(TEXT("SEditableText"))) return;
    if(WasInputKeyJustPressed(EKeys::SpaceBar)) Village->TogglePause();
    if(WasInputKeyJustPressed(EKeys::R)) Village->RestartVillage();
    if(WasInputKeyJustPressed(EKeys::One)) Village->SelectResident(0);
    if(WasInputKeyJustPressed(EKeys::Two)) Village->SelectResident(1);
    if(WasInputKeyJustPressed(EKeys::Three)) Village->SelectResident(2);
    if(WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        FHitResult Hit;
        if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)) if(auto* Person=Cast<AHearthVillager>(Hit.GetActor())) Village->SelectResident(Person->ResidentIndex);
    }
    if(WasInputKeyJustPressed(EKeys::F)) FocusResident();
    if(WasInputKeyJustPressed(EKeys::Home)) ShowIsland();
    if(WasInputKeyJustPressed(EKeys::MouseScrollUp)) ZoomCamera(0.85f);
    if(WasInputKeyJustPressed(EKeys::MouseScrollDown)) ZoomCamera(1.f/0.85f);
    const float Forward=(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)?1.f:0.f)-(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)?1.f:0.f);
    const float Right=(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)?1.f:0.f)-(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)?1.f:0.f);
    const float PanSpeed=(bIslandCamera?5000.f:1500.f)*CameraZoom*FMath::Min(DeltaTime,0.1f);
    PanCamera(Right*PanSpeed,Forward*PanSpeed);
    UpdateCamera();
}
void AHearthPlayerController::ShowIsland()
{
    CameraCenter=bIslandCamera?FVector(-350,-50,0):FVector::ZeroVector;
    CameraZoom=1.f;
    UpdateCamera();
}
void AHearthPlayerController::FocusResident()
{
    if(!Village.IsValid() || !Village->Residents.IsValidIndex(Village->SelectedResident)) return;
    if(auto* Person=Village->Residents[Village->SelectedResident].Actor.Get())
    {
        CameraCenter=Person->GetActorLocation();
        CameraZoom=bIslandCamera?0.19f:0.75f;
        UpdateCamera();
    }
}
void AHearthPlayerController::ZoomCamera(float Factor)
{
    if(!FMath::IsFinite(Factor) || Factor<=0) return;
    CameraZoom=FMath::Clamp(CameraZoom*Factor,bIslandCamera?0.12f:0.45f,bIslandCamera?1.6f:2.f);
    UpdateCamera();
}
void AHearthPlayerController::PanCamera(float Right,float Forward)
{
    if(!FMath::IsFinite(Right) || !FMath::IsFinite(Forward)) return;
    const FRotator Rotation=(-CameraOffset).Rotation();
    FVector Direction=Rotation.Vector(); Direction.Z=0; Direction.Normalize();
    CameraCenter+=Rotation.Quaternion().GetRightVector()*Right+Direction*Forward;
    const float Limit=bIslandCamera?7500.f:2500.f;
    CameraCenter.X=FMath::Clamp(CameraCenter.X,-Limit,Limit);
    CameraCenter.Y=FMath::Clamp(CameraCenter.Y,-Limit,Limit);
    UpdateCamera();
}
void AHearthPlayerController::UpdateCamera()
{
    if(auto* Camera=Cast<ACameraActor>(GetViewTarget()))
    {
        const FVector ScreenRight=(-CameraOffset).Rotation().Quaternion().GetRightVector();
        const float PanelOffset=(bIslandCamera?2400.f:620.f)*CameraZoom;
        Camera->SetActorLocation(CameraCenter+CameraOffset*CameraZoom-ScreenRight*PanelOffset);
        FVector2D Size(1440,900);
        if(GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Size);
        const float Aspect=static_cast<float>(Size.X/FMath::Max(Size.Y,1.0));
        const float Fov=FMath::RadiansToDegrees(2.f*FMath::Atan(FMath::Tan(FMath::DegreesToRadians(CameraBaseFov*0.5f))*FMath::Max(1.f,Aspect/1.6f)));
        Camera->GetCameraComponent()->SetFieldOfView(Fov);
    }
}
void AHearthPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if(VillageUI.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(VillageUI.ToSharedRef());
    VillageUI.Reset();
    Super::EndPlay(Reason);
}
AHearthGameMode::AHearthGameMode()
{
    PlayerControllerClass=AHearthPlayerController::StaticClass();
    DefaultPawnClass=nullptr;
    HUDClass=nullptr;
}
