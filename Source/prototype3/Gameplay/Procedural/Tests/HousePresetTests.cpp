#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerationPreset.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace HousePresetTests
{
bool InRange(int32 Value, const FHouseIntegerRange& R) { return Value >= R.Min && Value <= R.Max; }
bool InRange(float Value, const FHouseFloatRange& R) { return Value >= R.Min - 1e-4f && Value <= R.Max + 1e-4f; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHousePresetResolutionTest, "Prototype.House.Presets.Resolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHousePresetResolutionTest::RunTest(const FString&)
{
	using namespace HousePresetTests;
	for (int32 Index = 0; Index < HousePresetDefaults::Initializers().Num(); ++Index)
	{
		UHouseGenerationPreset* P = NewObject<UHouseGenerationPreset>();
		HousePresetDefaults::Initializers()[Index](*P);
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("Built-in preset %d validates: %s"), Index, *Error), P->Validate(Error))) return false;
		TSet<FIntVector> Sizes; TSet<int32> Zombies, Items;
		for (int32 Seed = -15; Seed < 15; ++Seed)
		{
			FHousePresetResult A, B;
			if (!TestTrue(TEXT("Preset resolves"), P->Resolve(Seed, A, Error) && P->Resolve(Seed, B, Error))) { AddError(Error); return false; }
			TestEqual(TEXT("Same seed reproduces every resolved value"), A.Describe(), B.Describe());
			TestTrue(TEXT("Same seed reproduces the request"), FHouseGenerationSettings::StaticStruct()->CompareScriptStruct(&A.Request, &B.Request, 0));
			TestEqual(TEXT("Request keeps seed"), A.Request.Seed, Seed);
			TestTrue(TEXT("Width within preset"), InRange(A.WidthCells, P->Size.WidthCells));
			TestTrue(TEXT("Depth within preset"), InRange(A.DepthCells, P->Size.DepthCells));
			TestTrue(TEXT("Rooms within preset"), InRange(A.RoomCount, P->Size.RoomCount));
			TestTrue(TEXT("Connection chance within preset"), InRange(A.Request.ExtraConnectionChance, P->ExtraConnectionChance));
			TestTrue(TEXT("Density within preset"), A.Request.Household.Density >= P->MinFurnishingDensity && A.Request.Household.Density <= P->MaxFurnishingDensity);
			TestTrue(TEXT("Zombies within preset limits"), A.ZombieCount >= P->MinZombies && A.ZombieCount <= P->MaxZombies);
			TestTrue(TEXT("Items within preset limits"), A.ItemCount >= P->MinItems && A.ItemCount <= P->MaxItems);
			TestTrue(TEXT("Zombies follow per-room rate"), A.ZombieCount == FMath::Clamp(A.ZombieCount,
				FMath::Clamp(FMath::RoundToInt(P->ZombiesPerRoom.Min * (A.RoomCount - 1)), P->MinZombies, P->MaxZombies),
				FMath::Clamp(FMath::RoundToInt(P->ZombiesPerRoom.Max * (A.RoomCount - 1)), P->MinZombies, P->MaxZombies)));
			TestTrue(TEXT("Preset fixed layout pattern"), A.Request.Navigation.Pattern == P->FixedSettings.Navigation.Pattern);
			Sizes.Add(FIntVector(A.WidthCells, A.DepthCells, A.RoomCount)); Zombies.Add(A.ZombieCount); Items.Add(A.ItemCount);
		}
		TestTrue(TEXT("Seeds vary size and room count"), Sizes.Num() > 5);
		// A tight zombie cap can fix the zombie count (Family House), so require variation in either count.
		TestTrue(TEXT("Seeds vary population"), Zombies.Num() > 1 || Items.Num() > 1);
		int32 CustomZombies = -1, CustomItems = -1;
		const FHouseGenerationSettings Custom = P->MakeCustomSettings(77, CustomZombies, CustomItems);
		TestEqual(TEXT("Custom keeps seed"), Custom.Seed, 77);
		TestTrue(TEXT("Custom starts with preset ranges"), Custom.bUseGenerationRanges && Custom.GenerationRanges.RoomCount.Max == P->Size.RoomCount.Max
			&& Custom.GenerationRanges.WidthCells.Min == P->Size.WidthCells.Min);
		TestTrue(TEXT("Custom population within preset limits"), CustomZombies >= P->MinZombies && CustomZombies <= P->MaxZombies && CustomItems >= P->MinItems && CustomItems <= P->MaxItems);
	}
	UHouseGenerationPreset* Bad = NewObject<UHouseGenerationPreset>();
	HousePresetDefaults::SmallOffice(*Bad);
	Bad->MinZombies = 6; Bad->MaxZombies = 2;
	FString Error; FHousePresetResult Result;
	TestFalse(TEXT("Inverted population limits are rejected"), Bad->Resolve(1, Result, Error));
	HousePresetDefaults::SmallOffice(*Bad);
	Bad->Size.RoomCount = FHouseIntegerRange(20, 20);
	TestFalse(TEXT("Room count outside the office program is rejected"), Bad->Validate(Error));

	// Actor integration: the builder uses the preset and leaves its own controls untouched.
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	UHouseGenerationPreset* Office = NewObject<UHouseGenerationPreset>();
	HousePresetDefaults::SmallOffice(*Office);
	AHouseGenerator* Builder = World->SpawnActor<AHouseGenerator>();
	Builder->Settings.Seed = 4242;
	const FHouseGenerationSettings Before = Builder->Settings;
	Builder->Preset = Office;
	Builder->GenerateHouse();
	TestTrue(TEXT("Builder generates from preset: ") + Builder->LastError, Builder->LastError.IsEmpty());
	FHousePresetResult Expected; Office->Resolve(4242, Expected, Error);
	TestEqual(TEXT("Builder uses preset pattern"), Builder->GeneratedSettings.Navigation.Pattern, EHouseLayoutPattern::ConnectedOffice);
	TestEqual(TEXT("Builder footprint is the seed-resolved size"), Builder->Layout.FootprintCells, FIntPoint(Expected.WidthCells, Expected.DepthCells));
	TestEqual(TEXT("Builder room count is the seed-resolved count"), Builder->Layout.Rooms.Num(), Expected.RoomCount);
	TestTrue(TEXT("Builder controls are unchanged"), FHouseGenerationSettings::StaticStruct()->CompareScriptStruct(&Before, &Builder->Settings, 0));

	// Clearing a preset in Details switches to Custom, starting from that preset's values.
	AHouseEncounter* Encounter = World->SpawnActor<AHouseEncounter>();
	Encounter->HouseSettings.Seed = 99; Encounter->ZombieCount = 15; Encounter->ItemCount = 23;
	Encounter->Preset = Office;
	FProperty* PresetProperty = AHouseEncounter::StaticClass()->FindPropertyByName(TEXT("Preset"));
	Encounter->PreEditChange(PresetProperty);
	Encounter->Preset = nullptr;
	FPropertyChangedEvent Changed(PresetProperty);
	Encounter->PostEditChangeProperty(Changed);
	TestEqual(TEXT("Custom keeps encounter seed"), Encounter->HouseSettings.Seed, 99);
	TestEqual(TEXT("Custom adopts preset pattern"), Encounter->HouseSettings.Navigation.Pattern, EHouseLayoutPattern::ConnectedOffice);
	TestTrue(TEXT("Custom adopts preset population"), Encounter->ZombieCount <= Office->MaxZombies && Encounter->ItemCount <= Office->MaxItems);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHousePresetLayoutSweepTest, "Prototype.House.Presets.LayoutSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHousePresetLayoutSweepTest::RunTest(const FString&)
{
	constexpr int32 Seeds = 200;
	for (int32 Index = 0; Index < HousePresetDefaults::AssetPaths().Num(); ++Index)
	{
		bool bAsset = false;
		UHouseGenerationPreset* P = HousePresetDefaults::LoadOrDefault(Index, &bAsset);
		int32 Failures = 0, TotalAttempts = 0, MaxAttempts = 0;
		TArray<FString> Examples;
		TSet<FString> Shapes;
		const double Start = FPlatformTime::Seconds();
		for (int32 Seed = 0; Seed < Seeds; ++Seed)
		{
			FHousePresetResult R; FString Error;
			if (!P->Resolve(Seed, R, Error)) { AddError(Error); return false; }
			FHouseGenerationSettings Effective; FHouseLayout Layout; int32 Attempts = 0;
			if (!FHouseLayoutGenerator::GenerateWithRetries(R.Request, Effective, Layout, Attempts, Error))
			{
				++Failures;
				if (Examples.Num() < 6) Examples.Add(FString::Printf(TEXT("seed %d (%s): %s"), Seed, *R.Describe(), *Error.Left(160)));
				continue;
			}
			TotalAttempts += Attempts; MaxAttempts = FMath::Max(MaxAttempts, Attempts);
			TestEqual(TEXT("Footprint equals resolved size"), Layout.FootprintCells, FIntPoint(R.WidthCells, R.DepthCells));
			TestEqual(TEXT("Room count equals resolved count"), Layout.Rooms.Num(), R.RoomCount);
			FString Shape;
			for (const FHouseRoom& Room : Layout.Rooms) Shape += FString::Printf(TEXT("%d,%d,%d,%d;"), Room.Min.X, Room.Min.Y, Room.Max.X, Room.Max.Y);
			Shapes.Add(Shape);
		}
		const FString Name = FPackageName::GetShortName(HousePresetDefaults::AssetPaths()[Index]);
		AddInfo(FString::Printf(TEXT("%s (%s): %d/%d layouts generated, %d distinct, mean %.2f / max %d seed attempts, %.2f s total."),
			*Name, bAsset ? TEXT("asset") : TEXT("built-in values"), Seeds - Failures, Seeds, Shapes.Num(),
			Seeds > Failures ? double(TotalAttempts) / (Seeds - Failures) : 0.0, MaxAttempts, FPlatformTime::Seconds() - Start));
		if (Failures)
			AddError(FString::Printf(TEXT("%s: %d/%d seeds failed. Examples: %s"), *Name, Failures, Seeds, *FString::Join(Examples, TEXT(" | "))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHousePresetAssetsTest, "Prototype.House.Presets.CreateAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHousePresetAssetsTest::RunTest(const FString&)
{
	// Explicit opt-in: ordinary test runs never write Content.
	if (!FParse::Param(FCommandLine::Get(), TEXT("CreateHousePresetAssets")))
	{
		AddInfo(TEXT("Preset asset creation skipped. Use -CreateHousePresetAssets in a fresh verification editor."));
		return true;
	}
	for (int32 Index = 0; Index < HousePresetDefaults::AssetPaths().Num(); ++Index)
	{
		const FString& Path = HousePresetDefaults::AssetPaths()[Index];
		if (FPackageName::DoesPackageExist(Path)) { AddInfo(Path + TEXT(" already exists; left unchanged.")); continue; }
		UPackage* Package = CreatePackage(*Path);
		UHouseGenerationPreset* Preset = NewObject<UHouseGenerationPreset>(Package, FName(FPackageName::GetShortName(Path)), RF_Public | RF_Standalone | RF_Transactional);
		HousePresetDefaults::Initializers()[Index](*Preset);
		FString Error;
		if (!TestTrue(TEXT("New preset validates: ") + Error, Preset->Validate(Error))) return false;
		FAssetRegistryModule::AssetCreated(Preset);
		Package->MarkPackageDirty();
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString File = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		if (!TestTrue(TEXT("Preset asset saves: ") + File, UPackage::SavePackage(Package, Preset, *File, Args))) return false;
		AddInfo(TEXT("Created ") + Path);
	}
	for (int32 Index = 0; Index < HousePresetDefaults::AssetPaths().Num(); ++Index)
	{
		bool bAsset = false;
		HousePresetDefaults::LoadOrDefault(Index, &bAsset);
		TestTrue(TEXT("Preset asset loads: ") + HousePresetDefaults::AssetPaths()[Index], bAsset);
	}
	return true;
}
#endif
