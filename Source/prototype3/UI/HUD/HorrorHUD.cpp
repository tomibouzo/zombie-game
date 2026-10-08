// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HUD/HorrorHUD.h"
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

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

	const auto* Controller = Cast<Aprototype3PlayerController>(PlayerOwner);
	const auto* Use = PlayerCharacter->FindComponentByClass<UPlayerItemUseComponent>();
	if (!Use || (Controller && Controller->IsInventoryInterfaceOpen()) || PlayerOwner->IsPaused()) return;
	if (Use->IsPickupBlockedNoticeVisible())
	{
		constexpr float Width = 380.f;
		const float Left = (Canvas->SizeX - Width) * .5f;
		DrawRect(FLinearColor(.42f,.035f,.035f,.95f), Left, 35.f, Width, 38.f);
		DrawText(TEXT("Item replacement is off in Settings"), FLinearColor::White, Left + 18.f, 45.f, GEngine->GetSmallFont(), 1.f);
	}
	if (const auto* Floor = Use->GetLookedAtFloorItem(); Floor && Floor->GetItem().IsValid())
	{
		const FString Prompt = FString::Printf(TEXT("pick up (%s)"), *Floor->GetItem().Definition->DisplayName.ToString());
		DrawText(Prompt, FLinearColor::White, Canvas->SizeX * .5f - 100.f, Canvas->SizeY * .62f, GEngine->GetSmallFont(), 1.f);
	}
}
