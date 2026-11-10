#pragma once

#include "CoreMinimal.h"
#include "ShelterBuildModel.generated.h"

/** Edge slots hold wall-sized pieces between cells; cell slots hold one object on a floor cell. */
UENUM(BlueprintType)
enum class EShelterSlotKind : uint8 { Edge, Cell };

UENUM(BlueprintType)
enum class EShelterBuildResult : uint8
{
	Success, InvalidGrid, OutOfBounds, Occupied, WrongSlotKind, InvalidPiece, NotFound, Blocked, TooFar
};

/**
 * One position on the build grid.
 * Edge along X: from corner (X,Y) to (X+1,Y). Edge along Y: from corner (X,Y) to (X,Y+1).
 * Cell: the cell whose minimum corner is (X,Y); bAlongX is ignored and stored as false.
 */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FShelterSlot
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") EShelterSlotKind Kind = EShelterSlotKind::Edge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") FIntPoint Coord = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") bool bAlongX = true;
	bool operator==(const FShelterSlot& Other) const
	{
		return Kind == Other.Kind && Coord == Other.Coord && (Kind == EShelterSlotKind::Cell || bAlongX == Other.bAlongX);
	}
};

/** A placed piece. Turns: edges use 0/1 (facing flipped); cells use quarter turns 0..3. */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FShelterPlacement
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") FShelterSlot Slot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") FName PieceId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") int32 Turns = 0;
};

/**
 * Data-only build grid: no world, assets or actors. Each slot holds at most one piece;
 * placing on an occupied slot is refused. Actors only present this state.
 */
USTRUCT(BlueprintType)
struct PROTOTYPE3_API FShelterBuildModel
{
	GENERATED_BODY()

	static constexpr int32 MaxCells = 64;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") FIntPoint SizeCells = FIntPoint(6, 6);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shelter") TArray<FShelterPlacement> Placements;

	bool HasValidSize() const;
	bool IsValidSlot(const FShelterSlot& Slot) const;
	int32 FindIndex(const FShelterSlot& Slot) const;
	const FShelterPlacement* Find(const FShelterSlot& Slot) const;
	/** PieceKind is the slot kind the piece declares; it must match the slot. */
	EShelterBuildResult CanPlace(const FShelterSlot& Slot, FName PieceId, EShelterSlotKind PieceKind) const;
	EShelterBuildResult Place(const FShelterSlot& Slot, FName PieceId, EShelterSlotKind PieceKind, int32 Turns);
	EShelterBuildResult Remove(const FShelterSlot& Slot, FShelterPlacement* OutRemoved = nullptr);
	/** Rejects out-of-range slots, duplicate slots, unnamed pieces and out-of-range turns. */
	bool Validate(FString& OutError) const;

	static int32 NormalizeTurns(EShelterSlotKind Kind, int32 Turns);
	/** Grid-local centre of a slot, in centimetres, for a square module of ModuleWidth. */
	static FVector2D SlotCenter(const FShelterSlot& Slot, double ModuleWidth);
	/** Grid-local yaw in degrees. Edge pieces run along local X; Turns flips or quarter-turns them. */
	static double SlotYaw(const FShelterSlot& Slot, int32 Turns);
	/** Nearest slot of Kind to a grid-local point, clamped to the grid. False only for an invalid grid. */
	bool NearestSlot(FVector2D LocalPoint, EShelterSlotKind Kind, double ModuleWidth, FShelterSlot& OutSlot) const;
	static FString Describe(EShelterBuildResult Result);
};
