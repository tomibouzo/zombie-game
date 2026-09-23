#include "Gameplay/Items/Data/ItemDefinition.h"

bool UItemDefinition::IsValidDefinition() const
{
	if (!bStorable || MaxStackSize < 1 || OccupiedCells.IsEmpty())
	{
		return false;
	}
	TSet<FIntPoint> Unique;
	for (const FIntPoint& Cell : OccupiedCells)
	{
		if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= 256 || Cell.Y >= 256 || Unique.Contains(Cell))
		{
			return false;
		}
		Unique.Add(Cell);
	}
	return true;
}

TArray<FIntPoint> UItemDefinition::GetRotatedCells(int32 QuarterTurns) const
{
	TArray<FIntPoint> Result = OccupiedCells;
	const int32 Turns = ((QuarterTurns % 4) + 4) % 4;
	for (int32 Turn = 0; Turn < Turns; ++Turn)
	{
		for (FIntPoint& Cell : Result)
		{
			Cell = FIntPoint(-Cell.Y, Cell.X);
		}
	}
	// Normalize the rotated silhouette; the anchor always describes its top-left.
	FIntPoint Minimum(MAX_int32, MAX_int32);
	for (const FIntPoint& Cell : Result)
	{
		Minimum.X = FMath::Min(Minimum.X, Cell.X);
		Minimum.Y = FMath::Min(Minimum.Y, Cell.Y);
	}
	for (FIntPoint& Cell : Result)
	{
		Cell -= Minimum;
	}
	return Result;
}
