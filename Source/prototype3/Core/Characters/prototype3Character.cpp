// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
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
	VitalsComponent = CreateDefaultSubobject<UPlayerVitalsComponent>(TEXT("Vitals"));
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

	VitalsComponent->InitializeVitals(MaxHealth, MaxStamina, StaminaRecoveryPerSecond);
	bIsSprinting = false;
	bSprintInputHeld = false;
	bRunInputHeld = false;
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

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &Aprototype3Character::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &Aprototype3Character::LookInput);

		// Hold Shift to run. The controller owns the shared runtime action and mappings.
		if (Aprototype3PlayerController* PlayerController = Cast<Aprototype3PlayerController>(GetController()))
		{
			if (UInputAction* RunAction = PlayerController->GetRunAction())
			{
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &Aprototype3Character::DoStartRun);
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &Aprototype3Character::DoEndRun);
				EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Canceled, this, &Aprototype3Character::DoEndRun);
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

		// Hold Alt to sprint; releasing it returns to running or walking.
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &Aprototype3Character::DoStartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &Aprototype3Character::DoEndSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &Aprototype3Character::DoEndSprint);
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
	bRunInputHeld = true;
	if (!bIsCrouched && !GetCharacterMovement()->bWantsToCrouch && !bIsSprinting
		&& !VitalsComponent->IsStaminaExhausted() && VitalsComponent->GetCurrentStamina() > 0.0f)
	{
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	}
}

void Aprototype3Character::DoEndRun()
{
	bRunInputHeld = false;
	if (!bIsSprinting)
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

void Aprototype3Character::DoStartSprint()
{
	bSprintInputHeld = true;
}

void Aprototype3Character::DoEndSprint()
{
	bSprintInputHeld = false;
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = !bIsCrouched && !GetCharacterMovement()->bWantsToCrouch
		&& bRunInputHeld && !VitalsComponent->IsStaminaExhausted()
		&& VitalsComponent->GetCurrentStamina() > 0.0f ? RunSpeed : WalkSpeed;
	OnSprintStateChanged.Broadcast(false, GetStaminaPercent());
}

void Aprototype3Character::DoStartCrouch()
{
	Crouch();
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	OnSprintStateChanged.Broadcast(false, GetStaminaPercent());
}

void Aprototype3Character::DoPrimaryActionStart_Implementation()
{
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

	// The parent body mesh offsets upward to keep its feet planted. Cancel that
	// offset, then lower the head-mounted view within the shorter capsule too.
	FirstPersonMesh->SetRelativeLocation(StandingFirstPersonMeshLocation - FVector(0.0f, 0.0f, 2.0f * HalfHeightAdjust));
}

void Aprototype3Character::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	FirstPersonMesh->SetRelativeLocation(StandingFirstPersonMeshLocation);
}

void Aprototype3Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float PreviousStamina = VitalsComponent->GetCurrentStamina();
	const bool bIsMoving = GetVelocity().SizeSquared2D() > FMath::Square(1.0f);
	// Include the request so Ctrl suppresses stamina drain immediately, and the
	// actual state so releasing Ctrl under a ceiling cannot resume run/sprint.
	const bool bCrouchActive = bIsCrouched || GetCharacterMovement()->bWantsToCrouch;
	const bool bCanSprint = !bCrouchActive && bSprintInputHeld && bIsMoving
		&& !VitalsComponent->IsStaminaExhausted() && VitalsComponent->GetCurrentStamina() > 0.0f;
	const bool bWasSprinting = bIsSprinting;
	bIsSprinting = bCanSprint;
	bool bIsRunning = !bCrouchActive && !bIsSprinting && bRunInputHeld && bIsMoving
		&& !VitalsComponent->IsStaminaExhausted() && VitalsComponent->GetCurrentStamina() > 0.0f;

	if (bIsSprinting || bIsRunning)
	{
		const float DrainPerSecond = bIsSprinting ? StaminaDrainPerSecond : RunStaminaDrainPerSecond;
		VitalsComponent->DrainStamina(DrainPerSecond * DeltaSeconds);
		if (VitalsComponent->IsStaminaExhausted())
		{
			bIsSprinting = false;
			bIsRunning = false;
		}
	}
	else
	{
		const float RecoveryMultiplier = bIsCrouched ? CrouchStaminaRecoveryMultiplier : 1.0f;
		VitalsComponent->RecoverStamina(DeltaSeconds, RecoveryMultiplier);
	}

	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : (bIsRunning ? RunSpeed : WalkSpeed);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	if (bWasSprinting != bIsSprinting || VitalsComponent->GetCurrentStamina() != PreviousStamina)
	{
		OnSprintStateChanged.Broadcast(bIsSprinting, GetStaminaPercent());
	}
}

float Aprototype3Character::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = VitalsComponent->ApplyDamage(Damage);
	if (AppliedDamage > 0.0f)
	{
		OnHealthUpdated.Broadcast(GetHealthPercent());
	}
	return AppliedDamage;
}

float Aprototype3Character::GetHealthPercent() const
{
	return VitalsComponent ? VitalsComponent->GetHealthPercent() : 0.0f;
}

bool Aprototype3Character::TryConsumeStamina(float Amount, float RecoveryDelay)
{
	if (!VitalsComponent || !VitalsComponent->TryConsumeStamina(Amount, RecoveryDelay))
	{
		return false;
	}

	if (VitalsComponent->IsStaminaExhausted())
	{
		bIsSprinting = false;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
	OnSprintStateChanged.Broadcast(bIsSprinting, GetStaminaPercent());
	return true;
}

float Aprototype3Character::GetStaminaPercent() const
{
	return VitalsComponent ? VitalsComponent->GetStaminaPercent() : 0.0f;
}

bool Aprototype3Character::IsAlive() const
{
	return VitalsComponent && VitalsComponent->IsAlive();
}
