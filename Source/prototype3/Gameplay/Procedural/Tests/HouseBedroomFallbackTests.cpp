#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseFurnishing.h"
#include "Gameplay/Procedural/HouseHousehold.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseBedroomFallbackTest, "Prototype.House.BedroomOfficeFallback",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseBedroomFallbackTest::RunTest(const FString& Parameters)
{
 FHouseGenerationSettings S;
 S.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
 S.Household.Profile = EHouseholdProfile::Couple;
 FHouseLayout L;
 FHouseRoom R;
 R.Min = FIntPoint(0, 0); R.Max = FIntPoint(3, 4);
 R.RoomType = EHouseRoomType::Bedroom; R.Purpose = EHouseRoomPurpose::GuestBedroom;
 L.Rooms.Add(R);
 FHouseWall W;
 W.Start = FIntPoint(0, 0); W.End = FIntPoint(3, 0);
 W.RoomA = 0; W.RoomB = INDEX_NONE; W.Opening = EHouseOpening::Door;
 L.Walls.Add(W);
 for (FHouseFurnitureRule& Rule : S.FurnitureRules)
  if (Rule.Type == EHouseFurnitureType::SingleBed || Rule.Type == EHouseFurnitureType::Bed) Rule.Size = FVector(600, 600, 60);
 FString Error;
 const FHouseLayout Original = L;
 if (!TestTrue(TEXT("Spare room can be furnished as an office"), FHouseFurnishing::Generate(S, L, Error))) return false;
 TestEqual(TEXT("Spare becomes office"), L.Rooms[0].Purpose, EHouseRoomPurpose::Office);
 TestTrue(TEXT("Office has a desk"), L.Furniture.ContainsByPredicate([](const FHouseFurniturePlacement& I) { return I.Type == EHouseFurnitureType::Desk; }));
 TestFalse(TEXT("Office has no bedroom furniture"), L.Furniture.ContainsByPredicate([](const FHouseFurniturePlacement& I) { return I.Type == EHouseFurnitureType::Bed || I.Type == EHouseFurnitureType::SingleBed || I.Type == EHouseFurnitureType::Wardrobe; }));
 TestTrue(TEXT("Converted furniture has safe clearance"), FHouseFurnishing::Validate(S, L, Error));
 FHouseLayout Repeat = Original;
 TestTrue(TEXT("Repeat succeeds"), FHouseFurnishing::Generate(S, Repeat, Error));
 TestEqual(TEXT("Same conversion"), Repeat.Rooms[0].Purpose, L.Rooms[0].Purpose);
 TestEqual(TEXT("Same count"), Repeat.Furniture.Num(), L.Furniture.Num());
 for (int32 I = 0; I < L.Furniture.Num(); ++I) TestEqual(TEXT("Same placement"), Repeat.Furniture[I].Center, L.Furniture[I].Center);
 L = Original; L.Rooms[0].Purpose = EHouseRoomPurpose::PrimaryBedroom;
 TestFalse(TEXT("Primary cannot silently disappear"), FHouseFurnishing::Generate(S, L, Error));
 L = Original; L.Rooms[0].Purpose = EHouseRoomPurpose::ChildBedroom;
 TestFalse(TEXT("Child bedroom cannot silently disappear"), FHouseFurnishing::Generate(S, L, Error));
 L = Original; L.Rooms[0].bUnfurnished = true;
 TestTrue(TEXT("Explicitly empty room supported"), FHouseFurnishing::Generate(S, L, Error));
 TestEqual(TEXT("Empty bedroom retains purpose"), L.Rooms[0].Purpose, EHouseRoomPurpose::GuestBedroom);
 TestTrue(TEXT("Empty stays empty"), L.Furniture.IsEmpty());
 L = Original;
 S.RoomSizing.Office.MaxArea = 8;
 TestFalse(TEXT("Fallback respects office area limits"), FHouseFurnishing::Generate(S, L, Error));
 S.RoomSizing.Office.MaxArea = 14;
 for (FHouseFurnitureRule& Rule : S.FurnitureRules)
  if (Rule.Type == EHouseFurnitureType::Desk) Rule.Size = FVector(600, 600, 75);
 L = Original;
 TestFalse(TEXT("Cannot just relabel a room without a fitting desk"), FHouseFurnishing::Generate(S, L, Error));
 return true;
}
#endif
