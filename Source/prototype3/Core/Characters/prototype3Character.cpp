// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "UObject/ConstructorHelpers.h"
#include "prototype3.h"

Aprototype3Character::Aprototype3Character()
{
	MeleeComponent = CreateDefaultSubobject<UPlayerMeleeComponent>(TEXT("Melee"));
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(CrouchHalfHeight);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;

	static ConstructorHelpers::FObjectFinder<UInputAction> SprintActionAsset(TEXT("/Game/Input/Actions/IA_Sprint.IA_Sprint"));
	if (SprintActionAsset.Succeeded())
	{
		SprintAction = SprintActionAsset.Object;
	}

	//Get rid of jump:
	JumpMaxCount = 0;
}

void Aprototype3Character::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	StaminaRecoveryResumeTime = 0.0;
	bSprintExhausted = false;
	LocomotionIntent = FPlayerLocomotionIntent();
	ActiveGait = EPlayerLocomotionGait::Walking;
	ActiveStance = bIsCrouched ? EPlayerLocomotionStance::Crouching : EPlayerLocomotionStance::Standing;
	bCrouchInputHeld = false;
	bCrouchedAtInputStart = false;
	CrouchInputStartTime = 0.0;
	StandingFirstPersonMeshLocation = FirstPersonMesh->GetRelativeLocation();
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	// Apply at startup too so existing Blueprint defaults pick up the ledge fix.
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(FMath::Clamp(CrouchHalfHeight,
		GetCapsuleComponent()->GetUnscaledCapsuleRadius(), GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	OnHealthUpdated.Broadcast(GetHealthPercent());
	OnSprintStateChanged.Broadcast(false, GetStaminaPercent());
}

void Aprototype3Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &Aprototype3Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &Aprototype3Character::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &Aprototype3Character::MoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &Aprototype3Character::MoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &Aprototype3Character::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Aprototype3Character::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &Aprototype3Character::LookInput);

		// Tap Shift to toggle run, or hold it to run only until release.
		if (Aprototype3PlayerController* PlayerController = Cast<Aprototype3PlayerController>(GetController()))
		{
			if (UInputAction* RunAction = PlayerController->GetRunAction())
			{
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &Aprototype3Character::RunInputStarted);
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &Aprototype3Character::RunInputCompleted);
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Canceled, this, &Aprototype3Character::RunInputCanceled);
			}

			if (UInputAction* CrouchAction = PlayerController->GetCrouchAction())
			{
				EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &Aprototype3Character::CrouchInputStarted);
				EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &Aprototype3Character::CrouchInputCompleted);
				EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &Aprototype3Character::CrouchInputCanceled);
			}

			if (UInputAction* PrimaryAction = PlayerController->GetPrimaryAction())
			{
				EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &Aprototype3Character::DoPrimaryActionStart);
				EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &Aprototype3Character::DoPrimaryActionEnd);
				EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Canceled, this, &Aprototype3Character::DoPrimaryActionEnd);
			}

			if (UInputAction* SecondaryAction = PlayerController->GetSecondaryAction())
			{
				EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Started, this, &Aprototype3Character::DoSecondaryActionStart);
				EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &Aprototype3Character::DoSecondaryActionEnd);
				EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Canceled, this, &Aprototype3Character::DoSecondaryActionEnd);
			}
		}

		// Tap Alt to toggle sprint, or hold it to sprint only until release.
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &Aprototype3Character::SprintInputStarted);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &Aprototype3Character::SprintInputCompleted);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &Aprototype3Character::SprintInputCanceled);
		}
	}
	else
	{
		UE_LOG(Logprototype3, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void Aprototype3Character::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}
void Aprototype3Character::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void Aprototype3Character::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void Aprototype3Character::DoMove(float Right, float Forward)
{
	LocomotionIntent.MovementInput = FVector2D(Right, Forward);
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void Aprototype3Character::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void Aprototype3Character::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void Aprototype3Character::DoStartRun()
{
	ToggleGaitRequest(EPlayerLocomotionGait::Running);
}

void Aprototype3Character::DoEndRun()
{
	LocomotionIntent.Run = FPlayerGaitInputIntent();
	ResolveLocomotionState();
}

void Aprototype3Character::DoStartSprint()
{
	ToggleGaitRequest(EPlayerLocomotionGait::Sprinting);
}

void Aprototype3Character::DoEndSprint()
{
	const bool bWasSprinting = IsSprinting();
	LocomotionIntent.Sprint = FPlayerGaitInputIntent();
	ResolveLocomotionState();
	if (bWasSprinting != IsSprinting())
	{
		OnSprintStateChanged.Broadcast(IsSprinting(), GetStaminaPercent());
	}
}

void Aprototype3Character::DoStartCrouch()
{
	// Resolve first, then let latched gait requests yield to crouch while a
	// physically held Shift/Alt key keeps crouch blocked until release.
	ResolveLocomotionState();
	if (!CanStartCrouch())
	{
		return;
	}

	ClearSpeedRequests();
	Crouch();
	SetActiveGait(EPlayerLocomotionGait::Walking);
	OnSprintStateChanged.Broadcast(false, GetStaminaPercent());
}

void Aprototype3Character::DoPrimaryActionStart_Implementation()
{
	RequestStandingForAction();
	MeleeComponent->StartAttacking();
}

void Aprototype3Character::DoPrimaryActionEnd_Implementation()
{
	MeleeComponent->StopAttacking();
}

void Aprototype3Character::DoSecondaryActionStart_Implementation()
{
	// Reserved for future use, aim, block, or interaction behavior.
}

void Aprototype3Character::DoSecondaryActionEnd_Implementation()
{
}

void Aprototype3Character::DoEndCrouch()
{
	UnCrouch();
}

void Aprototype3Character::RequestStandingForAction()
{
	if (bIsCrouched || GetCharacterMovement()->bWantsToCrouch)
	{
		// Keep bCrouchInputHeld intact. If Ctrl is still down, releasing it only
		// finishes that press; the player must press Ctrl again to crouch again.
		DoEndCrouch();
	}
}

bool Aprototype3Character::IsStandingForAction() const
{
	return !IsCrouchActive();
}

bool Aprototype3Character::HasForwardMovementInput() const
{
	return LocomotionIntent.MovementInput.Y > UE_KINDA_SMALL_NUMBER;
}

float Aprototype3Character::GetDirectionalMovementSpeedMultiplier() const
{
	const bool bHasMovementInput = !LocomotionIntent.MovementInput.IsNearlyZero(UE_KINDA_SMALL_NUMBER);
	return bHasMovementInput && !HasForwardMovementInput() ? SideAndBackSpeedMultiplier : 1.0f;
}

void Aprototype3Character::ToggleGaitRequest(EPlayerLocomotionGait RequestedGait)
{
	FPlayerGaitInputIntent* GaitIntent = FindGaitInputIntent(RequestedGait);
	if (!GaitIntent)
	{
		return;
	}

	if (IsCrouchActive() && !HasForwardMovementInput())
	{
		// A stationary crouch deliberately ignores Shift/Alt. Do not latch a
		// request that could unexpectedly stand the character later.
		return;
	}

	GaitIntent->bToggled = !GaitIntent->bToggled;
	if (IsCrouchActive())
	{
		if (HasRequestedGait())
		{
			// Character Movement keeps the crouched capsule if headroom is blocked.
			// The request remains pending and resolves only after standing succeeds.
			RequestStandingForAction();
		}
		else if (bIsCrouched)
		{
			// Cancel a pending stand request if the speed toggle is pressed again
			// while an overhead obstruction still keeps the character crouched.
			Crouch();
		}
	}

	ResolveLocomotionState();
}

void Aprototype3Character::GaitInputStarted(EPlayerLocomotionGait RequestedGait)
{
	FPlayerGaitInputIntent* GaitIntent = FindGaitInputIntent(RequestedGait);
	if (!GaitIntent || GaitIntent->bHeld)
	{
		return;
	}

	if (IsCrouchActive() && !HasForwardMovementInput())
	{
		// A gait key pressed during a stationary crouch is ignored completely,
		// including if the player keeps holding it and moves afterward.
		return;
	}

	GaitIntent->bHeld = true;
	GaitIntent->bToggledAtPress = GaitIntent->bToggled;
	GaitIntent->InputStartTime = FPlatformTime::Seconds();

	if (IsCrouchActive())
	{
		RequestStandingForAction();
	}

	ResolveLocomotionState();
}

void Aprototype3Character::GaitInputCompleted(EPlayerLocomotionGait RequestedGait)
{
	FPlayerGaitInputIntent* GaitIntent = FindGaitInputIntent(RequestedGait);
	if (!GaitIntent || !GaitIntent->bHeld)
	{
		return;
	}

	const double HeldSeconds = FPlatformTime::Seconds() - GaitIntent->InputStartTime;
	GaitIntent->bHeld = false;
	GaitIntent->bToggled = HeldSeconds < GaitHoldThreshold
		? !GaitIntent->bToggledAtPress
		: false;
	GaitIntent->bToggledAtPress = false;

	if (bIsCrouched && !HasRequestedGait())
	{
		// If standing was blocked for the entire hold, releasing the gait key
		// cancels that pending stand request and keeps the crouch latched.
		Crouch();
	}

	ResolveLocomotionState();
	if (bCrouchInputHeld && !HasHeldGaitInput())
	{
		// Ctrl may have been pressed while this gait key was held. Honor that
		// still-held crouch request as soon as the final gait key is released.
		DoStartCrouch();
	}
}

void Aprototype3Character::GaitInputCanceled(EPlayerLocomotionGait RequestedGait)
{
	FPlayerGaitInputIntent* GaitIntent = FindGaitInputIntent(RequestedGait);
	if (!GaitIntent)
	{
		return;
	}

	*GaitIntent = FPlayerGaitInputIntent();
	if (bIsCrouched && !HasRequestedGait())
	{
		Crouch();
	}
	ResolveLocomotionState();
	if (bCrouchInputHeld && !HasHeldGaitInput())
	{
		DoStartCrouch();
	}
}

FPlayerGaitInputIntent* Aprototype3Character::FindGaitInputIntent(EPlayerLocomotionGait RequestedGait)
{
	if (RequestedGait == EPlayerLocomotionGait::Running)
	{
		return &LocomotionIntent.Run;
	}
	if (RequestedGait == EPlayerLocomotionGait::Sprinting)
	{
		return &LocomotionIntent.Sprint;
	}
	return nullptr;
}

const FPlayerGaitInputIntent* Aprototype3Character::FindGaitInputIntent(EPlayerLocomotionGait RequestedGait) const
{
	if (RequestedGait == EPlayerLocomotionGait::Running)
	{
		return &LocomotionIntent.Run;
	}
	if (RequestedGait == EPlayerLocomotionGait::Sprinting)
	{
		return &LocomotionIntent.Sprint;
	}
	return nullptr;
}

bool Aprototype3Character::HasHeldGaitInput() const
{
	return LocomotionIntent.Run.bHeld || LocomotionIntent.Sprint.bHeld;
}

bool Aprototype3Character::HasRequestedGait() const
{
	return LocomotionIntent.Run.IsRequested() || LocomotionIntent.Sprint.IsRequested();
}

void Aprototype3Character::ResolveLocomotionState()
{
	ActiveStance = bIsCrouched
		? EPlayerLocomotionStance::Crouching
		: EPlayerLocomotionStance::Standing;

	const bool bIsMoving = GetVelocity().SizeSquared2D() > FMath::Square(1.0f);
	if (IsCrouchActive() || !HasForwardMovementInput() || !bIsMoving || !CanUseStaminaMovement())
	{
		SetActiveGait(EPlayerLocomotionGait::Walking);
		return;
	}

	if (LocomotionIntent.Sprint.IsRequested())
	{
		SetActiveGait(EPlayerLocomotionGait::Sprinting);
	}
	else if (LocomotionIntent.Run.IsRequested())
	{
		SetActiveGait(EPlayerLocomotionGait::Running);
	}
	else
	{
		SetActiveGait(EPlayerLocomotionGait::Walking);
	}
}

void Aprototype3Character::SetActiveGait(EPlayerLocomotionGait NewGait)
{
	ActiveGait = NewGait;

	float StandingSpeed = WalkSpeed;
	if (ActiveGait == EPlayerLocomotionGait::Running)
	{
		StandingSpeed = RunSpeed;
	}
	else if (ActiveGait == EPlayerLocomotionGait::Sprinting)
	{
		StandingSpeed = SprintSpeed;
	}

	const float DirectionalSpeedMultiplier = GetDirectionalMovementSpeedMultiplier();
	GetCharacterMovement()->MaxWalkSpeed = StandingSpeed * DirectionalSpeedMultiplier;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * DirectionalSpeedMultiplier;
}

void Aprototype3Character::ClearSpeedRequests()
{
	LocomotionIntent.Run = FPlayerGaitInputIntent();
	LocomotionIntent.Sprint = FPlayerGaitInputIntent();
}

bool Aprototype3Character::CanStartCrouch() const
{
	// Latched gait requests yield to crouch, even while they are actively moving.
	// A physically held Shift/Alt key blocks crouch until that key is released.
	return !HasHeldGaitInput();
}

bool Aprototype3Character::IsCrouchActive() const
{
	return bIsCrouched || GetCharacterMovement()->bWantsToCrouch;
}

bool Aprototype3Character::CanUseStaminaMovement() const
{
	return !bSprintExhausted && CurrentStamina > 0.0f;
}

void Aprototype3Character::RunInputStarted()
{
	GaitInputStarted(EPlayerLocomotionGait::Running);
}

void Aprototype3Character::RunInputCompleted()
{
	GaitInputCompleted(EPlayerLocomotionGait::Running);
}

void Aprototype3Character::RunInputCanceled()
{
	GaitInputCanceled(EPlayerLocomotionGait::Running);
}

void Aprototype3Character::SprintInputStarted()
{
	GaitInputStarted(EPlayerLocomotionGait::Sprinting);
}

void Aprototype3Character::SprintInputCompleted()
{
	GaitInputCompleted(EPlayerLocomotionGait::Sprinting);
}

void Aprototype3Character::SprintInputCanceled()
{
	GaitInputCanceled(EPlayerLocomotionGait::Sprinting);
}

void Aprototype3Character::CrouchInputStarted()
{
	if (bCrouchInputHeld)
	{
		return;
	}

	bCrouchInputHeld = true;
	bCrouchedAtInputStart = bIsCrouched || GetCharacterMovement()->bWantsToCrouch;
	CrouchInputStartTime = FPlatformTime::Seconds();
	// Respond immediately, then distinguish a tap from a hold on release.
	DoStartCrouch();
}

void Aprototype3Character::CrouchInputCompleted()
{
	if (!bCrouchInputHeld)
	{
		return;
	}

	bCrouchInputHeld = false;
	const double HeldSeconds = FPlatformTime::Seconds() - CrouchInputStartTime;
	if (bCrouchedAtInputStart || HeldSeconds >= CrouchHoldThreshold)
	{
		DoEndCrouch();
	}
	// A short tap from standing leaves the crouch request active.
}

void Aprototype3Character::CrouchInputCanceled()
{
	// A canceled action must never be interpreted as a tap that latches crouch.
	bCrouchInputHeld = false;
	DoEndCrouch();
}

void Aprototype3Character::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ActiveStance = EPlayerLocomotionStance::Crouching;
	SetActiveGait(EPlayerLocomotionGait::Walking);

	// The parent body mesh offsets upward to keep its feet planted. Cancel that
	// offset, then lower the head-mounted view within the shorter capsule too.
	FirstPersonMesh->SetRelativeLocation(StandingFirstPersonMeshLocation - FVector(0.0f, 0.0f, 2.0f * HalfHeightAdjust));
}

void Aprototype3Character::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ActiveStance = EPlayerLocomotionStance::Standing;
	FirstPersonMesh->SetRelativeLocation(StandingFirstPersonMeshLocation);
	ResolveLocomotionState();
}

void Aprototype3Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float PreviousStamina = CurrentStamina;
	const bool bWasSprinting = IsSprinting();
	ResolveLocomotionState();
	const bool bIsRunning = IsRunning();
	const bool bIsSprinting = IsSprinting();

	if (bIsSprinting || bIsRunning)
	{
		const float DrainPerSecond = bIsSprinting ? StaminaDrainPerSecond : RunStaminaDrainPerSecond;
		CurrentStamina = FMath::Max(CurrentStamina - (DrainPerSecond * DeltaSeconds), 0.0f);
		if (CurrentStamina <= 0.0f)
		{
			bSprintExhausted = true;
			SetActiveGait(EPlayerLocomotionGait::Walking);
		}
	}
	else
	{
		const float RecoveryMultiplier = bIsCrouched ? CrouchStaminaRecoveryMultiplier : 1.0f;
		const float RecoverySeconds = static_cast<float>(FMath::Clamp(
			GetWorld()->GetTimeSeconds() - StaminaRecoveryResumeTime, 0.0, static_cast<double>(DeltaSeconds)));
		CurrentStamina = FMath::Min(CurrentStamina + (StaminaRecoveryPerSecond * RecoveryMultiplier * RecoverySeconds), MaxStamina);
		if (CurrentStamina >= MaxStamina * ExhaustionRecoveryFraction)
		{
			bSprintExhausted = false;
		}
	}

	if (bWasSprinting != IsSprinting() || CurrentStamina != PreviousStamina)
	{
		OnSprintStateChanged.Broadcast(IsSprinting(), GetStaminaPercent());
	}
}

float Aprototype3Character::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (Damage <= 0.0f || CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Min(Damage, CurrentHealth);
	CurrentHealth = FMath::Max(CurrentHealth - AppliedDamage, 0.0f);
	OnHealthUpdated.Broadcast(GetHealthPercent());
	return AppliedDamage;
}

float Aprototype3Character::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

bool Aprototype3Character::TryConsumeStamina(float Amount, float RecoveryDelay)
{
	if (!IsAlive() || !GetWorld() || !FMath::IsFinite(Amount) || Amount < 0.0f || CurrentStamina < Amount)
	{
		return false;
	}

	CurrentStamina = FMath::Max(CurrentStamina - Amount, 0.0f);
	StaminaRecoveryResumeTime = FMath::Max(StaminaRecoveryResumeTime,
		GetWorld()->GetTimeSeconds() + FMath::Max(RecoveryDelay, 0.0f));
	if (CurrentStamina <= 0.0f)
	{
		bSprintExhausted = true;
		SetActiveGait(EPlayerLocomotionGait::Walking);
	}
	OnSprintStateChanged.Broadcast(IsSprinting(), GetStaminaPercent());
	return true;
}

float Aprototype3Character::GetStaminaPercent() const
{
	return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f;
}
