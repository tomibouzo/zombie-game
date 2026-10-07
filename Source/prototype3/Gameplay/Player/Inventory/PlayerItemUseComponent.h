#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Items/ItemInstance.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "PlayerItemUseComponent.generated.h"

class UInventoryComponent;
class UPlayerVitalsComponent;
class UStaticMeshComponent;
class AInventoryTestSpikes;
class UHealingItemActionData;
class ADroppedItem;

enum class EBackpackMode : uint8 { Closed, Selecting, Quick, Slow };
enum class EInventoryArrangement : uint8 { Move, PickUp, Drop, FloorDrop, Take, Stow };

/** A proposed operation only. Ownership stays at the source until commit. */
struct FInventoryArrangement
{
	EInventoryArrangement Kind = EInventoryArrangement::Move;
	FGuid ItemId;
	TWeakObjectPtr<ADroppedItem> FloorItem;
	FName Pocket;
	FVector2D Position = FVector2D::ZeroVector;
	double Angle = 0;
};

/** First playable item slice. Hands reference a reserved owned instance, never a copy. */
UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PROTOTYPE3_API UPlayerItemUseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerItemUseComponent();
	static constexpr int32 QuickPocketCount = 3;
	static FName QuickPocketId(int32 Index);
	static bool IsQuickPocket(FName Pocket);
	UPROPERTY(EditAnywhere, Category="Prototype") bool bEnablePrototype = true;
	UPROPERTY(EditAnywhere, Category="Prototype") bool bSpawnTestSpikes = true;
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(ClampMin="0.01")) float QuickBackpackOpenSeconds = .5f;
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(ClampMin="0.01")) float SlowBackpackOpenSeconds = 2.5f;
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(ClampMin="0.01")) float ArrangementSeconds = .3f;
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(ClampMin="0.01")) double QuickMaxItemMassKg = 0.6;
	UPROPERTY(EditAnywhere, Category="Prototype") FVector2D QuickSize = FVector2D(220,220);
	UPROPERTY(EditAnywhere, Category="Prototype") FVector2D BackpackSize = FVector2D(420,600);
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(ClampMin="0.1", ClampMax="1")) float QuickUseHoldSeconds = 0.3f;
	UPROPERTY(Transient) TObjectPtr<UInventoryComponent> Inventory;
	UPROPERTY(Transient) FItemInstance Backpack;
	bool EquipToHands(FGuid Id);
	void Stow();
	bool HandlePrimaryAction();
	bool PressItemAction(bool bSecondary);
	void ReleaseItemAction(bool bSecondary);
	void CancelUse();
	void PressQuickItem(int32 Slot);
	void ReleaseQuickItem(int32 Slot);
	void CancelQuickItemHold();
	bool BeginOpenBackpack(bool bSelectMode = false);
	void ReleaseBackpackInput();
	void CloseBackpack();
	EBackpackMode GetBackpackMode() const { return BackpackMode; }
	bool IsBackpackActive() const { return bOpening || bBackpackOpen; }
	bool IsArranging() const { return bArranging; }
	bool RequestArrangement(const FInventoryArrangement& Request);
	void CancelArrangement();
	const FInventoryArrangement& GetArrangement() const { return Arrangement; }
	const FInventoryEntry& GetArrangementSource() const { return ArrangementSource; }
	float GetArrangementProgress() const;
	void ToggleBackpackEquipment();
	bool CanAccess(FName Pocket) const;
	bool IsUsing() const { return bUsing; }
	bool IsOpeningBackpack() const { return bOpening; }
	bool IsBackpackOpen() const { return bBackpackOpen; }
	bool IsBackpackEquipped() const { return bBackpackEquipped; }
	bool HasHeldItem() const { return HeldId.IsValid(); }
	FGuid GetHeldId() const { return HeldId; }
	float GetProgress() const;
	FString GetHeldName() const;
	FString Status;
	/** Fixture/setup helper. Player storage placement is always manual through the panel. */
	bool Transfer(FGuid Id, FName Pocket);
	void Advance(float Seconds);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick) override;
private:
	UPROPERTY(Transient) TObjectPtr<UPlayerVitalsComponent> Vitals;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldVisual;
	UPROPERTY(Transient) TObjectPtr<AInventoryTestSpikes> Spikes;
	FGuid HeldId;
	bool bUsing = false, bOpening = false, bBackpackOpen = false, bBackpackEquipped = true;
	float Elapsed = 0, Duration = 3;
	EBackpackMode BackpackMode = EBackpackMode::Closed;
	float BackpackInputElapsed = 0;
	double BackpackInputStartTime = 0;
	bool bRestoreBackpackStance = false, bPreviouslyCrouched = false;
	bool bArranging = false;
	float ArrangementElapsed = 0;
	FInventoryArrangement Arrangement;
	UPROPERTY(Transient) FInventoryEntry ArrangementSource;
	FGuid ArrangementHeldId;
	bool ValidateArrangement(const FInventoryArrangement& Request, FInventoryEntry& Source) const;
	bool CommitArrangement(const FInventoryArrangement& Request);
	void SelectQuickBackpack();
	float BackpackHoldThreshold() const;
	void StopInventoryMovement();
	int32 HeldQuickItem = INDEX_NONE;
	float QuickHoldElapsed = 0;
	bool bQuickUseAttempted = false;
	bool bUsingFromQuickKey = false;
	bool bActiveSecondary = false;
	const UHealingItemActionData* HealingAction(bool bSecondary) const;
	void UpdateVisual();
	void TrySpawnSpikes();
	bool bSpikesSpawnAttempted = false;
	UFUNCTION() void HealthChanged(float Current, float Maximum, float Percentage);
	UFUNCTION() void DamageApplied(float Amount, bool bInterruptActions);
};
