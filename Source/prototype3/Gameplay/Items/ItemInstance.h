#pragma once

#include "CoreMinimal.h"
#include "ItemInstance.generated.h"

class UItemDefinition;

/** One owned item or future homogeneous stack; placement belongs to its container. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Item")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, Category="Item")
	TObjectPtr<UItemDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Item")
	int32 Quantity = 0;

	/** Returns an invalid instance if the definition or requested quantity is invalid. */
	static FItemInstance Create(UItemDefinition* InDefinition, int32 InQuantity = 1);

	bool IsValid() const;
};
