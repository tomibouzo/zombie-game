// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "prototype3PlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;

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

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Runtime-only mapping for Shift running and Alt sprinting. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeSprintMappingContext;

	/** Shared sprint action loaded for the runtime Alt mapping. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeSprintAction;

	/** Run action owned by this controller; no content asset is required. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeRunAction;

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
