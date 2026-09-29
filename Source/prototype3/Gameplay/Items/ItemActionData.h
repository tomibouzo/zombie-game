#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

/** Declares an intended use while its execution and tuning are still future work. */
UCLASS(BlueprintType, EditInlineNew)
class PROTOTYPE3_API UIntentItemActionData : public UItemActionData
{
	GENERATED_BODY()

public:
	/** A concrete Item.Action tag; this is not an input binding or an executable handler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Action", meta=(Categories="Item.Action"))
	FGameplayTag ActionTag;

	virtual bool IsValidActionData() const override;
};
