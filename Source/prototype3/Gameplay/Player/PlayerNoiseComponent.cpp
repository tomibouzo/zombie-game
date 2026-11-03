#include "Gameplay/Player/PlayerNoiseComponent.h"
#include "Core/Characters/prototype3Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionSystem.h"

UPlayerNoiseComponent::UPlayerNoiseComponent() { PrimaryComponentTick.bCanEverTick = true; }

void UPlayerNoiseComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousLocation = GetOwner()->GetActorLocation();
	if (auto* P = Cast<Aprototype3Character>(GetOwner()))
	{
		AddTickPrerequisiteActor(P);
		AddTickPrerequisiteComponent(P->GetCharacterMovement());
		UAIPerceptionSystem::RegisterPerceptionStimuliSource(this, UAISense_Sight::StaticClass(), P);
	}
}

void UPlayerNoiseComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaSeconds,TickType,Function);
	auto* P = Cast<Aprototype3Character>(GetOwner());
	if (!P) return;
	const float Travel = FVector::Dist2D(PreviousLocation,P->GetActorLocation());
	PreviousLocation = P->GetActorLocation();
	if (!P->HasAuthority() || !P->IsPlayerControlled() || !P->IsAlive() || P->bIsCrouched
		|| P->GetActiveStance() == EPlayerLocomotionStance::Crouching || !P->GetCharacterMovement()->IsMovingOnGround()
		|| Travel > FMath::Max(200.f, P->GetCharacterMovement()->GetMaxSpeed()*DeltaSeconds*2))
	{ TravelSinceStep = 0; return; }
	TravelSinceStep += Travel;
	if (TravelSinceStep >= FMath::Max(10.f,StepDistance))
	{
		TravelSinceStep = 0;
		const float Radius = P->IsRunning() || P->IsSprinting() ? RunNoiseRadius : WalkNoiseRadius;
		if (Radius > 0)
		{
			UAISense_Hearing::ReportNoiseEvent(this,P->GetActorLocation(),1,P,Radius,TEXT("Footstep"));
			++EmittedSteps;
		}
	}
}
