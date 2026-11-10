// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/HUD/HorrorHUD.h"
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "EngineUtils.h"
#include "Gameplay/Shelter/Build/ShelterBuildComponent.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "UI/Inventory/InventoryInputSettings.h"

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
	if (const auto* Build = Controller ? Controller->GetBuildComponent() : nullptr; Build && Build->IsActive() && !PlayerOwner->IsPaused())
		DrawBuildOverlay(*Build);
	const bool bGameplayAimVisible = !(Controller && Controller->IsInventoryInterfaceOpen()) && !PlayerOwner->IsPaused();
	if (bGameplayAimVisible)
	{
		const FVector2D Center(Canvas->SizeX * .5f, Canvas->SizeY * .5f);
		const float Scale = FMath::Clamp(Canvas->SizeY / 1080.f, .75f, 2.f);
		// Cyan stays bright in shadow; the dark rim separates it from bright surfaces.
		Canvas->K2_DrawPolygon(nullptr, Center, FVector2D(4.f * Scale), 32, FLinearColor(.01f, .015f, .02f, 1.f));
		Canvas->K2_DrawPolygon(nullptr, Center, FVector2D(2.5f * Scale), 32, FLinearColor(0.f, 1.f, 1.f, 1.f));
	}
	const auto* Use = PlayerCharacter->FindComponentByClass<UPlayerItemUseComponent>();
	if (bGameplayAimVisible)
	{
		FVector Eye; FRotator View;
		PlayerOwner->GetPlayerViewPoint(Eye,View);
		for (TActorIterator<AZombieCharacter> It(GetWorld()); It; ++It)
		{
			const auto* Z = *It;
			const FVector Head = Z->GetActorLocation()+FVector(0,0,115);
			if (!Z->IsAlive() || FVector::DistSquared(Eye,Head)>2200*2200 || FVector::DotProduct(View.Vector(),Head-Eye)<=0) continue;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieIndicator),false,PlayerCharacter);
			Params.AddIgnoredActor(Z);
			if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,Head,ECC_Visibility,Params)) continue;
			FVector2D Screen;
			if (!PlayerOwner->ProjectWorldLocationToScreen(Head,Screen)) continue;
			const auto* AI = Cast<AZombieAIController>(Z->GetController());
			const bool Alert = AI && AI->IsAlerted();
			const bool Contact = AI && AI->HasVisualContact();
			const FLinearColor Color = Alert && Contact ? FLinearColor(.95f,.08f,.06f) : FLinearColor(1.f,.65f,.1f);
			const float Fill = Alert ? 1.f : Z->Suspicion;
			DrawRect(FLinearColor(.03f,.03f,.03f,.9f),Screen.X-42,Screen.Y,84,9);
			DrawRect(Color,Screen.X-40,Screen.Y+2,80*Fill,5);
			FString Label = Alert ? (Contact ? TEXT("DETECTADO") : TEXT("BUSCANDO")) : (Fill>0 ? TEXT("SOSPECHA") : TEXT("CALMA"));
			if (!Alert && Z->Behavior==EZombieBehavior::Investigate && !Contact) Label=TEXT("INVESTIGANDO");
			if (Z->IsAttacking()) Label = Z->GetAttackKind()==EZombieAttackKind::Bite ? TEXT("MORDIDA") : TEXT("GOLPE");
			if (Z->IsStaggered()) Label = TEXT("INTERRUMPIDO");
			DrawText(Label,FLinearColor::White,Screen.X-40,Screen.Y-18,GEngine->GetSmallFont(),1);
		}
	}
	if (!Use || !bGameplayAimVisible) return;
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

void AHorrorHUD::DrawBuildOverlay(const UShelterBuildComponent& Build)
{
	using A = EInventoryControl;
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	UFont* Font = GEngine->GetSmallFont();
	const bool bMenu = Build.GetState() == EShelterBuildState::Menu;
	const TArray<FText>& Categories = Build.GetCategories();
	const TArray<UShelterPieceDefinition*> Pieces = Build.GetPiecesInCategory(Build.GetCategoryIndex());
	const int32 Rows = bMenu ? Categories.Num() : Pieces.Num();
	constexpr float Width = 330.f, Line = 22.f, Pad = 14.f;
	const float Height = Pad * 2 + Line * (Rows + 6);
	const float Left = 48.f;
	const float Top = FMath::Max(40.f, (Canvas->SizeY - Height) * .5f);
	DrawRect(FLinearColor(.02f, .03f, .045f, .88f), Left, Top, Width, Height);
	float Y = Top + Pad;
	auto Text = [&](const FString& Value, const FLinearColor& Color) { DrawText(Value, Color, Left + Pad, Y, Font, 1.f); Y += Line; };
	const FLinearColor Muted(.65f, .72f, .78f);
	const FString Category = Categories.IsValidIndex(Build.GetCategoryIndex()) ? Categories[Build.GetCategoryIndex()].ToString() : FString();
	Text(bMenu ? FString(TEXT("BUILD: choose a category")) : TEXT("BUILD: ") + Category, FLinearColor::White);
	Y += 4.f;
	for (int32 Index = 0; Index < Rows; ++Index)
	{
		const bool bSelected = Index == (bMenu ? Build.GetCategoryIndex() : Build.GetPieceIndex());
		if (bSelected) DrawRect(FLinearColor(.16f, .32f, .42f, .95f), Left + 6.f, Y - 2.f, Width - 12.f, Line);
		const FString Name = bMenu ? Categories[Index].ToString() : Pieces[Index]->DisplayName.ToString();
		Text(bMenu && Index < 9 ? FString::Printf(TEXT("%d  %s"), Index + 1, *Name) : Name, bSelected ? FLinearColor::White : Muted);
	}
	Y += 6.f;
	Text(Controls->KeyLabel(A::BuildPrevious) + TEXT(" / ") + Controls->KeyLabel(A::BuildNext) + (bMenu ? TEXT(": change category") : TEXT(": change piece")), Muted);
	if (bMenu) Text(Controls->KeyLabel(A::Primary) + TEXT(": choose"), Muted);
	else
	{
		Text(Controls->KeyLabel(A::Primary) + TEXT(": place   ") + Controls->KeyLabel(A::Secondary) + TEXT(": remove"), Muted);
		Text(Controls->KeyLabel(A::BuildRotate) + TEXT(": rotate   ") + Controls->KeyLabel(A::BuildMenu) + TEXT(": categories"), Muted);
	}
	Text(Controls->KeyLabel(A::BuildExit) + TEXT(" / ") + Controls->KeyLabel(A::Back) + TEXT(": exit build mode"), Muted);
	if (!bMenu && !Build.GetStatus().IsEmpty())
		Text(Build.GetStatus(), Build.HasValidTarget() ? FLinearColor(.3f, 1.f, .6f) : FLinearColor(1.f, .45f, .35f));
}
