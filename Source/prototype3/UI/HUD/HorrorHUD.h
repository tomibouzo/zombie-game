// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HorrorHUD.generated.h"

class UShelterBuildComponent;

/**
 * In-game vitals, centered aiming dot and pickup prompts for the horror prototype.
 * The bars intentionally live in C++ so they work immediately, while UHorrorUI
 * remains available for a later art-directed UMG replacement.
 */
UCLASS()
class PROTOTYPE3_API AHorrorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** Category or piece list with key hints; shown only while build mode is open. */
	void DrawBuildOverlay(const UShelterBuildComponent& Build);
};
