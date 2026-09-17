#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "Core/Characters/prototype3Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UPlayerMeleeComponent::UPlayerMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> PunchAsset(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	if (PunchAsset.Succeeded())
	{
		AttackAnimation = PunchAsset.Object;
	}
}

void UPlayerMeleeComponent::StartAttacking()
{
	SetComponentTickEnabled(true);
	TryAttack();
}

void UPlayerMeleeComponent::StopAttacking()
{
	SetComponentTickEnabled(false);
}

void UPlayerMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const Aprototype3Character* Character = Cast<Aprototype3Character>(GetOwner());
	if (!Character || !Character->HasAuthority() || !Character->IsAlive() || !Character->GetController())
	{
		StopAttacking();
		return;
	}

	// The cooldown limits accepted swings. Insufficient stamina is retried without
	// spending or postponing regeneration, so holding resumes once stamina permits.
	// At most one swing can start per frame; slow frames never produce a burst.
	TryAttack();
}

bool UPlayerMeleeComponent::TryAttack()
{
	Aprototype3Character* Character = Cast<Aprototype3Character>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || !Character->HasAuthority() || !Character->IsAlive()
		|| World->GetTimeSeconds() < NextAttackTime || World->GetTimerManager().IsTimerActive(HitTimer))
	{
		return false;
	}

	// Every repeated punch requires the standing posture, not just the initial
	// mouse press. Under a low ceiling, retry once the capsule can fully stand.
	Character->RequestStandingForAction();
	if (!Character->IsStandingForAction())
	{
		return false;
	}

	const float Interval = FMath::Max(AttackInterval, 0.05f);
	// Reserve the cooldown before spending: stamina listeners may invoke other actions.
	NextAttackTime = World->GetTimeSeconds() + Interval;
	if (!Character->TryConsumeStamina(FMath::Max(StaminaCost, 0.0f), Interval))
	{
		NextAttackTime = 0.0;
		return false;
	}

	const float Delay = FMath::Clamp(HitDelay, 0.0f, Interval * 0.9f);
	if (Delay > 0.0f)
	{
		World->GetTimerManager().SetTimer(HitTimer, this, &UPlayerMeleeComponent::ResolveAttack, Delay, false);
	}

	// The first-person mesh copies the body pose, including this existing slot.
	if (AttackAnimation)
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			const float PlayRate = FMath::Max(AttackAnimation->GetPlayLength() / Interval, 0.01f);
			if (UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
				AttackAnimation, AttackSlot, 0.05f, 0.1f, PlayRate))
			{
				if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage))
				{
					// Keep normal player movement; the punch must not drive capsule motion.
					Instance->PushDisableRootMotion();
				}
			}
		}
	}

	OnAttackStarted.Broadcast();
	if (Delay <= 0.0f)
	{
		ResolveAttack();
	}
	return true;
}

void UPlayerMeleeComponent::ResolveAttack()
{
	Aprototype3Character* Character = Cast<Aprototype3Character>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || !Character->HasAuthority() || !Character->IsAlive())
	{
		return;
	}

	const UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
	const FVector Start = Camera->GetComponentLocation();
	const FVector Direction = Camera->GetForwardVector();
	const float Reach = FMath::Max(AttackReach, 1.0f);
	const float Radius = FMath::Clamp(HitRadius, 0.0f, Reach * 0.5f);
	const FVector End = Start + Direction * (Reach - Radius);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerUnarmedAttack), false, Character);
	FHitResult Hit;
	// A single blocking hit stops at walls and prevents damaging multiple actors per punch.
	const bool bHit = Radius > UE_KINDA_SMALL_NUMBER
		? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, HitTraceChannel,
			FCollisionShape::MakeSphere(Radius), QueryParams)
		: World->LineTraceSingleByChannel(Hit, Start, End, HitTraceChannel, QueryParams);
	if (bHit && Hit.GetActor())
	{
		const float AppliedDamage = UGameplayStatics::ApplyPointDamage(Hit.GetActor(),
			FMath::Max(UnarmedDamage, 0.0f), Direction, Hit, Character->GetController(), Character,
			UDamageType::StaticClass());
		OnMeleeHit.Broadcast(Hit, AppliedDamage);
	}
}

void UPlayerMeleeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttacking();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimer);
	}
	Super::EndPlay(EndPlayReason);
}
