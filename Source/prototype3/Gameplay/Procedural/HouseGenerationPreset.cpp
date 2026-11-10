#include "Gameplay/Procedural/HouseGenerationPreset.h"

#include "Gameplay/Procedural/HouseOfficePlan.h"
#include "Math/RandomStream.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

FString FHousePresetResult::Describe() const
{
	return FString::Printf(TEXT("%d x %d cells, %d rooms, %d zombies, %d items, extra connection chance %.2f, %s furnishing (size choice %d)"),
		WidthCells, DepthCells, RoomCount, ZombieCount, ItemCount, Request.ExtraConnectionChance,
		*StaticEnum<EHouseFurnishingDensity>()->GetDisplayNameTextByValue(static_cast<int64>(Request.Household.Density)).ToString(), SizeCandidatesTried);
}

int32 UHouseGenerationPreset::Population(float PerRoom, int32 Rooms, int32 Minimum, int32 Maximum)
{
	return FMath::Clamp(FMath::RoundToInt(PerRoom * FMath::Max(0, Rooms)), Minimum, Maximum);
}

bool UHouseGenerationPreset::Validate(FString& OutError) const
{
	auto ValidFloat = [](const FHouseFloatRange& R, float Maximum)
	{ return FMath::IsFinite(R.Min) && FMath::IsFinite(R.Max) && R.Min >= 0 && R.Min <= R.Max && R.Max <= Maximum; };
	if (!ValidFloat(ExtraConnectionChance, 1) || !ValidFloat(ZombiesPerRoom, 32) || !ValidFloat(ItemsPerRoom, 64)
		|| MinFurnishingDensity > MaxFurnishingDensity || MaxFurnishingDensity > EHouseFurnishingDensity::Full
		|| MinZombies < 0 || MinZombies > MaxZombies || MaxZombies > 32 || MinItems < 0 || MinItems > MaxItems || MaxItems > 64)
	{
		OutError = FString::Printf(TEXT("Preset %s needs Min <= Max for every range, connection chance 0..1, 0..32 zombies and 0..64 items."), *GetName());
		return false;
	}
	FHouseGenerationSettings Probe = FixedSettings;
	Probe.bUseGenerationRanges = true;
	Probe.GenerationRanges = Size;
	Probe.ExtraConnectionChance = ExtraConnectionChance.Min;
	if (!FHouseLayoutGenerator::ValidateSettings(Probe, OutError))
	{
		OutError = FString::Printf(TEXT("Preset %s: %s"), *GetName(), *OutError);
		return false;
	}
	return true;
}

bool UHouseGenerationPreset::Resolve(int32 Seed, FHousePresetResult& Out, FString& OutError) const
{
	Out = FHousePresetResult();
	if (!Validate(OutError)) return false;
	FHouseGenerationSettings& Request = Out.Request;
	Request = FixedSettings;
	Request.Seed = Seed;
	Request.bUseGenerationRanges = true;
	Request.bGenerationRangesInitialized = true;
	Request.GenerationRanges = Size;
	// Separate stream: independent of the size draws, layout, furniture and placement streams.
	// Draw order is part of the reproducibility contract; append new draws at the end.
	FRandomStream Random(static_cast<int32>(static_cast<uint32>(Seed) ^ 0x2545F491u));
	Request.ExtraConnectionChance = Random.FRandRange(ExtraConnectionChance.Min, ExtraConnectionChance.Max);
	Request.Household.Density = static_cast<EHouseFurnishingDensity>(Random.RandRange(
		static_cast<int32>(MinFurnishingDensity), static_cast<int32>(MaxFurnishingDensity)));
	const float Zombies = Random.FRandRange(ZombiesPerRoom.Min, ZombiesPerRoom.Max);
	const float Items = Random.FRandRange(ItemsPerRoom.Min, ItemsPerRoom.Max);
	// Width, depth and room count are not independent: each layout pattern only builds certain
	// combinations (offices need area to match room count; residential houses are narrow). Try the
	// combinations inside the ranges in seeded order and keep the first that builds with this seed.
	TArray<FIntVector> Sizes;
	for (int32 W = Size.WidthCells.Min; W <= Size.WidthCells.Max; ++W)
		for (int32 D = Size.DepthCells.Min; D <= Size.DepthCells.Max; ++D)
			for (int32 R = Size.RoomCount.Min; R <= Size.RoomCount.Max; ++R) Sizes.Add(FIntVector(W, D, R));
	for (int32 I = Sizes.Num() - 1; I > 0; --I) Sizes.Swap(I, Random.RandRange(0, I));
	FString LastError;
	for (int32 I = 0; I < FMath::Min(Sizes.Num(), MaxSizeCandidates); ++I)
	{
		const FIntVector Candidate = Sizes[I];
		FHouseGenerationSettings Exact = Request;
		Exact.GenerationRanges.WidthCells = FHouseIntegerRange(Candidate.X, Candidate.X);
		Exact.GenerationRanges.DepthCells = FHouseIntegerRange(Candidate.Y, Candidate.Y);
		Exact.GenerationRanges.RoomCount = FHouseIntegerRange(Candidate.Z, Candidate.Z);
		FHouseGenerationSettings Effective; FHouseLayout Layout; int32 Attempts = 0;
		++Out.SizeCandidatesTried;
		if (!FHouseLayoutGenerator::GenerateWithRetries(Exact, Effective, Layout, Attempts, LastError)) continue;
		Request = Exact;
		Out.WidthCells = Candidate.X;
		Out.DepthCells = Candidate.Y;
		Out.RoomCount = Candidate.Z;
		Out.ZombieCount = Population(Zombies, Candidate.Z - 1, MinZombies, MaxZombies);
		Out.ItemCount = Population(Items, Candidate.Z, MinItems, MaxItems);
		return true;
	}
	OutError = FString::Printf(TEXT("Preset %s: none of %d width/depth/room combinations tried could be built (seed %d). Widen or shift its size ranges. Last reason: %s"),
		*GetName(), Out.SizeCandidatesTried, Seed, *LastError);
	return false;
}

FHouseGenerationSettings UHouseGenerationPreset::MakeCustomSettings(int32 Seed, int32& OutZombies, int32& OutItems) const
{
	FHouseGenerationSettings S = FixedSettings;
	S.Seed = Seed;
	S.bUseGenerationRanges = true;
	S.bGenerationRangesInitialized = true;
	S.GenerationRanges = Size;
	S.ExtraConnectionChance = (ExtraConnectionChance.Min + ExtraConnectionChance.Max) * 0.5f;
	S.Household.Density = static_cast<EHouseFurnishingDensity>((static_cast<int32>(MinFurnishingDensity) + static_cast<int32>(MaxFurnishingDensity)) / 2);
	const int32 Rooms = (Size.RoomCount.Min + Size.RoomCount.Max) / 2;
	OutZombies = Population((ZombiesPerRoom.Min + ZombiesPerRoom.Max) * 0.5f, Rooms - 1, MinZombies, MaxZombies);
	OutItems = Population((ItemsPerRoom.Min + ItemsPerRoom.Max) * 0.5f, Rooms, MinItems, MaxItems);
	return S;
}

namespace HousePresetDefaults
{
void SmallOffice(UHouseGenerationPreset& P)
{
	P.Description = NSLOCTEXT("HousePreset", "SmallOffice", "Small connected office suite: reception, bathroom, storage and offices; workrooms may connect in loops.");
	P.FixedSettings = FHouseOfficePlan::Preset(0);
	P.Size = FHouseGenerationRanges();
	// Nov 9 feasibility probe: offices build when room count matches floor area (about half this box).
	P.Size.WidthCells = FHouseIntegerRange(8, 13);
	P.Size.DepthCells = FHouseIntegerRange(6, 11);
	P.Size.RoomCount = FHouseIntegerRange(5, 8);
	P.ExtraConnectionChance = FHouseFloatRange(0.3f, 0.6f);
	// Density only filters optional household furniture; the office catalogue has none, so keep it fixed.
	P.MinFurnishingDensity = EHouseFurnishingDensity::Standard;
	P.MaxFurnishingDensity = EHouseFurnishingDensity::Standard;
	P.ZombiesPerRoom = FHouseFloatRange(0.5f, 0.8f); P.MinZombies = 2; P.MaxZombies = 5;
	P.ItemsPerRoom = FHouseFloatRange(1.0f, 1.5f); P.MinItems = 4; P.MaxItems = 10;
}

void CompactHouse(UHouseGenerationPreset& P)
{
	P.Description = NSLOCTEXT("HousePreset", "CompactHouse", "Compact house for a couple: living room, kitchen and one hallway serving the private rooms.");
	P.FixedSettings = FHouseGenerationSettings();
	P.FixedSettings.Navigation.Pattern = EHouseLayoutPattern::CentralHub;
	P.FixedSettings.Navigation.Difficulty = EHouseNavigationDifficulty::Easy;
	P.FixedSettings.Household.Profile = EHouseholdProfile::Couple;
	P.FixedSettings.FurnitureArrangementAttempts = 4;
	// Nov 9 feasibility probe: the residential template builds only narrow houses with matching depths;
	// five rooms almost never fit an exact footprint with the default room sizing.
	P.Size.WidthCells = FHouseIntegerRange(8, 11);
	P.Size.DepthCells = FHouseIntegerRange(9, 15);
	P.Size.RoomCount = FHouseIntegerRange(6, 7);
	P.ExtraConnectionChance = FHouseFloatRange(0.2f, 0.2f);
	P.MinFurnishingDensity = EHouseFurnishingDensity::Sparse;
	P.MaxFurnishingDensity = EHouseFurnishingDensity::Full;
	P.ZombiesPerRoom = FHouseFloatRange(0.4f, 0.7f); P.MinZombies = 2; P.MaxZombies = 6;
	P.ItemsPerRoom = FHouseFloatRange(0.8f, 1.2f); P.MinItems = 3; P.MaxItems = 10;
}

void FamilyHouse(UHouseGenerationPreset& P)
{
	P.Description = NSLOCTEXT("HousePreset", "FamilyHouse", "Family house: living room, kitchen and a longer hallway chain with parents' and children's bedrooms.");
	P.FixedSettings = FHouseGenerationSettings();
	P.FixedSettings.Navigation.Pattern = EHouseLayoutPattern::DeepBranches;
	P.FixedSettings.Navigation.Difficulty = EHouseNavigationDifficulty::Normal;
	P.FixedSettings.Household.Profile = EHouseholdProfile::Family;
	P.FixedSettings.Household.ChildrenBedrooms = 2;
	P.FixedSettings.FurnitureArrangementAttempts = 4;
	// Nov 9 feasibility probe: with default room sizing, family houses only build 10..11 cells wide.
	P.Size.WidthCells = FHouseIntegerRange(10, 11);
	P.Size.DepthCells = FHouseIntegerRange(13, 17);
	P.Size.RoomCount = FHouseIntegerRange(9, 11);
	P.ExtraConnectionChance = FHouseFloatRange(0.2f, 0.2f);
	P.MinFurnishingDensity = EHouseFurnishingDensity::Sparse;
	P.MaxFurnishingDensity = EHouseFurnishingDensity::Full;
	// Narrow family houses reliably fit 4 initial zombies outside the entrance room (Nov 9 encounter sweep).
	P.ZombiesPerRoom = FHouseFloatRange(0.4f, 0.7f); P.MinZombies = 3; P.MaxZombies = 4;
	P.ItemsPerRoom = FHouseFloatRange(0.8f, 1.2f); P.MinItems = 5; P.MaxItems = 14;
}

const TArray<FString>& AssetPaths()
{
	static const TArray<FString> Paths = {
		TEXT("/Game/Procedural/Presets/DA_HousePreset_SmallOffice"),
		TEXT("/Game/Procedural/Presets/DA_HousePreset_CompactHouse"),
		TEXT("/Game/Procedural/Presets/DA_HousePreset_FamilyHouse")};
	return Paths;
}

const TArray<FInit>& Initializers()
{
	static const TArray<FInit> Inits = {&SmallOffice, &CompactHouse, &FamilyHouse};
	return Inits;
}

UHouseGenerationPreset* LoadOrDefault(int32 Index, bool* bOutFromAsset)
{
	if (bOutFromAsset) *bOutFromAsset = false;
	if (!AssetPaths().IsValidIndex(Index)) return nullptr;
	const FString& Path = AssetPaths()[Index];
	const FString ObjectPath = Path + TEXT(".") + FPackageName::GetShortName(Path);
	if (FPackageName::DoesPackageExist(Path))
		if (UHouseGenerationPreset* Asset = LoadObject<UHouseGenerationPreset>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (bOutFromAsset) *bOutFromAsset = true;
			return Asset;
		}
	UHouseGenerationPreset* Preset = NewObject<UHouseGenerationPreset>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(),
		UHouseGenerationPreset::StaticClass(), FName(FPackageName::GetShortName(Path) + TEXT("_Default"))));
	Initializers()[Index](*Preset);
	return Preset;
}
}
