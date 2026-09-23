#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/Data/ItemDefinition.h"
#include "InventoryTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryAccess : uint8
{
	Quick,
	RequiresOpening
};

UENUM(BlueprintType)
enum class EInventoryLoad : uint8
{
	Low,
	Medium,
	High
};

UENUM(BlueprintType)
enum class EInventoryResult : uint8
{
	Success,
	InvalidItem,
	InvalidQuantity,
	InvalidContainer,
	InvalidCompartment,
	Closed,
	OutOfBounds,
	Occupied,
	IncompatibleStack,
	StackFull
};

/** Each layer or pocket is a separate grid. An item cannot span two grids. */
USTRUCT(BlueprintType)
struct FInventoryCompartment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id = TEXT("Main");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="256"))
	FIntPoint Size = FIntPoint(4, 4);
};

USTRUCT(BlueprintType)
struct FInventoryContainerDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FName Id = TEXT("Storage");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	EInventoryAccess Access = EInventoryAccess::Quick;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryCompartment> Compartments = { FInventoryCompartment() };
};

/** One owned item or ammo stack. Moving the full stack preserves its identity. */
USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TObjectPtr<UItemDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName CompartmentId;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FIntPoint Position = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 QuarterTurns = 0;
};

USTRUCT(BlueprintType)
struct FInventoryContainer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FInventoryContainerDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	bool bOpen = false;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryEntry> Entries;
};
