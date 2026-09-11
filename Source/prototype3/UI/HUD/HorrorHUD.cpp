// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HUD/HorrorHUD.h"
#include "Core/Characters/prototype3Character.h"
#include "Engine/Canvas.h"

void AHorrorHUD::DrawHUD()
{
	Super::DrawHUD();

	const Aprototype3Character* PlayerCharacter = PlayerOwner ? Cast<Aprototype3Character>(PlayerOwner->GetPawn()) : nullptr;
	if (!Canvas || !PlayerCharacter)
	{
		return;
	}

	constexpr float BarWidth = 260.0f;
	constexpr float BarHeight = 20.0f;
	constexpr float Border = 3.0f;
	constexpr float Spacing = 12.0f;
	const float X = 48.0f;
	const float HealthY = Canvas->SizeY - (BarHeight * 2.0f) - Spacing - 48.0f;
	const float StaminaY = HealthY + BarHeight + Spacing;

	const auto DrawBar = [this](float XPos, float YPos, float Percent, const FLinearColor& FillColor)
	{
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), XPos - Border, YPos - Border, BarWidth + (Border * 2.0f), BarHeight + (Border * 2.0f));
		DrawRect(FLinearColor(0.12f, 0.12f, 0.12f, 0.95f), XPos, YPos, BarWidth, BarHeight);
		DrawRect(FillColor, XPos, YPos, BarWidth * FMath::Clamp(Percent, 0.0f, 1.0f), BarHeight);
	};

	DrawBar(X, HealthY, PlayerCharacter->GetHealthPercent(), FLinearColor(0.80f, 0.05f, 0.05f, 1.0f));
	DrawBar(X, StaminaY, PlayerCharacter->GetStaminaPercent(), FLinearColor(0.04f, 0.32f, 0.95f, 1.0f));
}
