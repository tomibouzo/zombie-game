#pragma once
#include "Gameplay/Procedural/HouseLayout.h"

class PROTOTYPE3_API FHouseSizing
{
public:
	static bool ValidateSettings(const FHouseGenerationSettings& S, FString& Error);
	static const FHouseRoomSizeRange& Range(const FHouseGenerationSettings& S, const FHouseRoom& Room, bool bPrimary);
	static FVector2D ClearSize(const FHouseGenerationSettings& S, FIntPoint Cells);
	static bool Fits(const FHouseGenerationSettings& S, const FHouseRoom& Room, FIntPoint Cells, bool bPrimary = false);
	static bool Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error);
	static FIntPoint Footprint(const FHouseGenerationSettings& S, const FHouseLayout& L);
};
