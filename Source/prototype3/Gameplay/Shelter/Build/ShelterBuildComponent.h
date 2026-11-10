#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Gameplay/Shelter/Build/ShelterBuildModel.h"
#include "ShelterBuildComponent.generated.h"

class AShelterBuildArea;
class AShelterPiece;
class UShelterPieceDefinition;
class UPlayerVitalsComponent;
class Aprototype3PlayerController;

UENUM(BlueprintType)
enum class EShelterBuildState : uint8 { Closed, Menu, Placing };

/**
 * Build mode for the local player controller. An overlay, not an interface: no cursor and no
 * input-mode change, so the player keeps walking and looking. The controller offers every key
 * to HandleKey before its own routing; this component decides which ones build mode owns.
 */
UCLASS(ClassGroup=(Shelter))
class PROTOTYPE3_API UShelterBuildComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UShelterBuildComponent();

	/** Placement and removal distance from the player, measured on the floor. */
	UPROPERTY(EditAnywhere, Category="Shelter", meta=(ClampMin="100", Units="cm")) float Reach = 450;

	/** True when build mode consumed the key. Movement, look and gait keys are never consumed. */
	bool HandleKey(const FKey& Key, EInputEvent Event);
	/** Opens the category menu if the player is alive, inside a build area and no interface is open. */
	bool Open();
	void Close();
	bool IsActive() const { return State != EShelterBuildState::Closed; }
	EShelterBuildState GetState() const { return State; }

	/** Replaces Asset Manager discovery; used by checks and by maps without piece assets. */
	void SetCatalog(const TArray<UShelterPieceDefinition*>& Pieces);
	const TArray<FText>& GetCategories() const { return Categories; }
	int32 GetCategoryIndex() const { return CategoryIndex; }
	TArray<UShelterPieceDefinition*> GetPiecesInCategory(int32 Index) const;
	int32 GetPieceIndex() const { return PieceIndex; }
	UShelterPieceDefinition* GetSelectedPiece() const;
	int32 GetTurns() const { return Turns; }
	bool HasValidTarget() const { return bTargetValid; }
	const FShelterSlot& GetTargetSlot() const { return TargetSlot; }
	const FString& GetStatus() const { return Status; }
	AShelterBuildArea* GetArea() const { return Area.Get(); }
	/** Recomputes the aimed slot and preview immediately; Tick calls this every frame while placing. */
	void RefreshTarget();
	/** Closes build mode when the player dies or leaves the area; refreshes the preview while placing. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	Aprototype3PlayerController* Controller() const;
	void LoadCatalog();
	void RebuildCategories();
	void EnterPlacing(int32 InCategory);
	void ReturnToMenu();
	void Cycle(int32 Step);
	void TryPlace();
	void TryRemove();
	void DestroyPreview();
	void WatchVitals();
	UFUNCTION() void OwnerDamaged(float Amount, bool bInterruptActions);
	UFUNCTION() void OwnerDied();

	EShelterBuildState State = EShelterBuildState::Closed;
	UPROPERTY(Transient) TArray<TObjectPtr<UShelterPieceDefinition>> Catalog;
	TArray<FText> Categories;
	bool bCatalogLoaded = false;
	int32 CategoryIndex = 0;
	int32 PieceIndex = 0;
	int32 Turns = 0;
	TWeakObjectPtr<AShelterBuildArea> Area;
	UPROPERTY(Transient) TObjectPtr<AShelterPiece> Preview;
	UPROPERTY(Transient) TObjectPtr<UShelterPieceDefinition> PreviewPiece;
	TWeakObjectPtr<UPlayerVitalsComponent> WatchedVitals;
	FShelterSlot TargetSlot;
	bool bTargetValid = false;
	bool bPreviewTintValid = false;
	FString Status;
};
