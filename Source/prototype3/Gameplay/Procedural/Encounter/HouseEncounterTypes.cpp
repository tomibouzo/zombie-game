#include "Gameplay/Procedural/Encounter/HouseEncounterTypes.h"

bool FHouseExitDecision::Offer(const TArray<FName>& InOptions)
{
	if (bOffered || !Selected.IsNone() || InOptions.Num() < 2) return false;
	TSet<FName> Unique;
	for (FName Option : InOptions)
	{
		if (Option.IsNone() || Unique.Contains(Option)) return false;
		Unique.Add(Option);
	}
	Options = InOptions;
	bOffered = true;
	return true;
}

bool FHouseExitDecision::Choose(FName Choice)
{
	if (!IsAvailable(Choice)) return false;
	Selected = Choice;
	return true;
}
