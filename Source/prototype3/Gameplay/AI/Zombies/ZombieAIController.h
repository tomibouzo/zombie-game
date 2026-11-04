#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Gameplay/AI/Zombies/ZombieTypes.h"
#include "ZombieAIController.generated.h"

class AZombieCharacter;
class Aprototype3Character;
class UStateTreeAIComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
struct FAIStimulus;

UCLASS()
class PROTOTYPE3_API AZombieAIController : public AAIController
{
	GENERATED_BODY()
public:
	AZombieAIController();
	virtual void OnPossess(APawn* Pawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	EZombieBehavior GetDesiredBehavior() const { return DesiredBehavior; }
	void EnterBehavior(EZombieBehavior State);
	void ExecuteBehavior(EZombieBehavior State, float DeltaSeconds);
	void RefreshSenses();
	void NotifyDamaged(AActor* Causer);
	void ForgetDebugPlayer(Aprototype3Character* Player);
	FVector GetLastKnownLocation() const { return LastKnownLocation; }
	bool HasVisualContact() const { return bVisualContact; }
	bool IsAlerted() const { return bAlerted; }
	Aprototype3Character* GetTarget() const { return Target.Get(); }
	bool CanSee(const Aprototype3Character* Candidate) const;
	UStateTreeAIComponent* GetZombieStateTree() const { return StateTree; }
private:
	UFUNCTION() void PerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	void MoveToClue(const FVector& Location, float Acceptance = 50);
	void ForgetEncounter();
	UPROPERTY() TObjectPtr<UStateTreeAIComponent> StateTree;
	UPROPERTY() TObjectPtr<UAISenseConfig_Sight> Sight;
	UPROPERTY() TObjectPtr<UAISenseConfig_Hearing> Hearing;
	TWeakObjectPtr<AZombieCharacter> Zombie;
	TWeakObjectPtr<Aprototype3Character> Target;
	FVector LastKnownLocation = FVector::ZeroVector;
	FVector SearchOrigin = FVector::ZeroVector;
	FVector Destination = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	FVector PreviousMoveGoal = FVector(1.e9);
	EZombieBehavior DesiredBehavior = EZombieBehavior::Idle;
	bool bVisualContact = false;
	bool bAlerted = false;
	bool bHasClue = false;
	bool bRoaming = false;
	bool bReachedLastClue = false;
	double ClueExpires = 0;
	double SearchExpires = 0;
	double IdleUntil = 0;
	double NextSearchStep = 0;
	double NextMoveRequest = 0;
	double RoamExpires = 0;
};
