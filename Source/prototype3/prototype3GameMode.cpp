// Copyright Epic Games, Inc. All Rights Reserved.

#include "prototype3GameMode.h"
#include "Variant_Horror/HorrorHUD.h"

Aprototype3GameMode::Aprototype3GameMode()
{
	// Every game mode derived from the base prototype game mode uses the vitals HUD.
	HUDClass = AHorrorHUD::StaticClass();
}
