#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryTestPile.generated.h"

class UInventoryComponent;
class ADroppedItem;

/** Session-only mixed floor items for the prototype inventory test area. */
UCLASS(Config=Game)
class PROTOTYPE3_API AInventoryTestPile : public AActor
{
	GENERATED_BODY()
public:
	AInventoryTestPile();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(Config, EditAnywhere, Category="Inventory test pile", meta=(ClampMin="17", ClampMax="128"))
	int32 ItemCount = 36;
	UPROPERTY(Config, EditAnywhere, Category="Inventory test pile")
	int32 RandomSeed = 7307;
	/** Spawn once for this session; uses the same world items and pickup path as ordinary drops. */
	bool SpawnForPlayer(APawn* Player);
	const TArray<TWeakObjectPtr<ADroppedItem>>& GetSpawnedItems() const { return SpawnedItems; }
private:
	UPROPERTY(Transient) TObjectPtr<UInventoryComponent> Catalog;
	TArray<TWeakObjectPtr<ADroppedItem>> SpawnedItems;
	bool bSpawnAttempted = false;
	float WaitSeconds = 0;
};
