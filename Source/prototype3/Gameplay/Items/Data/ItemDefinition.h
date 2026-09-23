#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDefinition.generated.h"

/** Shared, immutable during play: describes a kind of item, not one owned instance. */
UCLASS(BlueprintType)
class PROTOTYPE3_API UItemDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FName Category;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	bool bStorable = true;

	/** Most objects (including bandages) use 1. Ammo definitions explicitly opt in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	/** Only these cells block placement. Coordinates are relative to the top-left. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Shape")
	TArray<FIntPoint> OccupiedCells = { FIntPoint(0, 0) };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Shape")
	bool bAllowRotation = true;

	/** Rejects empty, duplicate, negative, or unreasonably large cell coordinates. */
	bool IsValidDefinition() const;
	TArray<FIntPoint> GetRotatedCells(int32 QuarterTurns) const;
};
