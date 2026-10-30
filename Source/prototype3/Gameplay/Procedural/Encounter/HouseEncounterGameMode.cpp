#include "Gameplay/Procedural/Encounter/HouseEncounterGameMode.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AHouseEncounterGameMode::AHouseEncounterGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PawnClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	DefaultPawnClass = PawnClass.Class;
	PlayerControllerClass = ControllerClass.Class;
	bSpawnInventoryTestPile = false;
}

APawn* AHouseEncounterGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	APawn* Pawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	if (Pawn)
		if (auto* Use = Pawn->FindComponentByClass<UPlayerItemUseComponent>()) Use->bSpawnTestSpikes = false;
	return Pawn;
}
