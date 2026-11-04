#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseRoomRules.h"
#include "Gameplay/Procedural/HouseFurnishing.h"

namespace RoomRulesTests
{
FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name)); }
FHouseRoomRule Rule(const TCHAR* Name, const TCHAR* Match)
{
	FHouseRoomRule R;
	R.RuleName = FName(Name); R.MatchTags.AddTag(Tag(Match));
	return R;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRoomTagsTest, "Prototype.House.RoomRules.TagsAndCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRoomTagsTest::RunTest(const FString&)
{
	using namespace RoomRulesTests;
	for (const EHouseLayoutPattern Pattern : {EHouseLayoutPattern::LegacyRandom, EHouseLayoutPattern::CentralHub, EHouseLayoutPattern::DeepBranches})
		for (int32 Seed : {-20, 0, 234, 12345})
		{
			FHouseGenerationSettings S;
			S.Seed = Seed; S.WidthCells = S.DepthCells = 24; S.RoomCount = 8; S.Navigation.Pattern = Pattern;
			FHouseLayout Base, Tagged, Repeat;
			FString Error;
			if (!TestTrue(TEXT("Unmodified house generates"), FHouseLayoutGenerator::Generate(S, Base, Error))) { AddError(Error); return false; }
			FHouseRoomTagAssignment A;
			A.MatchTags.AddTag(Tag(TEXT("Room.Type"))); // Parent tags match child types.
			A.AdditionalTags.AddTag(Tag(TEXT("Room.Label.Loot")));
			S.RoomTagAssignments.Add(A);
			A.MatchTags.Reset(); A.MatchTags.AddTag(Tag(TEXT("Room.Role.Entrance")));
			A.AdditionalTags.Reset(); A.AdditionalTags.AddTag(Tag(TEXT("Room.Label.Safe")));
			S.RoomTagAssignments.Add(A);
			S.RoomRules.Add(Rule(TEXT("Keep safe"), TEXT("Room.Label.Safe")));
			S.RoomRules.Last().bAllowZombieSpawns = false;
			if (!TestTrue(TEXT("Tag rules generate"), FHouseLayoutGenerator::Generate(S, Tagged, Error))) { AddError(Error); return false; }
			FHouseRoomRules::Refresh(S, Base);
			TestTrue(TEXT("Adding metadata does not change geometry, windows, furnishing or seed draws"), FHouseLayout::StaticStruct()->CompareScriptStruct(&Base, &Tagged, 0));
			TestTrue(TEXT("Repeated generation succeeds"), FHouseLayoutGenerator::Generate(S, Repeat, Error));
			TestTrue(TEXT("All layout and tag metadata repeats"), FHouseLayout::StaticStruct()->CompareScriptStruct(&Tagged, &Repeat, 0));
			int32 Entrances = 0;
			for (const auto& Room : Tagged.Rooms)
			{
				TestTrue(TEXT("Parent selector adds label to every room"), Room.Tags.HasTagExact(Tag(TEXT("Room.Label.Loot"))));
				if (Room.Tags.HasTagExact(Tag(TEXT("Room.Role.Entrance"))))
				{
					++Entrances;
					TestTrue(TEXT("Entrance has safe label"), Room.Tags.HasTagExact(Tag(TEXT("Room.Label.Safe"))));
					TestFalse(TEXT("Entrance forbids initial zombies"), Room.bAllowZombieSpawns);
				}
			}
			TestEqual(TEXT("Exactly one entrance tag"), Entrances, 1);
			// An overlapping allow cannot override a deny; selection is all-match, not any-match.
			S.RoomRules.Add(Rule(TEXT("Deny loot everywhere"), TEXT("Room.Label.Loot")));
			S.RoomRules.Last().bAllowItemSpawns = false;
			S.RoomRules.Add(Rule(TEXT("Allow loot everywhere"), TEXT("Room")));
			S.RoomRules.Add(Rule(TEXT("Impossible conjunction"), TEXT("Room.Type.Kitchen")));
			S.RoomRules.Last().MatchTags.AddTag(Tag(TEXT("Room.Type.Bathroom")));
			S.RoomRules.Last().bAllowZombieSpawns = false;
			FHouseRoomRules::Refresh(S, Tagged);
			for (const auto& Room : Tagged.Rooms)
			{
				TestFalse(TEXT("Deny wins over allow"), Room.bAllowItemSpawns);
				if (!Room.Tags.HasTag(Tag(TEXT("Room.Role.Entrance")))) TestTrue(TEXT("Conjunction does not match either type alone"), Room.bAllowZombieSpawns);
			}
			S.RoomRules.Swap(0, 3);
			Repeat = Tagged; FHouseRoomRules::Refresh(S, Repeat);
			TestTrue(TEXT("Rule order does not change tags or permissions"), FHouseLayout::StaticStruct()->CompareScriptStruct(&Tagged, &Repeat, 0));
			Tagged.Rooms[0].bAllowItemSpawns = true;
			TestFalse(TEXT("Stale permissions fail validation"), FHouseLayoutGenerator::Validate(S, Tagged, Error));
			S.RoomRules.Reset(); S.RoomTagAssignments.Reset();
			for (auto& Room : Base.Rooms) Room.Tags.Reset();
			TestTrue(TEXT("Old no-rule layouts need no saved tag metadata"), FHouseLayoutGenerator::Validate(S, Base, Error));
		}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRoomRuleSettingsTest, "Prototype.House.RoomRules.Settings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRoomRuleSettingsTest::RunTest(const FString&)
{
	using namespace RoomRulesTests;
	FHouseGenerationSettings S;
	FString Error;
	FHouseRoomTagAssignment A;
	A.RoomIndex = 1; A.AdditionalTags.AddTag(Tag(TEXT("Room.Label.Safe")));
	S.RoomTagAssignments.Add(A);
	FHouseLayout L;
	if (!TestTrue(TEXT("Specific room label generates"), FHouseLayoutGenerator::Generate(S, L, Error))) { AddError(Error); return false; }
	for (int32 I=0; I<L.Rooms.Num(); ++I) TestEqual(TEXT("Only indexed room gets label"), L.Rooms[I].Tags.HasTag(Tag(TEXT("Room.Label.Safe"))), I==1);
	S.RoomTagAssignments[0].RoomIndex = S.RoomCount;
	TestFalse(TEXT("Out-of-range index rejected"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomTagAssignments[0] = A;
	S.RoomTagAssignments[0].AdditionalTags.AddTag(Tag(TEXT("Room.Role.Entrance")));
	TestFalse(TEXT("Labels cannot forge automatic entrance role"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomTagAssignments[0] = A;
	S.RoomTagAssignments[0].MatchTags.AddTag(Tag(TEXT("Room.Label.Loot")));
	TestFalse(TEXT("Assignments cannot depend on other labels"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomTagAssignments.Reset();
	S.RoomRules.AddDefaulted();
	TestFalse(TEXT("Accidental empty selector rejected"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomRules[0] = Rule(TEXT("Safe"), TEXT("Room"));
	const FHouseRoomRule Duplicate = S.RoomRules[0];
	S.RoomRules.Add(Duplicate);
	TestFalse(TEXT("Duplicate rule names rejected"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomRules.SetNum(1);
	S.RoomRules[0].RequiredFurniture.Add(EHouseFurnitureType::Count);
	TestFalse(TEXT("Unknown furniture rejected"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	S.RoomRules[0] = Rule(TEXT("Require office"), TEXT("Room.Purpose.Office"));
	S.RoomRules[0].bRequireMatchingRoom = true;
	FHouseGenerationSettings Effective; int32 Attempts;
	TestFalse(TEXT("Missing required room fails bounded generation"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error));
	TestTrue(TEXT("No partial layout after missing room"), L.Rooms.IsEmpty() && L.Furniture.IsEmpty());
	TestTrue(TEXT("Failure identifies rule"), Error.Contains(TEXT("Require office")));
	TestEqual(TEXT("Bounded attempts"), Attempts, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRequiredFurnitureTest, "Prototype.House.RoomRules.RequiredFurniture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRequiredFurnitureTest::RunTest(const FString&)
{
	using namespace RoomRulesTests;
	FHouseGenerationSettings S;
	S.WidthCells = S.DepthCells = 16;
	S.RoomCount = 1;
	S.RoomRules.Add(Rule(TEXT("Living room sofa"), TEXT("Room.Type.LivingRoom")));
	S.RoomRules[0].RequiredFurniture.Add(EHouseFurnitureType::Sofa);
	S.RoomRules[0].bRequireMatchingRoom = true;
	// Put the sofa last in the catalogue; explicit requirements must still place it first.
	const int32 SofaIndex = S.FurnitureRules.IndexOfByPredicate([](const FHouseFurnitureRule& F) { return F.Type==EHouseFurnitureType::Sofa; });
	S.FurnitureRules.Swap(SofaIndex, S.FurnitureRules.Num()-1);
	FHouseLayout L;
	FString Error;
	if (!TestTrue(TEXT("Required sofa fits"), FHouseLayoutGenerator::Generate(S, L, Error))) { AddError(Error); return false; }
	if (TestTrue(TEXT("Furniture placed"), !L.Furniture.IsEmpty())) TestEqual(TEXT("Required piece placed ahead of optional pieces"), L.Furniture[0].Type, EHouseFurnitureType::Sofa);
	TestTrue(TEXT("Required sofa and all access routes validate"), FHouseLayoutGenerator::Validate(S, L, Error));
	FHouseLayout Broken = L;
	Broken.Furniture.RemoveAll([](const FHouseFurniturePlacement& F) { return F.Type==EHouseFurnitureType::Sofa; });
	TestFalse(TEXT("Removing required furniture invalidates layout"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	const FHouseGenerationSettings Good = S;
	S.bGenerateFurniture = false;
	TestFalse(TEXT("Furniture toggle cannot bypass requirement"), FHouseLayoutGenerator::Generate(S, L, Error));
	TestTrue(TEXT("Failure exposes no partial layout"), L.Rooms.IsEmpty());
	S = Good;
	S.FurnitureRules.Last().AllowedRooms = {EHouseRoomType::Kitchen};
	TestFalse(TEXT("Required type cannot bypass catalogue eligibility"), FHouseLayoutGenerator::Generate(S, L, Error));
	S = Good; S.FurnitureRules.Last().Size = FVector(3000,3000,100);
	TestFalse(TEXT("Oversized required sofa fails instead of overlapping geometry"), FHouseLayoutGenerator::Generate(S, L, Error));
	TestTrue(TEXT("Error names furniture rule and type"), Error.Contains(TEXT("Living room sofa")) && Error.Contains(TEXT("Sofa")));
	// Test the density override on a valid catalogue item in a household room.
	S = Good; S.Household.Profile = EHouseholdProfile::Couple;
	FHouseRoom R; R.RoomType = EHouseRoomType::LivingRoom; R.Density = EHouseFurnishingDensity::Sparse;
	R.Tags.AddTag(Tag(TEXT("Room.Type.LivingRoom")));
	S.FurnitureRules.Last().MinimumDensity = EHouseFurnishingDensity::Full;
	S.FurnitureRules.Last().bEssential = false;
	TestTrue(TEXT("Explicit requirement overrides sparse density"), FHouseRoomRules::WantsFurniture(S, R, 0, S.FurnitureRules.Num()-1));
	R.bUnfurnished = true;
	TestFalse(TEXT("Requirement does not override explicit unfurnished room"), FHouseRoomRules::WantsFurniture(S, R, 0, S.FurnitureRules.Num()-1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRoomConnectionsTest, "Prototype.House.RoomRules.Connections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRoomConnectionsTest::RunTest(const FString&)
{
	using namespace RoomRulesTests;
	FHouseGenerationSettings S;
	S.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
	S.WidthCells = S.DepthCells = 24; S.RoomCount = 8;
	S.RoomRules.Add(Rule(TEXT("Private access"), TEXT("Room.Type.Bedroom")));
	S.RoomRules[0].AllowedNeighborTags.AddTag(Tag(TEXT("Room.Type.Hallway")));
	S.RoomRules[0].bRequireMatchingRoom = true;
	FHouseLayout L; FString Error;
	if (!TestTrue(TEXT("Existing residential access satisfies hallway rule"), FHouseLayoutGenerator::Generate(S, L, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Neighbor constraint checks actual doors"), FHouseRoomRules::ValidateConnections(S, L, Error));
	// A connection must satisfy restrictions at both ends, regardless of Wall.RoomA ordering.
	S.RoomRules.Add(Rule(TEXT("Hallway refuses bedrooms"), TEXT("Room.Type.Hallway")));
	S.RoomRules.Last().AllowedNeighborTags.AddTag(Tag(TEXT("Room.Type.LivingRoom")));
	TestFalse(TEXT("Reciprocal restriction rejects existing connection"), FHouseRoomRules::ValidateConnections(S, L, Error));
	for (auto& W : L.Walls) if (W.RoomB != INDEX_NONE) Swap(W.RoomA, W.RoomB);
	TestFalse(TEXT("Swapping wall endpoints cannot bypass restriction"), FHouseRoomRules::ValidateConnections(S, L, Error));
	FHouseGenerationSettings Effective; int32 Attempts;
	TestFalse(TEXT("Contradictory graph fails bounded generation"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, L, Attempts, Error));
	TestTrue(TEXT("Graph failure clears layout and names offending rule"), L.Rooms.IsEmpty() && Error.Contains(TEXT("Hallway refuses bedrooms")));
	S.RoomRules.SetNum(1);
	S.RoomRules[0].AllowedNeighborTags.Reset();
	S.RoomRules[0].AllowedNeighborTags.AddTag(Tag(TEXT("Room.Label.Safe")));
	FHouseRoomTagAssignment A;
	A.MatchTags.AddTag(Tag(TEXT("Room.Type.Hallway"))); A.AdditionalTags.AddTag(Tag(TEXT("Room.Label.Safe")));
	S.RoomTagAssignments.Add(A);
	TestTrue(TEXT("Connections may target custom labels"), FHouseLayoutGenerator::Generate(S, L, Error));
	return true;
}
#endif
