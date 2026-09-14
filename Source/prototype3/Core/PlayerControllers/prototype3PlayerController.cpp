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
