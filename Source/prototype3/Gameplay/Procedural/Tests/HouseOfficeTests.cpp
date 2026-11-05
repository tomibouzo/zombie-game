#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseOfficePlan.h"
#include "Gameplay/Procedural/HouseNavigation.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#if WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Engine/World.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseOfficeTest,"Prototype.House.OfficeSuite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseOfficeTest::RunTest(const FString&)
{
#if WITH_EDITOR
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World=UWorld::CreateWorld(EWorldType::Editor,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	auto* Builder=World->SpawnActor<AHouseGenerator>();
	Builder->Settings.Seed=456; Builder->ApplySmallOfficePreset();
	TestEqual(TEXT("Builder preset retains seed"),Builder->Settings.Seed,456);
	TestEqual(TEXT("Builder preset selects office"),Builder->Settings.Navigation.Pattern,EHouseLayoutPattern::ConnectedOffice);
	auto* Encounter=World->SpawnActor<AHouseEncounter>();
	Encounter->HouseSettings.Seed=789; Encounter->ZombieCount=2; Encounter->ItemCount=5;
	const auto Selection=Encounter->ItemSelection;
	Encounter->ApplySmallOfficePreset();
	TestEqual(TEXT("Encounter preset retains seed"),Encounter->HouseSettings.Seed,789);
	TestEqual(TEXT("Encounter preset selects office"),Encounter->HouseSettings.Navigation.Pattern,EHouseLayoutPattern::ConnectedOffice);
	TestEqual(TEXT("Preset preserves zombie count"),Encounter->ZombieCount,2);
	TestEqual(TEXT("Preset preserves item count"),Encounter->ItemCount,5);
	TestTrue(TEXT("Preset preserves loot selection"),Encounter->ItemSelection==Selection);
	World->DestroyWorld(false);
#endif
	TSet<FString> Shapes; int32 Looped=0;
	for (int32 Seed=-8; Seed<16; ++Seed)
	{
		auto S=FHouseOfficePlan::Preset(Seed);
		FHouseGenerationSettings Effective,RepeatedSettings; FHouseLayout L,Repeat; FString Error; int32 Attempts;
		if (!TestTrue(TEXT("Office preset generates"),FHouseLayoutGenerator::GenerateWithRetries(S,Effective,L,Attempts,Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Office geometry, sizes, furniture, graph and tags validate"),FHouseLayoutGenerator::Validate(Effective,L,Error));
		TestEqual(TEXT("Preset preserves authored seed"),S.Seed,Seed);
		TestEqual(TEXT("Exact office footprint"),L.FootprintCells,FIntPoint(10,8));
		TestEqual(TEXT("Exact office room count"),L.Rooms.Num(),6);
		TestTrue(TEXT("All rooms reachable"),!L.Navigation.RoomDoorDepths.Contains(INDEX_NONE));
		TestEqual(TEXT("Entrance reception"),L.Rooms[L.Navigation.EntranceRoom].RoomType,EHouseRoomType::Reception);
		TestFalse(TEXT("Reception protected from initial zombies"),L.Rooms[L.Navigation.EntranceRoom].bAllowZombieSpawns);
		Looped+=L.Navigation.LoopCount>0;
		FString Shape;
		for (int32 I=0; I<L.Rooms.Num(); ++I)
		{
			const auto& R=L.Rooms[I]; Shape+=FString::Printf(TEXT("%d,%d,%d,%d;"),R.Min.X,R.Min.Y,R.Max.X,R.Max.Y);
			TestFalse(TEXT("Office preset has no residential bedrooms or halls"),R.RoomType==EHouseRoomType::Bedroom || R.RoomType==EHouseRoomType::Hallway);
			if (R.RoomType==EHouseRoomType::Office)
			{
				TestTrue(TEXT("Office type tag"),R.Tags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Room.Type.Office"))));
				TestTrue(TEXT("Every office has required desk"),L.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& F) { return F.RoomIndex==I && F.Type==EHouseFurnitureType::Desk; }));
			}
		}
		Shapes.Add(Shape);
		TestTrue(TEXT("Repeated office succeeds"),FHouseLayoutGenerator::GenerateWithRetries(S,RepeatedSettings,Repeat,Attempts,Error));
		TestTrue(TEXT("Same settings and seed reproduce complete office"),FHouseLayout::StaticStruct()->CompareScriptStruct(&L,&Repeat,0));
		// Exercise reflected serialization of new enum values, rules and derived room data.
		TArray<uint8> Bytes;
		{ FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer,false); FHouseLayout::StaticStruct()->SerializeItem(Archive,&L,nullptr); }
		FHouseLayout Loaded;
		{ FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader,true); FHouseLayout::StaticStruct()->SerializeItem(Archive,&Loaded,nullptr); }
		TestTrue(TEXT("Office metadata round trips"),FHouseLayout::StaticStruct()->CompareScriptStruct(&L,&Loaded,0));
	}
	TestTrue(TEXT("Seeds vary office partitions"),Shapes.Num()>1);
	TestTrue(TEXT("Optional workroom routes produce loops"),Looped>0);
	AddInfo(FString::Printf(TEXT("24 office seeds: %d distinct partitions, %d with loops."),Shapes.Num(),Looped));
	auto S=FHouseOfficePlan::Preset(12345); FString Error; FHouseLayout L;
	S.bUseGenerationRanges=true; S.GenerationRanges.WidthCells=FHouseIntegerRange(10,10);
	S.GenerationRanges.DepthCells=FHouseIntegerRange(8,8); S.GenerationRanges.RoomCount=FHouseIntegerRange(6,6);
	TestTrue(TEXT("Office supports exact seeded range controls"),FHouseLayoutGenerator::Generate(S,L,Error));
	TestEqual(TEXT("Range mode preserves office footprint"),L.FootprintCells,FIntPoint(10,8));
	TArray<uint8> SettingsBytes;
	{ FMemoryWriter Writer(SettingsBytes); FObjectAndNameAsStringProxyArchive Archive(Writer,false); FHouseGenerationSettings::StaticStruct()->SerializeItem(Archive,&S,nullptr); }
	FHouseGenerationSettings LoadedSettings;
	{ FMemoryReader Reader(SettingsBytes); FObjectAndNameAsStringProxyArchive Archive(Reader,true); FHouseGenerationSettings::StaticStruct()->SerializeItem(Archive,&LoadedSettings,nullptr); }
	TestTrue(TEXT("Office settings and required-desk rule round trip"),FHouseGenerationSettings::StaticStruct()->CompareScriptStruct(&S,&LoadedSettings,0));
	S=FHouseOfficePlan::Preset(12345);
	S.Household.Profile=EHouseholdProfile::Family;
	TestFalse(TEXT("Residential household cannot silently change office program"),FHouseLayoutGenerator::Generate(S,L,Error));
	S=FHouseOfficePlan::Preset(12345); S.RoomCount=3;
	TestFalse(TEXT("Office program requires reception, bathroom, storage and an office"),FHouseLayoutGenerator::Generate(S,L,Error));
	S=FHouseOfficePlan::Preset(12345); S.WidthCells=S.DepthCells=3;
	TestFalse(TEXT("Impossible office footprint fails"),FHouseLayoutGenerator::Generate(S,L,Error));
	TestTrue(TEXT("Failed office leaves no partial layout"),L.Rooms.IsEmpty() && L.Furniture.IsEmpty());
	return true;
}
#endif
