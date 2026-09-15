// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "prototype3Character.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UPlayerMeleeComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHealthUpdatedDelegate, float, Percentage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerSprintStateChangedDelegate, bool, bSprinting, float, StaminaPercent);

/** Mutually exclusive movement speeds the character can actually perform. */
UENUM(BlueprintType)
enum class EPlayerLocomotionGait : uint8
{
	Walking,
	Running,
	Sprinting
};

/** Physical posture reported to gameplay systems. */
UENUM(BlueprintType)
enum class EPlayerLocomotionStance : uint8
{
	Standing,
	Crouching
};

/** One gait key's hybrid tap-to-toggle and hold-until-release intent. */
struct FPlayerGaitInputIntent
{
	bool bToggled = false;
	bool bHeld = false;
	bool bToggledAtPress = false;
	double InputStartTime = 0.0;

	bool IsRequested() const { return bToggled || bHeld; }
};

/** Persistent input requests, kept separate from the resolved physical state. */
struct FPlayerLocomotionIntent
{
	FVector2D MovementInput = FVector2D::ZeroVector;
	FPlayerGaitInputIntent Run;
	FPlayerGaitInputIntent Sprint;
};

/**
 *  A basic first person character
 */
UCLASS(abstract)
class Aprototype3Character : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPlayerMeleeComponent> MeleeComponent;

protected:
	/** Maximum health available to this character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta = (ClampMin = 1.0, AllowPrivateAccess = "true"))
	float MaxHealth = 100.0f;

	/** Current health remaining. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	/** Maximum stamina available for sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina", meta = (ClampMin = 1.0, AllowPrivateAccess = "true"))
	float MaxStamina = 100.0f;

	/** Current stamina remaining. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stamina", meta = (AllowPrivateAccess = "true"))
	float CurrentStamina = 0.0f;

	/** Movement speed when neither run nor sprint is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float WalkSpeed = 350.0f;

	/** Speed multiplier used whenever the player has side or backward movement input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta = (ClampMin = 0.0, ClampMax = 1.0, AllowPrivateAccess = "true"))
	float SideAndBackSpeedMultiplier = 0.666667f;

	/** What the player is requesting; the resolver decides what can actually happen. */
	FPlayerLocomotionIntent LocomotionIntent;

	/** Current resolved gait. Input toggles alone do not change this state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement|State")
	EPlayerLocomotionGait ActiveGait = EPlayerLocomotionGait::Walking;

	/** Current physical stance. Unreal's crouch state remains the collision authority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement|State")
	EPlayerLocomotionStance ActiveStance = EPlayerLocomotionStance::Standing;

	/** Presses shorter than this toggle run/sprint; longer presses last until release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement|Input", meta = (ClampMin = 0.0, Units = "s"))
	float GaitHoldThreshold = 0.25f;

	/** Movement speed while crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crouch", meta = (ClampMin = 0.0, Units = "cm/s"))
	float CrouchSpeed = 150.0f;

	/** Half the crouched capsule height; clamped to the capsule radius and standing height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crouch", meta = (ClampMin = 0.0, Units = "cm"))
	float CrouchHalfHeight = 60.0f;

	/** Presses shorter than this toggle crouch; longer holds stand on release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crouch", meta = (ClampMin = 0.0, Units = "s"))
	float CrouchHoldThreshold = 0.25f;

	/** Multiplies normal stamina recovery while physically crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crouch", meta = (ClampMin = 0.0))
	float CrouchStaminaRecoveryMultiplier = 1.5f;

	bool bCrouchInputHeld = false;
	bool bCrouchedAtInputStart = false;
	double CrouchInputStartTime = 0.0;

	/** Standing first-person mesh position, including Blueprint adjustments. */
	FVector StandingFirstPersonMeshLocation = FVector::ZeroVector;

	/** Movement speed while run is toggled on, unless sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float RunSpeed = 500.0f;

	/** Stamina consumed each second of actual running. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float RunStaminaDrainPerSecond = 2.0f;

	/** Movement speed while sprint is toggled on and stamina remains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float SprintSpeed = 700.0f;

	/** Stamina consumed each second of actual sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float StaminaDrainPerSecond = 4.0f;

	/** Stamina restored each second while neither running nor sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float StaminaRecoveryPerSecond = 50.0f;

	/** Running and sprinting lock at zero stamina until the recovery threshold. */
	bool bSprintExhausted = false;

	/** Fraction of maximum stamina required to unlock run and sprint after exhaustion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float ExhaustionRecoveryFraction = 0.25f;

	/** Attacks defer regeneration until their recovery period ends. */
	double StaminaRecoveryResumeTime = 0.0;


	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Sprint input action. The shared player controller maps this to Left/Right Alt. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SprintAction;
	
public:
	Aprototype3Character();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Toggles the persistent run request for Blueprint or non-keyboard callers. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartRun();

	/** Explicitly clears the persistent run input state. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoEndRun();

	/** Toggles the persistent sprint request for Blueprint or non-keyboard callers. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartSprint();

	/** Explicitly clears the persistent sprint input state. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoEndSprint();

	/** Generic action entry points. Future held items can provide their own behavior. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Input|Actions")
	void DoPrimaryActionStart();
	virtual void DoPrimaryActionStart_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Input|Actions")
	void DoPrimaryActionEnd();
	virtual void DoPrimaryActionEnd_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Input|Actions")
	void DoSecondaryActionStart();
	virtual void DoSecondaryActionStart_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Input|Actions")
	void DoSecondaryActionEnd();
	virtual void DoSecondaryActionEnd_Implementation();

	/** Starts crouching unless run or sprint is actually being performed. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartCrouch();

	/** Requests standing; Character Movement waits until there is enough headroom. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoEndCrouch();

	/** Centralized locomotion requests, state resolution, and transition policy. */
	void ToggleGaitRequest(EPlayerLocomotionGait RequestedGait);
	void GaitInputStarted(EPlayerLocomotionGait RequestedGait);
	void GaitInputCompleted(EPlayerLocomotionGait RequestedGait);
	void GaitInputCanceled(EPlayerLocomotionGait RequestedGait);
	FPlayerGaitInputIntent* FindGaitInputIntent(EPlayerLocomotionGait RequestedGait);
	const FPlayerGaitInputIntent* FindGaitInputIntent(EPlayerLocomotionGait RequestedGait) const;
	bool HasHeldGaitInput() const;
	bool HasRequestedGait() const;
	void ResolveLocomotionState();
	void SetActiveGait(EPlayerLocomotionGait NewGait);
	void ClearSpeedRequests();
	bool CanStartCrouch() const;
	bool IsCrouchActive() const;
	bool CanUseStaminaMovement() const;

	/** Run and sprint are available whenever movement has a forward component. */
	bool HasForwardMovementInput() const;
	float GetDirectionalMovementSpeedMultiplier() const;

	/** Keyboard tap/hold interpretation for run and sprint. */
	void RunInputStarted();
	void RunInputCompleted();
	void RunInputCanceled();
	void SprintInputStarted();
	void SprintInputCompleted();
	void SprintInputCanceled();

	/** Keyboard tap/hold interpretation, separate from explicit Blueprint crouch requests. */
	void CrouchInputStarted();
	void CrouchInputCompleted();
	void CrouchInputCanceled();

	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Initializes universal player vitals. */
	virtual void BeginPlay() override;

	/** Updates stamina drain/recovery and movement speed. */
	virtual void Tick(float DeltaSeconds) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsAlive() const { return CurrentHealth > 0.0f; }

	/** Pays a one-off action cost atomically; rejected actions spend no stamina. */
	UFUNCTION(BlueprintCallable, Category="Stamina")
	bool TryConsumeStamina(float Amount, float RecoveryDelay = 0.0f);

	/** Requests a standing posture without re-arming crouch input. */
	UFUNCTION(BlueprintCallable, Category="Movement")
	void RequestStandingForAction();

	/** True once the capsule is standing and no crouch request is active. */
	UFUNCTION(BlueprintPure, Category="Movement")
	bool IsStandingForAction() const;

	/** Delegate called whenever health changes. */
	FPlayerHealthUpdatedDelegate OnHealthUpdated;

	/** Delegate called whenever the shared sprint state changes. */
	FPlayerSprintStateChangedDelegate OnSprintStateChanged;

	/** Applies incoming damage and updates the health display. */
	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Returns health normalized to the 0-1 range used by progress bars. */
	UFUNCTION(BlueprintPure, Category="Health")
	virtual float GetHealthPercent() const;

	/** Returns stamina normalized to the 0-1 range used by progress bars. */
	UFUNCTION(BlueprintPure, Category="Stamina")
	virtual float GetStaminaPercent() const;

	/** Returns whether the character is currently sprinting. */
	UFUNCTION(BlueprintPure, Category="Sprint")
	bool IsSprinting() const { return ActiveGait == EPlayerLocomotionGait::Sprinting; }

	/** Returns whether the character is currently running. */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsRunning() const { return ActiveGait == EPlayerLocomotionGait::Running; }

	UFUNCTION(BlueprintPure, Category="Movement|State")
	EPlayerLocomotionGait GetActiveGait() const { return ActiveGait; }

	UFUNCTION(BlueprintPure, Category="Movement|State")
	EPlayerLocomotionStance GetActiveStance() const { return ActiveStance; }

};
