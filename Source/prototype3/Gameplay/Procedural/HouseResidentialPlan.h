#pragma once
#include "Gameplay/Procedural/HouseLayout.h"

/** Bounded residential packing: each corridor section serves at least two private rooms. */
class FHouseResidentialPlan
{
public:
	static bool Build(const FHouseGenerationSettings& S, FHouseLayout& L, TArray<FIntPoint>& Connections, FString& Error);
};
