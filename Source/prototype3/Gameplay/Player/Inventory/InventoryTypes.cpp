#include "Gameplay/Player/Inventory/InventoryTypes.h"
#include "Gameplay/Items/ItemDefinition.h"

bool FInventoryItemProfile::IsValid() const
{
	if (Id.IsNone() || !::IsValid(Definition.Get()) || !Definition->IsValidDefinition()
		|| OccupiedCells.IsEmpty() || OccupiedCells.Num() > 65536) return false;
	TSet<FIntPoint> Unique;
	FIntPoint Minimum(MAX_int32, MAX_int32);
	for (FIntPoint Cell : OccupiedCells)
	{
		if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= 256 || Cell.Y >= 256 || Unique.Contains(Cell)) return false;
		Unique.Add(Cell);
		Minimum.X = FMath::Min(Minimum.X, Cell.X);
		Minimum.Y = FMath::Min(Minimum.Y, Cell.Y);
	}
	return Minimum == FIntPoint::ZeroValue;
}

TArray<FIntPoint> FInventoryItemProfile::GetRotatedCells(int32 QuarterTurns) const
{
	TArray<FIntPoint> Cells = OccupiedCells;
	const int32 Turns = ((QuarterTurns % 4) + 4) % 4;
	for (int32 Turn = 0; Turn < Turns; ++Turn)
		for (FIntPoint& Cell : Cells) Cell = FIntPoint(-Cell.Y, Cell.X);
	FIntPoint Minimum(MAX_int32, MAX_int32);
	for (FIntPoint Cell : Cells)
	{
		Minimum.X = FMath::Min(Minimum.X, Cell.X);
		Minimum.Y = FMath::Min(Minimum.Y, Cell.Y);
	}
	for (FIntPoint& Cell : Cells) Cell -= Minimum;
	return Cells;
}
