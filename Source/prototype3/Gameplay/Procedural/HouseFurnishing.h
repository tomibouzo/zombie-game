#pragma once

#include "Gameplay/Procedural/HouseLayout.h"

/** Deterministic placement with geometric passage checks. No world or navigation system needed. */
class PROTOTYPE3_API FHouseFurnishing
{
public:
	static bool Generate(const FHouseGenerationSettings& Settings, FHouseLayout& Layout, FString& OutError);
	static bool Validate(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, FString& OutError);
	static FBox2D Footprint(const FHouseGenerationSettings& Settings, const FHouseFurniturePlacement& Item);
	/** Paths from the first door anchor to all other door/furniture access anchors in this room. */
	static bool FindWalkPaths(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout,
		int32 RoomIndex, TArray<TArray<FVector2D>>& OutPaths);
};
