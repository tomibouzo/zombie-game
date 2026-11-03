#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/HouseFurnishing.h"
#include "Components/TextRenderComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
	FString DescribeLayout(const FHouseLayout& Layout)
	{
		FString Result;
		for (const FHouseRoom& Room : Layout.Rooms)
			Result += FString::Printf(TEXT("R%d,%d,%d,%d,%d;"), Room.Min.X, Room.Min.Y, Room.Max.X, Room.Max.Y, static_cast<int32>(Room.RoomType));
		for (const FHouseWall& Wall : Layout.Walls)
		{
			Result += FString::Printf(TEXT("W%d,%d,%d,%d,%d,%d,%d;"), Wall.Start.X, Wall.Start.Y,
				Wall.End.X, Wall.End.Y, Wall.RoomA, Wall.RoomB, static_cast<int32>(Wall.Opening));
			for (const FHouseWindow& Window : Wall.Windows)
				Result += FString::Printf(TEXT("O%.9g,%.9g,%.9g,%.9g;"), Window.Offset, Window.Width, Window.Height, Window.SillHeight);
		}
		for (const FHouseFurniturePlacement& Item : Layout.Furniture)
			Result += FString::Printf(TEXT("F%d,%d,%d,%.17g,%.17g,%d;"), Item.RoomIndex, Item.RuleIndex, static_cast<int32>(Item.Type), Item.Center.X, Item.Center.Y, Item.QuarterTurns);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseSeedTest, "Prototype.House.LayoutSeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseSeedTest::RunTest(const FString& Parameters)
{
	TSet<FString> UniqueLayouts;
	TSet<int32> FurnitureTypes;
	TSet<int32> WindowWidths;
	int32 MultipleWindowWalls = 0, DoorWindowWalls = 0, FurnishedCases = 0;
	for (int32 Variant = 0; Variant < 5; ++Variant)
	{
		FHouseGenerationSettings Settings;
		Settings.RoomCount = Variant == 0 ? 1 : Variant == 1 ? 4 : Variant == 2 ? 5 : Variant == 3 ? 8 : 16;
		Settings.WidthCells = Variant == 3 ? 18 : 12;
		Settings.DepthCells = Variant == 3 ? 16 : Variant == 4 ? 12 : 10;
		Settings.ExtraConnectionChance = Variant == 2 ? 1.0f : 0.0f;
		for (int32 Seed = -125; Seed < 125; ++Seed)
		{
			Settings.Seed = Seed;
			FHouseLayout First, Second;
			FString Error;
			const FString Context = FString::Printf(TEXT("Variant %d seed %d"), Variant, Seed);
			if (!TestTrue(Context + TEXT(" generates: ") + Error, FHouseLayoutGenerator::Generate(Settings, First, Error))) return false;
			TestTrue(Context + TEXT(" validates"), FHouseLayoutGenerator::Validate(Settings, First, Error));
			TestTrue(Context + TEXT(" repeats"), FHouseLayoutGenerator::Generate(Settings, Second, Error));
			const FString Signature = DescribeLayout(First);
			TestEqual(Context + TEXT(" is deterministic"), DescribeLayout(Second), Signature);
			UniqueLayouts.Add(Signature);
			if (!First.Furniture.IsEmpty()) ++FurnishedCases;
			for (const FHouseFurniturePlacement& Item : First.Furniture) FurnitureTypes.Add(static_cast<int32>(Item.Type));
			TMap<int32, FHouseWindow> RoomProfiles;
			for (const FHouseWall& Wall : First.Walls)
			{
				if (Wall.Windows.Num() > 1) ++MultipleWindowWalls;
				if (Wall.Opening == EHouseOpening::Door && !Wall.Windows.IsEmpty()) ++DoorWindowWalls;
				for (const FHouseWindow& Window : Wall.Windows)
				{
					WindowWidths.Add(FMath::RoundToInt(Window.Width));
					if (const FHouseWindow* Profile = RoomProfiles.Find(Wall.RoomA))
					{
						TestEqual(TEXT("Room windows share width across walls"), Window.Width, Profile->Width);
						TestEqual(TEXT("Room windows share height across walls"), Window.Height, Profile->Height);
						TestEqual(TEXT("Room windows share sill across walls"), Window.SillHeight, Profile->SillHeight);
					}
					else RoomProfiles.Add(Wall.RoomA, Window);
				}
				if (Wall.Opening != EHouseOpening::Door && Wall.Windows.Num() > 1)
				{
					const float Length = (Wall.End.X - Wall.Start.X + Wall.End.Y - Wall.Start.Y) * Settings.CellSize;
					TestTrue(TEXT("Window group is centred on wall"), FMath::IsNearlyEqual(Wall.Windows[0].Offset + Wall.Windows.Last().Offset, Length, 0.01f));
					for (int32 I = 2; I < Wall.Windows.Num(); ++I)
						TestTrue(TEXT("Window centres are evenly spaced"), FMath::IsNearlyEqual(Wall.Windows[I].Offset - Wall.Windows[I-1].Offset,
							Wall.Windows[1].Offset - Wall.Windows[0].Offset, 0.01f));
				}
			}
		}
	}
	TestTrue(TEXT("Furniture is present across seeds"), FurnishedCases > 1000);
	TestEqual(TEXT("Every legacy furniture type can be placed"), FurnitureTypes.Num(), 8);
	TestTrue(TEXT("Windows vary in width"), WindowWidths.Num() > 20);
	TestTrue(TEXT("Some walls have multiple windows"), MultipleWindowWalls > 0);
	TestTrue(TEXT("Entrance walls can also have windows"), DoorWindowWalls > 0);
	TestTrue(TEXT("Seeds produce varied layouts"), UniqueLayouts.Num() > 300);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseValidationTest, "Prototype.House.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseValidationTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings Settings;
	FHouseLayout Layout;
	FString Error;
	if (!TestTrue(TEXT("Default layout"), FHouseLayoutGenerator::Generate(Settings, Layout, Error))) return false;
	FHouseLayout Broken = Layout;
	Broken.Rooms[1] = Broken.Rooms[0];
	TestFalse(TEXT("Overlapping rooms rejected"), FHouseLayoutGenerator::Validate(Settings, Broken, Error));
	Broken = Layout;
	const FHouseWall Duplicate = Broken.Walls[0];
	Broken.Walls.Add(Duplicate);
	TestFalse(TEXT("Duplicate wall rejected"), FHouseLayoutGenerator::Validate(Settings, Broken, Error));
	Broken = Layout;
	for (FHouseWall& Wall : Broken.Walls) if (Wall.RoomB != INDEX_NONE) Wall.Opening = EHouseOpening::None;
	TestFalse(TEXT("Disconnected rooms rejected"), FHouseLayoutGenerator::Validate(Settings, Broken, Error));
	Broken = Layout;
	for (FHouseWall& Wall : Broken.Walls) if (Wall.RoomB == INDEX_NONE) Wall.Opening = EHouseOpening::None;
	TestFalse(TEXT("Missing entrance rejected"), FHouseLayoutGenerator::Validate(Settings, Broken, Error));
	Settings.RoomCount = 32;
	TestFalse(TEXT("Impossible room count rejected"), FHouseLayoutGenerator::Generate(Settings, Broken, Error));
	TestTrue(TEXT("Failure gives a diagnostic"), !Error.IsEmpty());
	TestTrue(TEXT("Failure leaves no partial layout"), Broken.Rooms.IsEmpty() && Broken.Walls.IsEmpty());
	Settings = FHouseGenerationSettings();
	Settings.DoorWidth = 99.0f;
	TestFalse(TEXT("Narrow door rejected"), FHouseLayoutGenerator::ValidateSettings(Settings, Error));
	Settings = FHouseGenerationSettings();
	Settings.WindowHeight = Settings.WallHeight;
	TestFalse(TEXT("Window above ceiling rejected"), FHouseLayoutGenerator::ValidateSettings(Settings, Error));
	Settings = FHouseGenerationSettings();
	Settings.WidthCells = MAX_int32;
	TestFalse(TEXT("Unbounded footprint rejected"), FHouseLayoutGenerator::ValidateSettings(Settings, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseGeometryTest, "Prototype.House.GeometryAndCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseGeometryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AHouseGenerator* House = World->SpawnActor<AHouseGenerator>();
	AHouseGenerator* Neighbour = World->SpawnActor<AHouseGenerator>(FVector(10000, 0, 0), FRotator::ZeroRotator);
	Neighbour->GenerateHouse();
	const int32 NeighbourWalls = Neighbour->Walls->GetInstanceCount();
	// Include a rotated/translated actor: all emitted geometry must use local coordinates.
	House->SetActorLocationAndRotation(FVector(300, -600, 50), FRotator(0, 37, 0));
	for (int32 Seed = 0; Seed < 42; ++Seed)
	{
		if (Seed >= 24)
		{
			House->Settings.RoomCount = 12;
			House->Settings.Household.Profile = static_cast<EHouseholdProfile>(1 + 2 * ((Seed - 24) / 6));
			House->Settings.Household.Density = EHouseFurnishingDensity::Full;
			House->Settings.WidthCells = House->Settings.DepthCells = 24;
			House->Settings.Navigation.Pattern = Seed % 2 ? EHouseLayoutPattern::CentralHub : EHouseLayoutPattern::DeepBranches;
			House->Settings.Navigation.Difficulty = static_cast<EHouseNavigationDifficulty>((Seed - 24) / 2 % 3);
			House->Settings.RoomCount = House->Settings.Navigation.Difficulty == EHouseNavigationDifficulty::Easy ? 7 : House->Settings.Navigation.Difficulty == EHouseNavigationDifficulty::Normal ? 8 : 11;
		}
		if (Seed == 24) House->FurnitureMeshes.SetNum(8); // Simulate a pre-household saved component lookup.
		House->Settings.Seed = Seed;
		House->GenerateHouse();
		TestTrue(TEXT("Generation succeeds"), House->LastError.IsEmpty());
		TestEqual(TEXT("One floor slab per room"), House->Floors->GetInstanceCount(), House->Settings.RoomCount);
		TestEqual(TEXT("One roof slab"), House->Roof->GetInstanceCount(), 1);
		int32 FurnitureInstances = 0;
		for (UInstancedStaticMeshComponent* Component : House->FurnitureMeshes) FurnitureInstances += Component->GetInstanceCount();
		TestEqual(TEXT("Furniture geometry matches placements"), FurnitureInstances, House->Layout.Furniture.Num());
		TArray<UTextRenderComponent*> Labels;
		House->GetComponents(Labels);
		TestEqual(TEXT("Room and furniture labels do not accumulate"), Labels.Num(), House->Layout.Rooms.Num() + House->Layout.Furniture.Num());
		const FTransform Transform = House->GetActorTransform();
		for (const FHouseWall& Wall : House->Layout.Walls)
		{
			const FVector Center((Wall.Start.X + Wall.End.X) * House->Settings.CellSize * 0.5f,
				(Wall.Start.Y + Wall.End.Y) * House->Settings.CellSize * 0.5f, 100.0f);
			const FVector Normal = Wall.Start.X == Wall.End.X ? FVector::ForwardVector : FVector::RightVector;
			FHitResult Hit;
			const FVector Start = Transform.TransformPosition(Center - Normal * 85.0f);
			const FVector End = Transform.TransformPosition(Center + Normal * 85.0f);
			const bool Blocked = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(34.0f, 96.0f));
			if (Wall.Opening == EHouseOpening::Door)
				TestFalse(FString::Printf(TEXT("Seed %d doorway rooms %d/%d [%d,%d]-[%d,%d] passes standing capsule (hit %s at %s)"),
					Seed, Wall.RoomA, Wall.RoomB, Wall.Start.X, Wall.Start.Y, Wall.End.X, Wall.End.Y,
					*GetNameSafe(Hit.GetComponent()), *Transform.InverseTransformPosition(Hit.ImpactPoint).ToString()), Blocked);
			else
				TestTrue(TEXT("Solid walls and window sills block standing capsule"), Blocked);
		}
		for (const FHouseWall& Wall : House->Layout.Walls)
		{
			const bool AlongX = Wall.Start.Y == Wall.End.Y;
			const FVector Direction = AlongX ? FVector::ForwardVector : FVector::RightVector;
			const FVector Normal = AlongX ? FVector::RightVector : FVector::ForwardVector;
			for (const FHouseWindow& Window : Wall.Windows)
			{
				const FVector Center = FVector(Wall.Start.X * House->Settings.CellSize, Wall.Start.Y * House->Settings.CellSize,
					Window.SillHeight + Window.Height * 0.5f) + Direction * Window.Offset;
				FHitResult Hit;
				TestFalse(TEXT("Every variable window is a real opening"), World->LineTraceSingleByChannel(Hit,
					Transform.TransformPosition(Center - Normal * 40), Transform.TransformPosition(Center + Normal * 40), ECC_Visibility));
			}
		}
		for (int32 RoomIndex = 0; RoomIndex < House->Layout.Rooms.Num(); ++RoomIndex)
		{
			TArray<TArray<FVector2D>> Paths;
			TestTrue(TEXT("Furniture leaves routes to all doors and item fronts"), FHouseFurnishing::FindWalkPaths(House->Settings, House->Layout, RoomIndex, Paths));
			for (const TArray<FVector2D>& Path : Paths)
				for (int32 Step = 1; Step < Path.Num(); ++Step)
				{
					FHitResult Hit;
					const FVector Start = Transform.TransformPosition(FVector(Path[Step - 1].X, Path[Step - 1].Y, 100));
					const FVector End = Transform.TransformPosition(FVector(Path[Step].X, Path[Step].Y, 100));
					TestFalse(FString::Printf(TEXT("Seed %d furniture walking route passes standing capsule"), Seed),
						World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34, 96)));
				}
		}
		for (const FHouseRoom& Room : House->Layout.Rooms)
		{
			const FVector Center((Room.Min.X + Room.Max.X) * House->Settings.CellSize * 0.5f,
				(Room.Min.Y + Room.Max.Y) * House->Settings.CellSize * 0.5f, 0.0f);
			FHitResult Hit;
			TestTrue(TEXT("Floor supports pawn"), World->LineTraceSingleByChannel(Hit,
				Transform.TransformPosition(Center + FVector(0, 0, 50)),
				Transform.TransformPosition(Center - FVector(0, 0, 50)), ECC_Pawn));
		}
	}
	House->Settings.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
	House->Settings.Navigation.Difficulty = EHouseNavigationDifficulty::Normal;
	House->Settings.RoomCount = 5;
	House->GenerateHouse();
	TestTrue(TEXT("Generate button repairs preset/count mismatch"), House->LastError.IsEmpty());
	TestEqual(TEXT("Requested room count remains editable input"), House->Settings.RoomCount, 5);
	TestEqual(TEXT("Generated room count recorded separately"), House->GeneratedSettings.RoomCount, 8);
	TestEqual(TEXT("Corrected room count matches geometry"), House->Floors->GetInstanceCount(), 8);
	TestTrue(TEXT("Correction explained in summary"), House->GenerationSummary.Contains(TEXT("Used 8 rooms (requested 5)")));
	const int32 WallCount = House->Walls->GetInstanceCount();
	const FString Before = DescribeLayout(House->Layout);
	House->GenerateHouse();
	TestEqual(TEXT("Regeneration does not accumulate instances"), House->Walls->GetInstanceCount(), WallCount);
	TestEqual(TEXT("Regeneration preserves layout"), DescribeLayout(House->Layout), Before);
	const FString SummaryBefore = House->NavigationSummary;
	AddExpectedError(TEXT("Existing house was preserved"), EAutomationExpectedErrorFlags::Contains, 1);
	House->Settings.RoomCount = 0;
	House->GenerateHouse();
	TestFalse(TEXT("Invalid request reports an error"), House->LastError.IsEmpty());
	TestEqual(TEXT("Invalid request preserves geometry"), House->Walls->GetInstanceCount(), WallCount);
	TestEqual(TEXT("Invalid request preserves layout"), DescribeLayout(House->Layout), Before);
	TestEqual(TEXT("Invalid request preserves navigation report"), House->NavigationSummary, SummaryBefore);
	House->SetRoofHidden(true);
	TestFalse(TEXT("Roof can be hidden"), House->Roof->IsVisible());
	House->ClearHouse();
	TestEqual(TEXT("Walls clear"), House->Walls->GetInstanceCount(), 0);
	TestEqual(TEXT("Floors clear"), House->Floors->GetInstanceCount(), 0);
	TestEqual(TEXT("Roof clears"), House->Roof->GetInstanceCount(), 0);
	TestTrue(TEXT("Layout clears"), House->Layout.Rooms.IsEmpty());
	TestTrue(TEXT("Navigation report clears"), House->NavigationSummary.IsEmpty());
	TestEqual(TEXT("Other builder is untouched"), Neighbour->Walls->GetInstanceCount(), NeighbourWalls);
	for (UInstancedStaticMeshComponent* Component : House->FurnitureMeshes)
		TestEqual(TEXT("Furniture instances clear"), Component->GetInstanceCount(), 0);
	TArray<UTextRenderComponent*> ClearedLabels;
	House->GetComponents(ClearedLabels);
	TestEqual(TEXT("Generated labels clear"), ClearedLabels.Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseFurnishingValidationTest, "Prototype.House.FurnishingValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseFurnishingValidationTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings S;
	FHouseLayout L;
	FString Error;
	if (!TestTrue(TEXT("Furnished house generates"), FHouseLayoutGenerator::Generate(S, L, Error))) return false;
	if (!TestTrue(TEXT("Default seed has furniture"), !L.Furniture.IsEmpty())) return false;
	FHouseLayout Broken = L;
	Broken.Furniture[0].Center = FVector2D(-100, -100);
	TestFalse(TEXT("Out-of-room furniture rejected"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	Broken = L;
	const FHouseFurniturePlacement Duplicate = Broken.Furniture[0];
	Broken.Furniture.Add(Duplicate);
	TestFalse(TEXT("Overlapping furniture rejected"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	Broken = L;
	Broken.Furniture[0].Type = EHouseFurnitureType::Count;
	TestFalse(TEXT("Wrong furniture type rejected"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	Broken = L;
	for (FHouseWall& Wall : Broken.Walls)
	{
		if (Wall.Windows.IsEmpty()) continue;
		Wall.Windows[0].Offset = 0;
		break;
	}
	TestFalse(TEXT("Out-of-bounds window rejected"), FHouseLayoutGenerator::Validate(S, Broken, Error));
	S.bGenerateFurniture = false;
	TestTrue(TEXT("Furniture can be disabled"), FHouseLayoutGenerator::Generate(S, Broken, Error));
	TestTrue(TEXT("Disabling furniture leaves no pieces"), Broken.Furniture.IsEmpty());
	TestEqual(TEXT("Furniture toggle does not change rooms"), Broken.Rooms.Num(), L.Rooms.Num());
	for (int32 I = 0; I < L.Rooms.Num(); ++I)
	{
		TestEqual(TEXT("Room bounds stable"), Broken.Rooms[I].Min, L.Rooms[I].Min);
		TestEqual(TEXT("Room types stable"), Broken.Rooms[I].RoomType, L.Rooms[I].RoomType);
	}
	S.WindowChance = 0;
	TestTrue(TEXT("Windows can be disabled"), FHouseLayoutGenerator::Generate(S, Broken, Error));
	for (const FHouseWall& Wall : Broken.Walls) TestTrue(TEXT("Zero window chance produces no windows"), Wall.Windows.IsEmpty());
	S.WalkwayWidth = S.DoorWidth + 1;
	TestFalse(TEXT("Passage wider than doorway rejected"), FHouseLayoutGenerator::ValidateSettings(S, Error));
	return true;
}
#endif
