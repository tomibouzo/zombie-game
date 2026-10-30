#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "HouseEncounterSupport.generated.h"

/** Visible stand-in only. No chase, attack or other NPC behavior is added here. */
UCLASS()
class PROTOTYPE3_API AHouseZombiePlaceholder : public ACharacter
{
	GENERATED_BODY()
public:
	AHouseZombiePlaceholder();
};

/** Runtime navigation stays local to the encounter map instead of changing project-wide defaults. */
UCLASS()
class PROTOTYPE3_API AHouseEncounterNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()
public:
	virtual void PostInitProperties() override;
};

/** Selected by this map's World Settings; uses the encounter's nav data class for the standing agent. */
UCLASS()
class PROTOTYPE3_API UHouseEncounterNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()
public:
	virtual void PostInitProperties() override;
	static FNavDataConfig AgentConfig();
};

/** Box-backed bounds work both in saved maps and automation worlds without an editor BSP builder. */
UCLASS()
class PROTOTYPE3_API AHouseEncounterNavBounds : public ANavMeshBoundsVolume
{
	GENERATED_BODY()
public:
	AHouseEncounterNavBounds();
};
