#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "ZombieTypes.generated.h"

UENUM(BlueprintType)
enum class EZombieBehavior : uint8 { Idle, Roam, Investigate, Chase, Search, Attack, Staggered, Dead };

UENUM(BlueprintType)
enum class EZombieAttackKind : uint8 { Strike, Bite };

/** Per-instance tuning. Future abilities can compose changes to this profile. Units are cm and seconds. */
USTRUCT(BlueprintType)
struct FZombieTuning
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="1")) float MaxHealth = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0")) float WanderSpeed = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0")) float PursuitSpeed = 260;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0")) float WanderRadius = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.1")) float IdleSeconds = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="1")) float SightRadius = 1200;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="1", ClampMax="89")) float SightHalfAngle = 65;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="0")) float CloseSightRadius = 250;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="0.05")) float CloseDetectionSeconds = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="0.05")) float FarDetectionSeconds = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sight", meta=(ClampMin="0.01")) float SuspicionDecayPerSecond = 0.25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hearing", meta=(ClampMin="0")) float HearingRadius = 700;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.1")) float SearchSeconds = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="1")) float SearchRadius = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.1")) float InvestigateSeconds = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="40")) float AttackRange = 135;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="1", ClampMax="89")) float AttackHalfAngle = 55;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0")) float StrikeDamage = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0")) float BiteDamage = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.05")) float StrikeWindup = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.05")) float BiteWindup = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.05")) float RecoverySeconds = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0")) float InterruptThreshold = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.05")) float StaggerSeconds = 0.7f;
};

/** Carries attack identity to future player status systems without implementing radiation. */
struct FZombieDamageEvent : public FDamageEvent
{
	static const int32 ClassID = 10702;
	EZombieAttackKind Kind = EZombieAttackKind::Strike;
	virtual int32 GetTypeID() const override { return ClassID; }
	virtual bool IsOfType(int32 InID) const override { return InID == ClassID || FDamageEvent::IsOfType(InID); }
};

