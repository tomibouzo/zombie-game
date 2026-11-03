#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseNavigation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseSizingTest, "Prototype.House.SizingAndHallways",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseSizingTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings S;
	S.WidthCells = S.DepthCells = 24;
	S.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
	FString Error;
	for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
		for (int32 Seed = 0; Seed < 40; ++Seed)
		{
			S.Navigation.Difficulty = static_cast<EHouseNavigationDifficulty>(Difficulty);
			S.RoomCount = 5 + 3 * Difficulty;
			S.Seed = Seed;
			FHouseLayout L;
			const bool bGenerated = FHouseLayoutGenerator::Generate(S, L, Error);
			if (!TestTrue(FString::Printf(TEXT("Residential sizing D%d seed%d: %s"), Difficulty, Seed, *Error), bGenerated)) return false;
			TestTrue(TEXT("House uses compact footprint inside envelope"), L.FootprintCells.X < S.WidthCells && L.FootprintCells.Y < S.DepthCells);
			double Bath = 0, SmallestBed = 10000;
			for (int32 I = 0; I < L.Rooms.Num(); ++I)
			{
				const FHouseRoom& Room = L.Rooms[I];
				const FVector2D Clear = FHouseSizing::ClearSize(S, Room.Max - Room.Min);
				const double Area = Clear.X * Clear.Y;
				if (Room.RoomType == EHouseRoomType::Bathroom)
				{
					Bath = Area;
					TestTrue(TEXT("Bathroom clear area stays 4..8.5 m2"), Area >= 4 && Area <= 8.5);
				}
				if (Room.RoomType == EHouseRoomType::Bedroom)
				{
					SmallestBed = FMath::Min(SmallestBed, Area);
					TestTrue(TEXT("Bedroom exceeds single-room baseline"), Area >= 7.5 && FMath::Min(Clear.X, Clear.Y) >= 2.15);
					if (I == L.Navigation.DeepestRoom) TestTrue(TEXT("Primary-sized destination exceeds double-room baseline"), Area >= 11.5);
				}
				if (Room.RoomType == EHouseRoomType::Hallway)
					TestTrue(TEXT("Hallway is a narrow circulation space"), FMath::Min(Clear.X, Clear.Y) >= 1.2 && FMath::Min(Clear.X, Clear.Y) <= 1.801);
			}
			TestTrue(TEXT("Bathroom never larger than smallest bedroom"), Bath <= SmallestBed);
			FHouseGenerationSettings Bigger = S; Bigger.WidthCells = Bigger.DepthCells = 64;
			FHouseLayout Same;
			if (!TestTrue(TEXT("Larger envelope is feasible"), FHouseLayoutGenerator::Generate(Bigger, Same, Error))) return false;
			TestEqual(TEXT("Unused envelope does not inflate rooms"), Same.FootprintCells, L.FootprintCells);
		}
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Normal;
	S.RoomCount = 8;
	FHouseLayout L;
	if (!TestTrue(TEXT("Malformed hallway baseline"), FHouseLayoutGenerator::Generate(S, L, Error))) return false;
	// Degree alone is insufficient: turn a destination of the entry hallway into another hallway.
	for (const FHouseWall& W : L.Walls)
		if (W.Opening == EHouseOpening::Door && W.RoomA == 2 && W.RoomB != INDEX_NONE && L.Rooms[W.RoomB].RoomType == EHouseRoomType::Bedroom)
		{ L.Rooms[W.RoomB].RoomType = EHouseRoomType::Hallway; break; }
	TestFalse(TEXT("Multiple hallway links do not excuse a single served room"), FHouseNavigation::ValidatePlan(S, L, Error));
	TestTrue(TEXT("Failure identifies useful-room rule"), Error.Contains(TEXT("directly serve at least two")));
	S.RoomSizing.Bathroom.MinArea = 10;
	TestFalse(TEXT("Inverted size ranges rejected"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.RoomSizing = FHouseSizingSettings();
	S.WidthCells = S.DepthCells = 5;
	TestFalse(TEXT("Too-small envelope fails without shrinking rooms"), FHouseLayoutGenerator::Generate(S, L, Error));
	TestTrue(TEXT("Failure returns no partial layout"), L.Rooms.IsEmpty());
	return true;
}
#endif
