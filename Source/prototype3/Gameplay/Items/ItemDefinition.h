#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemDefinition.generated.h"

class UItemActionData;
class UTexture2D;

/** Shared, immutable-at-runtime data for one item type. */
UCLASS(BlueprintType)
class PROTOTYPE3_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable gameplay identity. Do not change this when renaming the asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity", meta=(MultiLine="true"))
	FText Description;

	/** Optional; inventory art and physical collision are separate future work. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Presentation")
	TSoftObjectPtr<UTexture2D> Icon;

	/** One primary family. Categories do not determine an item's use behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Classification", meta=(Categories="Item.Category"))
	FGameplayTag Category;

	/** Independent properties such as Handheld; equipment traits can be added later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Classification", meta=(Categories="Item.Trait"))
	FGameplayTagContainer Traits;

	/** Mass of one unit, in kilograms. Inventory totals can multiply by Quantity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Storage", meta=(ClampMin="0.0", Units="kg"))
	double MassKg = 0.0;

	/** One means unstackable. Shape and destination compatibility are later rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Storage", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	/** Left mouse action, if any. Action data does not contain target or input rules. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Item|Actions")
	TObjectPtr<UItemActionData> PrimaryAction = nullptr;

	/** Right mouse action, if any. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Item|Actions")
	TObjectPtr<UItemActionData> SecondaryAction = nullptr;

	UFUNCTION(BlueprintPure, Category="Item")
	bool IsValidDefinition() const;
};
