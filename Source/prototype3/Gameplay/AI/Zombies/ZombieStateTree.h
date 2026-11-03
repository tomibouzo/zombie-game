#pragma once
#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "Gameplay/AI/Zombies/ZombieTypes.h"
#include "ZombieStateTree.generated.h"

USTRUCT()
struct FZombieNodeData { GENERATED_BODY() };

USTRUCT(meta=(DisplayName="Zombie behavior matches", Category="Zombie"))
struct FZombieStateCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FZombieNodeData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UPROPERTY(EditAnywhere, Category="Condition") EZombieBehavior Behavior = EZombieBehavior::Idle;
	UPROPERTY(EditAnywhere, Category="Condition") bool bInvert = false;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT(meta=(DisplayName="Execute zombie behavior", Category="Zombie"))
struct FZombieBehaviorTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FZombieNodeData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UPROPERTY(EditAnywhere, Category="Task") EZombieBehavior Behavior = EZombieBehavior::Idle;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaSeconds) const override;
};
