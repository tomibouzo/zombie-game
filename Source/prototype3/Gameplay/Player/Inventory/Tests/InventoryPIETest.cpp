#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "UI/Inventory/InventoryDemoWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "UI/Inventory/InventoryInputSettings.h"

class FInventoryPIECheck : public IAutomationLatentCommand
{
public:
	explicit FInventoryPIECheck(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual ~FInventoryPIECheck() override
	{
		if (OriginalToggle.IsValid())
		{
			FString Error;
			GetMutableDefault<UInventoryInputSettings>()->TrySetKey(EInventoryControl::Toggle, OriginalToggle, Error);
		}
	}
	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		Aprototype3PlayerController* Controller = World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!Controller)
		{
			if (FPlatformTime::Seconds() - Started < 60) return false;
			Test->AddError(TEXT("PIE did not create the expected player controller within 60 seconds."));
			return true;
		}
		if (Phase == 0)
		{
			bOriginalCursor = Controller->bShowMouseCursor;
			auto* Settings = GetMutableDefault<UInventoryInputSettings>();
			OriginalToggle = Settings->GetKey(EInventoryControl::Toggle);
			FString Error;
			for (FKey Candidate : { EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10 })
				if (Candidate != OriginalToggle && Settings->TrySetKey(EInventoryControl::Toggle, Candidate, Error)) break;
			Test->TestTrue(TEXT("Toggle is rebound during Play"), Settings->GetKey(EInventoryControl::Toggle) != OriginalToggle);
			OpenWithInput(Controller);
			++Phase;
			return false;
		}
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UInventoryDemoWidget::StaticClass(), false);
		int32 Visible = 0;
		for (UUserWidget* Widget : Widgets) if (Widget->IsInViewport()) ++Visible;
		if (Phase == 1 || Phase == 3)
		{
			Test->TestEqual(TEXT("One inventory screen is in the game viewport"), Visible, 1);
			Test->TestTrue(TEXT("Cursor enabled"), Controller->bShowMouseCursor);
			Test->TestTrue(TEXT("Movement disabled while UI is open"), Controller->IsMoveInputIgnored());
			const auto Focus = FSlateApplication::Get().GetKeyboardFocusedWidget();
			Test->TestTrue(TEXT("Keyboard focus reaches the inventory panel"), Focus.IsValid() && Focus->GetType() == TEXT("SInventoryPanel"));
			FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::Toggle), FModifierKeysState(), 0, false, 0, 0));
			++Phase;
			return false;
		}
		Test->TestEqual(TEXT("Rebound key removes the screen"), Visible, 0);
		Test->TestEqual(TEXT("Cursor restored"), Controller->bShowMouseCursor, bOriginalCursor);
		Test->TestFalse(TEXT("Movement restored"), Controller->IsMoveInputIgnored());
		Test->TestFalse(TEXT("Look restored"), Controller->IsLookInputIgnored());
		if (Phase == 2)
		{
			OpenWithInput(Controller);
			++Phase;
			return false;
		}
		Controller->CloseInventoryDemo();
		return true;
	}
private:
	void OpenWithInput(Aprototype3PlayerController* Controller)
	{
		FInputKeyEventArgs Event;
		Event.Key = GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::Toggle);
		Event.Event = IE_Pressed;
		Controller->InputKey(Event);
	}
	FKey OriginalToggle;
	FAutomationTestBase* Test;
	double Started;
	int32 Phase = 0;
	bool bOriginalCursor = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryPIETest, "Prototype.Inventory.PlayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryPIETest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("InventoryPIE")))
	{
		AddInfo(TEXT("PIE integration skipped: use -InventoryPIE in a fresh verification editor."));
		return true;
	}
	if (GEditor->PlayWorld) { AddError(TEXT("Run this test in a fresh editor, outside an existing Play session.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/FirstPerson/Lvl_FirstPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FInventoryPIECheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
