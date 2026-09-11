// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HorrorHUD.generated.h"

/**
 * Minimal in-game vitals display for the horror prototype.
 * The bars intentionally live in C++ so they work immediately, while UHorrorUI
 * remains available for a later art-directed UMG replacement.
 */
UCLASS()
class PROTOTYPE3_API AHorrorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
