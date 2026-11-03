#pragma once

#include "CoreMinimal.h"
#include "Core/GameModes/prototype3GameMode.h"
#include "HouseEncounterGameMode.generated.h"

UCLASS()
class PROTOTYPE3_API AHouseEncounterGameMode : public Aprototype3GameMode
{
	GENERATED_BODY()
public:
	AHouseEncounterGameMode();
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};
