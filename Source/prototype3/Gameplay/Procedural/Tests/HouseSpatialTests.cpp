#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseSpatial.h"
#include "Gameplay/Procedural/HouseFurnishing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseSpatialTest, "Prototype.House.SpatialClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHouseSpatialTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings S;
	S.FurnitureRules.SetNum(1);
	S.FurnitureRules[0].Size = FVector(200, 80, 90);
	S.FurnitureRules[0].FrontClearance = 60;
	FHouseFurniturePlacement Item;
	Item.RuleIndex = 0;
	Item.Center = FVector2D(300, 250);
	// Explicit expected front regions catch a reversed normal or size swap at any quarter turn.
	const FBox2D ExpectedAccess[] = {
		FBox2D(FVector2D(200, 290), FVector2D(400, 350)),
		FBox2D(FVector2D(200, 150), FVector2D(260, 350)),
		FBox2D(FVector2D(200, 150), FVector2D(400, 210)),
		FBox2D(FVector2D(340, 150), FVector2D(400, 350))};
	for (int32 Turn = 0; Turn < 4; ++Turn)
	{
		Item.QuarterTurns = Turn;
		const FBox2D Body = FHouseSpatial::FurnitureBody(S, Item), Access = FHouseSpatial::FurnitureAccess(S, Item);
		TestEqual(TEXT("Body width rotates with furniture"), Body.GetSize(), Turn % 2 ? FVector2D(80, 200) : FVector2D(200, 80));
		TestEqual(TEXT("Access begins at front face"), Access.Min, ExpectedAccess[Turn].Min);
		TestEqual(TEXT("Access covers the full front width"), Access.Max, ExpectedAccess[Turn].Max);
		TestFalse(TEXT("Body and access touch without overlapping"), FHouseSpatial::Overlaps(Body, Access));
		TestTrue(TEXT("Body margin enters its access area"), FHouseSpatial::Overlaps(Body.ExpandBy(1), Access));
		TestEqual(TEXT("Furnishing uses the shared body region"), FHouseFurnishing::Footprint(S, Item).Min, Body.Min);
	}

	S.CellSize = 100;
	S.WallThickness = 20;
	S.WalkwayWidth = 90;
	S.DoorWidth = 140;
	FHouseLayout Layout;
	FHouseRoom& Left = Layout.Rooms.AddDefaulted_GetRef();
	Left.Min = FIntPoint(0, 0); Left.Max = FIntPoint(4, 4);
	FHouseRoom& Right = Layout.Rooms.AddDefaulted_GetRef();
	Right.Min = FIntPoint(4, 0); Right.Max = FIntPoint(8, 4);
	FHouseWall Door;
	Door.Start = FIntPoint(4, 0); Door.End = FIntPoint(4, 4);
	Door.Opening = EHouseOpening::Door; Door.RoomA = 0; Door.RoomB = 1;
	const FBox2D LeftAccess = FHouseSpatial::DoorAccess(S, Layout.Rooms[0], Door);
	const FBox2D RightAccess = FHouseSpatial::DoorAccess(S, Layout.Rooms[1], Door);
	TestEqual(TEXT("Left doorway approaches threshold from inside room"), LeftAccess.Min, FVector2D(245, 120));
	TestEqual(TEXT("Left doorway stops at threshold"), LeftAccess.Max, FVector2D(400, 280));
	TestEqual(TEXT("Right doorway starts at threshold"), RightAccess.Min, FVector2D(400, 120));
	TestEqual(TEXT("Right doorway extends into its room"), RightAccess.Max, FVector2D(555, 280));
	TestFalse(TEXT("Opposite approaches only touch at threshold"), FHouseSpatial::Overlaps(LeftAccess, RightAccess));
	Door.Start = FIntPoint(0, 0); Door.End = FIntPoint(4, 0); Door.RoomB = INDEX_NONE;
	const FBox2D Entrance = FHouseSpatial::DoorAccess(S, Layout.Rooms[0], Door);
	TestEqual(TEXT("Exterior door has inward approach"), Entrance.Min, FVector2D(120, 0));
	TestEqual(TEXT("Exterior door reserves swept player clearance"), Entrance.Max, FVector2D(280, 155));
	TestTrue(TEXT("Door rectangle is within physical room bounds"), FHouseSpatial::Contains(FHouseSpatial::RoomBounds(S, Layout.Rooms[0]), Entrance));
	TestEqual(TEXT("Room inset measures usable wall clearance"), FHouseSpatial::RoomBounds(S, Layout.Rooms[0], 15).Min, FVector2D(15, 15));
	TestEqual(TEXT("Point selects left room"), FHouseSpatial::RoomAt(S, Layout, FVector2D(399, 200)), 0);
	TestEqual(TEXT("Point selects right room"), FHouseSpatial::RoomAt(S, Layout, FVector2D(401, 200)), 1);
	TestEqual(TEXT("Shared wall is not assigned arbitrarily"), FHouseSpatial::RoomAt(S, Layout, FVector2D(400, 200)), INDEX_NONE);
	TestEqual(TEXT("Outside point has no room"), FHouseSpatial::RoomAt(S, Layout, FVector2D(-1, 200)), INDEX_NONE);
	return true;
}
#endif
