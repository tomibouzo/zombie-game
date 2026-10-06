#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/Inventory/InventoryInputSettings.h"

class UPlayerItemUseComponent;
class SWidgetSwitcher;

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
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	void HandleEscape();
	void ShowOptions();
	bool IsOptionsOpen() const { return bOptionsOpen; }
private:
	TWeakObjectPtr<UInventoryInputSettings> Controls;
	TWeakObjectPtr<UPlayerItemUseComponent> ItemUse;
	TSharedPtr<SWidgetSwitcher> Screens;
	FSimpleDelegate OnResume, OnExitGame, OnExitDesktop;
	bool bOptionsOpen = false;
	bool bSaveControls = true;
	int32 Rebinding = INDEX_NONE;
	int32 ShortcutRebinding = INDEX_NONE;
	FString Status;
	bool IsCapturing() const { return Rebinding != INDEX_NONE || ShortcutRebinding != INDEX_NONE; }
	void AssignKey(FKey Key);
	void ResetControls();
	TSharedRef<SWidget> MakeControlRow(EInventoryControl Action);
	TSharedRef<SWidget> MakeShortcutRow(int32 Slot);
};
