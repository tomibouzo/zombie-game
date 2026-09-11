// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "prototype3.h"

Aprototype3Character::Aprototype3Character()
{
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
	bSprintExhausted = false;
	bIsSprinting = false;
	bSprintInputHeld = false;
	bRunInputHeld = false;
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
	if (!bIsSprinting && !bSprintExhausted && CurrentStamina > 0.0f)
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
	GetCharacterMovement()->MaxWalkSpeed = bRunInputHeld && !bSprintExhausted && CurrentStamina > 0.0f ? RunSpeed : WalkSpeed;
	OnSprintStateChanged.Broadcast(false, GetStaminaPercent());
}

void Aprototype3Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bIsMoving = GetVelocity().SizeSquared2D() > FMath::Square(1.0f);
	const bool bCanSprint = bSprintInputHeld && bIsMoving && !bSprintExhausted && CurrentStamina > 0.0f;
	const bool bWasSprinting = bIsSprinting;
	bIsSprinting = bCanSprint;
	bool bIsRunning = !bIsSprinting && bRunInputHeld && bIsMoving && !bSprintExhausted && CurrentStamina > 0.0f;

	if (bIsSprinting || bIsRunning)
	{
		const float DrainPerSecond = bIsSprinting ? StaminaDrainPerSecond : RunStaminaDrainPerSecond;
		CurrentStamina = FMath::Max(CurrentStamina - (DrainPerSecond * DeltaSeconds), 0.0f);
		if (CurrentStamina <= 0.0f)
		{
			bSprintExhausted = true;
			bIsSprinting = false;
			bIsRunning = false;
		}
	}
	else
	{
		CurrentStamina = FMath::Min(CurrentStamina + (StaminaRecoveryPerSecond * DeltaSeconds), MaxStamina);
		if (CurrentStamina >= MaxStamina)
		{
			bSprintExhausted = false;
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : (bIsRunning ? RunSpeed : WalkSpeed);
	if (bWasSprinting != bIsSprinting || !FMath::IsNearlyEqual(CurrentStamina, MaxStamina))
	{
		OnSprintStateChanged.Broadcast(bIsSprinting, GetStaminaPercent());
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

float Aprototype3Character::GetStaminaPercent() const
{
	return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f;
}
