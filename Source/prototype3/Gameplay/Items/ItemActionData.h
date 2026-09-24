#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemActionData.generated.h"

/** Data describing an item action. Input bindings live on the definition. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PROTOTYPE3_API UItemActionData : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsValidActionData() const { return true; }
};

/** Healing configuration, independent of the input button and future target rules. */
UCLASS(BlueprintType, EditInlineNew)
class PROTOTYPE3_API UHealingItemActionData : public UItemActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Healing", meta=(ClampMin="0.001"))
	double HealAmount = 0.0;

	virtual bool IsValidActionData() const override;
};
