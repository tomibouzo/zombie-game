#pragma once
#include "Gameplay/Procedural/HouseLayout.h"

/** Residential program, separate from geometry/navigation and furniture placement. */
class PROTOTYPE3_API FHouseHousehold
{
public:
	static int32 RequiredPrivateRooms(const FHouseholdSettings& H);
	static bool ValidateSettings(const FHouseGenerationSettings& S, FString& Error);
	static bool Assign(const FHouseGenerationSettings& S, FHouseLayout& L, FString& Error);
	static bool Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error);
	static bool WantsFurniture(const FHouseGenerationSettings& S, const FHouseRoom& Room, int32 RoomIndex, int32 RuleIndex);
	static FString RoomLabel(const FHouseRoom& Room);
};
