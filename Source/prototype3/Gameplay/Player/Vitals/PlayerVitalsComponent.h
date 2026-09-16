#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerVitalsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerVitalValueChangedDelegate,
	float, CurrentValue, float, MaxValue, float, Percentage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerDiedDelegate);

/** Owns the player's health and stamina state. Movement policy stays on the character. */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UPlayerVitalsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerVitalsComponent();

	/** Initializes both resources from character/Blueprint-compatible configuration. */
	void InitializeVitals(float InMaxHealth, float InMaxStamina, float InStaminaRecoveryPerSecond);

	/** Applies damage without allowing health to fall below zero. */
	UFUNCTION(BlueprintCallable, Category="Player|Vitals")
	float ApplyDamage(float Amount);

	/** Restores health without reviving a dead owner. Returns the amount restored. */
	UFUNCTION(BlueprintCallable, Category="Player|Vitals")
	float Heal(float Amount);

	/** Pays an action cost atomically and optionally postpones stamina recovery. */
	UFUNCTION(BlueprintCallable, Category="Player|Vitals")
	bool TryConsumeStamina(float Amount, float RecoveryDelay = 0.0f);

	/** Drains a continuous stamina amount. Reaching zero enables the exhaustion lock. */
	bool DrainStamina(float Amount);

	/** Recovers stamina for this frame, respecting any active recovery delay. */
	bool RecoverStamina(float DeltaSeconds, float RecoveryMultiplier = 1.0f);

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	bool IsAlive() const { return CurrentHealth > 0.0f; }

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	bool IsStaminaExhausted() const { return bStaminaExhausted; }

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetStaminaPercent() const;

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category="Player|Vitals")
	float GetMaxStamina() const { return MaxStamina; }

	UPROPERTY(BlueprintAssignable, Category="Player|Vitals")
	FPlayerVitalValueChangedDelegate OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Player|Vitals")
	FPlayerVitalValueChangedDelegate OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category="Player|Vitals")
	FPlayerDiedDelegate OnDeath;

private:
	void BroadcastHealthChanged();
	void BroadcastStaminaChanged();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player|Vitals", meta=(AllowPrivateAccess="true"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player|Vitals", meta=(AllowPrivateAccess="true"))
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player|Vitals", meta=(AllowPrivateAccess="true"))
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player|Vitals", meta=(AllowPrivateAccess="true"))
	float CurrentStamina = 0.0f;

	float StaminaRecoveryPerSecond = 50.0f;
	double StaminaRecoveryResumeTime = 0.0;
	bool bStaminaExhausted = false;
};
