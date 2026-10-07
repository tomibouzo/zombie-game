#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "UObject/StrongObjectPtr.h"

class UPlayerItemUseComponent;
class UItemUseSettings;
class SWidgetSwitcher;

enum class EOptionsCategory : uint8 { Controls, Inventory = Controls, Sound, Graphics };

/** Pause and key assignment screens, owned by the local player controller. */
class SPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPauseMenu)
		: _Controls(nullptr), _ItemUse(nullptr), _CanExitGame(false), _CanExitDesktop(false), _SaveControls(true) {}
		SLATE_ARGUMENT(UInventoryInputSettings*, Controls)
		SLATE_ARGUMENT(UPlayerItemUseComponent*, ItemUse)
		SLATE_ARGUMENT(bool, CanExitGame)
		SLATE_ARGUMENT(bool, CanExitDesktop)
		SLATE_ARGUMENT(bool, SaveControls)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnExitGame)
		SLATE_EVENT(FSimpleDelegate, OnExitDesktop)
		SLATE_EVENT(FSimpleDelegate, OnControlsChanged)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	void HandleEscape();
	void ShowOptions();
	void ShowCategory(EOptionsCategory Category);
	void ShowInventorySection(int32 Section);
	bool IsOptionsOpen() const { return bOptionsOpen; }
	EOptionsCategory GetCategory() const { return ActiveCategory; }
	bool IsCapturing() const { return Rebinding != INDEX_NONE; }
	bool HasUnsavedChanges() const;
	bool IsConfirmingExit() const { return bConfirmExit; }
	const UInventoryInputSettings* GetEditingControls() const { return Controls.Get(); }
private:
	TStrongObjectPtr<UInventoryInputSettings> Controls;
	TWeakObjectPtr<UInventoryInputSettings> AppliedControls;
	TWeakObjectPtr<UPlayerItemUseComponent> ItemUse;
	TSharedPtr<SWidgetSwitcher> Screens;
	TSharedPtr<SWidgetSwitcher> Categories;
	TSharedPtr<SWidgetSwitcher> InventorySections;
	EOptionsCategory ActiveCategory = EOptionsCategory::Inventory;
	int32 ActiveSection = 0;
	FSimpleDelegate OnResume, OnExitGame, OnExitDesktop, OnControlsChanged;
	bool bOptionsOpen = false;
	bool bSaveControls = true;
	int32 Rebinding = INDEX_NONE;
	int32 RebindingSlot = 0;
	FKey PendingKey;
	bool bConflictPending = false;
	bool bConfirmReset = false;
	bool bConfirmExit = false;
	FString Status;
	void AssignKey(FKey Key, bool bReplace = false);
	void ResetControls(bool bAll);
	void ControlsEdited();
	void ApplyControls();
	void FinishOptions();
	void CancelExit();
	TSharedRef<SWidget> MakeControlRow(EInventoryControl Action, const FString& Label = FString(), const FString& Detail = FString());
};
