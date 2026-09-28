#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/ItemInstance.h"
#include "InventoryTypes.generated.h"

class UItemDefinition;

UENUM(BlueprintType)
enum class EInventoryResult : uint8
{
	Success, InvalidItem, InvalidProfile, InvalidPocket, InvalidRotation,
	DuplicateId, NotFound, OutOfBounds, Occupied
};

/** Temporary inventory geometry, separate from the shared item contract. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryItemProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TObjectPtr<UItemDefinition> Definition = nullptr;
	/** Explicit geometry is required: a missing icon never implies a size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FIntPoint> OccupiedCells;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bAllowRotation = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	bool bProvisional = true;
	bool IsValid() const;
	TArray<FIntPoint> GetRotatedCells(int32 QuarterTurns) const;
};

USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryPocket
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FIntPoint Size = FIntPoint(4, 4);
};

/** Placement belongs to the inventory; identity and quantity belong to the instance. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FInventoryEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FItemInstance Item;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName ProfileId;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName PocketId;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FIntPoint Position = FIntPoint::ZeroValue;
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 QuarterTurns = 0;
};
