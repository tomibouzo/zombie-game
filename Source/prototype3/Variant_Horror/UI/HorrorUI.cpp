// Copyright Epic Games, Inc. All Rights Reserved.


#include "HorrorUI.h"
#include "HorrorCharacter.h"

void UHorrorUI::SetupCharacter(AHorrorCharacter* HorrorCharacter)
{
	if (!IsValid(HorrorCharacter))
	{
		return;
	}

	HorrorCharacter->OnSprintMeterUpdated.AddDynamic(this, &UHorrorUI::OnSprintMeterUpdated);
	HorrorCharacter->OnSprintStateChanged.AddDynamic(this, &UHorrorUI::OnSprintStateChanged);
	HorrorCharacter->OnHealthUpdated.AddDynamic(this, &UHorrorUI::OnHealthUpdated);

	// The delegates are broadcast during character BeginPlay, before the controller
	// creates this widget, so initialize it explicitly as well.
	OnSprintMeterUpdated(HorrorCharacter->GetStaminaPercent());
	OnHealthUpdated(HorrorCharacter->GetHealthPercent());
}

void UHorrorUI::OnSprintMeterUpdated(float Percent)
{
	// call the BP handler
	BP_SprintMeterUpdated(Percent);
}

void UHorrorUI::OnSprintStateChanged(bool bSprinting)
{
	// call the BP handler
	BP_SprintStateChanged(bSprinting);
}

void UHorrorUI::OnHealthUpdated(float Percent)
{
	// call the BP handler
	BP_HealthUpdated(Percent);
}
