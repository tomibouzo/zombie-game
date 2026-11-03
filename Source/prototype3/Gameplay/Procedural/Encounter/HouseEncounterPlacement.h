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
	bool Plan(AHouseGenerator* House, APawn* Player, float ZombieRadius, float ZombieHalfHeight,
		int32 ZombieCount, int32 ItemCount, int32 Seed, FHouseEncounterPlacement& Out, FString& Error);
}
