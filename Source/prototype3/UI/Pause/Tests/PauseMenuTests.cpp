#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Pause/SPauseMenu.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

namespace
{
bool ContainsText(const TSharedRef<SWidget>& Widget, const FString& Text)
{
	if (Widget->GetType() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Text) return true;
	FChildren* Children = Widget->GetChildren();
	for (int32 I = 0; I < Children->Num(); ++I)
		if (ContainsText(Children->GetChildAt(I), Text)) return true;
	return false;
}

TSharedPtr<SButton> FindButton(const TSharedRef<SWidget>& Widget, const FString& Text)
{
	if (Widget->GetType() == TEXT("SButton") && ContainsText(Widget, Text)) return StaticCastSharedRef<SButton>(Widget);
	FChildren* Children = Widget->GetChildren();
	for (int32 I = 0; I < Children->Num(); ++I)
		if (const auto Button = FindButton(Children->GetChildAt(I), Text)) return Button;
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuControlsTest, "Prototype.Inventory.PauseMenuControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuControlsTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	TStrongObjectPtr<UPlayerItemUseComponent> Use(NewObject<UPlayerItemUseComponent>());
	Use->Shortcuts = NewObject<UItemUseSettings>(Use.Get());
	Use->bSavePreferences = false;
	Use->Shortcuts->Keys = { EKeys::E, EKeys::F, EKeys::G };
	Use->Shortcuts->ItemTypes = { FName(TEXT("Bandage")), FName(TEXT("CannedBeans")), FName(TEXT("WaterBottle")) };
	const auto AssignedItems = Use->Shortcuts->ItemTypes;
	int32 ResumeCount = 0, ExitCount = 0;
	const auto Menu = SNew(SPauseMenu).Controls(Settings.Get()).ItemUse(Use.Get()).SaveControls(false)
		.CanExitGame(true).CanExitDesktop(false)
		.OnResume(FSimpleDelegate::CreateLambda([&]() { ++ResumeCount; }))
		.OnExitGame(FSimpleDelegate::CreateLambda([&]() { ++ExitCount; }));
	auto Click = [&](const FString& Label)
	{
		const auto Button = FindButton(Menu, Label);
		if (!TestTrue(*FString::Printf(TEXT("Button exists: %s"), *Label), Button.IsValid())) return false;
		Button->SimulateClick();
		return true;
	};
	const FGeometry G = FGeometry::MakeRoot(FVector2D(760,680), FSlateLayoutTransform());
	auto Key = [&](FKey K, bool bRepeat = false) { Menu->OnPreviewKeyDown(G, FKeyEvent(K, FModifierKeysState(),0,bRepeat,0,0)); };
	const auto Desktop = FindButton(Menu, TEXT("Exit to Desktop"));
	TestTrue(TEXT("Desktop exit is disabled in the editor menu"), Desktop.IsValid() && !Desktop->IsEnabled());
	Click(TEXT("Exit Game"));
	TestEqual(TEXT("Exit Game invokes the stop-Play action"), ExitCount, 1);
	if (!Click(TEXT("Options"))) return false;
	TestTrue(TEXT("Options opens"), Menu->IsOptionsOpen());
	if (!Click(TEXT("I"))) return false;
	Key(EKeys::E);
	TestTrue(TEXT("Inventory-open key cannot shadow a gameplay shortcut"),Settings->GetKey(EInventoryControl::Toggle)==EKeys::I);
	Key(EKeys::K);
	TestTrue(TEXT("Options changes inventory-open key"),Settings->GetKey(EInventoryControl::Toggle)==EKeys::K);
	if (!Click(TEXT("K"))) return false;
	Key(EKeys::Escape);
	TestFalse(TEXT("Escape closes Options while capturing a key"),Menu->IsOptionsOpen());
	TestTrue(TEXT("Escape does not replace the captured binding"),Settings->GetKey(EInventoryControl::Toggle)==EKeys::K);
	TestEqual(TEXT("Closing Options stays paused"),ResumeCount,0);
	Key(EKeys::Escape,true);
	TestEqual(TEXT("Held Escape does not resume twice"),ResumeCount,0);
	Key(EKeys::Escape);
	TestEqual(TEXT("Escape from Pause resumes"),ResumeCount,1);
	Click(TEXT("Options"));
	if (!Click(EKeys::RightMouseButton.GetDisplayName().ToString())) return false;
	Menu->OnPreviewMouseButtonDown(G,FPointerEvent(0,FVector2D::ZeroVector,FVector2D::ZeroVector,TSet<FKey>(),EKeys::ThumbMouseButton,0,FModifierKeysState()));
	TestTrue(TEXT("Options accepts mouse bindings"),Settings->GetKey(EInventoryControl::RotateWithMouse)==EKeys::ThumbMouseButton);
	Use->Shortcuts->Keys[0] = EKeys::J;
	Settings->TurnSpeed = 210;
	Click(TEXT("Reset controls to default"));
	TestTrue(TEXT("Reset restores inventory and shortcut keys"),Settings->GetKey(EInventoryControl::Toggle)==EKeys::I && Use->Shortcuts->Keys[0]==EKeys::E);
	TestEqual(TEXT("Reset restores saved rotation speed"),Settings->TurnSpeed,120.f);
	TestTrue(TEXT("Reset preserves assigned item types"),Use->Shortcuts->ItemTypes==AssignedItems);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuDefaultSettingsTest, "Prototype.Inventory.PauseMenuDefaultSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuDefaultSettingsTest::RunTest(const FString&)
{
	// Nonzero storage makes omitted raw-pointer arguments deterministic, instead of
	// accidentally passing when a fresh stack page happens to contain zeroes.
	using FMenuArguments = SPauseMenu::FArguments;
	alignas(FMenuArguments) uint8 Storage[sizeof(FMenuArguments)];
	FMemory::Memset(Storage, 0xcd, sizeof(Storage));
	FMenuArguments* Args = new (Storage) FMenuArguments;
	const bool bControlsNull = Args->_Controls == nullptr;
	const bool bItemUseNull = Args->_ItemUse == nullptr;
	Args->~FMenuArguments();
	TestTrue(TEXT("Omitted settings pointer defaults to null"), bControlsNull);
	TestTrue(TEXT("Omitted item-use pointer defaults to null"), bItemUseNull);
	if (!bControlsNull || !bItemUseNull) return false; // Do not dereference poisoned arguments.

	// Match the controller: omit Controls so the menu resolves the saved defaults.
	// Preserve the process defaults and never write the player's preferences.
	auto* Settings = GetMutableDefault<UInventoryInputSettings>();
	TStrongObjectPtr<UInventoryInputSettings> Previous(DuplicateObject<UInventoryInputSettings>(Settings, GetTransientPackage()));
	ON_SCOPE_EXIT
	{
		for (TFieldIterator<FProperty> Property(Settings->GetClass()); Property; ++Property)
			if (Property->HasAnyPropertyFlags(CPF_Config)) Property->CopyCompleteValue_InContainer(Settings, Previous.Get());
	};
	Settings->ResetDefaults();
	FString Error;
	Settings->TrySetKey(EInventoryControl::Toggle, EKeys::K, Error);
	Settings->TurnSpeed = 210;
	TStrongObjectPtr<UPlayerItemUseComponent> Use(NewObject<UPlayerItemUseComponent>());
	Use->Shortcuts = NewObject<UItemUseSettings>(Use.Get());
	Use->bSavePreferences = false;
	Use->Shortcuts->Keys = { EKeys::J, EKeys::F, EKeys::G };
	for (int32 Open = 0; Open < 2; ++Open)
	{
		const auto Menu = SNew(SPauseMenu).ItemUse(Use.Get()).SaveControls(false);
		Menu->ShowOptions();
		for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
		{
			const auto Action = static_cast<EInventoryControl>(I);
			if (Action == EInventoryControl::Add) continue;
			TestTrue(TEXT("Default-backed menu displays every configured key"),
				FindButton(Menu, Settings->GetKey(Action).GetDisplayName().ToString()).IsValid());
		}
		const auto Reset = FindButton(Menu, TEXT("Reset controls to default"));
		if (!TestTrue(TEXT("Default-backed menu has reset control"), Reset.IsValid())) return false;
		Reset->SimulateClick();
		Reset->SimulateClick();
		TestTrue(TEXT("Repeated reset restores and displays defaults"), Settings->GetKey(EInventoryControl::Toggle)==EKeys::I
			&& FindButton(Menu, TEXT("I")).IsValid() && Use->Shortcuts->Keys[0]==EKeys::E);
		TestEqual(TEXT("Default-backed reset restores speed"), Settings->TurnSpeed,120.f);
	}
	return true;
}
#endif
