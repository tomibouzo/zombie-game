#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Gameplay/Player/Inventory/InventoryTypes.h"

class UInventoryComponent;

/** Fixed logical canvas scaled by the host widget. All mutations go through the component. */
class SInventoryPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SInventoryPanel) {}
		SLATE_ARGUMENT(UInventoryComponent*, Inventory)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1000, 650); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
	virtual void OnFocusLost(const FFocusEvent&) override;
private:
	TWeakObjectPtr<UInventoryComponent> Inventory;
	FSimpleDelegate OnClose;
	FGuid SelectedId;
	bool bDragging = false;
	bool bAdding = false;
	FInventoryEntry Pending;
	FVector2D Cursor = FVector2D::ZeroVector;
	FIntPoint GrabOffset = FIntPoint::ZeroValue;
	int32 PreviewRotation = 0;
	FString Status = TEXT("Arrastra una pieza. R gira; Supr retira la seleccion. I cierra.");
	static FVector2D PocketOrigin(int32 Index);
	bool Locate(FVector2D Point, FName& PocketId, FIntPoint& Cell) const;
	bool HitItem(FVector2D Point, FInventoryEntry& Entry, FIntPoint& Cell) const;
	EInventoryResult Preview(FName& PocketId, FIntPoint& Position) const;
	void CancelGesture();
	void Rotate();
	void RemoveSelected();
	void BeginBandage();
	void CommitGesture();
	static FString Describe(EInventoryResult Result);
};
