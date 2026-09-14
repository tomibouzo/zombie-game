#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/TimerHandle.h"
#include "PlayerMeleeComponent.generated.h"

class UAnimSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerMeleeAttackStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayerMeleeHit, const FHitResult&, Hit, float, AppliedDamage);

/** Unarmed attack behavior. Input routing stays on the character for future held items. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UPlayerMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerMeleeComponent();

	/** Starts one punch if alive, off cooldown, and able to pay the stamina cost. */
	UFUNCTION(BlueprintCallable, Category="Melee")
	bool TryAttack();

	/** Attack immediately, then retry at the normal interval while held. */
	UFUNCTION(BlueprintCallable, Category="Melee")
	void StartAttacking();

	/** Stop future swings; a punch already started finishes its hit check. */
	UFUNCTION(BlueprintCallable, Category="Melee")
	void StopAttacking();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="0"))
	float UnarmedDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="0"))
	float StaminaCost = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="0.05", Units="s"))
	float AttackInterval = 0.525f;

	/** Time from the click to the hit check; must be shorter than the attack interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="0", Units="s"))
	float HitDelay = 0.175f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="1", Units="cm"))
	float AttackReach = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee", meta=(ClampMin="0", Units="cm"))
	float HitRadius = 12.0f;

	/** Camera channel blocks stock Pawn capsules and walls; Visibility ignores stock Pawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee")
	TEnumAsByte<ECollisionChannel> HitTraceChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee|Animation")
	TObjectPtr<UAnimSequence> AttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee|Animation")
	FName AttackSlot = TEXT("DefaultSlot");

	UPROPERTY(BlueprintAssignable, Category="Melee")
	FPlayerMeleeAttackStarted OnAttackStarted;

	/** Broadcast for the first blocking impact, even if the actor accepts no damage. */
	UPROPERTY(BlueprintAssignable, Category="Melee")
	FPlayerMeleeHit OnMeleeHit;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveAttack();
	FTimerHandle HitTimer;
	double NextAttackTime = 0.0;
};
