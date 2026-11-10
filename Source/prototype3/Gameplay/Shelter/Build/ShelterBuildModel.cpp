#include "Gameplay/Shelter/Build/ShelterBuildModel.h"

bool FShelterBuildModel::HasValidSize() const
{
	return SizeCells.X >= 1 && SizeCells.Y >= 1 && SizeCells.X <= MaxCells && SizeCells.Y <= MaxCells;
}

bool FShelterBuildModel::IsValidSlot(const FShelterSlot& Slot) const
{
	if (!HasValidSize() || Slot.Coord.X < 0 || Slot.Coord.Y < 0) return false;
	if (Slot.Kind == EShelterSlotKind::Cell) return Slot.Coord.X < SizeCells.X && Slot.Coord.Y < SizeCells.Y;
	if (Slot.Kind != EShelterSlotKind::Edge) return false;
	return Slot.bAlongX ? Slot.Coord.X < SizeCells.X && Slot.Coord.Y <= SizeCells.Y
		: Slot.Coord.X <= SizeCells.X && Slot.Coord.Y < SizeCells.Y;
}

int32 FShelterBuildModel::FindIndex(const FShelterSlot& Slot) const
{
	return Placements.IndexOfByPredicate([&](const FShelterPlacement& P) { return P.Slot == Slot; });
}

const FShelterPlacement* FShelterBuildModel::Find(const FShelterSlot& Slot) const
{
	const int32 Index = FindIndex(Slot);
	return Index == INDEX_NONE ? nullptr : &Placements[Index];
}

EShelterBuildResult FShelterBuildModel::CanPlace(const FShelterSlot& Slot, FName PieceId, EShelterSlotKind PieceKind) const
{
	if (!HasValidSize()) return EShelterBuildResult::InvalidGrid;
	if (PieceId.IsNone()) return EShelterBuildResult::InvalidPiece;
	if (Slot.Kind != PieceKind) return EShelterBuildResult::WrongSlotKind;
	if (!IsValidSlot(Slot)) return EShelterBuildResult::OutOfBounds;
	return FindIndex(Slot) == INDEX_NONE ? EShelterBuildResult::Success : EShelterBuildResult::Occupied;
}

EShelterBuildResult FShelterBuildModel::Place(const FShelterSlot& Slot, FName PieceId, EShelterSlotKind PieceKind, int32 Turns)
{
	const EShelterBuildResult Result = CanPlace(Slot, PieceId, PieceKind);
	if (Result != EShelterBuildResult::Success) return Result;
	FShelterPlacement Placement;
	Placement.Slot = Slot;
	if (Slot.Kind == EShelterSlotKind::Cell) Placement.Slot.bAlongX = false;
	Placement.PieceId = PieceId;
	Placement.Turns = NormalizeTurns(Slot.Kind, Turns);
	Placements.Add(Placement);
	return EShelterBuildResult::Success;
}

EShelterBuildResult FShelterBuildModel::Remove(const FShelterSlot& Slot, FShelterPlacement* OutRemoved)
{
	const int32 Index = FindIndex(Slot);
	if (Index == INDEX_NONE) return EShelterBuildResult::NotFound;
	if (OutRemoved) *OutRemoved = Placements[Index];
	Placements.RemoveAt(Index);
	return EShelterBuildResult::Success;
}

bool FShelterBuildModel::Validate(FString& OutError) const
{
	OutError.Reset();
	if (!HasValidSize()) { OutError = TEXT("Build grid must be 1..64 cells on each side."); return false; }
	for (int32 I = 0; I < Placements.Num(); ++I)
	{
		const FShelterPlacement& P = Placements[I];
		if (!IsValidSlot(P.Slot)) { OutError = TEXT("A placed piece is outside the build grid."); return false; }
		if (P.PieceId.IsNone()) { OutError = TEXT("A placed piece has no identity."); return false; }
		if (P.Turns != NormalizeTurns(P.Slot.Kind, P.Turns)) { OutError = TEXT("A placed piece has an invalid rotation."); return false; }
		for (int32 J = I + 1; J < Placements.Num(); ++J)
			if (Placements[J].Slot == P.Slot) { OutError = TEXT("Two pieces share one slot."); return false; }
	}
	return true;
}

int32 FShelterBuildModel::NormalizeTurns(EShelterSlotKind Kind, int32 Turns)
{
	const int32 Count = Kind == EShelterSlotKind::Edge ? 2 : 4;
	return ((Turns % Count) + Count) % Count;
}

FVector2D FShelterBuildModel::SlotCenter(const FShelterSlot& Slot, double ModuleWidth)
{
	const FVector2D Corner(Slot.Coord.X * ModuleWidth, Slot.Coord.Y * ModuleWidth);
	if (Slot.Kind == EShelterSlotKind::Cell) return Corner + FVector2D(ModuleWidth * 0.5);
	return Corner + (Slot.bAlongX ? FVector2D(ModuleWidth * 0.5, 0) : FVector2D(0, ModuleWidth * 0.5));
}

double FShelterBuildModel::SlotYaw(const FShelterSlot& Slot, int32 Turns)
{
	if (Slot.Kind == EShelterSlotKind::Cell) return NormalizeTurns(Slot.Kind, Turns) * 90.0;
	return (Slot.bAlongX ? 0.0 : 90.0) + NormalizeTurns(Slot.Kind, Turns) * 180.0;
}

bool FShelterBuildModel::NearestSlot(FVector2D LocalPoint, EShelterSlotKind Kind, double ModuleWidth, FShelterSlot& OutSlot) const
{
	if (!HasValidSize() || !FMath::IsFinite(ModuleWidth) || ModuleWidth <= 0 || !FMath::IsFinite(LocalPoint.X) || !FMath::IsFinite(LocalPoint.Y)) return false;
	const FVector2D Cells = LocalPoint / ModuleWidth;
	OutSlot = FShelterSlot();
	OutSlot.Kind = Kind;
	const int32 CellX = FMath::Clamp(FMath::FloorToInt32(Cells.X), 0, SizeCells.X - 1);
	const int32 CellY = FMath::Clamp(FMath::FloorToInt32(Cells.Y), 0, SizeCells.Y - 1);
	if (Kind == EShelterSlotKind::Cell)
	{
		OutSlot.Coord = FIntPoint(CellX, CellY);
		OutSlot.bAlongX = false;
		return true;
	}
	// Compare the nearest edge of each orientation by distance to its centre.
	FShelterSlot AlongX, AlongY;
	AlongX.bAlongX = true;
	AlongX.Coord = FIntPoint(CellX, FMath::Clamp(FMath::RoundToInt32(Cells.Y), 0, SizeCells.Y));
	AlongY.bAlongX = false;
	AlongY.Coord = FIntPoint(FMath::Clamp(FMath::RoundToInt32(Cells.X), 0, SizeCells.X), CellY);
	const double DistanceX = FVector2D::DistSquared(SlotCenter(AlongX, ModuleWidth), LocalPoint);
	const double DistanceY = FVector2D::DistSquared(SlotCenter(AlongY, ModuleWidth), LocalPoint);
	OutSlot = DistanceX <= DistanceY ? AlongX : AlongY;
	return true;
}

FString FShelterBuildModel::Describe(EShelterBuildResult Result)
{
	switch (Result)
	{
	case EShelterBuildResult::Success: return TEXT("Can build here");
	case EShelterBuildResult::Occupied: return TEXT("Slot is occupied: remove the piece first");
	case EShelterBuildResult::OutOfBounds: return TEXT("Outside the build area");
	case EShelterBuildResult::WrongSlotKind: return TEXT("This piece does not fit this kind of slot");
	case EShelterBuildResult::NotFound: return TEXT("Nothing to remove here");
	case EShelterBuildResult::Blocked: return TEXT("Someone is standing in the way");
	case EShelterBuildResult::TooFar: return TEXT("Too far away");
	case EShelterBuildResult::InvalidPiece: return TEXT("This piece is not set up correctly");
	default: return TEXT("Build area is not set up correctly");
	}
}
