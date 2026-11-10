#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterTypes.h"

class AHouseGenerator;
class APawn;
class UWorld;

/** Collision checks include world-static, dynamic, physics and pawn geometry, not just room rectangles. */
namespace HouseEncounterPlacement
{
	bool ClearCapsule(UWorld* World, FVector Feet, float Radius, float HalfHeight, const AActor* Ignored);
	bool Reachable(UWorld* World, FVector From, FVector To, float Radius, float HalfHeight, const AActor* Ignored, FString* Error = nullptr);
	bool ClearItem(UWorld* World, FVector Feet, FVector HalfExtent, FQuat Rotation, const AActor* Ignored);
	bool Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
		int32 ZombieCount, const TArray<FVector>& ItemHalfExtents, int32 Seed, FHouseEncounterPlacement& Out,
		FString& Error, FHouseEncounterPlacementReport* Report = nullptr);
	/** Compatibility helper for count-only callers; reserves a conservative 50 cm item box. */
	bool Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
		int32 ZombieCount, int32 ItemCount, int32 Seed, FHouseEncounterPlacement& Out, FString& Error);
}
