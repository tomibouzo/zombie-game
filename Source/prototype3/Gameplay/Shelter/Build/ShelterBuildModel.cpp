#include "Gameplay/Shelter/Build/ShelterBuildModel.h"

namespace
{
const FIntPoint RoofNeighbors[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
}


bool FShelterBuildModel::HasValidSize() const
{
	return SizeCells.X >= 1 && SizeCells.Y >= 1 && SizeCells.X <= MaxCells && SizeCells.Y <= MaxCells;
}

bool FShelterBuildModel::IsValidSlot(const FShelterSlot& Slot) const
{
	if (!HasValidSize() || Slot.Coord.X < 0 || Slot.Coord.Y < 0) return false;
	if (Slot.Kind == EShelterSlotKind::Cell || Slot.Kind == EShelterSlotKind::Roof)
		return Slot.Coord.X < SizeCells.X && Slot.Coord.Y < SizeCells.Y;
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
	if (FindIndex(Slot) != INDEX_NONE) return EShelterBuildResult::Occupied;
	if (Slot.Kind == EShelterSlotKind::Roof && !HasRoofSupport(Slot.Coord)) return EShelterBuildResult::UnsupportedRoof;
	return EShelterBuildResult::Success;
}

EShelterBuildResult FShelterBuildModel::Place(const FShelterSlot& Slot, FName PieceId, EShelterSlotKind PieceKind, int32 Turns, bool bSupportsRoof)
{
	const EShelterBuildResult Result = CanPlace(Slot, PieceId, PieceKind);
	if (Result != EShelterBuildResult::Success) return Result;
	FShelterPlacement Placement;
	Placement.Slot = Slot;
	if (Slot.Kind != EShelterSlotKind::Edge) Placement.Slot.bAlongX = false;
	Placement.PieceId = PieceId;
	Placement.Turns = NormalizeTurns(Slot.Kind, Turns);
	Placement.bSupportsRoof = Slot.Kind == EShelterSlotKind::Edge && bSupportsRoof;
	Placements.Add(Placement);
	return EShelterBuildResult::Success;
}

bool FShelterBuildModel::HasRoofSupport(FIntPoint Coord) const
{
	for (const FShelterPlacement& Placement : Placements)
	{
		const FShelterSlot& Slot = Placement.Slot;
		if (Slot.Kind == EShelterSlotKind::Roof)
		{
			const FIntPoint Offset = Slot.Coord - Coord;
			if (FMath::Abs(Offset.X) + FMath::Abs(Offset.Y) == 1) return true;
		}
		else if (Placement.bSupportsRoof)
		{
			const FIntPoint Across = Slot.bAlongX ? FIntPoint(0, 1) : FIntPoint(1, 0);
			if (Slot.Coord == Coord || Slot.Coord - Across == Coord) return true;
		}
	}
	return false;
}

bool FShelterBuildModel::AllRoofsSupported(int32 IgnoredIndex) const
{
	TSet<FIntPoint> Unsupported;
	for (int32 Index = 0; Index < Placements.Num(); ++Index)
		if (Index != IgnoredIndex && Placements[Index].Slot.Kind == EShelterSlotKind::Roof)
			Unsupported.Add(Placements[Index].Slot.Coord);
	if (Unsupported.IsEmpty()) return true;

	TArray<FIntPoint> Anchored;
	const auto ConnectRoof = [&Unsupported, &Anchored](FIntPoint Coord)
	{
		if (Unsupported.Remove(Coord) > 0) Anchored.Add(Coord);
	};
	for (int32 Index = 0; Index < Placements.Num(); ++Index)
	{
		if (Index == IgnoredIndex || !Placements[Index].bSupportsRoof) continue;
		const FShelterSlot& Wall = Placements[Index].Slot;
		ConnectRoof(Wall.Coord);
		ConnectRoof(Wall.Coord - (Wall.bAlongX ? FIntPoint(0, 1) : FIntPoint(1, 0)));
	}
	for (int32 Index = 0; Index < Anchored.Num(); ++Index)
	{
		const FIntPoint Coord = Anchored[Index];
		for (const FIntPoint Offset : RoofNeighbors) ConnectRoof(Coord + Offset);
	}
	return Unsupported.IsEmpty();
}

EShelterBuildResult FShelterBuildModel::CanRemove(const FShelterSlot& Slot) const
{
	const int32 Index = FindIndex(Slot);
	if (Index == INDEX_NONE) return EShelterBuildResult::NotFound;
	if ((Slot.Kind == EShelterSlotKind::Roof || Placements[Index].bSupportsRoof) && !AllRoofsSupported(Index))
		return EShelterBuildResult::SupportsRoof;
	return EShelterBuildResult::Success;
}

EShelterBuildResult FShelterBuildModel::Remove(const FShelterSlot& Slot, FShelterPlacement* OutRemoved)
{
	const EShelterBuildResult Result = CanRemove(Slot);
	if (Result != EShelterBuildResult::Success) return Result;
	const int32 Index = FindIndex(Slot);
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
	if (!AllRoofsSupported()) { OutError = TEXT("A roof has no path to a supporting wall."); return false; }
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
	if (Slot.Kind != EShelterSlotKind::Edge) return Corner + FVector2D(ModuleWidth * 0.5);
	return Corner + (Slot.bAlongX ? FVector2D(ModuleWidth * 0.5, 0) : FVector2D(0, ModuleWidth * 0.5));
}

double FShelterBuildModel::SlotYaw(const FShelterSlot& Slot, int32 Turns)
{
	if (Slot.Kind != EShelterSlotKind::Edge) return NormalizeTurns(Slot.Kind, Turns) * 90.0;
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
	if (Kind == EShelterSlotKind::Cell || Kind == EShelterSlotKind::Roof)
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
	case EShelterBuildResult::UnsupportedRoof: return TEXT("A roof needs a supporting wall or an adjacent roof");
	case EShelterBuildResult::PieceHasContents: return TEXT("Empty this piece before removing it");
	case EShelterBuildResult::SupportsRoof: return TEXT("This piece supports the roof");
	case EShelterBuildResult::TooFar: return TEXT("Too far away");
	case EShelterBuildResult::InvalidPiece: return TEXT("This piece is not set up correctly");
	default: return TEXT("Build area is not set up correctly");
	}
}
