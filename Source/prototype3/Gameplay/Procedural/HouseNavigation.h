#pragma once

#include "Gameplay/Procedural/HouseLayout.h"

/** Parametric room plans and door-graph measurements; independent of world/mesh state. */
class PROTOTYPE3_API FHouseNavigation
{
public:
	static int32 MinimumRoomCount(const FHouseGenerationSettings& Settings);
	static int32 TargetDepth(const FHouseNavigationSettings& Settings);
	static bool ValidateSettings(const FHouseGenerationSettings& Settings, FString& OutError);
	static bool BuildPlan(const FHouseGenerationSettings& Settings, FHouseLayout& Layout,
		TArray<FIntPoint>& Connections, FString& OutError);
	static FHouseNavigationReport Measure(const FHouseLayout& Layout);
	/** Doorway grammar; sharing a solid wall does not imply a connection. */
	static bool IsResidentialConnectionAllowed(EHouseRoomType A, EHouseRoomType B);
	static bool ValidatePlan(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, FString& OutError);
	static FString Describe(const FHouseGenerationSettings& Settings, const FHouseNavigationReport& Report);
};
