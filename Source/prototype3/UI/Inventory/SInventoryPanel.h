#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "UI/Inventory/InventoryInputSettings.h"

class UInventoryComponent;
class UPlayerItemUseComponent;
class ADroppedItem;
class APawn;

/** Continuous logical canvas, scaled by its host. Mutations go through the component. */
class SInventoryPanel : public SLeafWidget
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnDropItem, FGuid, FString&);
	SLATE_BEGIN_ARGS(SInventoryPanel) : _Inventory(nullptr), _Controls(nullptr), _SaveControls(true), _ItemUse(nullptr), _Player(nullptr), _PocketsOnly(false) {}
		SLATE_ARGUMENT(UInventoryComponent*, Inventory)
		SLATE_ARGUMENT(UInventoryInputSettings*, Controls)
		SLATE_ARGUMENT(bool, SaveControls)
		SLATE_ARGUMENT(UPlayerItemUseComponent*, ItemUse)
		SLATE_ARGUMENT(APawn*, Player)
		SLATE_ARGUMENT(bool, PocketsOnly)
		SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_EVENT(FOnDropItem, OnDropItem)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetSaveControls(bool bSave) { bSaveControls = bSave; }
	void SetPocketsOnly(bool bOnly);
	bool IsPocketsOnly() const { return bPocketsOnly; }
	bool IsInterfaceReady() const;
	void CycleQuickPocket();
	FName GetVisibleQuickPocket() const;
	void CancelInteraction();
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1540, 940); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry&, double, float) override;
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
	virtual void OnFocusLost(const FFocusEvent&) override;
private:
	TWeakObjectPtr<APawn> Player;
	TWeakObjectPtr<ADroppedItem> FloorSource;
	TArray<TWeakObjectPtr<ADroppedItem>> FloorItems;
	bool bFromFloor = false;
	int32 FloorScroll = 0;
	float FloorRefreshRemaining = 0;
	FVector2D FloorSize() const;
	int32 FloorRows() const { return FMath::Max(1, FMath::FloorToInt(FloorSize().Y / 36)); }
	static FVector2D FloorOrigin() { return FVector2D(760, 150); }
	bool bPocketsOnly = false;
	int32 QuickPocketIndex = 0;
	bool bRequestedClose = false;
	FVector2D PanelSize() const { return FVector2D(1040, ItemUse.IsValid() ? (bPocketsOnly ? 620 : 940) : 800); }
	void RefreshFloor();
	bool BeginFloorDrag();
	bool IsWorldDrop() const;
	bool SourceAvailable() const;
	TWeakObjectPtr<UPlayerItemUseComponent> ItemUse;
	struct FPocketView
	{
		FInventoryPocket Pocket;
		FVector2D Origin;
		bool bAccessible;
	};
	TArray<FPocketView> DisplayPockets() const;
	FVector2D PreviewCenter() const { return bMouseRotating ? RotationCenter : Cursor - GrabOffset; }
	bool HandlePlayerControl(FKey Key);
	TWeakObjectPtr<UInventoryComponent> Inventory;
	TWeakObjectPtr<UInventoryInputSettings> Controls;
	bool bSaveControls = true;
	FSimpleDelegate OnClose;
	FOnDropItem OnDropItem;
	FGuid SelectedId;
	bool bDragging = false;
	bool bPendingDrag = false;
	bool bCanDoubleClick = false;
	double LastClickTime = 0;
	FGuid LastClickItem;
	FKey LastClickKey;
	FVector2D GrabStart = FVector2D::ZeroVector;
	TSet<FKey> PressedKeys;
	bool IsHeldSelection() const;
	bool IsActionDown(EInventoryControl Action) const;
	bool bAdding = false;
	bool bTurnLeft = false;
	bool bTurnRight = false;
	bool bMouseRotating = false;
	bool bHasRotationDirection = false;
	FVector2D RotationCenter = FVector2D::ZeroVector;
	FVector2D RotationDirection = FVector2D::ZeroVector;
	FInventoryEntry Pending;
	FVector2D Cursor = FVector2D::ZeroVector;
	FVector2D GrabOffset = FVector2D::ZeroVector;
	double PreviewAngle = 0;
	FString Status = TEXT("Grab an item to move it. Green fits; red cannot be placed.");
	static FVector2D PocketOrigin() { return FVector2D(40, 150); }
	bool Active() const { return bDragging || bAdding; }
	bool HitItem(FVector2D Point, FInventoryEntry& Entry) const;
	EInventoryResult Preview(FName& PocketId, FVector2D& Center) const;
	FReply Press(FKey Key);
	FReply Release(FKey Key);
	FReply Reply();
	void SavePreferences();
	void CancelGesture();
	void Turn(double Delta);
	void UpdateCursor(FVector2D Position);
	void BeginMouseRotation();
	void EndMouseRotation();
	void DropSelected();
	void BeginBandage();
	void CommitGesture();
	static FString Describe(EInventoryResult Result);
};
