#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseHousehold.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseHouseholdTest, "Prototype.House.HouseholdProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseHouseholdTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings S;
	S.RoomCount = 12;
	S.WidthCells = S.DepthCells = 24;
	TSet<EHouseFurnitureType> Seen;
	FString Error;
	for (int32 Profile = 1; Profile <= 5; ++Profile)
		for (int32 Pattern = 1; Pattern <= 2; ++Pattern)
			for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
				for (int32 Seed = 0; Seed < 5; ++Seed)
				{
					S.Household.Profile = static_cast<EHouseholdProfile>(Profile);
					S.Household.Density = EHouseFurnishingDensity::Full;
					S.Navigation.Pattern = static_cast<EHouseLayoutPattern>(Pattern);
					S.Navigation.Difficulty = static_cast<EHouseNavigationDifficulty>(Difficulty);
					S.RoomCount = Difficulty == 0 ? 7 : Difficulty == 1 ? 8 : 11;
					S.Seed = Seed;
					FHouseLayout L, Repeat;
					const bool bGenerated = FHouseLayoutGenerator::Generate(S, L, Error);
					if (!TestTrue(FString::Printf(TEXT("Profile %d pattern %d difficulty %d seed %d: %s"), Profile, Pattern, Difficulty, Seed, *Error), bGenerated)) return false;
					int32 Primary = 0, Children = 0, Offices = 0;
					for (const FHouseRoom& R : L.Rooms)
					{
						Primary += R.Purpose == EHouseRoomPurpose::PrimaryBedroom;
						Children += R.Purpose == EHouseRoomPurpose::ChildBedroom;
						Offices += R.Purpose == EHouseRoomPurpose::Office;
					}
					for (int32 RoomIndex = 0; RoomIndex < L.Rooms.Num(); ++RoomIndex)
					{
						const FHouseRoom& R = L.Rooms[RoomIndex];
						if (R.Purpose == EHouseRoomPurpose::PrimaryBedroom || R.Purpose == EHouseRoomPurpose::GuestBedroom || R.Purpose == EHouseRoomPurpose::ChildBedroom)
							TestTrue(TEXT("Every furnished bedroom has a bed"), L.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& I)
							{ return I.RoomIndex == RoomIndex && (I.Type == EHouseFurnitureType::Bed || I.Type == EHouseFurnitureType::SingleBed); }));
					}
					TestEqual(TEXT("One primary bedroom"), Primary, 1);
					if (Profile == 3) TestEqual(TEXT("Family gets two children's bedrooms"), Children, 2);
					if (Profile == 5) TestTrue(TEXT("Working couple retains at least two offices"), Offices >= 2);
					TestEqual(TEXT("Household does not change target route depth"), L.Navigation.MaxDoorDepth, Difficulty + 2);
					for (const FHouseFurniturePlacement& Item : L.Furniture)
					{
						Seen.Add(Item.Type);
						const EHouseRoomPurpose Purpose = L.Rooms[Item.RoomIndex].Purpose;
						if (Item.Type == EHouseFurnitureType::Bed) TestEqual(TEXT("Double beds only in primary bedrooms"), Purpose, EHouseRoomPurpose::PrimaryBedroom);
						if (Purpose == EHouseRoomPurpose::Office) TestTrue(TEXT("Offices have office furniture"), Item.Type == EHouseFurnitureType::Desk || Item.Type == EHouseFurnitureType::Chair || Item.Type == EHouseFurnitureType::StorageShelf);
					}
					if (!TestTrue(TEXT("Repeat generation succeeds"), FHouseLayoutGenerator::Generate(S, Repeat, Error))) return false;
					TestEqual(TEXT("Repeat furniture count"), Repeat.Furniture.Num(), L.Furniture.Num());
					for (int32 I = 0; I < L.Furniture.Num(); ++I)
					{
						TestEqual(TEXT("Repeat rule"), Repeat.Furniture[I].RuleIndex, L.Furniture[I].RuleIndex);
						TestEqual(TEXT("Repeat position"), Repeat.Furniture[I].Center, L.Furniture[I].Center);
					}
				}
	TestEqual(TEXT("All blockout types are exercised"), Seen.Num(), static_cast<int32>(EHouseFurnitureType::Count));
	FHouseLayout Settled, Empty;
	S.Household.Profile = EHouseholdProfile::Couple;
	TestTrue(TEXT("Settled baseline"), FHouseLayoutGenerator::Generate(S, Settled, Error));
	for (EHouseOccupancy Old : {EHouseOccupancy::MovingIn, EHouseOccupancy::MovingOut, EHouseOccupancy::Vacant})
	{
		S.Household.Occupancy = Old;
		if (!TestTrue(TEXT("Old occupancy loads as settled"), FHouseLayoutGenerator::Generate(S, Empty, Error))) return false;
		TestEqual(TEXT("Normalized occupancy"), Empty.Household.Occupancy, EHouseOccupancy::Settled);
		TestEqual(TEXT("No occupancy omissions"), Empty.Furniture.Num(), Settled.Furniture.Num());
		for (int32 I = 0; I < Empty.Furniture.Num(); ++I)
			TestEqual(TEXT("Same furniture placement"), Empty.Furniture[I].Center, Settled.Furniture[I].Center);
	}
	S.Household.Occupancy = EHouseOccupancy::Settled;
	S.RoomCount = 5;
	S.WidthCells = S.DepthCells = 12;
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Easy;
	S.Household.Profile = EHouseholdProfile::WorkingFromHomeCouple;
	TestFalse(TEXT("Small house cannot silently omit offices"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	TestTrue(TEXT("Failed generation returns no partial house"), Empty.Rooms.IsEmpty());
	S.RoomCount = 7; S.WidthCells = S.DepthCells = 24;
	S.Household.Profile = EHouseholdProfile::Couple;
	TestTrue(TEXT("Override baseline"), FHouseLayoutGenerator::Generate(S, Settled, Error));
	int32 Guest = INDEX_NONE;
	for (int32 I = 0; I < Settled.Rooms.Num(); ++I)
		if (Settled.Rooms[I].Purpose == EHouseRoomPurpose::GuestBedroom) { Guest = I; break; }
	if (!TestTrue(TEXT("Guest exists"), Guest != INDEX_NONE)) return false;
	FHouseRoomOverride O; O.RoomIndex = Guest; O.Purpose = EHouseRoomPurpose::Office;
	O.bOverrideDensity = true; O.Density = EHouseFurnishingDensity::Sparse;
	S.Household.RoomOverrides.Add(O);
	TestTrue(TEXT("Guest can become an office"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	TestEqual(TEXT("Purpose override applied"), Empty.Rooms[Guest].Purpose, EHouseRoomPurpose::Office);
	TestEqual(TEXT("Density override applied"), Empty.Rooms[Guest].Density, EHouseFurnishingDensity::Sparse);
	S.Household.RoomOverrides[0].bUnfurnished = true;
	TestTrue(TEXT("Specific room can be left empty"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	TestFalse(TEXT("Unfurnished room has no furniture"), Empty.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& I) { return I.RoomIndex == Guest; }));
	S.Household.RoomOverrides[0].RoomIndex = 0;
	TestFalse(TEXT("Cannot replace entrance living room"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	S.Household.RoomOverrides.Reset();
	S.Household.FurnitureStyle = EHouseFurnitureStyle::Traditional;
	S.FurnitureRules[0].AllowedStyles = {EHouseFurnitureStyle::Modern};
	TestFalse(TEXT("Missing matching bed gives a catalogue error"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	TestTrue(TEXT("Missing bed error is actionable"), Error.Contains(TEXT("eligible bed rule")));
	S.FurnitureRules[0].AllowedStyles.Reset();
	S.Household.Density = EHouseFurnishingDensity::Sparse;
	TestTrue(TEXT("Sparse house generates"), FHouseLayoutGenerator::Generate(S, Empty, Error));
	for (const FHouseFurniturePlacement& I : Empty.Furniture)
		TestTrue(TEXT("Sparse includes only essential defaults"), S.FurnitureRules[I.RuleIndex].bEssential);
	return true;
}
#endif
