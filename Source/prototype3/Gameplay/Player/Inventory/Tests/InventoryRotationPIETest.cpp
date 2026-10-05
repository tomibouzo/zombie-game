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
			Controller->ToggleInventoryDemo(true); // Keep the established 560x560 rotation regression fixture.
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
			Panel->SetSaveControls(false);
			ItemId = Inventory->GetEntries()[0].Item.InstanceId;
			InitialItemCount = Inventory->GetEntries().Num();
			Phase = 2;
		}
		if (Case >= 18)
		{
			Controller->CloseInventoryDemo();
			return true;
		}
		if (Case >= 9) return UpdateMouseRotation();
		if (Phase == 2)
		{
			Settings->ResetDefaults();
			Settings->TurnSpeed = 90;
			Settings->bToggleGrab = Case == 4;
			FString Error;
			if (Case == 2) Settings->TrySetKey(EInventoryControl::TurnRight, EKeys::ThumbMouseButton, Error);
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
			if (Case == 2) MouseButton(EKeys::ThumbMouseButton, true);
			else
			{
				if (Case == 3)
				{
					RouteWheel();
					Test->TestEqual(Label(TEXT("wheel raises speed during drag")), Settings->TurnSpeed, 105.f);
				}
				App.ProcessKeyDownEvent(FKeyEvent(Case == 1 ? EKeys::Q : EKeys::E, FModifierKeysState(), 0, false, 0, 0));
			}
			WaitUntil = FPlatformTime::Seconds() + 0.25;
			Phase = 3;
			return false;
		}
		if (Phase == 3)
		{
			if (FPlatformTime::Seconds() < WaitUntil) return false;
			if (Case == 2) MouseButton(EKeys::ThumbMouseButton, false);
			else App.ProcessKeyUpEvent(FKeyEvent(Case == 1 ? EKeys::Q : EKeys::E, FModifierKeysState(), 0, false, 0, 0));
			Test->TestTrue(Label(TEXT("rotation release preserves capture")), Panel->HasMouseCapture());
			// Wait again: releasing rotation must not drop the still-held object.
			WaitUntil = FPlatformTime::Seconds() + 0.1;
			Phase = 4;
			return false;
		}
		if (FPlatformTime::Seconds() < WaitUntil) return false;
		if (Case == 5) { MouseButton(EKeys::MiddleMouseButton, true); MouseButton(EKeys::MiddleMouseButton, false); }
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
			Test->TestTrue(Label(TEXT("held input rotates over real frames in expected direction")), Case == 1 ? Change < -1 : Change > 1);
		}
		Test->TestFalse(Label(TEXT("gesture end releases capture")), Panel->HasMouseCapture());
		Test->TestEqual(Label(TEXT("no objects lost")), Inventory->GetEntries().Num(), InitialItemCount);
		++Case;
		Phase = 2;
		return false;
	}
private:
	bool UpdateMouseRotation()
	{
		auto& App = FSlateApplication::Get();
		auto* Settings = GetMutableDefault<UInventoryInputSettings>();
		const FVector2D Center(330,480), Pivot = Center + FVector2D(40,150);
		auto RotationButton = [&](bool Down)
		{
			if (Case != 13) MouseButton(EKeys::RightMouseButton, Down);
			else if (Down) App.ProcessKeyDownEvent(FKeyEvent(EKeys::Z,FModifierKeysState(),0,false,0,0));
			else App.ProcessKeyUpEvent(FKeyEvent(EKeys::Z,FModifierKeysState(),0,false,0,0));
		};
		if (Phase == 2)
		{
			Settings->ResetDefaults();
			Settings->bToggleGrab = Case == 14;
			FString Error;
			if (Case == 13) Test->TestTrue(Label(TEXT("rebind mouse rotation")),Settings->TrySetKey(EInventoryControl::RotateWithMouse,EKeys::Z,Error));
			Test->TestTrue(Label(TEXT("reset placement")),Inventory->MoveItem(ItemId,TEXT("Main"),Center,350)==EInventoryResult::Success);
			Inventory->GetItem(ItemId, Before);
			Pointer = Pivot + (Case == 11 ? FVector2D::ZeroVector : FVector2D(10,0));
			MouseButton(EKeys::LeftMouseButton,true);
			if (Case == 14) MouseButton(EKeys::LeftMouseButton,false);
			RotationButton(true);
			Pointer = Pivot + FVector2D(70,0); RouteMove();
			Pointer = Pivot + FVector2D(0,70); RouteMove();
			Test->TestTrue(Label(TEXT("both buttons preserve capture")),Panel->HasMouseCapture());
			WaitUntil = FPlatformTime::Seconds() + 0.15;
			Phase = 3;
			return false;
		}
		if (FPlatformTime::Seconds() < WaitUntil) return false;
		if (Phase == 3)
		{
			FInventoryEntry Preview;
			Inventory->GetItem(ItemId,Preview);
			Test->TestEqual(Label(TEXT("rotation preview preserves stored angle")),Preview.AngleDegrees,Before.AngleDegrees);
			if (Case == 12) { MouseButton(EKeys::MiddleMouseButton,true); MouseButton(EKeys::MiddleMouseButton,false); }
			else if (Case == 15) App.ClearKeyboardFocus();
			else if (Case == 16) App.ReleaseAllPointerCapture();
			else if (Case != 10)
			{
				RotationButton(false);
				Test->TestTrue(Label(TEXT("release rotation keeps item held")),Panel->HasMouseCapture());
				if (Case == 9 || Case == 13 || Case == 14) { Pointer += FVector2D(-20,-20); RouteMove(); }
				if (Case == 17) { Pointer = FVector2D(0,0); RouteMove(); }
			}
			WaitUntil = FPlatformTime::Seconds() + 0.1;
			Phase = 4;
			return false;
		}
		if (Case == 14) MouseButton(EKeys::LeftMouseButton,true);
		MouseButton(EKeys::LeftMouseButton,false);
		if (Case == 10 || Case == 12 || Case == 15 || Case == 16) RotationButton(false);
		FInventoryEntry After;
		Inventory->GetItem(ItemId,After);
		const bool Cancelled = Case == 12 || Case >= 15;
		const FVector2D ExpectedCenter = Case == 9 || Case == 13 || Case == 14 ? Center + FVector2D(-20,-20) : Center;
		Test->TestTrue(Label(TEXT("mouse rotation position")),After.Position.Equals(Cancelled ? Before.Position : ExpectedCenter,0.001));
		Test->TestTrue(Label(TEXT("mouse rotation angle")),FMath::IsNearlyEqual(After.AngleDegrees,Cancelled ? Before.AngleDegrees : 80.0,0.001));
		Test->TestFalse(Label(TEXT("gesture releases capture")),Panel->HasMouseCapture());
		Test->TestEqual(Label(TEXT("no objects lost")),Inventory->GetEntries().Num(),InitialItemCount);
		++Case;
		Phase = 2;
		return false;
	}

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
	int32 InitialItemCount = 0;
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
