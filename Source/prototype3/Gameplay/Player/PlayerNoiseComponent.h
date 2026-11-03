#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerNoiseComponent.generated.h"

/** Perception events from actual ground travel, independent of placeholder footstep audio. */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UPlayerNoiseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerNoiseComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* Function) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Noise", meta=(ClampMin="0")) float WalkNoiseRadius = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Noise", meta=(ClampMin="0")) float RunNoiseRadius = 600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Noise", meta=(ClampMin="10")) float StepDistance = 120;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Noise") int32 EmittedSteps = 0;
private:
	FVector PreviousLocation = FVector::ZeroVector;
	float TravelSinceStep = 0;
};
