#include "Gameplay/AI/Zombies/ZombieStateTree.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "StateTreeExecutionContext.h"

bool FZombieStateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const auto* AI = Cast<AZombieAIController>(Context.GetOwner());
	return AI && ((AI->GetDesiredBehavior() == Behavior) != bInvert);
}

EStateTreeRunStatus FZombieBehaviorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	if (auto* AI = Cast<AZombieAIController>(Context.GetOwner()))
	{ AI->EnterBehavior(Behavior); return EStateTreeRunStatus::Running; }
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FZombieBehaviorTask::Tick(FStateTreeExecutionContext& Context, float DeltaSeconds) const
{
	if (auto* AI = Cast<AZombieAIController>(Context.GetOwner()))
	{ AI->ExecuteBehavior(Behavior,DeltaSeconds); return EStateTreeRunStatus::Running; }
	return EStateTreeRunStatus::Failed;
}
