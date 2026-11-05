#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseFurnishing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseFurnitureRecoveryTest, "Prototype.House.FurnitureRearrangement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseFurnitureRecoveryTest::RunTest(const FString&)
{
	int32 Improvements = 0;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FHouseGenerationSettings S;
		S.Seed = Seed; S.WidthCells = S.DepthCells = 5 + Seed % 3; S.RoomCount = 1;
		S.FurnitureRules.Reset();
		for (EHouseFurnitureType Type : {EHouseFurnitureType::Sofa,EHouseFurnitureType::CoffeeTable,EHouseFurnitureType::Wardrobe,EHouseFurnitureType::Desk,EHouseFurnitureType::Chair})
		{
			FHouseFurnitureRule F; F.Type = Type; F.AllowedRooms.Add(EHouseRoomType::LivingRoom);
			F.Size = FVector(160,80,80); F.FrontClearance = 80;
			S.FurnitureRules.Add(F);
		}
		FHouseLayout Original, Recovered, Repeat;
		FString Error;
		if (!TestTrue(TEXT("Original arrangement generates"),FHouseLayoutGenerator::Generate(S,Original,Error))) { AddError(Error); return false; }
		S.FurnitureArrangementAttempts = 4;
		if (!TestTrue(TEXT("Rearranged house generates"),FHouseLayoutGenerator::Generate(S,Recovered,Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Rearrangement never loses optional furniture when requirements are equal"),Recovered.Furniture.Num() >= Original.Furniture.Num());
		TestTrue(TEXT("Every arrangement stays bounded"),Recovered.FurnitureArrangementsTried >= 1 && Recovered.FurnitureArrangementsTried <= 4);
		TestTrue(TEXT("All body, door, access and walking checks pass"),FHouseLayoutGenerator::Validate(S,Recovered,Error));
		TestTrue(TEXT("Same settings repeat"),FHouseLayoutGenerator::Generate(S,Repeat,Error));
		TestTrue(TEXT("Results and diagnostic counts reproduce exactly"),FHouseLayout::StaticStruct()->CompareScriptStruct(&Recovered,&Repeat,0));
		Improvements += Recovered.Furniture.Num() > Original.Furniture.Num();
	}
	AddInfo(FString::Printf(TEXT("Rearrangement increased furniture count in %d of 64 crowded fixtures."),Improvements));
	TestTrue(TEXT("Bounded retries recover real missed placements"),Improvements > 0);
	FHouseGenerationSettings S; FHouseLayout L; FString Error;
	S.FurnitureArrangementAttempts = 0;
	TestFalse(TEXT("Zero attempts rejected"),FHouseLayoutGenerator::Generate(S,L,Error));
	S.FurnitureArrangementAttempts = 9;
	TestFalse(TEXT("Unbounded search rejected"),FHouseLayoutGenerator::Generate(S,L,Error));
	S.FurnitureArrangementAttempts = 8;
	FHouseRoomRule Required;
	Required.RuleName = TEXT("Required sofa");
	Required.MatchTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Room.Type.LivingRoom")));
	Required.RequiredFurniture.Add(EHouseFurnitureType::Sofa);
	S.RoomRules.Add(Required);
	for (auto& F : S.FurnitureRules) if (F.Type == EHouseFurnitureType::Sofa) F.Size = FVector(5000,5000,100);
	TestFalse(TEXT("Rearrangement cannot evade impossible required furniture"),FHouseLayoutGenerator::Generate(S,L,Error));
	TestTrue(TEXT("Failed generation exposes no partial rooms or furniture"),L.Rooms.IsEmpty() && L.Furniture.IsEmpty());
	return true;
}
#endif
