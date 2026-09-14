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

	/** Movement speed while crouched, including when run or sprint is held. */
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

	/** Movement speed while Shift is held, unless sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float RunSpeed = 500.0f;

	/** Stamina consumed each second of actual running. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float RunStaminaDrainPerSecond = 2.0f;

	/** Movement speed while Alt is held and stamina remains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float SprintSpeed = 700.0f;

	/** Stamina consumed each second of actual sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float StaminaDrainPerSecond = 4.0f;

	/** Stamina restored each second while neither running nor sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sprint", meta = (ClampMin = 0.0, AllowPrivateAccess = "true"))
	float StaminaRecoveryPerSecond = 50.0f;

	/** Running and sprinting lock at zero stamina until full recovery. */
	bool bSprintExhausted = false;

	/** True only while the sprint key is held, the player is moving, and stamina is available. */
	bool bIsSprinting = false;

	/** Set by the sprint input action while Alt is held. */
	bool bSprintInputHeld = false;

	/** Set by the run input action while Shift is held. */
	bool bRunInputHeld = false;

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

	/** Starts running while the input remains held. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartRun();

	/** Stops running when the input is released. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoEndRun();

	/** Starts sprinting while the input remains held and stamina permits it. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartSprint();

	/** Stops sprinting immediately when the input is released. */
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

	/** Hold to crouch. Crouching takes priority over running and sprinting. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStartCrouch();

	/** Requests standing; Character Movement waits until there is enough headroom. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoEndCrouch();

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
	bool IsSprinting() const { return bIsSprinting; }

};
