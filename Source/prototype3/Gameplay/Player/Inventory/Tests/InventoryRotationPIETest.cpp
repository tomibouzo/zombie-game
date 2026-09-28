#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "UI/Inventory/InventoryDemoWidget.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

/** Routes input through Slate and waits for actual PIE frames. Never calls Panel->Tick. */
class FInventoryRotationPIECheck : public IAutomationLatentCommand
{
public:
	explicit FInventoryRotationPIECheck(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual ~FInventoryRotationPIECheck() override
	{
		if (SavedSettings.IsValid())
		{
			auto* Settings = GetMutableDefault<UInventoryInputSettings>();
			for (TFieldIterator<FProperty> P(Settings->GetClass()); P; ++P)
				if (P->HasAnyPropertyFlags(CPF_Config)) P->CopyCompleteValue_InContainer(Settings, SavedSettings.Get());
			FSlateApplication::Get().ReleaseAllPointerCapture();
			FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(bPreviousBackgroundInput);
		}
	}

	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* Controller = World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!Controller)
		{
			if (FPlatformTime::Seconds() - Started < 60) return false;
			Test->AddError(TEXT("Rotation test could not start PIE."));
			return true;
		}
		auto& App = FSlateApplication::Get();
		auto* Settings = GetMutableDefault<UInventoryInputSettings>();
		if (Phase == 0)
		{
			SavedSettings.Reset(DuplicateObject<UInventoryInputSettings>(Settings, GetTransientPackage()));
			bPreviousBackgroundInput = App.GetHandleDeviceInputWhenApplicationNotActive();
			// Offscreen verification must still apply input replies, including pointer capture.
			App.SetHandleDeviceInputWhenApplicationNotActive(true);
			Controller->ToggleInventoryDemo();
			Phase = 1;
			return false;
		}
		if (Phase == 1)
		{
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UInventoryDemoWidget::StaticClass(), false);
			for (UUserWidget* Widget : Widgets)
			{
				if (!Widget->IsInViewport()) continue;
				Panel = StaticCastSharedPtr<SInventoryPanel>(CastChecked<UInventoryDemoWidget>(Widget)->GetInventoryFocusTarget());
				ForEachObjectWithOuter(Widget, [&](UObject* Object)
				{
					if (auto* Component = Cast<UInventoryComponent>(Object)) Inventory = Component;
				});
			}
			if (!Panel.IsValid() || !Inventory.IsValid())
			{
				Test->AddError(TEXT("Rotation test could not find the live inventory panel."));
				Controller->CloseInventoryDemo();
				return true;
			}
			ItemId = Inventory->GetEntries()[0].Item.InstanceId;
			Phase = 2;
		}
		if (Phase == 2)
		{
			if (Case >= 9)
			{
				Controller->CloseInventoryDemo();
				return true;
			}
			Settings->ResetDefaults();
			Settings->TurnSpeed = 90;
			Settings->bToggleGrab = Case == 4;
			FString Error;
			if (Case == 2) Settings->TrySetKey(EInventoryControl::TurnRight, EKeys::MiddleMouseButton, Error);
			if (Case == 3) Settings->TrySetKey(EInventoryControl::TurnRight, EKeys::MouseScrollUp, Error);
			Inventory->GetItem(ItemId, Before);
			Pointer = Before.Position + FVector2D(40,150);
			MouseButton(EKeys::LeftMouseButton, true);
			Test->TestTrue(Label(TEXT("grab owns capture")), Panel->HasMouseCapture());
			if (Case == 4)
			{
				MouseButton(EKeys::LeftMouseButton, false);
				Test->TestTrue(Label(TEXT("toggle grab survives button release")), Panel->HasMouseCapture());
			}
			Pointer = FVector2D(370.25,630.75);
			RouteMove();
			if (Case == 2) MouseButton(EKeys::MiddleMouseButton, true);
			else if (Case == 3) RouteWheel();
			else App.ProcessKeyDownEvent(FKeyEvent(Case == 1 ? EKeys::Q : EKeys::E, FModifierKeysState(), 0, false, 0, 0));
			WaitUntil = FPlatformTime::Seconds() + 0.25;
			Phase = 3;
			return false;
		}
		if (Phase == 3)
		{
			if (FPlatformTime::Seconds() < WaitUntil) return false;
			if (Case == 2) MouseButton(EKeys::MiddleMouseButton, false);
			else if (Case != 3) App.ProcessKeyUpEvent(FKeyEvent(Case == 1 ? EKeys::Q : EKeys::E, FModifierKeysState(), 0, false, 0, 0));
			Test->TestTrue(Label(TEXT("rotation release preserves capture")), Panel->HasMouseCapture());
			// Wait again: releasing rotation must not drop the still-held object.
			WaitUntil = FPlatformTime::Seconds() + 0.1;
			Phase = 4;
			return false;
		}
		if (FPlatformTime::Seconds() < WaitUntil) return false;
		if (Case == 5) { MouseButton(EKeys::RightMouseButton, true); MouseButton(EKeys::RightMouseButton, false); }
		if (Case == 6) App.ReleaseAllPointerCapture();
		if (Case == 7) { Pointer = FVector2D(45,155); RouteMove(); }
		if (Case == 8) App.ClearKeyboardFocus();
		if (Case == 4) MouseButton(EKeys::LeftMouseButton, true);
		MouseButton(EKeys::LeftMouseButton, false);
		FInventoryEntry After;
		Inventory->GetItem(ItemId, After);
		if (Case >= 5)
		{
			Test->TestEqual(Label(TEXT("cancel/invalid drop preserves position")), After.Position, Before.Position);
			Test->TestEqual(Label(TEXT("cancel/invalid drop preserves angle")), After.AngleDegrees, Before.AngleDegrees);
		}
		else
		{
			Test->TestTrue(Label(TEXT("drag commits after rotation")), After.Position.Equals(FVector2D(330.25,480.75), 0.01));
			const double Change = FMath::FindDeltaAngleDegrees(Before.AngleDegrees, After.AngleDegrees);
			if (Case == 3) Test->TestTrue(Label(TEXT("wheel rotates two degrees")), FMath::IsNearlyEqual(Change, 2.0, 0.01));
			else Test->TestTrue(Label(TEXT("held input rotates over real frames in expected direction")), Case == 1 ? Change < -1 : Change > 1);
		}
		Test->TestFalse(Label(TEXT("gesture end releases capture")), Panel->HasMouseCapture());
		Test->TestEqual(Label(TEXT("no objects lost")), Inventory->GetEntries().Num(), 5);
		++Case;
		Phase = 2;
		return false;
	}
private:
	FString Label(const TCHAR* Message) const { return FString::Printf(TEXT("Rotation case %d: %s"), Case, Message); }
	FWidgetPath Path() const
	{
		FWidgetPath Result;
		FSlateApplication::Get().GeneratePathToWidgetChecked(Panel.ToSharedRef(), Result);
		// Generated focus paths lack the per-widget pointer slots required by input routing.
		TArray<FWidgetAndPointer> PointerPath;
		for (int32 Index = 0; Index < Result.Widgets.Num(); ++Index)
			PointerPath.Emplace(Result.Widgets[Index], TOptional<FVirtualPointerPosition>());
		return FWidgetPath(PointerPath);
	}
	FPointerEvent MouseEvent(FKey Button, float Wheel = 0) const
	{
		const FVector2D Absolute = Panel->GetCachedGeometry().LocalToAbsolute(Pointer);
		return FPointerEvent(0, FSlateApplication::CursorPointerIndex, Absolute, Absolute, Buttons, Button, Wheel, FModifierKeysState());
	}
	void MouseButton(FKey Button, bool Down)
	{
		if (Down) Buttons.Add(Button); else Buttons.Remove(Button);
		if (Down) FSlateApplication::Get().RoutePointerDownEvent(Path(), MouseEvent(Button));
		else FSlateApplication::Get().RoutePointerUpEvent(Path(), MouseEvent(Button));
	}
	void RouteMove() { FSlateApplication::Get().RoutePointerMoveEvent(Path(), MouseEvent(EKeys::Invalid), false); }
	void RouteWheel() { FSlateApplication::Get().RouteMouseWheelOrGestureEvent(Path(), MouseEvent(EKeys::Invalid, 1)); }
	FAutomationTestBase* Test;
	TStrongObjectPtr<UInventoryInputSettings> SavedSettings;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TSharedPtr<SInventoryPanel> Panel;
	FGuid ItemId;
	FInventoryEntry Before;
	TSet<FKey> Buttons;
	FVector2D Pointer;
	double Started;
	double WaitUntil = 0;
	int32 Phase = 0;
	int32 Case = 0;
	bool bPreviousBackgroundInput = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryRotationPIETest, "Prototype.Inventory.RotationPlayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryRotationPIETest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("InventoryPIE")))
	{
		AddInfo(TEXT("Rotation PIE integration skipped: use -InventoryPIE in a fresh verification editor."));
		return true;
	}
	if (GEditor->PlayWorld) { AddError(TEXT("Run in a fresh editor outside an existing Play session.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/FirstPerson/Lvl_FirstPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FInventoryRotationPIECheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
