// Copyright Epic Games, Inc. All Rights Reserved.


#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Core/Camera/prototype3CameraManager.h"
#include "Core/Characters/prototype3Character.h"
#include "Blueprint/UserWidget.h"
#include "prototype3.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/Inventory/InventoryDemoWidget.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "InputKeyEventArgs.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "UI/Pause/SPauseMenu.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Kismet/KismetSystemLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
UPlayerItemUseComponent* ItemUse(APlayerController* Controller)
{
	return Controller && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UPlayerItemUseComponent>() : nullptr;
}

// Consume Escape before the Editor's Stop Play shortcut, only while this game's
// viewport or one of its interfaces has focus. Never changes Editor preferences.
class FGameInterfaceInputProcessor : public IInputProcessor
{
public:
	explicit FGameInterfaceInputProcessor(Aprototype3PlayerController* InController) : Controller(InController) {}
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (!Controller.IsValid() || !Controller->HasInterfaceFocus(Event.GetUserIndex())) return false;
		const bool bBack = Controller->IsAssigningControls() ? Event.GetKey() == EKeys::Escape
			: GetDefault<UInventoryInputSettings>()->Matches(EInventoryControl::Back, Event.GetKey());
		if (!bBack) return Controller->HandleInventoryControlKey(Event.GetKey(), Event.IsRepeat() ? IE_Repeat : IE_Pressed);
		bConsumedEscape = true;
		ConsumedKey = Event.GetKey();
		if (!Event.IsRepeat()) Controller->HandleInterfaceEscape();
		return true;
	}
	virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		if (Controller.IsValid() && Controller->HasInterfaceFocus(Event.GetUserIndex())
			&& Controller->HandleInventoryControlKey(Event.GetKey(), IE_Released)) return true;
		if (Event.GetKey() != ConsumedKey || !bConsumedEscape) return false;
		bConsumedEscape = false;
		return true;
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		return Controller.IsValid() && Controller->HasInterfaceFocus(Event.GetUserIndex())
			&& Controller->HandleInventoryControlKey(Event.GetEffectingButton(), IE_Pressed);
	}
	virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& Event) override
	{
		return Controller.IsValid() && Controller->HasInterfaceFocus(Event.GetUserIndex())
			&& Controller->HandleInventoryControlKey(Event.GetEffectingButton(), IE_Released);
	}
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App, const FPointerEvent& Event) override
	{
		return HandleMouseButtonDownEvent(App, Event);
	}
private:
	TWeakObjectPtr<Aprototype3PlayerController> Controller;
	bool bConsumedEscape = false;
	FKey ConsumedKey;
};
}

Aprototype3PlayerController::Aprototype3PlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = Aprototype3CameraManager::StaticClass();

	RuntimeRunAction = CreateDefaultSubobject<UInputAction>(TEXT("RunAction"));
	RuntimeRunAction->ValueType = EInputActionValueType::Boolean;
	RuntimeRunAction->bConsumeInput = true;

	RuntimeCrouchAction = CreateDefaultSubobject<UInputAction>(TEXT("CrouchAction"));
	RuntimeCrouchAction->ValueType = EInputActionValueType::Boolean;
	RuntimeCrouchAction->bConsumeInput = true;

}

UInputAction* Aprototype3PlayerController::GetPrimaryAction()
{
	if (!RuntimePrimaryAction || RuntimePrimaryAction->HasAnyFlags(RF_DefaultSubObject)
		|| RuntimePrimaryAction->GetOuter() != this)
	{
		RuntimePrimaryAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		RuntimePrimaryAction->ValueType = EInputActionValueType::Boolean;
		RuntimePrimaryAction->bConsumeInput = true;
	}
	return RuntimePrimaryAction.Get();
}

UInputAction* Aprototype3PlayerController::GetSecondaryAction()
{
	if (!RuntimeSecondaryAction || RuntimeSecondaryAction->HasAnyFlags(RF_DefaultSubObject)
		|| RuntimeSecondaryAction->GetOuter() != this)
	{
		RuntimeSecondaryAction = NewObject<UInputAction>(this, NAME_None, RF_Transient);
		RuntimeSecondaryAction->ValueType = EInputActionValueType::Boolean;
		RuntimeSecondaryAction->bConsumeInput = true;
	}
	return RuntimeSecondaryAction.Get();
}

void Aprototype3PlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && FSlateApplication::IsInitialized())
	{
		InterfaceInputProcessor = MakeShared<FGameInterfaceInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InterfaceInputProcessor, 0);
	}
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logprototype3, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void Aprototype3PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	RebuildControls();
}

void Aprototype3PlayerController::RebuildControls()
{
	if (!IsLocalPlayerController()) return;
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem) return;
	FlushPressedKeys();
	for (const auto& Context : FilteredMappingContexts) Subsystem->RemoveMappingContext(Context);
	FilteredMappingContexts.Empty();
	TArray<UInputMappingContext*> Contexts = DefaultMappingContexts;
	if (!ShouldUseTouchControls()) Contexts.Append(MobileExcludedMappingContexts);
	for (auto* Source : Contexts)
	{
		if (!Source) continue;
		Subsystem->RemoveMappingContext(Source);
		auto* Copy = DuplicateObject<UInputMappingContext>(Source, this);
		// Keyboard/mouse buttons are defined exclusively by the saved controls.
		// Retain analog look and platform/gamepad mappings without modifying assets.
		const auto Mappings = Copy->GetMappings();
		for (const auto& Mapping : Mappings)
			if (Mapping.Key.IsDigital() && !Mapping.Key.IsGamepadKey() && !Mapping.Key.IsTouch())
				Copy->UnmapKey(Mapping.Action, Mapping.Key);
		FilteredMappingContexts.Add(Copy);
		Subsystem->AddMappingContext(Copy, 0);
	}
	if (!RuntimeSprintMappingContext) RuntimeSprintMappingContext = NewObject<UInputMappingContext>(this);
	else Subsystem->RemoveMappingContext(RuntimeSprintMappingContext);
	RuntimeSprintMappingContext->UnmapAll();
	RuntimeSprintAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Sprint.IA_Sprint"));
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	auto Map = [&](EInventoryControl Action, UInputAction* Input)
	{
		if (!Input) return;
		for (int32 Slot = 0; Slot < 2; ++Slot)
			if (const FKey Key = Controls->GetKey(Action, Slot); Key.IsValid()) RuntimeSprintMappingContext->MapKey(Input, Key);
	};
	Map(EInventoryControl::Run, RuntimeRunAction);
	Map(EInventoryControl::Sprint, RuntimeSprintAction);
	Map(EInventoryControl::Crouch, RuntimeCrouchAction);
	Map(EInventoryControl::Primary, GetPrimaryAction());
	Map(EInventoryControl::Secondary, GetSecondaryAction());
	if (auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move")))
	{
		for (auto Direction : { EInventoryControl::MoveForward, EInventoryControl::MoveBackward, EInventoryControl::MoveLeft, EInventoryControl::MoveRight })
			for (int32 Slot = 0; Slot < 2; ++Slot)
			{
				const FKey Key = Controls->GetKey(Direction, Slot);
				if (!Key.IsValid()) continue;
				auto& Mapping = RuntimeSprintMappingContext->MapKey(Move, Key);
				if (Direction == EInventoryControl::MoveBackward || Direction == EInventoryControl::MoveLeft)
					Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(RuntimeSprintMappingContext));
				if (Direction == EInventoryControl::MoveForward || Direction == EInventoryControl::MoveBackward)
				{
					auto* Axis = NewObject<UInputModifierSwizzleAxis>(RuntimeSprintMappingContext);
					Axis->Order = EInputAxisSwizzle::YXZ;
					Mapping.Modifiers.Add(Axis);
				}
			}
	}
	Subsystem->AddMappingContext(RuntimeSprintMappingContext, 1);
}

bool Aprototype3PlayerController::IsAssigningControls() const { return PauseMenu.IsValid() && PauseMenu->IsCapturing(); }

bool Aprototype3PlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void Aprototype3PlayerController::FlushPressedKeys()
{
	QuickKeysDown.Empty();
	BackpackKeysDown.Empty();
	if (auto* Use = ItemUse(this)) Use->CancelUse();
	if (auto* PlayerCharacter = Cast<Aprototype3Character>(GetPawn())) PlayerCharacter->ClearControlIntents();
	Super::FlushPressedKeys();
}

TArray<FKey> Aprototype3PlayerController::HeldMovementKeys() const
{
	TArray<FKey> Keys;
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	for (auto Action : { EInventoryControl::MoveForward, EInventoryControl::MoveBackward, EInventoryControl::MoveLeft, EInventoryControl::MoveRight })
		for (int32 Slot = 0; Slot < 2; ++Slot)
			if (const FKey Key = Controls->GetKey(Action, Slot); Key.IsValid() && IsInputKeyDown(Key)) Keys.AddUnique(Key);
	return Keys;
}

void Aprototype3PlayerController::RestoreMovementKeys(const TArray<FKey>& Keys)
{
	for (FKey Key : Keys) Super::InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
}

bool Aprototype3PlayerController::HandleInventoryControlKey(FKey Key, EInputEvent Event)
{
	if (!bInventoryDemoOpen || PauseMenu.IsValid()) return false;
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	bool bRoute = Controls->Matches(EInventoryControl::Toggle, Key);
	if (const auto* Use = ItemUse(this); Use && Use->IsBackpackActive())
		for (auto Action : { EInventoryControl::MoveForward, EInventoryControl::MoveBackward, EInventoryControl::MoveLeft,
			EInventoryControl::MoveRight, EInventoryControl::Run, EInventoryControl::Sprint }) bRoute |= Controls->Matches(Action, Key);
	if (bRoute) InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Released ? 0.f : 1.f));
	return bRoute;
}

bool Aprototype3PlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	if (IsLocalController() && Controls->Matches(EInventoryControl::Back, Params.Key))
	{
		if (Params.Event == IE_Pressed) HandleInterfaceEscape();
		return true;
	}
	if (PauseMenu.IsValid()) return true;
	if (IsLocalController() && Controls->Matches(EInventoryControl::ShowQuick, Params.Key) && !bInventoryDemoOpen)
	{
		if (Params.Event == IE_Pressed)
		{
			ToggleInventoryDemo(false, true);
		}
		return true;
	}
	if (IsLocalController() && Controls->Matches(EInventoryControl::Toggle, Params.Key))
	{
		if (Params.Event == IE_Pressed || Params.Event == IE_DoubleClick)
		{
			if (BackpackKeysDown.IsEmpty()) ToggleInventoryDemo();
			if (auto* Use = ItemUse(this); Use && Use->IsBackpackActive()) BackpackKeysDown.Add(Params.Key);
		}
		else if (Params.Event == IE_Released)
		{
			const bool bWasDown = BackpackKeysDown.Remove(Params.Key) > 0;
			if (bWasDown && BackpackKeysDown.IsEmpty()) if (auto* Use = ItemUse(this)) Use->ReleaseBackpackInput();
		}
		return true;
	}
	if (bInventoryDemoOpen)
	{
		auto* Use = ItemUse(this);
		if (!Use || !Use->IsBackpackActive()) return true;
		const bool bGait = Controls->Matches(EInventoryControl::Run, Params.Key) || Controls->Matches(EInventoryControl::Sprint, Params.Key);
		bool bMove = false;
		for (auto Action : { EInventoryControl::MoveForward, EInventoryControl::MoveBackward, EInventoryControl::MoveLeft, EInventoryControl::MoveRight })
			bMove |= Controls->Matches(Action, Params.Key);
		if (!bMove && !bGait) return true;
		if (Params.Event == IE_Pressed && (bGait || (bMove && Use->GetBackpackMode() == EBackpackMode::Quick))) CloseInventoryDemo();
		return Super::InputKey(Params);
	}
	if (IsLocalController() && !bInventoryDemoOpen)
		if (auto* Use = ItemUse(this))
		{
			if (Controls->Matches(EInventoryControl::Stow, Params.Key))
			{
				if (Params.Event == IE_Pressed) Use->Stow();
				return true;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
				if (const auto Action = static_cast<EInventoryControl>(static_cast<int32>(EInventoryControl::Bandage) + Slot); Controls->Matches(Action, Params.Key))
				{
					const bool bWasDown = QuickKeysDown.Contains(Controls->GetKey(Action)) || QuickKeysDown.Contains(Controls->GetKey(Action, 1));
					if (Params.Event == IE_Pressed || Params.Event == IE_DoubleClick)
					{
						QuickKeysDown.Add(Params.Key);
						if (!bWasDown) Use->PressQuickItem(Slot);
					}
					else if (Params.Event == IE_Released)
					{
						QuickKeysDown.Remove(Params.Key);
						if (!QuickKeysDown.Contains(Controls->GetKey(Action)) && !QuickKeysDown.Contains(Controls->GetKey(Action, 1))) Use->ReleaseQuickItem(Slot);
					}
					return true;
				}
		}
	return Super::InputKey(Params);
}

void Aprototype3PlayerController::ToggleInventoryDemo(bool bLegacyLab, bool bPocketsOnly)
{
	if (PauseMenu.IsValid()) return;
	if (bInventoryDemoOpen)
	{
		if (InventoryDemoWidget && InventoryDemoWidget->IsPocketsOnly() != bPocketsOnly)
		{
			if (!bPocketsOnly) if (auto* Use = ItemUse(this); Use && !Use->BeginOpenBackpack(true)) return;
			InventoryDemoWidget->SetPocketsOnly(bPocketsOnly);
			ResetIgnoreMoveInput();
			if (bPocketsOnly) SetIgnoreMoveInput(true);
		}
		else CloseInventoryDemo();
		return;
	}
	if (!IsLocalController()) return;
	if (!InventoryDemoWidget)
	{
		InventoryDemoWidget = CreateWidget<UInventoryDemoWidget>(this);
		if (!InventoryDemoWidget) return;
		InventoryDemoWidget->OnClose.BindUObject(this, &Aprototype3PlayerController::CloseInventoryDemo);
		if (!bLegacyLab) InventoryDemoWidget->ConfigurePlayerInventory(ItemUse(this));
	}
	if (auto* Use=ItemUse(this))
	{
		if (!bLegacyLab && !bPocketsOnly && !Use->BeginOpenBackpack(true)) return;
		Use->CancelUse();
		if (bPocketsOnly) Use->CloseBackpack();
	}
	InventoryDemoWidget->SetPocketsOnly(bPocketsOnly);
	bCursorBeforeInventory = bShowMouseCursor;
	bInventoryDemoOpen = true;
	const auto MovementKeys = HeldMovementKeys();
	FlushPressedKeys();
	// A held attack is driven by the pawn component's tick, independently of UI input.
	if (APawn* ControlledPawn = GetPawn())
		if (UPlayerMeleeComponent* Melee = ControlledPawn->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	ResetIgnoreMoveInput();
	if (bPocketsOnly || bLegacyLab) SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	InventoryDemoWidget->AddToViewport(100);
	// Focus can be resolved only after the Slate panel has joined the viewport tree.
	FInputModeUIOnly InventoryInputMode;
	InventoryInputMode.SetWidgetToFocus(InventoryDemoWidget->GetInventoryFocusTarget());
	SetInputMode(InventoryInputMode);
	if (!bPocketsOnly && !bLegacyLab) RestoreMovementKeys(MovementKeys);
}

void Aprototype3PlayerController::CloseInventoryDemo()
{
	if (!bInventoryDemoOpen) return;
	const auto MovementKeys = HeldMovementKeys();
	bInventoryDemoOpen = false;
	if (InventoryDemoWidget)
	{
		InventoryDemoWidget->CancelInteraction();
		InventoryDemoWidget->RemoveFromParent();
	}
	if (auto* Use=ItemUse(this)) Use->CloseBackpack();
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = bCursorBeforeInventory;
	SetInputMode(FInputModeGameOnly());
	FlushPressedKeys();
	RestoreMovementKeys(MovementKeys);
}

bool Aprototype3PlayerController::HasInterfaceFocus(int32 UserIndex) const
{
	if (!IsLocalController() || !GetWorld() || !GetWorld()->IsGameWorld()) return false;
	auto Focused = [UserIndex](const TSharedPtr<SWidget>& Widget)
	{
		return Widget.IsValid() && (Widget->HasUserFocus(UserIndex).IsSet() || Widget->HasUserFocusedDescendants(UserIndex));
	};
	if (Focused(PauseMenu)) return true;
	if (bInventoryDemoOpen && InventoryDemoWidget && Focused(InventoryDemoWidget->GetInventoryFocusTarget())) return true;
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	return Viewport && Focused(Viewport->GetGameViewportWidget());
}

void Aprototype3PlayerController::HandleInterfaceEscape()
{
	if (!IsLocalController()) return;
	// Hold a local reference because Resume removes the controller's menu reference.
	if (const TSharedPtr<SPauseMenu> Menu = PauseMenu) Menu->HandleEscape();
	else if (bInventoryDemoOpen) CloseInventoryDemo();
	else OpenPauseMenu();
}

void Aprototype3PlayerController::OpenPauseMenu()
{
	if (!IsLocalController() || PauseMenu.IsValid() || !GetWorld()) return;
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport) return;
	if (bInventoryDemoOpen) CloseInventoryDemo();
	if (!SetPause(true)) return;
	const bool bPlayInEditor = GetWorld()->WorldType == EWorldType::PIE;
	bCursorBeforePause = bShowMouseCursor;
	FlushPressedKeys();
	if (APawn* ControlledPawn = GetPawn())
		if (UPlayerMeleeComponent* Melee = ControlledPawn->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	SAssignNew(PauseMenu, SPauseMenu).ItemUse(ItemUse(this))
		.CanExitGame(bPlayInEditor).CanExitDesktop(!bPlayInEditor)
		.OnResume(FSimpleDelegate::CreateUObject(this, &Aprototype3PlayerController::ClosePauseMenu))
		.OnControlsChanged(FSimpleDelegate::CreateUObject(this, &Aprototype3PlayerController::RebuildControls))
		.OnExitGame(FSimpleDelegate::CreateUObject(this, &Aprototype3PlayerController::ExitPlaySession))
		.OnExitDesktop(FSimpleDelegate::CreateUObject(this, &Aprototype3PlayerController::ExitToDesktop));
	Viewport->AddViewportWidgetContent(PauseMenu.ToSharedRef(), 200);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseMenu);
	SetInputMode(Mode);
}

void Aprototype3PlayerController::ClosePauseMenu()
{
	if (!PauseMenu.IsValid()) return;
	if (GetWorld() && GetWorld()->GetGameViewport())
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseMenu.ToSharedRef());
	PauseMenu.Reset();
	SetPause(false);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = bCursorBeforePause;
	SetInputMode(FInputModeGameOnly());
	FlushPressedKeys();
}

void Aprototype3PlayerController::ExitPlaySession()
{
#if WITH_EDITOR
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE && GEditor)
		GEditor->RequestEndPlayMap();
#endif
}

void Aprototype3PlayerController::ExitToDesktop()
{
	if (GetWorld() && GetWorld()->WorldType != EWorldType::PIE)
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void Aprototype3PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InterfaceInputProcessor.IsValid() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().UnregisterInputPreProcessor(InterfaceInputProcessor);
	InterfaceInputProcessor.Reset();
	ClosePauseMenu();
	CloseInventoryDemo();
	Super::EndPlay(EndPlayReason);
}
