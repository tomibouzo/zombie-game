#include "Gameplay/Procedural/HouseSpatial.h"

FBox2D FHouseSpatial::RoomBounds(const FHouseGenerationSettings& S, const FHouseRoom& Room, double Inset)
{
	return FBox2D(FVector2D(Room.Min) * S.CellSize + FVector2D(Inset),
		FVector2D(Room.Max) * S.CellSize - FVector2D(Inset));
}

int32 FHouseSpatial::RoomAt(const FHouseGenerationSettings& S, const FHouseLayout& Layout, const FVector2D& Point)
{
	return Layout.Rooms.IndexOfByPredicate([&](const FHouseRoom& Room)
	{
		const FBox2D Bounds = RoomBounds(S, Room);
		return Point.X > Bounds.Min.X && Point.X < Bounds.Max.X && Point.Y > Bounds.Min.Y && Point.Y < Bounds.Max.Y;
	});
}

FVector2D FHouseSpatial::FurnitureFacing(int32 Turns)
{
	switch (Turns)
	{
	case 1: return FVector2D(-1, 0);
	case 2: return FVector2D(0, -1);
	case 3: return FVector2D(1, 0);
	default: return FVector2D(0, 1);
	}
}

FBox2D FHouseSpatial::FurnitureBody(const FHouseGenerationSettings& S, const FHouseFurniturePlacement& Item)
{
	const FVector& Size = S.FurnitureRules[Item.RuleIndex].Size;
	const FVector2D Half = (Item.QuarterTurns % 2 ? FVector2D(Size.Y, Size.X) : FVector2D(Size.X, Size.Y)) * 0.5;
	return FBox2D(Item.Center - Half, Item.Center + Half);
}

FBox2D FHouseSpatial::FurnitureAccess(const FHouseGenerationSettings& S, const FHouseFurniturePlacement& Item)
{
	const FBox2D Body = FurnitureBody(S, Item);
	const FVector2D Front = FurnitureFacing(Item.QuarterTurns);
	const FVector2D Face = Item.Center + Front * Body.GetExtent();
	const double Depth = S.FurnitureRules[Item.RuleIndex].FrontClearance;
	const FVector2D Size = Item.QuarterTurns % 2 ? FVector2D(Depth, Body.GetSize().Y) : FVector2D(Body.GetSize().X, Depth);
	const FVector2D Center = Face + Front * (Depth * 0.5);
	return FBox2D(Center - Size * 0.5, Center + Size * 0.5);
}

FVector2D FHouseSpatial::DoorCenter(const FHouseGenerationSettings& S, const FHouseWall& Wall)
{
	return (FVector2D(Wall.Start) + FVector2D(Wall.End)) * (S.CellSize * 0.5);
}

FVector2D FHouseSpatial::DoorInward(const FHouseRoom& Room, const FHouseWall& Wall)
{
	if (Wall.Start.X == Wall.End.X) return FVector2D(Wall.Start.X == Room.Min.X ? 1 : -1, 0);
	return FVector2D(0, Wall.Start.Y == Room.Min.Y ? 1 : -1);
}

FBox2D FHouseSpatial::DoorAccess(const FHouseGenerationSettings& S, const FHouseRoom& Room, const FHouseWall& Wall)
{
	// Include the approaching body beyond its centre anchor, so turning at a doorway cannot clip furniture.
	const double Depth = 1.5 * S.WalkwayWidth + S.WallThickness;
	const bool AlongX = Wall.Start.Y == Wall.End.Y;
	const FVector2D Half = AlongX ? FVector2D(S.DoorWidth * 0.5 + 10, Depth * 0.5) : FVector2D(Depth * 0.5, S.DoorWidth * 0.5 + 10);
	const FVector2D Center = DoorCenter(S, Wall) + DoorInward(Room, Wall) * Depth * 0.5;
	return FBox2D(Center - Half, Center + Half);
}

bool FHouseSpatial::Overlaps(const FBox2D& A, const FBox2D& B)
{
	return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
}

bool FHouseSpatial::Contains(const FBox2D& Outer, const FBox2D& Inner)
{
	return Inner.Min.X >= Outer.Min.X && Inner.Min.Y >= Outer.Min.Y &&
		Inner.Max.X <= Outer.Max.X && Inner.Max.Y <= Outer.Max.Y;
}
