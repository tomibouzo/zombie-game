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
class UHouseGenerationPreset;

/** Single-player, single-use encounter. Preparation is transactional; player placement commits last. */
UCLASS(Blueprintable)
class PROTOTYPE3_API AHouseEncounter : public AActor
{
	GENERATED_BODY()
public:
	AHouseEncounter();
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
	/** Optional building model. When set, the preset's fixed values and seed-resolved ranges replace House Settings
	 * (only Seed is read) and decide Zombie/Item Count. Clear it to use Custom controls, starting from this preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TObjectPtr<UHouseGenerationPreset> Preset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") FHouseGenerationSettings HouseSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TObjectPtr<UHouseStyle> HouseStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin="0", ClampMax="32", EditCondition="Preset == nullptr")) int32 ZombieCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin="0", ClampMax="64", EditCondition="Preset == nullptr")) int32 ItemCount = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TSubclassOf<ACharacter> ZombieClass;
	/** Existing ItemIds. Repetitions weight the seeded selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TArray<FName> ItemSelection;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter") TArray<FName> NextSections;
	UPROPERTY(EditAnywhere, Category="Encounter") bool bStartOnBeginPlay = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") EHouseEncounterState State = EHouseEncounterState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FString LastError;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") int32 LayoutAttempts = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") int32 RequestSeed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FString GenerationStatus;
	/** Values the preset resolved for the current request seed; empty when using Custom controls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FString PresetSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FHouseEncounterPlacement Placement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FHouseEncounterPlacementReport PlacementReport;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") FHouseExitDecision ExitDecision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TObjectPtr<AHouseGenerator> House;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TArray<TObjectPtr<ACharacter>> Zombies;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Encounter|Result") TArray<TObjectPtr<ADroppedItem>> Items;

	/** Separately callable for headless integration checks; repeated starts are rejected. */
	bool Prepare(ACharacter* InPlayer);
	bool Activate();
	/** A new seeded request after failure; preserves the snapshotted configuration and exact spawn counts. */
	UFUNCTION(BlueprintCallable, Category="Encounter") bool RetryGeneration();
	bool CanRetryGeneration() const;
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
	UPROPERTY(Transient) TObjectPtr<UHouseStyle> PreparedStyle;
	UPROPERTY(Transient) TObjectPtr<UHouseGenerationPreset> PreparedPreset;
	TWeakObjectPtr<UHouseGenerationPreset> PresetBeforeEdit;
	/** Resolve the request for Seed: preset values and counts, or the snapshotted Custom controls. */
	bool ResolveRequest(int32 Seed, FString& OutError);
	FHouseGenerationSettings RequestedHouseSettings;
	FHouseGenerationSettings PreparedHouseSettings;
	uint32 RetryNumber = 0;
	bool bRetryAllowed = false;
	bool bLayoutRetryPending = false;
	static constexpr int32 MaxLayoutAttempts = 4;
	TArray<FName> PreparedItemSelection;
	int32 PreparedSeed = 0;
	int32 PreparedZombieCount = 0;
	int32 PreparedItemCount = 0;
	float WaitSeconds = 0;
	float NavigationWait = 0;
	FVector PreviousFeet = FVector::ZeroVector;
	TArray<FName> PreparedNextSections;
	bool Fail(const FString& Error);
	bool BeginLayoutAttempt();
	bool RecoverPlacement(const FString& Error);
	void ClearLayout();
	void Cleanup();
};
