#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Procedural/HouseLayout.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterTypes.h"
#include "HouseEncounter.generated.h"

class ACharacter;
class AHouseGenerator;
class ADroppedItem;
class UInventoryComponent;
class UHouseStyle;

/** Single-player, single-use encounter. Preparation is transactional; player placement commits last. */
UCLASS(Blueprintable)
class PROTOTYPE3_API AHouseEncounter : public AActor
{
	GENERATED_BODY()
public:
	AHouseEncounter();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") FHouseGenerationSettings HouseSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TObjectPtr<UHouseStyle> HouseStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin="0", ClampMax="32")) int32 ZombieCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin="0", ClampMax="64")) int32 ItemCount = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TSubclassOf<ACharacter> ZombieClass;
	/** Existing ItemIds. Repetitions weight the seeded selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TArray<FName> ItemSelection;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TArray<FName> NextSections;
	UPROPERTY(EditAnywhere, Category="Encounter") bool bStartOnBeginPlay = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") EHouseEncounterState State = EHouseEncounterState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FString LastError;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FHouseEncounterPlacement Placement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FHouseExitDecision ExitDecision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TObjectPtr<AHouseGenerator> House;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TArray<TObjectPtr<ACharacter>> Zombies;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TArray<TObjectPtr<ADroppedItem>> Items;

	/** Separately callable for headless integration checks; repeated starts are rejected. */
	bool Prepare(ACharacter* InPlayer);
	bool Activate();
	UFUNCTION(BlueprintCallable, Category="Encounter") bool ChooseNextSection(FName Choice);
	/** Polling also catches crossings when overlap events are not generated. Only the assigned player counts. */
	bool CheckExit();
	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(Transient) TObjectPtr<ACharacter> Player;
	UPROPERTY(Transient) TObjectPtr<UInventoryComponent> Catalog;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> OwnedActors;
	UPROPERTY(Transient) TSubclassOf<ACharacter> PreparedZombieClass;
	TArray<FName> PreparedItemSelection;
	int32 PreparedSeed = 0;
	int32 PreparedZombieCount = 0;
	int32 PreparedItemCount = 0;
	float WaitSeconds = 0;
	float NavigationWait = 0;
	FVector PreviousFeet = FVector::ZeroVector;
	TArray<FName> PreparedNextSections;
	bool Fail(const FString& Error);
	void Cleanup();
};
