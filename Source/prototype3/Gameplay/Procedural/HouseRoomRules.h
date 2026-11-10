#pragma once

#include "Gameplay/Procedural/HouseLayout.h"

/** Deterministic room semantics. No random draws, world objects, or changes to room geometry. */
class PROTOTYPE3_API FHouseRoomRules
{
public:
	static bool ValidateSettings(const FHouseGenerationSettings& Settings, FString& Error);
	static void Refresh(const FHouseGenerationSettings& Settings, FHouseLayout& Layout);
	static bool ValidateConnections(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, FString& Error);
	static bool RequiresFurniture(const FHouseGenerationSettings& Settings, const FHouseRoom& Room, EHouseFurnitureType Type);
	static bool WantsFurniture(const FHouseGenerationSettings& Settings, const FHouseRoom& Room, int32 RoomIndex, int32 FurnitureRuleIndex);
	static bool ValidateFurniture(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, int32 RoomIndex, FString& Error);
	static bool Validate(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, FString& Error);
};
