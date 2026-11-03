#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "StateTree.h"
#include "GameFramework/PlayerController.h"

AZombieAIController::AZombieAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	StateTree = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("ZombieStateTree"));
	StateTree->SetStartLogicAutomatically(false);
	auto* Senses = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ZombiePerception"));
	SetPerceptionComponent(*Senses);
	Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	Sight->DetectionByAffiliation.bDetectEnemies = Sight->DetectionByAffiliation.bDetectFriendlies = Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Hearing->DetectionByAffiliation = Sight->DetectionByAffiliation;
	Sight->AutoSuccessRangeFromLastSeenLocation = -1;
	Sight->SetMaxAge(1);
	Hearing->SetMaxAge(1);
	Senses->ConfigureSense(*Sight);
	Senses->ConfigureSense(*Hearing);
	Senses->SetDominantSense(UAISense_Sight::StaticClass());
	Senses->OnTargetPerceptionUpdated.AddDynamic(this, &AZombieAIController::PerceptionUpdated);
}

void AZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Zombie = Cast<AZombieCharacter>(InPawn);
	if (!Zombie.IsValid()) return;
	Home = InPawn->GetActorLocation();
	IdleUntil = GetWorld()->GetTimeSeconds() + Zombie->Tuning.IdleSeconds;
	RefreshSenses();
	UStateTree* Tree = Zombie->BehaviorTree;
	if (!Tree) Tree = LoadObject<UStateTree>(nullptr, TEXT("/Game/Zombies/ST_ZombieBasic.ST_ZombieBasic"));
	if (Tree)
	{
		StateTree->SetStateTree(Tree);
		StateTree->AddTickPrerequisiteActor(this);
		StateTree->StartLogic();
	}
	else UE_LOG(LogTemp, Error, TEXT("Zombie requires ST_ZombieBasic. Run Scripts/VerifyZombies.ps1 -CreateFixtures."));
}

void AZombieAIController::RefreshSenses()
{
	if (!Zombie.IsValid()) return;
	const auto& T = Zombie->Tuning;
	Sight->SightRadius = FMath::Max(1.f, T.SightRadius);
	Sight->LoseSightRadius = Sight->SightRadius;
	Sight->PeripheralVisionAngleDegrees = FMath::Clamp(T.SightHalfAngle, 1.f, 89.f);
	Hearing->HearingRange = FMath::Max(0.f, T.HearingRadius);
	GetPerceptionComponent()->ConfigureSense(*Sight);
	GetPerceptionComponent()->ConfigureSense(*Hearing);
}

bool AZombieAIController::CanSee(const Aprototype3Character* Candidate) const
{
	if (!Zombie.IsValid() || !IsValid(Candidate) || !Candidate->IsAlive() || Candidate->IsDebugInvisibleToZombies()) return false;
	const auto& T = Zombie->Tuning;
	const FVector Eye = Zombie->GetActorLocation() + FVector(0,0,60);
	const FVector Offset = Candidate->GetActorLocation() - Eye;
	if (Offset.Size() > FMath::Max(1.f,T.SightRadius) || FVector::DotProduct(Zombie->GetActorForwardVector().GetSafeNormal2D(), Offset.GetSafeNormal2D())
		< FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(T.SightHalfAngle,1.f,89.f)))) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSight), false, Zombie.Get());
	Params.AddIgnoredActor(Candidate);
	// Sample torso and head using the current capsule, including crouched height.
	// Low cover must not hide an exposed head; full walls still block both rays.
	const float HalfHeight = Candidate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	for (float Height : { .2f, .8f })
		if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Candidate->GetActorLocation()+FVector(0,0,HalfHeight*Height),ECC_Visibility,Params)) return true;
	return false;
}

void AZombieAIController::PerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	auto* Player = Cast<Aprototype3Character>(Actor);
	if (!Zombie.IsValid() || !Zombie->IsAlive() || !Player || !Player->IsPlayerControlled() || !Player->IsAlive() || Player->IsDebugInvisibleToZombies()) return;
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		if (Stimulus.WasSuccessfullySensed() && (!bVisualContact || Target.Get() == Player)) Target = Player;
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed() && !bVisualContact)
	{
		// Copy the event location only. Never navigate to a hidden actor's live transform.
		LastKnownLocation = SearchOrigin = Destination = Stimulus.StimulusLocation;
		bHasClue = true; bReachedLastClue = false;
		ClueExpires = GetWorld()->GetTimeSeconds() + Zombie->Tuning.InvestigateSeconds;
		if (bAlerted) SearchExpires = GetWorld()->GetTimeSeconds() + Zombie->Tuning.SearchSeconds;
		Zombie->Suspicion = FMath::Max(Zombie->Suspicion, .25f);
	}
}

void AZombieAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Zombie.IsValid() || !HasAuthority()) return;
	auto* Z = Zombie.Get();
	if (!Z->IsAlive()) { DesiredBehavior = EZombieBehavior::Dead; return; }
	const auto& T = Z->Tuning;
	const double Now = GetWorld()->GetTimeSeconds();
	if (!FMath::IsNearlyEqual(Sight->SightRadius, FMath::Max(1.f,T.SightRadius)) || !FMath::IsNearlyEqual(Sight->PeripheralVisionAngleDegrees,T.SightHalfAngle)
		|| !FMath::IsNearlyEqual(Hearing->HearingRange,T.HearingRadius)) RefreshSenses();
	const bool bWasVisible = bVisualContact;
	// Perception identifies candidates; re-check visibility each frame so a stale sense
	// notification cannot follow the player through a wall during its update interval.
	bVisualContact = CanSee(Target.Get());
	// The basic zombie targets the local player. Validate directly so initial
	// acquisition and exposed-head sight do not wait on a coarse sight update.
	if (!bVisualContact)
		if (const auto* PC=GetWorld()->GetFirstPlayerController())
			if (auto* P=Cast<Aprototype3Character>(PC->GetPawn()); P && CanSee(P))
			{ Target=P; bVisualContact=true; }
	if (!bVisualContact)
	{
		TArray<AActor*> Candidates;
		GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Candidates);
		for (AActor* Actor : Candidates)
			if (auto* P = Cast<Aprototype3Character>(Actor); P && P->IsPlayerControlled() && CanSee(P))
			{ Target = P; bVisualContact = true; break; }
	}
	if (Target.IsValid() && (!Target->IsAlive() || Target->IsDebugInvisibleToZombies())) { ForgetEncounter(); StopMovement(); }
	if (bVisualContact)
	{
		LastKnownLocation = SearchOrigin = Target->GetActorLocation();
		bHasClue = true; bReachedLastClue = false;
		const float Distance = FVector::Dist(Z->GetActorLocation(), LastKnownLocation);
		// Avoid an abrupt detection-time jump when crossing the close radius.
		const float Proximity = FMath::Clamp((Distance-T.CloseSightRadius)/FMath::Max(1.f,T.SightRadius-T.CloseSightRadius),0.f,1.f);
		const float Seconds = FMath::Lerp(T.CloseDetectionSeconds,T.FarDetectionSeconds,Proximity);
		Z->Suspicion = FMath::Clamp(Z->Suspicion + DeltaSeconds / FMath::Max(.05f,Seconds), 0.f, 1.f);
		bAlerted |= Z->Suspicion >= 1.f;
		SearchExpires = Now + FMath::Max(.1f,T.SearchSeconds);
		ClueExpires = Now + FMath::Max(.1f,T.InvestigateSeconds);
	}
	else
	{
		Z->Suspicion = FMath::Max(0.f, Z->Suspicion - FMath::Max(0.f,T.SuspicionDecayPerSecond)*DeltaSeconds);
		if (bWasVisible) { Destination = LastKnownLocation; bReachedLastClue = false; NextSearchStep = Now; }
		if ((bAlerted && Now >= SearchExpires) || (!bAlerted && bHasClue && Now >= ClueExpires)) ForgetEncounter();
	}
	if (Z->IsStaggered()) DesiredBehavior = EZombieBehavior::Staggered;
	else if (Z->IsAttacking()) DesiredBehavior = EZombieBehavior::Attack;
	else if (bVisualContact && bAlerted) DesiredBehavior = Z->CanHit(Target.Get()) ? EZombieBehavior::Attack : EZombieBehavior::Chase;
	else if (bAlerted) DesiredBehavior = EZombieBehavior::Search;
	else if (bHasClue) DesiredBehavior = EZombieBehavior::Investigate;
	else DesiredBehavior = bRoaming ? EZombieBehavior::Roam : EZombieBehavior::Idle;
}

void AZombieAIController::ForgetEncounter()
{
	bAlerted = bHasClue = bVisualContact = bRoaming = false;
	Target.Reset();
	Zombie->Suspicion = 0;
	Zombie->NextAttackIndex = 0;
	Zombie->CancelAttack();
	IdleUntil = GetWorld()->GetTimeSeconds() + FMath::Max(.1f,Zombie->Tuning.IdleSeconds);
}

void AZombieAIController::NotifyDamaged(AActor* Causer)
{
	if (!Zombie.IsValid() || !Zombie->IsAlive()) return;
	if (const auto* Player = Cast<Aprototype3Character>(Causer); Player && Player->IsDebugInvisibleToZombies()) return;
	// An unseen impact is a clue at the victim, not knowledge of the shooter's position.
	LastKnownLocation = SearchOrigin = Destination = Zombie->GetActorLocation();
	bHasClue = true; bReachedLastClue = false;
	ClueExpires = GetWorld()->GetTimeSeconds() + Zombie->Tuning.InvestigateSeconds;
	if (auto* Player = Cast<Aprototype3Character>(Causer); Player && CanSee(Player))
	{
		Target = Player; Zombie->Suspicion = 1;
	}
}

void AZombieAIController::ForgetDebugPlayer(Aprototype3Character* Player)
{
	if (!Zombie.IsValid() || !Player || !Player->IsDebugInvisibleToZombies()) return;
	GetPerceptionComponent()->ForgetActor(Player);
	ForgetEncounter();
	StopMovement();
	DesiredBehavior = Zombie->IsAlive() ? EZombieBehavior::Idle : EZombieBehavior::Dead;
}

void AZombieAIController::EnterBehavior(EZombieBehavior State)
{
	if (!Zombie.IsValid()) return;
	Zombie->Behavior = State;
	StopMovement(); PreviousMoveGoal = FVector(1.e9); NextMoveRequest = 0;
	const bool bCalm = State == EZombieBehavior::Idle || State == EZombieBehavior::Roam;
	Zombie->GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.f,bCalm ? Zombie->Tuning.WanderSpeed : Zombie->Tuning.PursuitSpeed);
	if (State == EZombieBehavior::Attack && bVisualContact) Zombie->BeginAttack(Target.Get());
	if (State == EZombieBehavior::Search) { Destination = LastKnownLocation; bReachedLastClue = false; NextSearchStep = 0; }
	if (State == EZombieBehavior::Staggered || State == EZombieBehavior::Dead)
	{
		Zombie->CancelAttack(); Zombie->GetCharacterMovement()->StopMovementImmediately();
	}
}

void AZombieAIController::MoveToClue(const FVector& Location, float Acceptance)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextMoveRequest) return;
	NextMoveRequest = Now + .35;
	if (FVector::DistSquared(PreviousMoveGoal,Location) < 30*30 && GetMoveStatus() == EPathFollowingStatus::Moving) return;
	PreviousMoveGoal = Location;
	MoveToLocation(Location, Acceptance, false, true, false, true, nullptr, false);
}

void AZombieAIController::ExecuteBehavior(EZombieBehavior State, float DeltaSeconds)
{
	if (!Zombie.IsValid() || State != DesiredBehavior) return;
	auto* Z = Zombie.Get();
	const auto& T = Z->Tuning;
	const double Now = GetWorld()->GetTimeSeconds();
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (State == EZombieBehavior::Attack)
	{
		if (!Z->IsAttacking() && bVisualContact) Z->BeginAttack(Target.Get());
		Z->TickAttack(); return;
	}
	if (State == EZombieBehavior::Chase)
	{
		MoveToClue(LastKnownLocation, 35);
		if (FVector::DistSquared(Z->GetActorLocation(),LastKnownLocation) < FMath::Square(T.AttackRange+50))
		{
			const FRotator Facing = (LastKnownLocation-Z->GetActorLocation()).Rotation();
			Z->SetActorRotation(FMath::RInterpConstantTo(Z->GetActorRotation(), FRotator(0,Facing.Yaw,0),DeltaSeconds,180));
		}
	}
	else if (State == EZombieBehavior::Idle && Now >= IdleUntil)
	{
		FNavLocation Point;
		if (Nav && T.WanderRadius > 0 && Nav->GetRandomReachablePointInRadius(Home,T.WanderRadius,Point))
		{ Destination = Point.Location; bRoaming = true; RoamExpires = Now+8; }
		IdleUntil = Now + FMath::Max(.1f,T.IdleSeconds);
	}
	else if (State == EZombieBehavior::Roam)
	{
		MoveToClue(Destination);
		if (FVector::Dist2D(Z->GetActorLocation(),Destination) < 70 || Now > RoamExpires)
		{ bRoaming = false; IdleUntil = Now+FMath::Max(.1f,T.IdleSeconds); }
	}
	else if (State == EZombieBehavior::Investigate)
	{
		MoveToClue(LastKnownLocation);
		if (!bVisualContact && FVector::Dist2D(Z->GetActorLocation(),LastKnownLocation)<80)
			Z->AddActorWorldRotation(FRotator(0,40*DeltaSeconds,0));
	}
	else if (State == EZombieBehavior::Search)
	{
		MoveToClue(Destination);
		const bool bArrived = FVector::Dist2D(Z->GetActorLocation(),Destination)<80;
		if (bArrived || GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Z->AddActorWorldRotation(FRotator(0,65*DeltaSeconds,0));
			if (Now >= NextSearchStep)
			{
				bReachedLastClue = true;
				FNavLocation Point;
				if (Nav && Nav->GetRandomReachablePointInRadius(SearchOrigin,FMath::Max(1.f,T.SearchRadius),Point)) Destination = Point.Location;
				NextSearchStep = Now+2;
			}
		}
	}
}

void AZombieAIController::EndPlay(const EEndPlayReason::Type Reason)
{
	StateTree->StopLogic(TEXT("EndPlay"));
	Super::EndPlay(Reason);
}

void AZombieAIController::OnUnPossess()
{
	// AI schema needs the pawn while stopping; the base controller clears it first.
	StateTree->StopLogic(TEXT("Unpossess"));
	Super::OnUnPossess();
}

