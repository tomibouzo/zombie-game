// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/GameModes/prototype3GameMode.h"
#include "UI/HUD/HorrorHUD.h"
#include "Gameplay/Player/Inventory/InventoryTestPile.h"
#include "Engine/World.h"

Aprototype3GameMode::Aprototype3GameMode()
{
	// Every game mode derived from the base prototype game mode uses the vitals HUD.
	HUDClass = AHorrorHUD::StaticClass();
}

void Aprototype3GameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// Blueprint class defaults can retain a pre-Live-Coding HUD value. Apply the
	// shared HUD again before the game mode begins initializing its players.
	HUDClass = AHorrorHUD::StaticClass();
	Super::InitGame(MapName, Options, ErrorMessage);
}

void Aprototype3GameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnInventoryTestPile && GetWorld()->GetMapName().EndsWith(TEXT("Lvl_FirstPerson")))
		GetWorld()->SpawnActor<AInventoryTestPile>();
}
