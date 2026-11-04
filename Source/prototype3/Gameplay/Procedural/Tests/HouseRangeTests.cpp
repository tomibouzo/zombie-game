#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Gameplay/Procedural/HouseLayout.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	FString RangeLayoutSignature(const FHouseLayout& Layout)
	{
		FString Signature;
		FHouseLayout::StaticStruct()->ExportText(Signature, &Layout, nullptr, nullptr, PPF_None, nullptr);
		return Signature;
	}

	FIntPoint MeasuredFootprint(const FHouseLayout& Layout)
	{
		FIntPoint Bounds = FIntPoint::ZeroValue;
		for (const FHouseRoom& Room : Layout.Rooms)
		{
			Bounds.X = FMath::Max(Bounds.X, Room.Max.X);
			Bounds.Y = FMath::Max(Bounds.Y, Room.Max.Y);
		}
		return Bounds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRangeSettingsTest, "Prototype.House.GenerationRangeSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRangeSettingsTest::RunTest(const FString& Parameters)
{
	FHouseGenerationSettings Request, Resolved, Repeat;
	Request.bUseGenerationRanges = true;
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(20, 39);
	Request.GenerationRanges.DepthCells = FHouseIntegerRange(12, 16);
	Request.GenerationRanges.RoomCount = FHouseIntegerRange(4, 8);
	FString Error;
	TSet<int32> Widths, Depths, Counts;
	for (int32 Seed = -64; Seed < 64; ++Seed)
	{
		Request.Seed = Seed;
		if (!TestTrue(TEXT("Valid inclusive ranges resolve"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error))) return false;
		TestTrue(TEXT("Actual width choice stays within bounds"), Resolved.WidthCells >= 20 && Resolved.WidthCells <= 39);
		TestTrue(TEXT("Actual depth choice stays within bounds"), Resolved.DepthCells >= 12 && Resolved.DepthCells <= 16);
		TestTrue(TEXT("Exact count choice stays within bounds"), Resolved.RoomCount >= 4 && Resolved.RoomCount <= 8);
		Widths.Add(Resolved.WidthCells); Depths.Add(Resolved.DepthCells); Counts.Add(Resolved.RoomCount);
		FHouseGenerationSettings ChangedDecorations = Request;
		ChangedDecorations.bGenerateFurniture = false;
		ChangedDecorations.WindowChance = 0;
		TestTrue(TEXT("Decoration choices do not affect range resolution"), FHouseLayoutGenerator::ResolveRanges(ChangedDecorations, Repeat, Error));
		TestEqual(TEXT("Width repeats"), Repeat.WidthCells, Resolved.WidthCells);
		TestEqual(TEXT("Depth repeats"), Repeat.DepthCells, Resolved.DepthCells);
		TestEqual(TEXT("Count repeats"), Repeat.RoomCount, Resolved.RoomCount);
		Resolved.Seed += 123;
		TestTrue(TEXT("Resolved retry settings do not reroll"), FHouseLayoutGenerator::ResolveRanges(Resolved, Repeat, Error));
		TestEqual(TEXT("Retry preserves width"), Repeat.WidthCells, Resolved.WidthCells);
		TestEqual(TEXT("Retry preserves depth"), Repeat.DepthCells, Resolved.DepthCells);
		TestEqual(TEXT("Retry preserves count"), Repeat.RoomCount, Resolved.RoomCount);
	}
	TestTrue(TEXT("Both width endpoints are attainable"), Widths.Contains(20) && Widths.Contains(39));
	TestTrue(TEXT("Both depth endpoints are attainable"), Depths.Contains(12) && Depths.Contains(16));
	TestTrue(TEXT("Both count endpoints are attainable"), Counts.Contains(4) && Counts.Contains(8));
	TestEqual(TEXT("Existing fixed controls are not mutated"), Request.WidthCells, 12);
	TestTrue(TEXT("Input remains a range request"), Request.bUseGenerationRanges);
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(14, 14);
	Request.GenerationRanges.DepthCells = FHouseIntegerRange(11, 11);
	Request.GenerationRanges.RoomCount = FHouseIntegerRange(6, 6);
	TestTrue(TEXT("Equal bounds resolve fixed dimensions"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error));
	TestEqual(TEXT("Fixed width"), Resolved.WidthCells, 14);
	TestEqual(TEXT("Fixed depth"), Resolved.DepthCells, 11);
	TestEqual(TEXT("Fixed room count"), Resolved.RoomCount, 6);
	TestTrue(TEXT("Resolved settings enforce an exact footprint"), Resolved.bRequireExactFootprint);
	TestFalse(TEXT("Resolved settings disable footprint growth"), Resolved.bAllowLargerFootprint);
	// The saved effective settings must retain the exact-footprint contract across reloads.
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Save(Writer, false);
	FHouseGenerationSettings::StaticStruct()->SerializeItem(Save, &Resolved, nullptr);
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Load(Reader, false);
	FHouseGenerationSettings Reloaded;
	FHouseGenerationSettings::StaticStruct()->SerializeItem(Load, &Reloaded, nullptr);
	TestFalse(TEXT("Resolved settings reload without rerolling"), Reloaded.bUseGenerationRanges);
	TestTrue(TEXT("Exact footprint requirement survives serialization"), Reloaded.bRequireExactFootprint);
	TestEqual(TEXT("Saved resolved width"), Reloaded.WidthCells, Resolved.WidthCells);
	TestEqual(TEXT("Saved resolved count"), Reloaded.RoomCount, Resolved.RoomCount);
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(15, 14);
	TestFalse(TEXT("Inverted bounds rejected"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error));
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(2, 14);
	TestFalse(TEXT("Too-small width rejected"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error));
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(14, 65);
	TestFalse(TEXT("Oversized width rejected"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error));
	Request.GenerationRanges.WidthCells = FHouseIntegerRange(14, 14);
	Request.GenerationRanges.RoomCount = FHouseIntegerRange(1, 33);
	TestFalse(TEXT("Room-count limit remains 32"), FHouseLayoutGenerator::ResolveRanges(Request, Resolved, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRangeLayoutTest, "Prototype.House.GenerationRangeLayouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRangeLayoutTest::RunTest(const FString& Parameters)
{
	FString Error;
	FHouseGenerationSettings S, Effective, Again;
	S.bUseGenerationRanges = true;
	S.GenerationRanges.WidthCells = FHouseIntegerRange(12, 18);
	S.GenerationRanges.DepthCells = FHouseIntegerRange(10, 16);
	S.GenerationRanges.RoomCount = FHouseIntegerRange(4, 8);
	FHouseLayout Layout, Repeat;
	int32 Attempts = 0, AgainAttempts = 0;
	for (int32 Seed = -12; Seed < 12; ++Seed)
	{
		S.Seed = Seed;
		if (!TestTrue(*FString::Printf(TEXT("Ranged furnished partition seed %d: %s"), Seed, *Error),
			FHouseLayoutGenerator::GenerateWithRetries(S, Effective, Layout, Attempts, Error))) return false;
		TestEqual(TEXT("Generated rooms fill the resolved footprint"), MeasuredFootprint(Layout), FIntPoint(Effective.WidthCells, Effective.DepthCells));
		TestEqual(TEXT("Generated count is exact"), Layout.Rooms.Num(), Effective.RoomCount);
		TestTrue(TEXT("Resolved layout validates"), FHouseLayoutGenerator::Validate(Effective, Layout, Error));
		if (!TestTrue(TEXT("Repeated request succeeds"), FHouseLayoutGenerator::GenerateWithRetries(S, Again, Repeat, AgainAttempts, Error))) return false;
		TestEqual(TEXT("Repeated full layout is identical"), RangeLayoutSignature(Repeat), RangeLayoutSignature(Layout));
		TestEqual(TEXT("Retry sequence repeats"), Attempts, AgainAttempts);
		TestTrue(TEXT("Saved effective settings regenerate directly"), FHouseLayoutGenerator::Generate(Effective, Repeat, Error));
		TestEqual(TEXT("Effective settings reproduce the full result"), RangeLayoutSignature(Repeat), RangeLayoutSignature(Layout));
	}
	// A known feasible small residential plan. Exact dimensions may not be replaced
	// by the old compact envelope, and planned room-size limits still apply.
	S.GenerationRanges.WidthCells = FHouseIntegerRange(8, 8);
	S.GenerationRanges.DepthCells = FHouseIntegerRange(9, 9);
	S.GenerationRanges.RoomCount = FHouseIntegerRange(5, 5);
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Easy;
	S.bGenerateFurniture = false;
	for (EHouseLayoutPattern Pattern : {EHouseLayoutPattern::CentralHub, EHouseLayoutPattern::DeepBranches})
		for (int32 Seed = 0; Seed < 8; ++Seed)
		{
			S.Navigation.Pattern = Pattern; S.Seed = Seed;
			if (!TestTrue(*FString::Printf(TEXT("Exact planned footprint pattern %d seed %d: %s"), static_cast<int32>(Pattern), Seed, *Error),
				FHouseLayoutGenerator::GenerateWithRetries(S, Effective, Layout, Attempts, Error))) return false;
			TestEqual(TEXT("Actual planned footprint equals fixed request"), MeasuredFootprint(Layout), FIntPoint(8, 9));
			TestEqual(TEXT("Hallways count toward exact room count"), Layout.Rooms.Num(), 5);
			TestTrue(TEXT("Room clear-size constraints still hold"), FHouseSizing::Validate(Effective, Layout, Error));
			TestTrue(TEXT("Original ranged request validates generated layout"), FHouseLayoutGenerator::Validate(S, Layout, Error));
			FHouseLayout Broken = Layout;
			--Broken.FootprintCells.X;
			TestFalse(TEXT("Smaller-than-requested footprint rejected"), FHouseLayoutGenerator::Validate(Effective, Broken, Error));
		}
	// Multiple hallway sections must divide the exact total depth instead of taking
	// the old compact minimum-plus-two-rows shortcut.
	for (EHouseLayoutPattern Pattern : {EHouseLayoutPattern::CentralHub, EHouseLayoutPattern::DeepBranches})
		for (int32 Difficulty = 1; Difficulty <= 2; ++Difficulty)
		{
			S.Navigation.Pattern = Pattern;
			S.Navigation.Difficulty = static_cast<EHouseNavigationDifficulty>(Difficulty);
			S.Seed = 17;
			const int32 Depth = Difficulty == 1 ? 11 : 15;
			S.GenerationRanges.WidthCells = FHouseIntegerRange(9, 9);
			S.GenerationRanges.DepthCells = FHouseIntegerRange(Depth, Depth);
			S.GenerationRanges.RoomCount = FHouseIntegerRange(5 + 3 * Difficulty, 5 + 3 * Difficulty);
			if (!TestTrue(TEXT("Exact multi-section plan fits"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, Layout, Attempts, Error)))
			{
				AddError(Error);
				return false;
			}
			TestEqual(TEXT("Sections exactly fill the chosen depth"), MeasuredFootprint(Layout), FIntPoint(9, Depth));
			TestEqual(TEXT("Requested doorway depth still holds"), Layout.Navigation.MaxDoorDepth, Difficulty + 2);
			TestTrue(TEXT("Multi-section room dimensions remain valid"), FHouseSizing::Validate(Effective, Layout, Error));
		}
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Hard;
	S.GenerationRanges.RoomCount = FHouseIntegerRange(5, 5);
	TestFalse(TEXT("Range count is never raised to fit a preset"), FHouseLayoutGenerator::GenerateWithRetries(S, Effective, Layout, Attempts, Error));
	TestEqual(TEXT("Invalid resolved preset fails before retrying"), Attempts, 0);
	TestEqual(TEXT("Requested count retained on failure"), Effective.RoomCount, 5);
	TestTrue(TEXT("No partial layout survives invalid preset"), Layout.Rooms.IsEmpty());
	S.Navigation.Difficulty = EHouseNavigationDifficulty::Easy;
	S.GenerationRanges.WidthCells = FHouseIntegerRange(24, 24);
	S.GenerationRanges.DepthCells = FHouseIntegerRange(24, 24);
	TestFalse(TEXT("Oversized exact house cannot silently shrink into its room-size limits"),
		FHouseLayoutGenerator::GenerateWithRetries(S, Effective, Layout, Attempts, Error));
	TestEqual(TEXT("Unpackable house exhausts bounded retries"), Attempts, 16);
	TestTrue(TEXT("No partial house after bounded failure"), Layout.Rooms.IsEmpty());
	TestEqual(TEXT("Width never changes during retry"), Effective.WidthCells, 24);
	TestEqual(TEXT("Depth never changes during retry"), Effective.DepthCells, 24);
	return true;
}
#endif
