#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Items/ItemInstance.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "DroppedItem.generated.h"

class UBoxComponent;
class UInventoryComponent;
class UStaticMesh;
class UMaterialInterface;
class UTextRenderComponent;
class APawn;

/** Temporary physical representation of one owned item, with guarded local transfers. */
UCLASS(Config=Game)
class PROTOTYPE3_API ADroppedItem : public AActor
{
	GENERATED_BODY()
public:
	ADroppedItem();
	virtual void Tick(float DeltaSeconds) override;

	/** Spawn first; remove from the source only when the world representation is ready. */
	static ADroppedItem* DropFromInventory(UInventoryComponent* Inventory, FGuid InstanceId, APawn* Player, FString& Error);
	static bool HasWorldRepresentation(const FItemInstance& Item);
	bool WouldMoveAtFeet(APawn* Player) const;
	static TArray<TWeakObjectPtr<ADroppedItem>> FindNearby(APawn* Player);
	bool CanInteract(APawn* Player) const;
	EInventoryResult PickUp(UInventoryComponent* Inventory, APawn* Player, FName Pocket, FVector2D Position, double Angle);
	/** Reserve a floor transfer while another ownership change commits. */
	EInventoryResult StagePickUp(UInventoryComponent* Inventory, APawn* Player, FName Pocket, FVector2D Position, double Angle);
	void FinishStagedPickUp();
	void CancelStagedPickUp(UInventoryComponent* Inventory);
	bool DropAtFeet(APawn* Player, FString& Error);
	const FItemInstance& GetItem() const { return Item; }
	FName GetInventoryProfileId() const { return InventoryProfileId; }
	UBoxComponent* GetBody() const { return Body; }
	float GetCleanupZ() const { return CleanupZ; }

protected:
	/** Nearby floor interaction distance in centimetres, measured from the player's feet. */
	UPROPERTY(Config, EditDefaultsOnly, Category="Item|World", meta=(ClampMin="1"))
	float PickupRadius = 250.f;
	/** World-space lower boundary in cm. Lower this for levels with playable areas below -100 m. */
	UPROPERTY(Config, EditDefaultsOnly, Category="Item|World")
	float CleanupZ = -10000.f;

private:
	UPROPERTY(VisibleAnywhere, Category="Item|World")
	TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Item|World")
	TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlaceholderMaterial;
	UPROPERTY(VisibleInstanceOnly, Category="Item")
	FItemInstance Item;
	UPROPERTY(VisibleInstanceOnly, Category="Item")
	FName InventoryProfileId;
	bool bTransferring = false;
	bool bStagedPickUp = false;
	bool Initialize(const FItemInstance& InItem, FName ProfileId);
};
