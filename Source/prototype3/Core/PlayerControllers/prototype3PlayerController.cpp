// Copyright Epic Games, Inc. All Rights Reserved.


#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Core/Camera/prototype3CameraManager.h"
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

namespace
{
UPlayerItemUseComponent* ItemUse(APlayerController* Controller)
{
	return Controller && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UPlayerItemUseComponent>() : nullptr;
}
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
	if (IsLocalPlayerController() && GEngine && GEngine->GameViewport)
	{
		SAssignNew(ItemUseOverlay, SOverlay).Visibility(EVisibility::HitTestInvisible)
		+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)
		[
			SNew(SBox).WidthOverride(600)
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(.02f,.03f,.04f,.85f)).Padding(12)
			[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),14)).Text_Lambda([this]()
			{
				auto* Use=ItemUse(this); auto* Vitals=GetPawn() ? GetPawn()->FindComponentByClass<UPlayerVitalsComponent>() : nullptr;
				return FText::FromString(Use && Vitals ? FString::Printf(TEXT("Health %.0f / %.0f   |   %s"),Vitals->GetCurrentHealth(),Vitals->GetMaxHealth(),*Use->GetHeldName()) : TEXT(""));
			})]
			+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),11)).AutoWrapText(true).Text_Lambda([this]()
			{
				auto* Use=ItemUse(this); if (!Use || !Use->Shortcuts) return FText::GetEmpty();
				FString Text;
				for (int32 I=0; I<3; ++I) Text += Use->Shortcuts->Keys[I].GetDisplayName().ToString()+TEXT(": ")+Use->Shortcuts->ItemTypes[I].ToString()+TEXT("   ");
				return FText::FromString(Text+TEXT("| Double tap: cycle | Left click: use | Right click: stow"));
			})]
			+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),11)).AutoWrapText(true).Text_Lambda([this]() { auto* Use=ItemUse(this); return FText::FromString(Use ? Use->Status : FString()); })]
			+SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(SProgressBar).Percent_Lambda([this]() -> TOptional<float>
			{ auto* Use=ItemUse(this); return Use && (Use->IsUsing() || Use->IsOpeningBackpack()) ? Use->GetProgress() : 0.f; })]
			]]
		];
		GEngine->GameViewport->AddViewportWidgetContent(ItemUseOverlay.ToSharedRef(),10);
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

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// Keep movement and primary/secondary actions independent of individual map IMCs.
			// This is intentionally runtime-only; it does not mutate any .uasset.
			if (!RuntimeSprintMappingContext)
			{
				RuntimeSprintMappingContext = NewObject<UInputMappingContext>(this, TEXT("RuntimeSprintMappingContext"));
			}
			else
			{
				Subsystem->RemoveMappingContext(RuntimeSprintMappingContext);
			}
			// Rebuild even when a context survived input setup or an editor reload.
			// Never retain mappings to an older action object than the character binds.
			{
				RuntimeSprintMappingContext->UnmapAll();
				// Higher-priority Shift mappings consume any legacy Shift-to-sprint bindings.
				RuntimeSprintMappingContext->MapKey(RuntimeRunAction, EKeys::LeftShift);
				RuntimeSprintMappingContext->MapKey(RuntimeRunAction, EKeys::RightShift);
				RuntimeSprintMappingContext->MapKey(RuntimeCrouchAction, EKeys::LeftControl);
				RuntimeSprintMappingContext->MapKey(RuntimeCrouchAction, EKeys::RightControl);
				RuntimeSprintMappingContext->MapKey(GetPrimaryAction(), EKeys::LeftMouseButton);
				RuntimeSprintMappingContext->MapKey(GetSecondaryAction(), EKeys::RightMouseButton);
				RuntimeSprintAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Sprint.IA_Sprint"));
				if (RuntimeSprintAction)
				{
					RuntimeSprintMappingContext->MapKey(RuntimeSprintAction, EKeys::LeftAlt);
					RuntimeSprintMappingContext->MapKey(RuntimeSprintAction, EKeys::RightAlt);
				}
				else
				{
					UE_LOG(Logprototype3, Error, TEXT("Could not load the shared sprint input action."));
				}
			}

			if (RuntimeSprintMappingContext)
			{
				Subsystem->AddMappingContext(RuntimeSprintMappingContext, 1);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool Aprototype3PlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

bool Aprototype3PlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (IsLocalController() && Params.Key == GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::Toggle))
	{
		if (Params.Event == IE_Pressed) ToggleInventoryDemo();
		return true;
	}
	if (IsLocalController() && !bInventoryDemoOpen)
		if (auto* Use = ItemUse(this); Use && Use->Shortcuts)
			for (int32 Slot=0; Slot<Use->Shortcuts->Keys.Num(); ++Slot)
				if (Params.Key == Use->Shortcuts->Keys[Slot])
				{
					if (Params.Event == IE_Pressed || Params.Event == IE_DoubleClick) Use->HandleShortcut(Slot, GetWorld()->GetTimeSeconds());
					return true;
				}
	return Super::InputKey(Params);
}

void Aprototype3PlayerController::ToggleInventoryDemo(bool bLegacyLab)
{
	if (bInventoryDemoOpen) { CloseInventoryDemo(); return; }
	if (!IsLocalController()) return;
	if (!InventoryDemoWidget)
	{
		InventoryDemoWidget = CreateWidget<UInventoryDemoWidget>(this);
		if (!InventoryDemoWidget) return;
		InventoryDemoWidget->OnClose.BindUObject(this, &Aprototype3PlayerController::CloseInventoryDemo);
		if (!bLegacyLab) InventoryDemoWidget->ConfigurePlayerInventory(ItemUse(this));
	}
	if (auto* Use=ItemUse(this)) Use->CancelUse();
	bCursorBeforeInventory = bShowMouseCursor;
	bInventoryDemoOpen = true;
	if (PlayerInput) PlayerInput->FlushPressedKeys();
	// A held attack is driven by the pawn component's tick, independently of UI input.
	if (APawn* ControlledPawn = GetPawn())
		if (UPlayerMeleeComponent* Melee = ControlledPawn->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;
	InventoryDemoWidget->AddToViewport(100);
	// Focus can be resolved only after the Slate panel has joined the viewport tree.
	FInputModeUIOnly InventoryInputMode;
	InventoryInputMode.SetWidgetToFocus(InventoryDemoWidget->GetInventoryFocusTarget());
	SetInputMode(InventoryInputMode);
}

void Aprototype3PlayerController::CloseInventoryDemo()
{
	if (!bInventoryDemoOpen) return;
	bInventoryDemoOpen = false;
	if (InventoryDemoWidget) InventoryDemoWidget->RemoveFromParent();
	if (auto* Use=ItemUse(this)) Use->CloseBackpack();
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = bCursorBeforeInventory;
	SetInputMode(FInputModeGameOnly());
	if (PlayerInput) PlayerInput->FlushPressedKeys();
}

void Aprototype3PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseInventoryDemo();
	if (ItemUseOverlay.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(ItemUseOverlay.ToSharedRef());
	ItemUseOverlay.Reset();
	Super::EndPlay(EndPlayReason);
}
