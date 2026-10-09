#include "UI/Pause/SPauseMenu.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
const FSlateFontInfo Body = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
const FSlateFontInfo Heading = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 25);
const FLinearColor Accent(.16f, .32f, .42f);
TSharedRef<SWidget> Description(const FString& Text)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Body).AutoWrapText(true);
}
}

void SPauseMenu::Construct(const FArguments& Args)
{
	AppliedControls = Args._Controls ? Args._Controls : GetMutableDefault<UInventoryInputSettings>();
	Controls.Reset(DuplicateObject<UInventoryInputSettings>(AppliedControls.Get(), GetTransientPackage()));
	ItemUse = Args._ItemUse; bSaveControls = Args._SaveControls;
	OnResume = Args._OnResume; OnExitGame = Args._OnExitGame; OnExitDesktop = Args._OnExitDesktop;
	OnControlsChanged = Args._OnControlsChanged;
	auto Button = [](const FString& Text, FOnClicked Click)
	{
		return SNew(SButton).ContentPadding(FMargin(14, 10)).OnClicked(Click)
			[SNew(STextBlock).Text(FText::FromString(Text)).Font(Body)];
	};
	TSharedRef<SVerticalBox> Pause = SNew(SVerticalBox);
	Pause->AddSlot().AutoHeight().Padding(0, 0, 0, 20)[SNew(STextBlock).Text(FText::FromString(TEXT("PAUSED"))).Font(Heading)];
	Pause->AddSlot().AutoHeight().Padding(0, 5)[Button(TEXT("Resume"), FOnClicked::CreateLambda([this]() { OnResume.ExecuteIfBound(); return FReply::Handled(); }))];
	Pause->AddSlot().AutoHeight().Padding(0, 5)[Button(TEXT("Options"), FOnClicked::CreateLambda([this]() { ShowOptions(); return FReply::Handled(); }))];
	const auto ExitGame = Button(TEXT("Exit Game"), FOnClicked::CreateLambda([this]() { OnExitGame.ExecuteIfBound(); return FReply::Handled(); }));
	ExitGame->SetEnabled(Args._CanExitGame);
	Pause->AddSlot().AutoHeight().Padding(0, 5)[ExitGame];
	const auto ExitDesktop = Button(TEXT("Exit to Desktop"), FOnClicked::CreateLambda([this]() { OnExitDesktop.ExecuteIfBound(); return FReply::Handled(); }));
	ExitDesktop->SetEnabled(Args._CanExitDesktop);
	Pause->AddSlot().AutoHeight().Padding(0, 5)[ExitDesktop];
	TSharedRef<SVerticalBox> Navigation = SNew(SVerticalBox);
	const TCHAR* CategoryNames[] = { TEXT("Controls"), TEXT("Sound"), TEXT("Graphics") };
	for (int32 I = 0; I < 3; ++I)
		Navigation->AddSlot().AutoHeight().Padding(0, 4)[Button(CategoryNames[I], FOnClicked::CreateLambda([this, I]() { ShowCategory(static_cast<EOptionsCategory>(I)); return FReply::Handled(); }))];
	TSharedRef<SScrollBox> Tabs = SNew(SScrollBox).Orientation(Orient_Horizontal).ScrollBarAlwaysVisible(true);
	const TCHAR* SectionNames[] = { TEXT("Movement"), TEXT("Item actions"), TEXT("Inventory"), TEXT("Gameplay") };
	for (int32 I : { 0, 1, 3, 2 })
		Tabs->AddSlot().Padding(0, 0, 6, 8)[SNew(SBox).WidthOverride(220)
			[Button(SectionNames[I], FOnClicked::CreateLambda([this, I]() { ShowInventorySection(I); return FReply::Handled(); }))]];
	InventorySections = SNew(SWidgetSwitcher);
	for (int32 I = 0; I < 4; ++I)
	{
		const auto Group = static_cast<EControlSection>(I);
		TSharedRef<SScrollBox> Page = SNew(SScrollBox);
		const TCHAR* Help[] = {
			TEXT("During gameplay. Run, sprint and crouch use tap to toggle or hold until release. Space is reserved."),
			TEXT("During gameplay. Item contracts determine primary/secondary actions. Bandage shortcut: tap takes into hands; hold uses; release stops unfinished use. Stow also works in inventory."),
			TEXT("While inventory is open. Double-activate Select / drag / place item to take/stow without closing inventory. Storage placement is manual. Opening keys are in Gameplay. Esc always closes the interface."),
			TEXT("Open pockets or backpack from gameplay. The same key closes its interface. In backpack view, Next pocket is a separate Inventory action. Esc is fixed: close an interface or open Pause.") };
		Page->AddSlot().Padding(0, 12, 0, 16)[Description(Help[I])];
		if (Group == EControlSection::Inventory)
		{
			using A = EInventoryControl;
			auto Rows = [&](const TCHAR* Title, std::initializer_list<A> Actions)
			{
				Page->AddSlot().Padding(0, 18, 0, 8)[SNew(STextBlock).Text(FText::FromString(Title)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),18))];
				for (auto Action : Actions) Page->AddSlot().Padding(0,3)[MakeControlRow(Action)];
			};
			Rows(TEXT("Backpack open"), { A::CyclePocket });
			Rows(TEXT("Pockets or backpack open — item handling"), { A::Grab });
			Page->AddSlot().Padding(0, 6)[SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return Controls->bToggleGrab ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Controls->bToggleGrab = State == ECheckBoxState::Checked; ControlsEdited(); })
				.ToolTipText(FText::FromString(TEXT("On: click to grab an item, then click again to place it. Off: hold to drag and release to place. Double press takes or stows an item in either mode.")))
				[Description(TEXT("Click to grab / click to place (off: hold to drag)"))]];
			for (auto Action : { A::Cancel, A::Drop }) Page->AddSlot().Padding(0,3)[MakeControlRow(Action)];
			Rows(TEXT("Item rotation"), {});
			const FText RotationHelp = FText::FromString(TEXT("Set how quickly items rotate. Unbind Increase and Decrease item rotation speed to keep this value fixed."));
			Page->AddSlot().Padding(0, 4)[SNew(STextBlock).Text(FText::FromString(TEXT("Item rotation speed"))).Font(Body).ToolTipText(RotationHelp)];
			Page->AddSlot()[SNew(SSpinBox<float>).MinValue(15.f).MaxValue(360.f).Delta(15.f).ToolTipText(RotationHelp)
				.Value_Lambda([this]() { return Controls->TurnSpeed; })
				.OnValueChanged_Lambda([this](float Value) { Controls->SetTurnSpeed(Value); ControlsEdited(); })];
			for (auto Action : { A::RotateWithMouse, A::TurnLeft, A::TurnRight, A::FasterRotation, A::SlowerRotation })
				Page->AddSlot().Padding(0,3)[MakeControlRow(Action)];
			Rows(TEXT("Over the floor list, with no item being manipulated"), { A::ScrollFloorUp, A::ScrollFloorDown });
		}
		else for (int32 J = 0; J < static_cast<int32>(EInventoryControl::Count); ++J)
		{
			const auto Action = static_cast<EInventoryControl>(J);
			if (UInventoryInputSettings::IsBindable(Action) && UInventoryInputSettings::Section(Action) == Group)
			{
				Page->AddSlot().Padding(0, 3)[MakeControlRow(Action)];
				if (Action == EInventoryControl::PickUpWorld)
				{
					Page->AddSlot().Padding(0, 6)[SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return Controls->bAllowItemReplace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { Controls->bAllowItemReplace = State == ECheckBoxState::Checked; ControlsEdited(); })
						.ToolTipText(FText::FromString(TEXT("Allow ground pickup to drop your held item when it cannot be stowed. When off, pickup is blocked in that situation. Items that can be stowed are replaced normally.")))
						[Description(TEXT("Allow item replace when picking up objects"))]];
				}
			}
		}
		if (Group == EControlSection::Movement)
		{
			Page->AddSlot().Padding(0, 14, 0, 4)[Description(TEXT("Mouse look sensitivity"))];
			Page->AddSlot()[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(5.f).Delta(.1f)
				.Value_Lambda([this]() { return Controls->LookSensitivity; })
				.OnValueChanged_Lambda([this](float Value) { Controls->LookSensitivity = Value; ControlsEdited(); })];
		}
		InventorySections->AddSlot()[Page];
	}
	TSharedRef<SVerticalBox> ControlsPage = SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[Tabs]
		+SVerticalBox::Slot().FillHeight(1)[InventorySections.ToSharedRef()];
	ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(.015f, .02f, .03f, 1.f)).Padding(24)
		[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
		[SNew(SBox).WidthOverride(1100).HeightOverride(760)
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(.025f, .035f, .05f, 1.f)).Padding(24)
		[SAssignNew(Screens, SWidgetSwitcher)
			+SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(360)[Pause]]
			+SWidgetSwitcher::Slot()[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 15)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("OPTIONS"))).Font(Heading)]
					+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Back"), FOnClicked::CreateLambda([this]() { HandleEscape(); return FReply::Handled(); }))]]
				+SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 18, 0)[SNew(SBox).WidthOverride(150)[Navigation]]
					+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Categories, SWidgetSwitcher)
						+SWidgetSwitcher::Slot()[ControlsPage]
						+SWidgetSwitcher::Slot()[Description(TEXT("Sound settings coming later."))]
						+SWidgetSwitcher::Slot()[Description(TEXT("Graphics settings coming later."))]]]
				+SVerticalBox::Slot().AutoHeight().Padding(168, 12, 0, 8)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Status); }).Font(Body).AutoWrapText(true)]
				+SVerticalBox::Slot().AutoHeight().Padding(168, 0)[SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return bConflictPending || bConfirmReset ? EVisibility::Visible : EVisibility::Collapsed; })
					+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ContentPadding(10)
						.OnClicked_Lambda([this]() { if (bConflictPending) AssignKey(PendingKey, true); else ResetControls(true); return FReply::Handled(); })
						[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(bConflictPending ? TEXT("Replace") : TEXT("Reset everything")); }).Font(Body)]]
					+SHorizontalBox::Slot().AutoWidth().Padding(8, 0)[Button(TEXT("Cancel"), FOnClicked::CreateLambda([this]() { Rebinding = INDEX_NONE; bConflictPending = bConfirmReset = false; Status = TEXT("Cancelled."); return FReply::Handled(); }))]]
				+SVerticalBox::Slot().AutoHeight().Padding(168, 8, 0, 0)[SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return ActiveCategory == EOptionsCategory::Controls ? EVisibility::Visible : EVisibility::Collapsed; })
					+SHorizontalBox::Slot().FillWidth(1)[Description(TEXT("Select a binding. Del clears; Esc cancels."))]
					+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Reset this section"), FOnClicked::CreateLambda([this]() { ResetControls(false); return FReply::Handled(); }))]
					+SHorizontalBox::Slot().AutoWidth().Padding(8, 0)[Button(TEXT("Reset all controls"), FOnClicked::CreateLambda([this]() { Rebinding = INDEX_NONE; bConflictPending = false; bConfirmReset = true; Status = TEXT("Restore all keys and control preferences?"); return FReply::Handled(); }))]]
				+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8)
				[SNew(SButton).ContentPadding(FMargin(14, 10))
					.IsEnabled_Lambda([this]() { return HasUnsavedChanges() && !IsCapturing() && !bConfirmReset; })
					.OnClicked_Lambda([this]() { ApplyControls(); return FReply::Handled(); })
					[SNew(STextBlock).Text(FText::FromString(TEXT("Apply changes"))).Font(Body)]]
			]
			+SWidgetSwitcher::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
					[SNew(STextBlock).Text(FText::FromString(TEXT("Unsaved changes"))).Font(Heading)]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
					[Description(TEXT("Save your changes before leaving Options?"))]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[Button(TEXT("Cancel"), FOnClicked::CreateLambda([this]() { CancelExit(); return FReply::Handled(); }))]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[Button(TEXT("Cancel and exit"), FOnClicked::CreateLambda([this]() { Controls->CopySettingsFrom(*AppliedControls.Get()); FinishOptions(); return FReply::Handled(); }))]
				+SVerticalBox::Slot().AutoHeight().Padding(0, 4)
					[Button(TEXT("Save and exit"), FOnClicked::CreateLambda([this]() { ApplyControls(); FinishOptions(); return FReply::Handled(); }))]
			]]]]]];
}

TSharedRef<SWidget> SPauseMenu::MakeControlRow(EInventoryControl Action, const FString& OverrideLabel, const FString& Detail)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	Row->AddSlot().FillWidth(1).VAlign(VAlign_Center)[Description(OverrideLabel.IsEmpty() ? UInventoryInputSettings::Label(Action) : OverrideLabel)];
	const FString Help = UInventoryInputSettings::HoverDescription(Action);
	if (!Help.IsEmpty()) Row->SetToolTipText(FText::FromString(Help));
	if (!UInventoryInputSettings::IsBindable(Action))
	{
		Row->AddSlot().AutoWidth().Padding(4,0)[SNew(SBox).WidthOverride(180).MinDesiredHeight(44)
			[SNew(STextBlock).Text(FText::FromString(Controls->KeyLabel(Action))).Font(Body).Justification(ETextJustify::Center)]];
		return Row;
	}
	for (int32 Slot = 0; Slot < 2; ++Slot)
		Row->AddSlot().AutoWidth().Padding(4, 0)[SNew(SBox).WidthOverride(180).MinDesiredHeight(44)
			[SNew(SButton).ContentPadding(FMargin(12, 9)).HAlign(HAlign_Center)
				.ToolTipText_Lambda([this, Action, Slot]()
				{
					const FKey Key = Controls->GetKey(Action, Slot);
					FString Text = Key.IsValid() ? Key.GetDisplayName().ToString() : TEXT("Unbound");
					const FString Help = UInventoryInputSettings::HoverDescription(Action);
					if (!Help.IsEmpty()) Text += TEXT("\n\n") + Help;
					return FText::FromString(Text);
				})
				.ButtonColorAndOpacity_Lambda([this, Action, Slot]() { return Rebinding == static_cast<int32>(Action) && RebindingSlot == Slot ? Accent : FLinearColor(.13f, .16f, .2f); })
				.OnClicked_Lambda([this, Action, Slot]() { Rebinding = static_cast<int32>(Action); RebindingSlot = Slot; bConflictPending = bConfirmReset = false; Status = TEXT("Press a replacement key. Delete clears; Escape cancels."); return FReply::Handled().SetUserFocus(SharedThis(this)); })
				[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
				[SNew(STextBlock).Font(Body).Text_Lambda([this, Action, Slot]()
				{
					return FText::FromString(UInventoryInputSettings::ShortKeyLabel(Controls->GetKey(Action, Slot)));
				})]]]];
	return Row;
}
bool SPauseMenu::HasUnsavedChanges() const
{
	return Controls.IsValid() && AppliedControls.IsValid() && !Controls->HasSameSettings(*AppliedControls.Get());
}
void SPauseMenu::ControlsEdited()
{
	Status = HasUnsavedChanges() ? TEXT("Unapplied changes.") : TEXT("No pending changes.");
}
void SPauseMenu::ApplyControls()
{
	if (!HasUnsavedChanges()) return;
	AppliedControls->CopySettingsFrom(*Controls.Get());
	if (bSaveControls) AppliedControls->SaveConfig();
	OnControlsChanged.ExecuteIfBound();
	Status = TEXT("Changes applied.");
}
void SPauseMenu::FinishOptions()
{
	bOptionsOpen = bConfirmExit = bConflictPending = bConfirmReset = false;
	Rebinding = INDEX_NONE;
	Screens->SetActiveWidgetIndex(0);
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
void SPauseMenu::CancelExit()
{
	bConfirmExit = false;
	Screens->SetActiveWidgetIndex(1);
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
void SPauseMenu::AssignKey(FKey Key, bool bReplace)
{
	if (!IsCapturing() || !Controls.IsValid()) return;
	if (Key == EKeys::Delete) Key = EKeys::Invalid;
	if (Controls->AssignKey(static_cast<EInventoryControl>(Rebinding), RebindingSlot, Key, bReplace, Status))
	{
		Rebinding = INDEX_NONE; bConflictPending = false; ControlsEdited(); Status = TEXT("Key updated. Apply to save.");
	}
	else
	{
		PendingKey = Key;
		bConflictPending = UInventoryInputSettings::Supports(static_cast<EInventoryControl>(Rebinding), Key)
			&& !Controls->Conflicts(static_cast<EInventoryControl>(Rebinding), Key).IsEmpty();
	}
}
void SPauseMenu::ResetControls(bool bAll)
{
	Rebinding = INDEX_NONE; bConflictPending = bConfirmReset = false;
	bool bComplete = true;
	if (bAll) Controls->ResetDefaults(); else bComplete = Controls->ResetSection(static_cast<EControlSection>(ActiveSection));
	ControlsEdited(); Status = bComplete ? TEXT("Defaults restored in draft. Apply to save.") : TEXT("Section reset. Keys used in another section were left unbound.");
}
void SPauseMenu::ShowOptions() { bOptionsOpen = true; Screens->SetActiveWidgetIndex(1); ShowCategory(EOptionsCategory::Controls); }
void SPauseMenu::ShowCategory(EOptionsCategory Category)
{
	Rebinding = INDEX_NONE; bConflictPending = bConfirmReset = false; Status.Empty(); ActiveCategory = Category;
	Categories->SetActiveWidgetIndex(static_cast<int32>(Category)); FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
void SPauseMenu::ShowInventorySection(int32 Section)
{
	if (Section < 0 || Section >= 4) return;
	Rebinding = INDEX_NONE; bConflictPending = bConfirmReset = false; Status.Empty(); ActiveSection = Section;
	InventorySections->SetActiveWidgetIndex(Section); FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
void SPauseMenu::HandleEscape()
{
	if (IsCapturing() || bConfirmReset) { Rebinding = INDEX_NONE; bConflictPending = bConfirmReset = false; Status = TEXT("Cancelled."); }
	else if (bConfirmExit) CancelExit();
	else if (bOptionsOpen)
	{
		if (HasUnsavedChanges()) { bConfirmExit = true; Screens->SetActiveWidgetIndex(2); }
		else FinishOptions();
	}
	else { OnResume.ExecuteIfBound(); return; }
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
FReply SPauseMenu::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.IsRepeat()) return IsCapturing() ? FReply::Handled() : FReply::Unhandled();
	if ((IsCapturing() && Event.GetKey() == EKeys::Escape) || (!IsCapturing() && Controls->Matches(EInventoryControl::Back, Event.GetKey()))) { HandleEscape(); return FReply::Handled(); }
	if (!IsCapturing()) return FReply::Unhandled();
	AssignKey(Event.GetKey()); return FReply::Handled();
}
FReply SPauseMenu::OnKeyDown(const FGeometry&, const FKeyEvent&) { return FReply::Handled(); }
FReply SPauseMenu::OnPreviewMouseButtonDown(const FGeometry&, const FPointerEvent& Event)
{
	if (bConflictPending) return FReply::Unhandled();
	if (IsCapturing()) { AssignKey(Event.GetEffectingButton()); return FReply::Handled(); }
	if (Controls->Matches(EInventoryControl::Back, Event.GetEffectingButton())) { HandleEscape(); return FReply::Handled(); }
	return FReply::Unhandled();
}
FReply SPauseMenu::OnMouseWheel(const FGeometry&, const FPointerEvent& Event)
{
	if (!IsCapturing()) return FReply::Unhandled();
	AssignKey(Event.GetWheelDelta() > 0 ? EKeys::MouseScrollUp : EKeys::MouseScrollDown); return FReply::Handled();
}
