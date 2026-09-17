#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"

#include "Engine/World.h"

UPlayerVitalsComponent::UPlayerVitalsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerVitalsComponent::InitializeVitals(float InMaxHealth, float InMaxStamina,
	float InStaminaRecoveryPerSecond, float InExhaustionRecoveryFraction)
{
	MaxHealth = FMath::IsFinite(InMaxHealth) ? FMath::Max(InMaxHealth, 1.0f) : 100.0f;
	MaxStamina = FMath::IsFinite(InMaxStamina) ? FMath::Max(InMaxStamina, 1.0f) : 100.0f;
	StaminaRecoveryPerSecond = FMath::IsFinite(InStaminaRecoveryPerSecond)
		? FMath::Max(InStaminaRecoveryPerSecond, 0.0f)
		: 50.0f;
	ExhaustionRecoveryFraction = FMath::IsFinite(InExhaustionRecoveryFraction)
		? FMath::Clamp(InExhaustionRecoveryFraction, 0.0f, 1.0f)
		: 0.25f;

	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	StaminaRecoveryResumeTime = 0.0;
	bStaminaExhausted = false;
	BroadcastHealthChanged();
	BroadcastStaminaChanged();
}

float UPlayerVitalsComponent::ApplyDamage(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || !IsAlive())
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(CurrentHealth - Amount, 0.0f);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	BroadcastHealthChanged();
	if (CurrentHealth <= 0.0f)
	{
		OnDeath.Broadcast();
	}
	return AppliedDamage;
}

float UPlayerVitalsComponent::Heal(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || !IsAlive() || CurrentHealth >= MaxHealth)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Min(CurrentHealth + Amount, MaxHealth);
	BroadcastHealthChanged();
	return CurrentHealth - PreviousHealth;
}

bool UPlayerVitalsComponent::TryConsumeStamina(float Amount, float RecoveryDelay)
{
	UWorld* World = GetWorld();
	if (!IsAlive() || !World || !FMath::IsFinite(Amount) || Amount < 0.0f || CurrentStamina < Amount)
	{
		return false;
	}

	CurrentStamina = FMath::Max(CurrentStamina - Amount, 0.0f);
	StaminaRecoveryResumeTime = FMath::Max(StaminaRecoveryResumeTime,
		World->GetTimeSeconds() + FMath::Max(RecoveryDelay, 0.0f));
	if (CurrentStamina <= 0.0f)
	{
		bStaminaExhausted = true;
	}
	BroadcastStaminaChanged();
	return true;
}

bool UPlayerVitalsComponent::DrainStamina(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || CurrentStamina <= 0.0f)
	{
		return false;
	}

	const float PreviousStamina = CurrentStamina;
	CurrentStamina = FMath::Max(CurrentStamina - Amount, 0.0f);
	if (CurrentStamina <= 0.0f)
	{
		bStaminaExhausted = true;
	}
	if (CurrentStamina != PreviousStamina)
	{
		BroadcastStaminaChanged();
		return true;
	}
	return false;
}

bool UPlayerVitalsComponent::RecoverStamina(float DeltaSeconds, float RecoveryMultiplier)
{
	UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(RecoveryMultiplier)
		|| DeltaSeconds <= 0.0f || RecoveryMultiplier < 0.0f)
	{
		return false;
	}

	const float RecoverySeconds = static_cast<float>(FMath::Clamp(
		World->GetTimeSeconds() - StaminaRecoveryResumeTime, 0.0, static_cast<double>(DeltaSeconds)));
	const float PreviousStamina = CurrentStamina;
	CurrentStamina = FMath::Min(CurrentStamina
		+ (StaminaRecoveryPerSecond * RecoveryMultiplier * RecoverySeconds), MaxStamina);
	if (CurrentStamina >= MaxStamina * ExhaustionRecoveryFraction)
	{
		bStaminaExhausted = false;
	}
	if (CurrentStamina != PreviousStamina)
	{
		BroadcastStaminaChanged();
		return true;
	}
	return false;
}

float UPlayerVitalsComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

float UPlayerVitalsComponent::GetStaminaPercent() const
{
	return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f;
}

void UPlayerVitalsComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, GetHealthPercent());
}

void UPlayerVitalsComponent::BroadcastStaminaChanged()
{
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina, GetStaminaPercent());
}
