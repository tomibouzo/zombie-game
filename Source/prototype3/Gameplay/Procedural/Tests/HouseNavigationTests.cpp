#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseNavigation.h"

namespace
{
	FString PlanSignature(const FHouseLayout& L)
	{
		FString Result;
		for (const FHouseRoom& R : L.Rooms)
			Result += FString::Printf(TEXT("%d,%d,%d,%d,%d;"), R.Min.X, R.Min.Y, R.Max.X, R.Max.Y, static_cast<int32>(R.RoomType));
		for (const FHouseWall& W : L.Walls)
			if (W.Opening == EHouseOpening::Door) Result += FString::Printf(TEXT("D%d,%d;"), W.RoomA, W.RoomB);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseNavigationPatternsTest, "Prototype.House.NavigationPatterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseNavigationPatternsTest::RunTest(const FString& Parameters)
{
	TSet<FString> Signatures;
	for (EHouseLayoutPattern Pattern : {EHouseLayoutPattern::CentralHub, EHouseLayoutPattern::DeepBranches})
		for (EHouseNavigationDifficulty Difficulty : {EHouseNavigationDifficulty::Easy, EHouseNavigationDifficulty::Normal, EHouseNavigationDifficulty::Hard})
			for (int32 Seed = -20; Seed < 20; ++Seed)
			{
				FHouseGenerationSettings S;
				S.Navigation.Pattern = Pattern;
				S.Navigation.Difficulty = Difficulty;
				S.RoomCount = 5 + 3 * static_cast<int32>(Difficulty);
				S.WidthCells = S.DepthCells = 24;
				S.Seed = Seed;
				FHouseLayout L, Repeated;
				FString Error;
				const bool bGenerated = FHouseLayoutGenerator::Generate(S, L, Error);
				if (!TestTrue(FString::Printf(TEXT("Pattern %d difficulty %d seed %d: %s"), static_cast<int32>(Pattern), static_cast<int32>(Difficulty), Seed, *Error), bGenerated)) return false;
				const int32 ExpectedDepth = 2 + static_cast<int32>(Difficulty);
				TestEqual(TEXT("Preset meets exact route depth"), L.Navigation.MaxDoorDepth, ExpectedDepth);
				TestEqual(TEXT("Requested room count includes hallways"), L.Rooms.Num(), S.RoomCount);
				TestEqual(TEXT("Main route includes entrance plus every doorway"), L.Navigation.MainRoute.Num(), ExpectedDepth + 1);
				TestEqual(TEXT("Deepest room is a bedroom"), L.Rooms[L.Navigation.DeepestRoom].RoomType, EHouseRoomType::Bedroom);
				TestEqual(TEXT("Tree plans do not contain shortcuts"), L.Navigation.LoopCount, 0);
				TestTrue(TEXT("Every room reachable"), !L.Navigation.RoomDoorDepths.Contains(INDEX_NONE));
				TestFalse(TEXT("Report has a readable summary"), L.Navigation.Summary.IsEmpty());
				for (int32 Step = 1; Step < L.Navigation.MainRoute.Num(); ++Step)
				{
					const int32 A = L.Navigation.MainRoute[Step - 1], B = L.Navigation.MainRoute[Step];
					TestTrue(TEXT("Every reported route step crosses an actual door"), L.Walls.ContainsByPredicate([&](const FHouseWall& W)
					{ return W.Opening == EHouseOpening::Door && ((W.RoomA == A && W.RoomB == B) || (W.RoomA == B && W.RoomB == A)); }));
				}
				for (int32 ID = 0; ID < L.Rooms.Num(); ++ID)
				{
					int32 Degree = 0;
					for (const FHouseWall& W : L.Walls)
						if (W.Opening == EHouseOpening::Door && W.RoomB != INDEX_NONE && (W.RoomA == ID || W.RoomB == ID)) ++Degree;
					const EHouseRoomType Type = L.Rooms[ID].RoomType;
					if (Type == EHouseRoomType::Bedroom || Type == EHouseRoomType::Bathroom || Type == EHouseRoomType::Kitchen)
						TestEqual(TEXT("Private rooms and kitchen never carry through traffic"), Degree, 1);
					if (Type == EHouseRoomType::Hallway) TestTrue(TEXT("Hallway has entry/continuation plus multiple destinations"), Degree >= 3);
					if (Type == EHouseRoomType::Hallway)
					{
						int32 Served = 0;
						for (const FHouseWall& W : L.Walls)
						{
							if (W.Opening != EHouseOpening::Door || W.RoomB == INDEX_NONE || (W.RoomA != ID && W.RoomB != ID)) continue;
							const EHouseRoomType Other = L.Rooms[W.RoomA == ID ? W.RoomB : W.RoomA].RoomType;
							Served += Other == EHouseRoomType::Bedroom || Other == EHouseRoomType::Bathroom;
						}
						TestTrue(TEXT("Every hallway directly serves at least two private rooms"), Served >= 2);
					}
					if (Type == EHouseRoomType::LivingRoom) TestEqual(TEXT("Living room has kitchen and hallway access"), Degree, 2);
					for (const FHouseWall& W : L.Walls)
					{
						if (W.Opening != EHouseOpening::Door || W.RoomB == INDEX_NONE || (W.RoomA != ID && W.RoomB != ID)) continue;
						const EHouseRoomType Other = L.Rooms[W.RoomA == ID ? W.RoomB : W.RoomA].RoomType;
						if (Type == EHouseRoomType::Bedroom || Type == EHouseRoomType::Bathroom)
							TestEqual(TEXT("Private rooms open onto hallways"), Other, EHouseRoomType::Hallway);
						if (Type == EHouseRoomType::Kitchen)
							TestEqual(TEXT("Kitchen opens onto living room"), Other, EHouseRoomType::LivingRoom);
					}
				}
				if (Pattern == EHouseLayoutPattern::CentralHub) TestEqual(TEXT("Hallway hub retains direct side-room access"), L.Navigation.DeadEndRooms.Num(), S.RoomCount - ExpectedDepth);
				if (!TestTrue(TEXT("Same seed repeats"), FHouseLayoutGenerator::Generate(S, Repeated, Error))) return false;
				TestEqual(TEXT("Same plan and seed are deterministic"), PlanSignature(Repeated), PlanSignature(L));
				TestEqual(TEXT("Navigation report repeats"), Repeated.Navigation.Summary, L.Navigation.Summary);
				Signatures.Add(PlanSignature(L));
				S.WindowChance = 0;
				S.bGenerateFurniture = false;
				S.ExtraConnectionChance = 1;
				if (!TestTrue(TEXT("Decorations can be disabled"), FHouseLayoutGenerator::Generate(S, Repeated, Error))) return false;
				TestEqual(TEXT("Decoration and legacy extra-door settings cannot change planned topology"), PlanSignature(Repeated), PlanSignature(L));
			}
	TestTrue(TEXT("Seeds vary geometry within plans"), Signatures.Num() > 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseNavigationConstraintsTest, "Prototype.House.NavigationConstraints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseNavigationConstraintsTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings S;
	S.Navigation.Pattern = EHouseLayoutPattern::DeepBranches;
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Hard;
	FHouseLayout L;
	FString Error;
	TestFalse(TEXT("Small default plan cannot silently become hard"), FHouseLayoutGenerator::Generate(S, L, Error));
	TestFalse(TEXT("Infeasible request gives a reason"), Error.IsEmpty());
	TestTrue(TEXT("No partial plan returned"), L.Rooms.IsEmpty());
	S.RoomCount = 12;
	S.WidthCells = S.DepthCells = 24;
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Custom;
	S.Navigation.CustomRouteDepth = 4;
	S.Navigation.CustomMaxDeadEnds = 8;
	for (int32 Seed = 0; Seed < 20; ++Seed)
	{
		S.Seed = Seed;
		if (!TestTrue(TEXT("Custom route and dead-end budget fit"), FHouseLayoutGenerator::Generate(S, L, Error))) return false;
		TestEqual(TEXT("Exact custom route depth"), L.Navigation.MaxDoorDepth, 4);
		TestEqual(TEXT("Custom dead-end limit is respected"), L.Navigation.DeadEndRooms.Num(), 8);
	}
	FHouseLayout Broken = L;
	for (FHouseRoom& R : Broken.Rooms)
	{
		if (R.RoomType == EHouseRoomType::Kitchen) R.RoomType = EHouseRoomType::Bathroom;
		else if (R.RoomType == EHouseRoomType::Bathroom) R.RoomType = EHouseRoomType::Kitchen;
	}
	TestFalse(TEXT("Swapping kitchen and bathroom violates adjacency despite unchanged graph metrics"), FHouseNavigation::ValidatePlan(S, Broken, Error));
	Broken = L;
	for (FHouseRoom& R : Broken.Rooms)
		if (R.RoomType == EHouseRoomType::Hallway) { R.RoomType = EHouseRoomType::Bedroom; break; }
	TestFalse(TEXT("Bedroom cannot replace a required hallway"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	Broken = L;
	for (FHouseWall& W : Broken.Walls)
		if (W.RoomB != INDEX_NONE && W.Opening == EHouseOpening::None) { W.Opening = EHouseOpening::Door; break; }
	TestFalse(TEXT("Unplanned shortcut rejected"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	S.Navigation.CustomMaxDeadEnds = 2;
	TestFalse(TEXT("Unsatisfiable private-room leaf budget rejected"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.Navigation.CustomMaxDeadEnds = 31;
	S.Navigation.CustomRouteDepth = 1;
	TestFalse(TEXT("Private rooms require a hallway between them and entrance"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.Navigation.CustomRouteDepth = 13;
	TestFalse(TEXT("Unbounded route depth rejected"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.Navigation.CustomRouteDepth = 3;
	S.WidthCells = 4;
	TestFalse(TEXT("Footprint needs three usable bands"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Easy;
	S.RoomCount = 5;
	S.WidthCells = 9;
	S.DepthCells = 12;
	TestTrue(TEXT("Minimum five-room residential layout fits"), FHouseLayoutGenerator::Generate(S, L, Error));
	S.RoomCount = 4;
	TestFalse(TEXT("Four rooms cannot include all residential roles"), FHouseLayoutGenerator::Generate(S, L, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseNavigationMetricsTest, "Prototype.House.NavigationMetrics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseNavigationMetricsTest::RunTest(const FString& Parameters)
{
	FHouseLayout L;
	L.Rooms.SetNum(5);
	auto Door = [&](int32 A, int32 B)
	{
		FHouseWall W;
		W.RoomA = A; W.RoomB = B; W.Opening = EHouseOpening::Door;
		L.Walls.Add(W);
	};
	Door(0, INDEX_NONE);
	Door(0, 1); Door(1, 2); Door(1, 3); Door(3, 4);
	FHouseNavigationReport R = FHouseNavigation::Measure(L);
	TestEqual(TEXT("Known tree depth"), R.MaxDoorDepth, 3);
	TestEqual(TEXT("Known tree dead ends exclude entrance"), R.DeadEndRooms.Num(), 2);
	TestEqual(TEXT("Known tree branch count"), R.JunctionCount, 1);
	TestEqual(TEXT("Known tree has no cycles"), R.LoopCount, 0);
	TestEqual(TEXT("Mean includes all rooms, entrance at zero"), R.AverageDoorDepth, 1.6f);
	Door(0, 4);
	R = FHouseNavigation::Measure(L);
	TestEqual(TEXT("Shortcut reduces shortest-door depth"), R.MaxDoorDepth, 2);
	TestEqual(TEXT("Independent cycle count"), R.LoopCount, 1);
	TestEqual(TEXT("Loop removes one dead end"), R.DeadEndRooms.Num(), 1);
	return true;
}
#endif
