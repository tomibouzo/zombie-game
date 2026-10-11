#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedDelegate);

/** Local, in-memory placement rules. Profiles/pockets are immutable once registered. */
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UInventoryComponent();
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FInventoryChangedDelegate OnInventoryChanged;
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult RegisterProfile(const FInventoryItemProfile& Profile);
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult AddPocket(const FInventoryPocket& Pocket);
	/** Caller hands over the instance after success. Duplicate IDs in this inventory are rejected. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult AddItem(const FItemInstance& Item, FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees);
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult MoveItem(FGuid InstanceId, FName PocketId, FVector2D Position, double AngleDegrees, bool bAllowReserved = false);
	/** Removes the whole instance and returns it. Failed operations return an invalid output. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	EInventoryResult RemoveItem(FGuid InstanceId, FItemInstance& OutItem, bool bAllowReserved = false);
	/** Preview for a new item. CheckMove ignores only the moving instance's silhouette. */
	UFUNCTION(BlueprintPure, Category="Inventory")
	EInventoryResult CheckPlacement(FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees) const;
	UFUNCTION(BlueprintPure, Category="Inventory")
	EInventoryResult CheckMove(FGuid InstanceId, FName PocketId, FVector2D Position, double AngleDegrees, bool bAllowReserved = false) const;
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetItem(FGuid InstanceId, FInventoryEntry& OutEntry) const;
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetProfile(FName ProfileId, FInventoryItemProfile& OutProfile) const;
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<FInventoryEntry> GetEntries() const { return Entries; }
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<FInventoryPocket> GetPockets() const { return Pockets; }
	/** Held items keep their storage space reserved until stowed or consumed. */
	bool ReserveItem(FGuid Id);
	void ReleaseItem(FGuid Id) { ReservedItems.Remove(Id); }
	bool IsReserved(FGuid Id) const { return ReservedItems.Contains(Id); }
	bool ConsumeReservedItem(FGuid Id);
	bool FindSpace(FGuid Id, FName Pocket, FVector2D& OutPosition) const;
private:
	TSet<FGuid> ReservedItems;
	UPROPERTY(Transient)
	TArray<FInventoryItemProfile> Profiles;
	UPROPERTY(Transient)
	TArray<FInventoryPocket> Pockets;
	UPROPERTY(Transient)
	TArray<FInventoryEntry> Entries;
	int32 FindItem(FGuid Id) const;
	EInventoryResult ValidatePlacement(FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees, FGuid IgnoredId) const;
};
