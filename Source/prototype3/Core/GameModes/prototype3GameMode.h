// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "prototype3GameMode.generated.h"

/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class Aprototype3GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	Aprototype3GameMode();

	/**
	 * Reapplies the shared vitals HUD before players are initialized. This keeps
	 * Blueprint game modes reliable after a Live Coding reload changes native
	 * class defaults.
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	/** Mixed pickup/scrolling fixture, only auto-created in Lvl_FirstPerson. */
	UPROPERTY(EditDefaultsOnly, Category="Prototype")
	bool bSpawnInventoryTestPile = true;
};



