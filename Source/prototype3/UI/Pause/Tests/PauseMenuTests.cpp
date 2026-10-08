#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Pause/SPauseMenu.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

namespace
{
bool ContainsText(const TSharedRef<SWidget>& Widget, const FString& Text)
{
	if (Widget->GetType() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Text) return true;
	for (int32 I = 0; I < Widget->GetChildren()->Num(); ++I)
		if (ContainsText(Widget->GetChildren()->GetChildAt(I), Text)) return true;
	return false;
}
TSharedPtr<SButton> FindButton(const TSharedRef<SWidget>& Widget, const FString& Text)
{
	if (Widget->GetVisibility() == EVisibility::Collapsed) return nullptr;
	if (Widget->GetType() == TEXT("SWidgetSwitcher"))
		return FindButton(StaticCastSharedRef<SWidgetSwitcher>(Widget)->GetActiveWidget().ToSharedRef(), Text);
	if (Widget->GetType() == TEXT("SButton") && ContainsText(Widget, Text)) return StaticCastSharedRef<SButton>(Widget);
	for (int32 I = 0; I < Widget->GetChildren()->Num(); ++I)
		if (const auto Button = FindButton(Widget->GetChildren()->GetChildAt(I), Text)) return Button;
	return nullptr;
}
TSharedPtr<SButton> Binding(const TSharedRef<SWidget>& Widget, EInventoryControl Action, int32 Slot, const UInventoryInputSettings* Settings)
{
	if (Widget->GetType() == TEXT("SHorizontalBox") && Widget->GetChildren()->Num() == 3
		&& ContainsText(Widget->GetChildren()->GetChildAt(0), UInventoryInputSettings::Label(Action)))
	{
		const FKey K = Settings->GetKey(Action, Slot);
		return FindButton(Widget->GetChildren()->GetChildAt(Slot + 1),
			UInventoryInputSettings::ShortKeyLabel(K));
	}
	for (int32 I = 0; I < Widget->GetChildren()->Num(); ++I)
		if (const auto Button = Binding(Widget->GetChildren()->GetChildAt(I), Action, Slot, Settings)) return Button;
	return nullptr;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuControlsTest, "Prototype.Inventory.PauseMenuControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuControlsTest::RunTest(const FString&)
{
	using A = EInventoryControl;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	int32 ResumeCount = 0, ExitCount = 0, Changes = 0;
	const auto Menu = SNew(SPauseMenu).Controls(Settings.Get()).SaveControls(false)
		.CanExitGame(true).CanExitDesktop(false)
		.OnResume(FSimpleDelegate::CreateLambda([&]() { ++ResumeCount; }))
		.OnExitGame(FSimpleDelegate::CreateLambda([&]() { ++ExitCount; }))
		.OnControlsChanged(FSimpleDelegate::CreateLambda([&]() { ++Changes; }));
	const auto* Draft = Menu->GetEditingControls();
	auto Click = [&](const FString& Label)
	{
		const auto Button = FindButton(Menu, Label);
		if (!TestTrue(*FString::Printf(TEXT("Button exists: %s"), *Label), Button.IsValid())) return;
		Button->SimulateClick();
	};
	auto Capture = [&](A Action, int32 Slot = 0)
	{
		const auto Button = Binding(Menu, Action, Slot, Draft);
		if (TestTrue(TEXT("Action has selectable key slot"), Button.IsValid())) Button->SimulateClick();
	};
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1100,760), FSlateLayoutTransform());
	auto Key = [&](FKey K, bool Repeat = false) { Menu->OnPreviewKeyDown(G, FKeyEvent(K,FModifierKeysState(),0,Repeat,0,0)); };
	const auto Desktop = FindButton(Menu, TEXT("Exit to Desktop"));
	TestTrue(TEXT("Desktop exit disabled in editor"), Desktop.IsValid() && !Desktop->IsEnabled());
	Click(TEXT("Exit Game")); TestEqual(TEXT("Exit invokes delegate"), ExitCount, 1);
	Click(TEXT("Options"));
	Click(TEXT("Sound")); TestTrue(TEXT("Sound placeholder clickable"), Menu->GetCategory() == EOptionsCategory::Sound);
	Click(TEXT("Graphics")); TestTrue(TEXT("Graphics placeholder clickable"), Menu->GetCategory() == EOptionsCategory::Graphics);
	Click(TEXT("Controls")); Click(TEXT("Movement"));
	TestTrue(TEXT("Movement includes run"), ContainsText(Menu, UInventoryInputSettings::Label(A::Run)));
	Capture(A::Run, 1); Key(EKeys::J);
	TestTrue(TEXT("Second binding assigned"), Draft->GetKey(A::Run,1) == EKeys::J);
	TestTrue(TEXT("First binding unchanged"), Draft->GetKey(A::Run) == EKeys::LeftShift);
	Capture(A::Run, 1); Key(EKeys::Delete);
	TestFalse(TEXT("Delete clears just selected slot"), Draft->GetKey(A::Run,1).IsValid());
	Click(TEXT("Gameplay")); Capture(A::Toggle); Key(EKeys::W);
	TestTrue(TEXT("Conflict waits without changing keys"), Draft->GetKey(A::Toggle) == EKeys::I && Draft->GetKey(A::MoveForward) == EKeys::W);
	Click(TEXT("Cancel"));
	TestFalse(TEXT("Cancel exits capture"), Menu->IsCapturing());
	Capture(A::Toggle); Key(EKeys::W); Click(TEXT("Replace"));
	TestTrue(TEXT("Explicit replace assigns key"), Draft->GetKey(A::Toggle) == EKeys::W);
	TestFalse(TEXT("Explicit replace clears overlapping slot"), Draft->GetKey(A::MoveForward).IsValid());
	Capture(A::Toggle); Key(EKeys::K);
	TestTrue(TEXT("Nonconflicting key applies immediately"), Draft->GetKey(A::Toggle) == EKeys::K);
	Capture(A::Toggle); Key(EKeys::Escape);
	TestTrue(TEXT("Escape cancels capture and keeps Options"), Menu->IsOptionsOpen() && !Menu->IsCapturing());
	TestTrue(TEXT("Escape preserves prior assignment"), Draft->GetKey(A::Toggle) == EKeys::K);
	Click(TEXT("Item actions")); Capture(A::Primary); Key(EKeys::J);
	TestTrue(TEXT("Primary action can use arbitrary free keyboard key"), Draft->GetKey(A::Primary) == EKeys::J);
	Capture(A::Secondary);
	Menu->OnPreviewMouseButtonDown(G,FPointerEvent(0,FVector2D::ZeroVector,FVector2D::ZeroVector,TSet<FKey>(),EKeys::ThumbMouseButton,0,FModifierKeysState()));
	TestTrue(TEXT("Mouse action can be rebound"), Draft->GetKey(A::Secondary) == EKeys::ThumbMouseButton);
	Capture(A::Bandage); Key(EKeys::Q);
	TestTrue(TEXT("Different contexts can reuse a key"), Draft->GetKey(A::Bandage) == EKeys::Q && Draft->GetKey(A::TurnLeft) == EKeys::Q);
	Click(TEXT("Reset all controls"));
	TestTrue(TEXT("Reset all awaits confirmation"), Draft->GetKey(A::Toggle) == EKeys::K);
	Click(TEXT("Cancel")); TestTrue(TEXT("Cancelled reset preserves key"), Draft->GetKey(A::Toggle) == EKeys::K);
	Click(TEXT("Reset all controls")); Click(TEXT("Reset everything"));
	TestTrue(TEXT("Confirmed reset restores defaults"), Draft->GetKey(A::Toggle) == EKeys::I && Draft->GetKey(A::Primary) == EKeys::LeftMouseButton);
		TestEqual(TEXT("Draft edits do not notify controller"), Changes, 0);
	TestFalse(TEXT("Reset to original values clears dirty state"), Menu->HasUnsavedChanges());
	Capture(A::Primary); Key(EKeys::J);
	TestTrue(TEXT("Live settings unchanged before Apply"), Settings->GetKey(A::Primary) == EKeys::LeftMouseButton);
	Click(TEXT("Apply changes"));
	TestTrue(TEXT("Apply commits draft"), Settings->GetKey(A::Primary) == EKeys::J && !Menu->HasUnsavedChanges());
	TestEqual(TEXT("Apply notifies once"), Changes, 1);
	Capture(A::Primary); Key(EKeys::K);
	Click(TEXT("Back"));
	TestTrue(TEXT("Dirty exit requires decision"), Menu->IsConfirmingExit());
	Click(TEXT("Cancel"));
	TestTrue(TEXT("Cancel retains draft and returns to Options"), Menu->IsOptionsOpen() && !Menu->IsConfirmingExit() && Draft->GetKey(A::Primary) == EKeys::K);
	Key(EKeys::Escape); Key(EKeys::Escape);
	TestTrue(TEXT("Escape dismisses unsaved prompt without discarding"), !Menu->IsConfirmingExit() && Menu->HasUnsavedChanges());
	Key(EKeys::Escape); Click(TEXT("Cancel and exit"));
	TestTrue(TEXT("Discard leaves applied settings unchanged"), !Menu->IsOptionsOpen() && Settings->GetKey(A::Primary) == EKeys::J && !Menu->HasUnsavedChanges());
	Menu->ShowOptions(); Click(TEXT("Item actions")); Capture(A::Primary); Key(EKeys::K);
	Click(TEXT("Sound")); Click(TEXT("Back")); Click(TEXT("Save and exit"));
	TestTrue(TEXT("Save and exit works from another category"), !Menu->IsOptionsOpen() && Settings->GetKey(A::Primary) == EKeys::K);
	TestEqual(TEXT("Discard never notifies, save does"), Changes, 2);
	Menu->ShowOptions();
	Key(EKeys::Escape); TestFalse(TEXT("Back returns to Pause"), Menu->IsOptionsOpen());
	Key(EKeys::Escape,true); TestEqual(TEXT("Repeat does not resume"), ResumeCount,0);
	Key(EKeys::Escape); TestEqual(TEXT("Next Back resumes"), ResumeCount,1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuDefaultSettingsTest, "Prototype.Inventory.PauseMenuDefaultSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuDefaultSettingsTest::RunTest(const FString&)
{
	using FMenuArguments = SPauseMenu::FArguments;
	alignas(FMenuArguments) uint8 Storage[sizeof(FMenuArguments)];
	FMemory::Memset(Storage,0xcd,sizeof(Storage));
	auto* Args = new (Storage) FMenuArguments;
	const bool Safe = Args->_Controls == nullptr && Args->_ItemUse == nullptr;
	Args->~FMenuArguments();
	if (!TestTrue(TEXT("Omitted settings pointers initialize to null"),Safe)) return false;
	auto* Settings = GetMutableDefault<UInventoryInputSettings>();
	TStrongObjectPtr<UInventoryInputSettings> Previous(DuplicateObject<UInventoryInputSettings>(Settings,GetTransientPackage()));
	ON_SCOPE_EXIT
	{
		for (TFieldIterator<FProperty> P(Settings->GetClass()); P; ++P)
			if (P->HasAnyPropertyFlags(CPF_Config)) P->CopyCompleteValue_InContainer(Settings,Previous.Get());
	};
	Settings->ResetDefaults();
	FString Error;
	Settings->TrySetKey(EInventoryControl::Toggle,EKeys::K,Error);
	Settings->TurnSpeed = 210;
	for (int32 Open = 0; Open < 2; ++Open)
	{
		const auto Menu = SNew(SPauseMenu).SaveControls(false);
		Menu->ShowOptions(); Menu->ShowInventorySection(3);
		TestTrue(TEXT("Default-backed menu resolves saved binding"),Binding(Menu,EInventoryControl::Toggle,0,Settings).IsValid());
		const auto Reset = FindButton(Menu,TEXT("Reset this section"));
		if (!TestTrue(TEXT("Section reset exists"),Reset.IsValid())) return false;
		Reset->SimulateClick(); Reset->SimulateClick();
		TestTrue(TEXT("Repeated section reset restores defaults"),Menu->GetEditingControls()->GetKey(EInventoryControl::Toggle) == EKeys::I);
				TestTrue(TEXT("Draft reset leaves live key unchanged"),Settings->GetKey(EInventoryControl::Toggle) == EKeys::K);
		Menu->ShowInventorySection(2);
		Reset->SimulateClick();
		TestEqual(TEXT("Inventory reset restores draft rotation speed"),Menu->GetEditingControls()->TurnSpeed,120.f);
		TestEqual(TEXT("Live rotation speed unchanged"),Settings->TurnSpeed,210.f);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickItemBindingsTest, "Prototype.Inventory.QuickItemBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuickItemBindingsTest::RunTest(const FString&)
{
	using A = EInventoryControl;
	TStrongObjectPtr<UInventoryInputSettings> S(NewObject<UInventoryInputSettings>()); S->ResetDefaults();
	FString Error;
		TestTrue(TEXT("Bandage defaults to G"),S->GetKey(A::Bandage) == EKeys::G);
	TestTrue(TEXT("World pickup and held drop have separate defaults"), S->GetKey(A::PickUpWorld) == EKeys::E && S->GetKey(A::DropHeld) == EKeys::P);
	TestFalse(TEXT("Replacement starts disabled"), S->bAllowItemReplace);
	TestFalse(TEXT("Pickup key cannot also activate a quick item"), S->TrySetKey(A::Bandage,EKeys::E,Error));
	TestFalse(TEXT("Escape action cannot be changed"),S->AssignKey(A::Back,0,EKeys::K,true,Error));
	TestFalse(TEXT("Escape cannot be claimed"),S->AssignKey(A::Primary,0,EKeys::Escape,true,Error));
	TestTrue(TEXT("Escape remains fixed"),S->Matches(A::Back,EKeys::Escape));
	TestTrue(TEXT("Unimplemented bindings unavailable"),!S->IsBindable(A::Food) && !S->IsBindable(A::Water) && !S->IsBindable(A::ToggleBackpack));
	TestFalse(TEXT("Quick item conflicts with movement"),S->TrySetKey(A::Bandage,EKeys::W,Error));
	TestFalse(TEXT("Quick item conflicts with primary"),S->TrySetKey(A::Bandage,EKeys::LeftMouseButton,Error));
	TestTrue(TEXT("Quick item can share inventory rotation key"),S->TrySetKey(A::Bandage,EKeys::Q,Error));
	TestTrue(TEXT("Second slot accepts mouse button"),S->AssignKey(A::Bandage,1,EKeys::ThumbMouseButton,false,Error));
	TestTrue(TEXT("Both slots activate the same fixed action"),S->Matches(A::Bandage,EKeys::Q) && S->Matches(A::Bandage,EKeys::ThumbMouseButton));
	TestTrue(TEXT("Replace can claim other action's second slot"),S->AssignKey(A::Secondary,0,EKeys::ThumbMouseButton,true,Error));
	TestFalse(TEXT("Only colliding slot cleared"),S->GetKey(A::Bandage,1).IsValid());
	TestTrue(TEXT("Noncolliding primary remains"),S->GetKey(A::Bandage) == EKeys::Q);
	TestTrue(TEXT("Old transfer actions retired"),!S->IsBindable(A::ToHands) && !S->IsBindable(A::ToQuick) && !S->IsBindable(A::ToBackpack));
	S->AssignKey(A::Primary,0,EKeys::I,true,Error);
	TestFalse(TEXT("Section reset reports a cross-section collision"),S->ResetSection(EControlSection::Gameplay));
	TestTrue(TEXT("Section reset preserves other section's custom key"),S->GetKey(A::Primary) == EKeys::I);
	TestFalse(TEXT("Conflicting default left unbound"),S->GetKey(A::Toggle).IsValid());
	S->bAllowItemReplace = true;
	S->ResetSection(EControlSection::ItemActions);
	TestFalse(TEXT("Item actions reset disables optional replacement"), S->bAllowItemReplace);
	return true;
}
#endif
