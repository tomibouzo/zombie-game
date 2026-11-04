#pragma once

#include "Gameplay/Procedural/HouseLayout.h"

/** Shared spatial rules in house-local centimetres. These are query/debug regions, not collision components. */
class PROTOTYPE3_API FHouseSpatial
{
public:
	/** Cell boundary rectangle, optionally inset on all four sides. Zero inset includes wall centre lines. */
	static FBox2D RoomBounds(const FHouseGenerationSettings& Settings, const FHouseRoom& Room, double Inset = 0);
	/** A point on a wall boundary is not inside either room. */
	static int32 RoomAt(const FHouseGenerationSettings& Settings, const FHouseLayout& Layout, const FVector2D& Point);
	static FVector2D FurnitureFacing(int32 QuarterTurns);
	static FBox2D FurnitureBody(const FHouseGenerationSettings& Settings, const FHouseFurniturePlacement& Item);
	static FBox2D FurnitureAccess(const FHouseGenerationSettings& Settings, const FHouseFurniturePlacement& Item);
	static FVector2D DoorCenter(const FHouseGenerationSettings& Settings, const FHouseWall& Wall);
	static FVector2D DoorInward(const FHouseRoom& Room, const FHouseWall& Wall);
	/** One side of a doorway's reserved approach. Call once for each connected room, including exterior doors. */
	static FBox2D DoorAccess(const FHouseGenerationSettings& Settings, const FHouseRoom& Room, const FHouseWall& Wall);
	/** Strict overlap: touching edges do not overlap. Containment includes touching edges. */
	static bool Overlaps(const FBox2D& A, const FBox2D& B);
	static bool Contains(const FBox2D& Outer, const FBox2D& Inner);
};
