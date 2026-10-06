#include "UI/Pause/SPauseMenu.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

void SPauseMenu::Construct(const FArguments& Args)
{
	Controls = Args._Controls ? Args._Controls : GetMutableDefault<UInventoryInputSettings>();
	ItemUse = Args._ItemUse;
	bSaveControls = Args._SaveControls;
	OnResume = Args._OnResume;
	OnExitGame = Args._OnExitGame;
	OnExitDesktop = Args._OnExitDesktop;
	const auto Heading = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26);
	const auto Body = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13);
	const FLinearColor Muted(.65f, .72f, .78f);
	TSharedRef<SVerticalBox> Pause = SNew(SVerticalBox);
	Pause->AddSlot().AutoHeight().Padding(0, 0, 0, 30)
		[SNew(STextBlock).Text(FText::FromString(TEXT("PAUSED"))).Font(Heading).Justification(ETextJustify::Center)];
	auto AddButton = [&](const TCHAR* Label, const FSimpleDelegate& Action, bool bEnabled)
	{
		Pause->AddSlot().AutoHeight().Padding(0, 6)
		[
			SNew(SBox).HeightOverride(54)
			[SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center).IsEnabled(bEnabled)
			.OnClicked_Lambda([Action]() { Action.ExecuteIfBound(); return FReply::Handled(); })
			[SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17))]]
		];
	};
	AddButton(TEXT("Resume"), OnResume, true);
	AddButton(TEXT("Options"), FSimpleDelegate::CreateSP(this, &SPauseMenu::ShowOptions), true);
	AddButton(TEXT("Exit Game"), OnExitGame, Args._CanExitGame);
	AddButton(TEXT("Exit to Desktop"), OnExitDesktop, Args._CanExitDesktop);
	Pause->AddSlot().AutoHeight().Padding(0, 22, 0, 0)
		[SNew(STextBlock).Text(FText::FromString(TEXT("Esc to resume"))).Font(Body).ColorAndOpacity(Muted).Justification(ETextJustify::Center)];

	TSharedRef<SScrollBox> Rows = SNew(SScrollBox);
	Rows->AddSlot().Padding(0, 0, 0, 10)
		[SNew(STextBlock).Text(FText::FromString(TEXT("INVENTORY"))).Font(Body).ColorAndOpacity(Muted)];
	for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
	{
		const auto Action = static_cast<EInventoryControl>(I);
		if (Action == EInventoryControl::Add) continue; // Laboratory-only fixture creation.
		Rows->AddSlot().Padding(0, 3)[MakeControlRow(Action)];
	}
	if (ItemUse.IsValid() && ItemUse->Shortcuts)
	{
		Rows->AddSlot().Padding(0, 20, 0, 10)
			[SNew(STextBlock).Text(FText::FromString(TEXT("QUICK ITEMS / DURING GAMEPLAY"))).Font(Body).ColorAndOpacity(Muted)];
		for (int32 I = 0; I < ItemUse->Shortcuts->Keys.Num(); ++I) Rows->AddSlot().Padding(0, 3)[MakeShortcutRow(I)];
	}
	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0, 0, 0, .65f)).Padding(24)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			[SNew(SBox).WidthOverride(760).HeightOverride(680)
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(.022f, .03f, .043f)).Padding(30)
			[
				SAssignNew(Screens, SWidgetSwitcher)
				+SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[SNew(SBox).WidthOverride(360)[Pause]]
				+SWidgetSwitcher::Slot()
				[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[SNew(STextBlock).Text(FText::FromString(TEXT("OPTIONS / KEY ASSIGNMENTS"))).Font(Heading)]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
				[SNew(STextBlock).Text(FText::FromString(TEXT("Select a control, then press its new key. Esc returns to Pause."))).Font(Body).AutoWrapText(true).ColorAndOpacity(Muted)]
				+SVerticalBox::Slot().FillHeight(1)[Rows]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 12)
				[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Status); }).Font(Body).AutoWrapText(true)]
				+SVerticalBox::Slot().AutoHeight()
				[SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(16, 12))
				.OnClicked_Lambda([this]() { ResetControls(); return FReply::Handled(); })
				[SNew(STextBlock).Text(FText::FromString(TEXT("Reset controls to default"))).Font(Body)]]]
			]]]
		]
	];
}

TSharedRef<SWidget> SPauseMenu::MakeControlRow(EInventoryControl Action)
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 12, 0)
		[SNew(STextBlock).Text(FText::FromString(UInventoryInputSettings::Label(Action))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))]
		+SHorizontalBox::Slot().AutoWidth()
		[SNew(SBox).WidthOverride(180).HeightOverride(36)
		[SNew(SButton).HAlign(HAlign_Center)
		.OnClicked_Lambda([this, Action]()
		{
			Rebinding = static_cast<int32>(Action); ShortcutRebinding = INDEX_NONE;
			Status = TEXT("Press a key or mouse button. Escape returns to Pause without changing this assignment.");
			return FReply::Handled().SetUserFocus(SharedThis(this));
		})
		[SNew(STextBlock).Text_Lambda([this, Action]()
		{
			return Rebinding == static_cast<int32>(Action) ? FText::FromString(TEXT("Press a control..."))
				: Controls.IsValid() ? Controls->GetKey(Action).GetDisplayName() : FText::GetEmpty();
		})]]];
}

TSharedRef<SWidget> SPauseMenu::MakeShortcutRow(int32 Slot)
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Quick-item shortcut %d"), Slot + 1))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))]
		+SHorizontalBox::Slot().AutoWidth()
		[SNew(SBox).WidthOverride(180).HeightOverride(36)
		[SNew(SButton).HAlign(HAlign_Center)
		.OnClicked_Lambda([this, Slot]()
		{
			ShortcutRebinding = Slot; Rebinding = INDEX_NONE;
			Status = TEXT("Press a free keyboard key. Escape returns to Pause.");
			return FReply::Handled().SetUserFocus(SharedThis(this));
		})
		[SNew(STextBlock).Text_Lambda([this, Slot]()
		{
			if (ShortcutRebinding == Slot) return FText::FromString(TEXT("Press a key..."));
			return ItemUse.IsValid() && ItemUse->Shortcuts ? ItemUse->Shortcuts->Keys[Slot].GetDisplayName() : FText::GetEmpty();
		})]]];
}

void SPauseMenu::ShowOptions()
{
	bOptionsOpen = true;
	Rebinding = ShortcutRebinding = INDEX_NONE;
	Status.Empty();
	Screens->SetActiveWidgetIndex(1);
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}

void SPauseMenu::HandleEscape()
{
	Rebinding = ShortcutRebinding = INDEX_NONE;
	if (bOptionsOpen)
	{
		bOptionsOpen = false;
		Screens->SetActiveWidgetIndex(0);
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
	}
	else OnResume.ExecuteIfBound();
}

void SPauseMenu::AssignKey(FKey Key)
{
	if (!Controls.IsValid()) return;
	if (Rebinding != INDEX_NONE)
	{
		const auto Action = static_cast<EInventoryControl>(Rebinding);
		if (Action == EInventoryControl::Toggle && ItemUse.IsValid() && ItemUse->Shortcuts && ItemUse->Shortcuts->Keys.Contains(Key))
		{ Status = TEXT("That key is assigned to a quick-item shortcut. Change the shortcut first."); return; }
		if (Controls->TrySetKey(Action, Key, Status))
		{
			Rebinding = INDEX_NONE;
			if (bSaveControls) Controls->SaveConfig();
			Status = TEXT("Control updated.");
		}
	}
	else if (ShortcutRebinding != INDEX_NONE && ItemUse.IsValid() && ItemUse->Shortcuts)
	{
		if (ItemUse->Shortcuts->TryBind(ShortcutRebinding, Key, Controls->GetKey(EInventoryControl::Toggle)))
		{
			ShortcutRebinding = INDEX_NONE;
			if (bSaveControls) ItemUse->SaveShortcuts();
			Status = TEXT("Shortcut updated.");
		}
		else Status = TEXT("Choose a free keyboard key. Movement, mouse, Escape and inventory-open controls are reserved.");
	}
}

void SPauseMenu::ResetControls()
{
	Rebinding = ShortcutRebinding = INDEX_NONE;
	if (!Controls.IsValid()) return;
	Controls->ResetDefaults();
	if (bSaveControls) Controls->SaveConfig();
	if (ItemUse.IsValid() && ItemUse->Shortcuts)
	{
		// Reset keys, preserving the player's assigned item types.
		ItemUse->Shortcuts->Keys = { EKeys::E, EKeys::F, EKeys::G };
		if (bSaveControls) ItemUse->SaveShortcuts();
	}
	Status = TEXT("Default controls restored.");
}

FReply SPauseMenu::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat()) HandleEscape();
		return FReply::Handled();
	}
	if (!IsCapturing()) return FReply::Unhandled();
	if (!Event.IsRepeat()) AssignKey(Event.GetKey());
	return FReply::Handled();
}

FReply SPauseMenu::OnKeyDown(const FGeometry&, const FKeyEvent&)
{
	return FReply::Handled();
}

FReply SPauseMenu::OnPreviewMouseButtonDown(const FGeometry&, const FPointerEvent& Event)
{
	if (!IsCapturing()) return FReply::Unhandled();
	AssignKey(Event.GetEffectingButton());
	return FReply::Handled();
}

FReply SPauseMenu::OnMouseWheel(const FGeometry&, const FPointerEvent&)
{
	if (!IsCapturing()) return FReply::Unhandled();
	Status = TEXT("Wheel scrolling adjusts rotation speed. Choose a key or mouse button.");
	return FReply::Handled();
}
