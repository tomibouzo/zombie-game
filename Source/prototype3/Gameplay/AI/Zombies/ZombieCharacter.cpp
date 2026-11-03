#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Combat/CombatImpact.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "StateTree.h"
#include "UObject/ConstructorHelpers.h"

AZombieCharacter::AZombieCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AZombieAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->InitCapsuleSize(34, 88);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 180, 0);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeBody"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	FacingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacingMarker"));
	FacingMarker->SetupAttachment(GetRootComponent());
	FacingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FacingMarker->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(.58, .58, 1.7));
	FacingMarker->SetStaticMesh(Cube.Object);
	FacingMarker->SetRelativeLocation(FVector(36, 0, 50));
	FacingMarker->SetRelativeScale3D(FVector(.3, .15, .15));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mannequin(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	GetMesh()->SetSkeletalMesh(Mannequin.Object);
	GetMesh()->SetRelativeLocation(FVector(0,0,-88));
	GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Strike(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Second(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Bite(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Death(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"));
	IdleAnimation=Idle.Object; WalkAnimation=Walk.Object; StrikeAnimation=Strike.Object;
	SecondStrikeAnimation=Second.Object; BiteAnimation=Bite.Object; DeathAnimation=Death.Object;
}

void AZombieCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = FMath::Max(Tuning.MaxHealth, 1.f);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(Tuning.WanderSpeed, 0.f);
	RefreshAppearance();
}

void AZombieCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	VisualTime += DeltaSeconds * (GetVelocity().Size2D()>5 ? FMath::Clamp(GetVelocity().Size2D()/150.f,.3f,2.f) : 1.f);
	RefreshAppearance();
}

bool AZombieCharacter::IsStaggered() const { return GetWorld()->GetTimeSeconds() < StaggerUntil; }

bool AZombieCharacter::CanHit(const AActor* Victim) const
{
	if (!IsValid(Victim)) return false;
	const FVector Offset = Victim->GetActorLocation() - GetActorLocation();
	if (Offset.Size() > FMath::Max(40.f, Tuning.AttackRange)
		|| FVector::DotProduct(GetActorForwardVector(), Offset.GetSafeNormal2D()) < FMath::Cos(FMath::DegreesToRadians(Tuning.AttackHalfAngle))) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieAttack), false, this);
	Params.AddIgnoredActor(Victim);
	return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0,0,35),
		Victim->GetActorLocation(), ECC_Visibility, Params);
}

bool AZombieCharacter::BeginAttack(Aprototype3Character* Victim)
{
	if (!IsAlive() || IsStaggered() || bAttackActive || !IsValid(Victim) || !Victim->IsAlive() || !CanHit(Victim)) return false;
	AttackVictim = Victim;
	AttackKind = NextAttackIndex == 2 ? EZombieAttackKind::Bite : EZombieAttackKind::Strike;
	NextAttackIndex = (NextAttackIndex + 1) % 3;
	bAttackActive = true;
	bImpactResolved = false;
	ImpactAt = GetWorld()->GetTimeSeconds() + FMath::Max(.05f, AttackKind == EZombieAttackKind::Bite ? Tuning.BiteWindup : Tuning.StrikeWindup);
	RecoveryUntil = ImpactAt + FMath::Max(.05f, Tuning.RecoverySeconds);
	GetCharacterMovement()->StopMovementImmediately();
	return true;
}

void AZombieCharacter::TickAttack()
{
	if (!bAttackActive) return;
	if (!IsAlive() || IsStaggered() || !AttackVictim.IsValid() || !AttackVictim->IsAlive()) { CancelAttack(); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bImpactResolved && Now >= ImpactAt)
	{
		// Mark before applying damage: listeners can re-enter combat code.
		bImpactResolved = true;
		if (CanHit(AttackVictim.Get()))
		{
			FZombieDamageEvent Event; Event.Kind = AttackKind;
			const float Damage = AttackKind == EZombieAttackKind::Bite ? Tuning.BiteDamage : Tuning.StrikeDamage;
			if (AttackVictim->TakeDamage(FMath::Max(0.f, Damage), Event, GetController(), this) > 0)
				OnAttackHit.Broadcast(AttackVictim.Get(), AttackKind);
		}
	}
	if (Now >= RecoveryUntil) CancelAttack();
}

void AZombieCharacter::CancelAttack() { bAttackActive = false; AttackVictim.Reset(); }

float AZombieCharacter::TakeDamage(float Damage, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
	if (!IsAlive() || !FMath::IsFinite(Damage) || Damage <= 0) return 0;
	const float Applied = FMath::Min(Health, Damage);
	Health -= Applied;
	float Strength = 0;
	if (Event.IsOfType(FCombatImpactDamageEvent::ClassID)) Strength = static_cast<const FCombatImpactDamageEvent&>(Event).ImpactStrength;
	if (IsAlive() && FMath::IsFinite(Strength) && Strength > 0 && Strength >= Tuning.InterruptThreshold)
	{
		CancelAttack();
		StaggerUntil = GetWorld()->GetTimeSeconds() + FMath::Max(.05f, Tuning.StaggerSeconds);
		GetCharacterMovement()->StopMovementImmediately();
	}
	if (auto* AI = Cast<AZombieAIController>(GetController()))
	{
		AI->NotifyDamaged(EventInstigator ? EventInstigator->GetPawn() : Causer);
		if (!IsAlive()) AI->StopMovement();
	}
	if (!IsAlive())
	{
		DeathAt = GetWorld()->GetTimeSeconds();
		CancelAttack(); Behavior = EZombieBehavior::Dead; Suspicion = 0;
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetRelativeRotation(FRotator(0,0,90));
		Body->SetRelativeLocation(FVector(0,0,-60));
		FacingMarker->SetVisibility(false);
	}
	return Applied;
}

void AZombieCharacter::RefreshAppearance()
{
	const bool bHumanoid = bUseMannequin && GetMesh()->GetSkeletalMeshAsset() && IdleAnimation && WalkAnimation && StrikeAnimation && SecondStrikeAnimation && BiteAnimation && DeathAnimation;
	Body->SetVisibility(!bHumanoid);
	GetMesh()->SetVisibility(bHumanoid);
	FacingMarker->SetVisibility(!bHumanoid && IsAlive());
	FacingMarker->SetRelativeScale3D(IsAttacking() ? FVector(.6,.3,.3) : FVector(.3,.15,.15));
	if (!bHumanoid) return;
	UAnimSequence* Animation = !IsAlive() ? DeathAnimation.Get() : IsAttacking() ?
		(AttackKind==EZombieAttackKind::Bite ? BiteAnimation.Get() : NextAttackIndex==2 ? SecondStrikeAnimation.Get() : StrikeAnimation.Get()) :
		GetVelocity().Size2D()>5 ? WalkAnimation.Get() : IdleAnimation.Get();
	if (CurrentAnimation != Animation)
	{
		CurrentAnimation=Animation; VisualTime=0;
		GetMesh()->PlayAnimation(Animation,false);
		// Extract and discard authored root motion: navigation owns all movement.
		GetMesh()->GetSingleNodeInstance()->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		GetMesh()->GetSingleNodeInstance()->SetPlaying(false);
	}
	float Position = FMath::Fmod(VisualTime,Animation->GetPlayLength());
	const double Now=GetWorld()->GetTimeSeconds();
	if (!IsAlive()) Position=FMath::Min<float>(Now-DeathAt,Animation->GetPlayLength());
	else if (IsAttacking())
	{
		const float Windup=FMath::Max(.05f,AttackKind==EZombieAttackKind::Bite ? Tuning.BiteWindup : Tuning.StrikeWindup);
		const float Phase=Now<ImpactAt ? .45f*(1.f-(ImpactAt-Now)/Windup) : .45f+.55f*(Now-ImpactAt)/FMath::Max(.05f,Tuning.RecoverySeconds);
		Position=FMath::Clamp(Phase,0.f,1.f)*Animation->GetPlayLength();
	}
	GetMesh()->GetSingleNodeInstance()->SetPosition(Position,false);
}

