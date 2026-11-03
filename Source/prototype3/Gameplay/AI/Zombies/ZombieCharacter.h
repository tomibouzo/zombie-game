#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Gameplay/AI/Zombies/ZombieTypes.h"
#include "ZombieCharacter.generated.h"

class UStaticMeshComponent;
class UStateTree;
class Aprototype3Character;
class UAnimSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZombieAttackHit, AActor*, Victim, EZombieAttackKind, Kind);

UCLASS()
class PROTOTYPE3_API AZombieCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AZombieCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie|Appearance") bool bUseMannequin = true;
	virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zombie", meta=(ShowOnlyInnerProperties)) FZombieTuning Tuning;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zombie") TObjectPtr<UStateTree> BehaviorTree;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") float Health = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") EZombieBehavior Behavior = EZombieBehavior::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") float Suspicion = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Zombie") int32 NextAttackIndex = 0;
	UPROPERTY(BlueprintAssignable, Category="Zombie") FZombieAttackHit OnAttackHit;
	bool IsAlive() const { return Health > 0; }
	bool IsAttacking() const { return bAttackActive; }
	EZombieAttackKind GetAttackKind() const { return AttackKind; }
	bool IsStaggered() const;
	bool BeginAttack(Aprototype3Character* Victim);
	void TickAttack();
	void CancelAttack();
	void RefreshAppearance();
	bool CanHit(const AActor* Victim) const;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FacingMarker;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> StrikeAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> SecondStrikeAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> BiteAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> DeathAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> CurrentAnimation;
	double VisualTime = 0;
	double DeathAt = 0;
	TWeakObjectPtr<Aprototype3Character> AttackVictim;
	EZombieAttackKind AttackKind = EZombieAttackKind::Strike;
	double ImpactAt = 0;
	double RecoveryUntil = 0;
	double StaggerUntil = 0;
	bool bAttackActive = false;
	bool bImpactResolved = false;
};
