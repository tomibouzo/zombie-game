#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryTestSpikes.generated.h"
class UBoxComponent;
UCLASS()
class PROTOTYPE3_API AInventoryTestSpikes : public AActor
{
	GENERATED_BODY()
public:
	AInventoryTestSpikes();
	UPROPERTY(EditAnywhere, Category="Test") float DamagePerPulse = 10;
	UPROPERTY(EditAnywhere, Category="Test") float PulseSeconds = 1;
	/** Returns actual applied damage, always preserving at least one health point. */
	float Hurt(AActor* Target);
	virtual void Tick(float Delta) override;
private:
	UPROPERTY() TObjectPtr<UBoxComponent> Area;
	float Elapsed = 0;
};
