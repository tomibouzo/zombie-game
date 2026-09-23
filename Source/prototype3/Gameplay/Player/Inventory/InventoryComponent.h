#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedDelegate);

/** Grid rules shared by players and world storage. No UI, clothing, or 3D dependency. */
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/** Empty by default: equipment or a Blueprint supplies the desired containers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TArray<FInventoryContainerDefinition> InitialContainers;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FInventoryChangedDelegate OnInventoryChanged;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult AddContainer(const FInventoryContainerDefinition& Definition);

	/** Moves the entire container with its contents; it does not need to be open. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult TransferContainer(FName ContainerId, UInventoryComponent* Target);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult SetContainerOpen(FName ContainerId, bool bOpen);

	/** Creates exactly one entry. Fails entirely if the quantity exceeds its stack limit. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult AddItem(UItemDefinition* Definition, int32 Quantity, FName ContainerId,
		FName CompartmentId, FIntPoint Position, int32 QuarterTurns, FGuid& OutItemId);

	/** Also moves/rotates within this inventory. A partial transfer splits a stack. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult TransferItem(FGuid ItemId, int32 Quantity, UInventoryComponent* Target,
		FName ContainerId, FName CompartmentId, FIntPoint Position, int32 QuarterTurns,
		FGuid& OutItemId);

	/** Explicit stacking avoids silently merging distinct instances on placement. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult StackItems(FGuid SourceItemId, UInventoryComponent* Target,
		FGuid TargetItemId, int32 Quantity);

	/** Consume/remove units. Dropping or stashing uses TransferItem instead. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult RemoveItem(FGuid ItemId, int32 Quantity);

	/** Read-only placement preview for future drag-and-drop UI. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	EInventoryResult CheckPlacement(UItemDefinition* Definition, FName ContainerId,
		FName CompartmentId, FIntPoint Position, int32 QuarterTurns, FGuid IgnoredItemId) const;

	/** Returns copies; callers cannot bypass validation by editing internal arrays. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<FInventoryContainer> GetContainers() const { return Containers; }

	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetItem(FGuid ItemId, FInventoryEntry& OutItem, FName& OutContainerId) const;

	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	float GetContainerFillRatio(FName ContainerId) const;

	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	EInventoryLoad GetContainerLoad(FName ContainerId) const;

	/** Aggregate occupied cells / available cells of all containers on this component. */
	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	float GetFillRatio() const;

	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	EInventoryLoad GetLoad() const;

	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	float GetMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Inventory|Load")
	float GetStaminaDrainMultiplier() const;

	/** Provisional balancing values. There are no item weights or kg limits yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="0", ClampMax="1"))
	float LowLoadMaxRatio = 1.0f / 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="0", ClampMax="1"))
	float MediumLoadMaxRatio = 2.0f / 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="0.1", ClampMax="1"))
	float MediumSpeedMultiplier = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="0.1", ClampMax="1"))
	float HighSpeedMultiplier = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="1"))
	float MediumStaminaMultiplier = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Load", meta=(ClampMin="1"))
	float HighStaminaMultiplier = 1.35f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TArray<FInventoryContainer> Containers;

	int32 FindContainer(FName Id) const;
	bool FindItem(FGuid Id, int32& OutContainer, int32& OutEntry) const;
	static bool IsAccessible(const FInventoryContainer& Container);
	static void CountCells(const FInventoryContainer& Container, int64& Occupied, int64& Capacity);
	EInventoryLoad ClassifyLoad(float Ratio) const;
	void NotifyChange(UInventoryComponent* Other = nullptr);
};
