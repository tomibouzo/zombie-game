#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class PROTOTYPE3_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

class PROTOTYPE3_API IInteractable
{
	GENERATED_BODY()
public:
	virtual bool CanInteract(const AActor* Instigator) const = 0;
	virtual FText GetInteractionPrompt(const AActor* Instigator) const = 0;
	virtual void Interact(AActor* Instigator) = 0;
};
