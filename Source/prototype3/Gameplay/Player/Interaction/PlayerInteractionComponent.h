#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerInteractionComponent.generated.h"

UCLASS()
class PROTOTYPE3_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerInteractionComponent();
	AActor* GetCurrentTarget() const;
	FText GetPrompt() const;
	void TryInteract();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	bool CanTarget() const;
	AActor* TraceTarget() const;
	TWeakObjectPtr<AActor> CurrentTarget;
};
