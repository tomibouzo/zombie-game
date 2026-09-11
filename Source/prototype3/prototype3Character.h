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
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPlayerHealthUpdatedDelegate, float, Percentage);

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

protected:
	/** Maximum health available to this character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health", meta = (ClampMin = 1.0, AllowPrivateAccess = "true"))
	float MaxHealth = 100.0f;

	/** Current health remaining. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.0f;

	/** Base stamina for characters that do not yet implement a stamina mechanic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stamina", meta = (ClampMin = 1.0, AllowPrivateAccess = "true"))
	float MaxStamina = 100.0f;

	/** Current base stamina. The Horror character overrides its displayed value with sprint stamina. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stamina", meta = (AllowPrivateAccess = "true"))
	float CurrentStamina = 0.0f;


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

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	/** Initializes universal player vitals. */
	virtual void BeginPlay() override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Delegate called whenever health changes. */
	FPlayerHealthUpdatedDelegate OnHealthUpdated;

	/** Applies incoming damage and updates the health display. */
	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Returns health normalized to the 0-1 range used by progress bars. */
	UFUNCTION(BlueprintPure, Category="Health")
	virtual float GetHealthPercent() const;

	/** Returns stamina normalized to the 0-1 range used by progress bars. */
	UFUNCTION(BlueprintPure, Category="Stamina")
	virtual float GetStaminaPercent() const;

};

