// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "prototype3PlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryDemoWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class PROTOTYPE3_API Aprototype3PlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	Aprototype3PlayerController();

	/** The run action shared by the controller's mappings and the possessed character. */
	UInputAction* GetRunAction() const { return RuntimeRunAction.Get(); }

	/** The tap/hold crouch action shared with the possessed character. */
	UInputAction* GetCrouchAction() const { return RuntimeCrouchAction.Get(); }

	/** Lazily created after Blueprint defaults load; mappings and bindings share these instances. */
	UInputAction* GetPrimaryAction();
	UInputAction* GetSecondaryAction();

	/** Temporary local inventory laboratory; its items persist until this controller ends. */
	void ToggleInventoryDemo(bool bLegacyLab = false, bool bPocketsOnly = false);
	void CloseInventoryDemo();
	void HandleInterfaceEscape();
	bool HasInterfaceFocus(int32 UserIndex) const;
	void OpenPauseMenu();
	void ClosePauseMenu();
	void ExitPlaySession();
	void ExitToDesktop();
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;
	void RebuildControls();
	bool IsAssigningControls() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UInventoryDemoWidget> InventoryDemoWidget;
	bool bInventoryDemoOpen = false;
	bool bCursorBeforeInventory = false;
	bool bCursorBeforePause = false;
	TSharedPtr<class SPauseMenu> PauseMenu;
	TSharedPtr<class IInputProcessor> InterfaceInputProcessor;
	TSet<FKey> QuickKeysDown;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputMappingContext>> FilteredMappingContexts;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Runtime-only movement and primary/secondary action mappings. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeSprintMappingContext;

	/** Shared sprint action loaded for the runtime Alt mapping. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeSprintAction;

	/** Run action owned by this controller; no content asset is required. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeRunAction;

	/** Crouch action owned by this controller; no content asset is required. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeCrouchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimePrimaryAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeSecondaryAction;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
