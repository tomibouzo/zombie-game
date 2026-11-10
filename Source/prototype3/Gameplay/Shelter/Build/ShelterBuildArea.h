#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Shelter/Build/ShelterBuildModel.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "ShelterBuildArea.generated.h"

class UBoxComponent;

/**
 * A fixed build grid placed in a level. Owns the data model and the actors that present it.
 * The actor origin is the grid's minimum corner on the floor; X and Y run along the grid.
 * Placed pieces are not saved: the grid starts empty each session.
 */
UCLASS()
class PROTOTYPE3_API AShelterBuildArea : public AActor
{
	GENERATED_BODY()
public:
	AShelterBuildArea();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="1", ClampMax="64")) FIntPoint SizeCells = FIntPoint(6, 6);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter") FShelterModule Module;
	/** Build mode stays available this far outside the grid, so outer walls can be built from outside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter", meta=(ClampMin="0", Units="cm")) float EntryMargin = 200;

	const FShelterBuildModel& GetModel() const { return Model; }
	bool ContainsLocation(const FVector& WorldLocation) const;
	FTransform SlotTransform(const FShelterSlot& Slot, int32 Turns) const;
	/** Slot nearest to where the view ray meets the floor, or to the ray's end when it does not. */
	bool FindSlotFromView(const FVector& Start, const FVector& Direction, float MaxDistance, EShelterSlotKind Kind, FShelterSlot& OutSlot) const;
	/** Grid rules plus a check that no character stands in the slot. Ignored may be the builder's preview. */
	EShelterBuildResult CanPlace(const FShelterSlot& Slot, const UShelterPieceDefinition* Piece) const;
	EShelterBuildResult PlacePiece(const FShelterSlot& Slot, const UShelterPieceDefinition* Piece, int32 Turns);
	EShelterBuildResult RemoveAt(const FShelterSlot& Slot);
	EShelterBuildResult RemovePiece(const AShelterPiece* Piece);
	AShelterPiece* GetPieceAt(const FShelterSlot& Slot) const;
	/** Unregistered copy for previews; the caller destroys it. */
	AShelterPiece* SpawnPreview(const UShelterPieceDefinition* Piece);
	static AShelterBuildArea* FindContaining(const UWorld* World, const FVector& WorldLocation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	AShelterPiece* SpawnPiece(const UShelterPieceDefinition* Piece, const FTransform& Transform);
	UPROPERTY(VisibleAnywhere, Category="Shelter") TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(VisibleAnywhere, Transient, Category="Shelter") FShelterBuildModel Model;
	/** Parallel to Model.Placements. */
	UPROPERTY(Transient) TArray<TObjectPtr<AShelterPiece>> PieceActors;
};
