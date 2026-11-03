#include "Gameplay/Procedural/HouseLayout.h"
#include "Gameplay/Procedural/HouseHousehold.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseFurnishing.h"
#include "Gameplay/Procedural/HouseNavigation.h"

#include "Math/RandomStream.h"

FHouseGenerationSettings::FHouseGenerationSettings()
{
	auto Rule = [&](EHouseFurnitureType Type, EHouseRoomType Room, FVector Size, bool bCenter = false)
	{
		FHouseFurnitureRule Item;
		Item.Type = Type;
		Item.AllowedRooms.Add(Room);
		Item.Size = Size;
		Item.bPreferCenter = bCenter;
		FurnitureRules.Add(Item);
	};
	Rule(EHouseFurnitureType::Bed, EHouseRoomType::Bedroom, FVector(160, 200, 60));
	Rule(EHouseFurnitureType::Wardrobe, EHouseRoomType::Bedroom, FVector(120, 60, 200));
	Rule(EHouseFurnitureType::Sofa, EHouseRoomType::LivingRoom, FVector(200, 90, 90));
	Rule(EHouseFurnitureType::CoffeeTable, EHouseRoomType::LivingRoom, FVector(100, 60, 45), true);
	Rule(EHouseFurnitureType::KitchenCounter, EHouseRoomType::Kitchen, FVector(180, 65, 90));
	Rule(EHouseFurnitureType::Fridge, EHouseRoomType::Kitchen, FVector(70, 70, 180));
	Rule(EHouseFurnitureType::Sink, EHouseRoomType::Bathroom, FVector(65, 50, 85));
	Rule(EHouseFurnitureType::Toilet, EHouseRoomType::Bathroom, FVector(60, 75, 80));
	FurnitureRules[0].AllowedPurposes = {EHouseRoomPurpose::PrimaryBedroom};
	FurnitureRules[1].AllowedPurposes = {EHouseRoomPurpose::PrimaryBedroom, EHouseRoomPurpose::GuestBedroom, EHouseRoomPurpose::ChildBedroom};
	FurnitureRules[1].bEssential = false;
	FurnitureRules[3].bEssential = false;
	auto HouseholdRule = [&](EHouseFurnitureType Type, FVector Size, TArray<EHouseRoomPurpose> Purposes, bool Essential)
	{
		Rule(Type, EHouseRoomType::Bedroom, Size);
		FHouseFurnitureRule& Added = FurnitureRules.Last();
		Added.AllowedPurposes = MoveTemp(Purposes);
		Added.bHouseholdOnly = true;
		Added.bEssential = Essential;
	};
	HouseholdRule(EHouseFurnitureType::SingleBed, FVector(95, 190, 55), {EHouseRoomPurpose::GuestBedroom, EHouseRoomPurpose::ChildBedroom}, true);
	HouseholdRule(EHouseFurnitureType::Desk, FVector(120, 60, 75), {EHouseRoomPurpose::Office}, true);
	HouseholdRule(EHouseFurnitureType::Chair, FVector(50, 50, 85), {EHouseRoomPurpose::Office}, true);
	HouseholdRule(EHouseFurnitureType::BedsideTable, FVector(45, 40, 50), {EHouseRoomPurpose::PrimaryBedroom, EHouseRoomPurpose::GuestBedroom}, false);
	HouseholdRule(EHouseFurnitureType::ToyBox, FVector(70, 40, 45), {EHouseRoomPurpose::ChildBedroom}, false);
	HouseholdRule(EHouseFurnitureType::StorageShelf, FVector(100, 40, 160), {EHouseRoomPurpose::Storage, EHouseRoomPurpose::Office}, true);
	HouseholdRule(EHouseFurnitureType::Desk, FVector(100, 50, 75), {EHouseRoomPurpose::ChildBedroom}, false);
	FurnitureRules.Last().MinimumDensity = EHouseFurnishingDensity::Full;

}

namespace
{
	void AddWall(TArray<FHouseWall>& Walls, FIntPoint Start, FIntPoint End, int32 A, int32 B = INDEX_NONE)
	{
		FHouseWall Wall;
		Wall.Start = Start;
		Wall.End = End;
		Wall.RoomA = A;
		Wall.RoomB = B;
		Walls.Add(Wall);
	}

	TArray<FHouseWall> BuildBoundaries(const FHouseGenerationSettings& S, const TArray<FHouseRoom>& Rooms)
	{
		TArray<FHouseWall> Walls;
		for (int32 A = 0; A < Rooms.Num(); ++A)
		{
			const FHouseRoom& R = Rooms[A];
			if (R.Min.X == 0) AddWall(Walls, R.Min, FIntPoint(0, R.Max.Y), A);
			if (R.Min.Y == 0) AddWall(Walls, R.Min, FIntPoint(R.Max.X, 0), A);
			if (R.Max.X == S.WidthCells) AddWall(Walls, FIntPoint(R.Max.X, R.Min.Y), R.Max, A);
			if (R.Max.Y == S.DepthCells) AddWall(Walls, FIntPoint(R.Min.X, R.Max.Y), R.Max, A);
			for (int32 B = A + 1; B < Rooms.Num(); ++B)
			{
				const FHouseRoom& Other = Rooms[B];
				const int32 LowY = FMath::Max(R.Min.Y, Other.Min.Y);
				const int32 HighY = FMath::Min(R.Max.Y, Other.Max.Y);
				if (LowY < HighY && (R.Max.X == Other.Min.X || Other.Max.X == R.Min.X))
				{
					const int32 X = R.Max.X == Other.Min.X ? R.Max.X : R.Min.X;
					AddWall(Walls, FIntPoint(X, LowY), FIntPoint(X, HighY), A, B);
				}
				const int32 LowX = FMath::Max(R.Min.X, Other.Min.X);
				const int32 HighX = FMath::Min(R.Max.X, Other.Max.X);
				if (LowX < HighX && (R.Max.Y == Other.Min.Y || Other.Max.Y == R.Min.Y))
				{
					const int32 Y = R.Max.Y == Other.Min.Y ? R.Max.Y : R.Min.Y;
					AddWall(Walls, FIntPoint(LowX, Y), FIntPoint(HighX, Y), A, B);
				}
			}
		}
		return Walls;
	}

	float WallLength(const FHouseWall& Wall, const FHouseGenerationSettings& S)
	{
		return (Wall.End.X - Wall.Start.X + Wall.End.Y - Wall.Start.Y) * S.CellSize;
	}

	bool FitsOpening(const FHouseWall& Wall, float Width, const FHouseGenerationSettings& S)
	{
		// Keep openings clear of perpendicular walls at corners and T junctions.
		return WallLength(Wall, S) >= Width + 2.0f * S.WallThickness;
	}
	void AssignRoomTypes(FHouseLayout& Layout, int32 EntranceRoom)
	{
		for (FHouseRoom& Room : Layout.Rooms) Room.RoomType = EHouseRoomType::Bedroom;
		Layout.Rooms[EntranceRoom].RoomType = EHouseRoomType::LivingRoom;
		int32 Kitchen = INDEX_NONE, BestArea = -1;
		for (const FHouseWall& Wall : Layout.Walls)
		{
			if (Wall.Opening != EHouseOpening::Door || Wall.RoomB == INDEX_NONE) continue;
			const int32 Other = Wall.RoomA == EntranceRoom ? Wall.RoomB : Wall.RoomB == EntranceRoom ? Wall.RoomA : INDEX_NONE;
			if (Other == INDEX_NONE) continue;
			const FIntPoint Size = Layout.Rooms[Other].Max - Layout.Rooms[Other].Min;
			if (Size.X * Size.Y > BestArea) { Kitchen = Other; BestArea = Size.X * Size.Y; }
		}
		if (Kitchen != INDEX_NONE) Layout.Rooms[Kitchen].RoomType = EHouseRoomType::Kitchen;
		if (Layout.Rooms.Num() >= 4)
		{
			int32 Bathroom = INDEX_NONE, Smallest = MAX_int32;
			for (int32 I = 0; I < Layout.Rooms.Num(); ++I)
			{
				if (Layout.Rooms[I].RoomType != EHouseRoomType::Bedroom) continue;
				const FIntPoint Size = Layout.Rooms[I].Max - Layout.Rooms[I].Min;
				if (Size.X * Size.Y < Smallest) { Bathroom = I; Smallest = Size.X * Size.Y; }
			}
			if (Bathroom != INDEX_NONE) Layout.Rooms[Bathroom].RoomType = EHouseRoomType::Bathroom;
		}
	}

	void GenerateWindows(const FHouseGenerationSettings& S, FHouseLayout& Layout)
	{
		FRandomStream Random(static_cast<int32>(static_cast<uint32>(S.Seed) ^ 0x3C6EF372u));
		// Sample one profile per room, independently of wall counts and placement decisions.
		FRandomStream Profiles(static_cast<int32>(static_cast<uint32>(S.Seed) ^ 0xA54FF53Au));
		TArray<FHouseWindow> RoomWindows;
		for (const FHouseRoom& Room : Layout.Rooms)
		{
			const EHouseRoomType Type = Room.RoomType;
			FHouseWindow Window;
			const float WidthFactor = Type == EHouseRoomType::LivingRoom ? 1.3f : Type == EHouseRoomType::Bathroom ? 0.65f : 1.0f;
			Window.Width = FMath::Max(50.0f, S.WindowWidth * WidthFactor * Profiles.FRandRange(1 - S.WindowSizeVariation, 1 + S.WindowSizeVariation));
			Window.SillHeight = FMath::Clamp(S.WindowSillHeight + (Type == EHouseRoomType::Bathroom ? 60.0f : 0.0f) +
				Profiles.FRandRange(-20, 20) * S.WindowSizeVariation, 50.0f, S.WallHeight - S.WallThickness - 50.0f);
			Window.Height = FMath::Clamp(S.WindowHeight * (Type == EHouseRoomType::Bathroom ? 0.65f : 1.0f) *
				Profiles.FRandRange(1 - S.WindowSizeVariation, 1 + S.WindowSizeVariation), 50.0f, S.WallHeight - S.WallThickness - Window.SillHeight);
			RoomWindows.Add(Window);
		}
		for (FHouseWall& Wall : Layout.Walls)
		{
			if (Wall.RoomB != INDEX_NONE || S.MaxWindowsPerWall == 0 || Random.FRand() >= S.WindowChance) continue;
			const FHouseWindow& Profile = RoomWindows[Wall.RoomA];
			const float Length = WallLength(Wall, S);
			TArray<FVector2D> Intervals;
			if (Wall.Opening == EHouseOpening::Door)
			{
				Intervals.Add(FVector2D(S.WallThickness, (Length - S.DoorWidth) * 0.5f - S.WindowSpacing));
				Intervals.Add(FVector2D((Length + S.DoorWidth) * 0.5f + S.WindowSpacing, Length - S.WallThickness));
			}
			else Intervals.Add(FVector2D(S.WallThickness, Length - S.WallThickness));
			for (const FVector2D& Interval : Intervals)
			{
				const int32 Capacity = FMath::Min(S.MaxWindowsPerWall - Wall.Windows.Num(),
					FMath::FloorToInt((Interval.Y - Interval.X + S.WindowSpacing) / (Profile.Width + S.WindowSpacing)));
				if (Capacity <= 0) continue;
				const int32 Count = Random.RandRange(1, Capacity);
				const float SlotWidth = (Interval.Y - Interval.X - (Count - 1) * S.WindowSpacing) / Count;
				for (int32 I = 0; I < Count; ++I)
				{
					FHouseWindow Window = Profile;
					// Evenly spaced centres keep the facade orderly; narrow intervals omit windows
					// instead of shrinking an individual opening away from its room's profile.
					Window.Offset = Interval.X + I * (SlotWidth + S.WindowSpacing) + SlotWidth * 0.5f;
					Wall.Windows.Add(Window);
				}
			}
			if (!Wall.Windows.IsEmpty() && Wall.Opening != EHouseOpening::Door) Wall.Opening = EHouseOpening::Window;
		}
	}

}

bool FHouseLayoutGenerator::ValidateSettings(const FHouseGenerationSettings& S, FString& OutError)
{
	OutError.Reset();
	if (S.MinRoomCells < 3 || S.MinRoomCells > 32 || S.WidthCells < 3 ||
		S.DepthCells < 3 || S.WidthCells > 64 || S.DepthCells > 64 ||
		S.RoomCount < 1 || S.RoomCount > 32 ||
		(S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom && S.RoomCount > (S.WidthCells / S.MinRoomCells) * (S.DepthCells / S.MinRoomCells)))
	{
		OutError = TEXT("Use a 3..64 cell footprint, minimum room side 3..32, and 1..32 rooms that fit the footprint.");
		return false;
	}
	const float Dimensions[] = {S.CellSize, S.WallHeight, S.WallThickness, S.SlabThickness,
		S.DoorWidth, S.DoorHeight, S.WindowWidth, S.WindowHeight, S.WindowSillHeight, S.WindowSpacing, S.WalkwayWidth};
	for (const float Value : Dimensions)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f || Value > 10000.0f)
		{
			OutError = TEXT("Dimensions must be finite, positive, and no greater than 10000 cm.");
			return false;
		}
	}
	if (S.CellSize < 100.0f || S.CellSize > 500.0f || S.WallThickness > S.CellSize * 0.5f ||
		S.DoorWidth < 100.0f || S.DoorHeight < 210.0f || S.WindowWidth < 50.0f ||
		S.DoorHeight + S.WallThickness > S.WallHeight ||
		S.WindowSillHeight + S.WindowHeight + S.WallThickness > S.WallHeight ||
		(S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom && FMath::Max(S.DoorWidth, S.WindowWidth) + 2.0f * S.WallThickness > S.MinRoomCells * S.CellSize) ||
		!FMath::IsFinite(S.ExtraConnectionChance) || S.ExtraConnectionChance < 0.0f || S.ExtraConnectionChance > 1.0f)
	{
		OutError = TEXT("Check cell size (100..500 cm), opening clearances (door at least 100 x 210 cm), wall dimensions and connection chance (0..1).");
		return false;
	}
	if (!FMath::IsFinite(S.WindowSizeVariation) || S.WindowSizeVariation < 0 || S.WindowSizeVariation > 0.75f ||
		!FMath::IsFinite(S.WindowChance) || S.WindowChance < 0 || S.WindowChance > 1 || S.MaxWindowsPerWall < 0 || S.MaxWindowsPerWall > 6 ||
		S.WindowSpacing < S.WallThickness || S.WindowSpacing < 20 || S.WalkwayWidth < 80 || S.WalkwayWidth > S.DoorWidth ||
		(S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom && S.WalkwayWidth + S.WallThickness + 10 >= S.MinRoomCells * S.CellSize) || S.FurnitureRules.Num() > 64)
	{
		OutError = TEXT("Check window variation (0..0.75), chance (0..1), count (0..6), spacing, walkway width, and furniture rules (at most 64).");
		return false;
	}
	for (const FHouseFurnitureRule& Rule : S.FurnitureRules)
	{
		if (Rule.Type >= EHouseFurnitureType::Count || Rule.AllowedRooms.IsEmpty() || Rule.Size.ContainsNaN() ||
			Rule.Size.GetMin() <= 0 || Rule.Size.GetMax() > 10000 || !FMath::IsFinite(Rule.FrontClearance) || Rule.FrontClearance < 50 || Rule.FrontClearance > 1000)
		{
			OutError = TEXT("Furniture rules need a valid type, allowed rooms, positive bounded dimensions, and front clearance of 50..1000 cm.");
			return false;
		}
		if (Rule.MinimumDensity > EHouseFurnishingDensity::Full) { OutError = TEXT("Unknown furniture density."); return false; }
		for (EHouseRoomPurpose Purpose : Rule.AllowedPurposes)
			if (Purpose > EHouseRoomPurpose::Storage) { OutError = TEXT("Unknown furniture purpose."); return false; }
		for (EHouseFurnitureStyle Style : Rule.AllowedStyles)
			if (Style > EHouseFurnitureStyle::MixedSecondhand) { OutError = TEXT("Unknown furniture style."); return false; }
		for (EHouseRoomType Type : Rule.AllowedRooms)
			if (Type > EHouseRoomType::Hallway) { OutError = TEXT("Unknown room type in furniture rule."); return false; }
	}
	return FHouseSizing::ValidateSettings(S, OutError) && FHouseHousehold::ValidateSettings(S, OutError) && FHouseNavigation::ValidateSettings(S, OutError);
}

bool FHouseLayoutGenerator::GenerateWithRetries(const FHouseGenerationSettings& Requested,
	FHouseGenerationSettings& EffectiveSettings, FHouseLayout& OutLayout, int32& Attempts, FString& OutError)
{
	OutLayout = FHouseLayout();
	Attempts = 0;
	EffectiveSettings = Requested;
	// Repair the common preset/count mismatch, but do not disguise malformed values.
	if (Requested.RoomCount >= 1 && Requested.RoomCount <= 32 &&
		Requested.Navigation.Pattern <= EHouseLayoutPattern::DeepBranches &&
		Requested.Navigation.Difficulty <= EHouseNavigationDifficulty::Custom &&
		FHouseNavigation::TargetDepth(Requested.Navigation) >= 2 && FHouseNavigation::TargetDepth(Requested.Navigation) <= 12)
		EffectiveSettings.RoomCount = FMath::Max(Requested.RoomCount, FHouseNavigation::MinimumRoomCount(Requested));
	// Validate against the permitted envelope first, while still rejecting invalid input dimensions.
	const bool bCanGrow = Requested.bAllowLargerFootprint && Requested.Navigation.Pattern != EHouseLayoutPattern::LegacyRandom &&
		Requested.WidthCells >= 3 && Requested.WidthCells <= 64 && Requested.DepthCells >= 3 && Requested.DepthCells <= 64;
	FHouseGenerationSettings ValidationSettings = EffectiveSettings;
	if (bCanGrow)
	{
		ValidationSettings.WidthCells = FMath::Max(24, Requested.WidthCells);
		ValidationSettings.DepthCells = FMath::Max(24, Requested.DepthCells);
	}
	if (!ValidateSettings(ValidationSettings, OutError)) return false;
	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		++Attempts;
		// Try the requested envelope first; changing seed alone cannot solve an undersized footprint.
		if (bCanGrow && Attempt == 4)
		{
			EffectiveSettings.WidthCells = ValidationSettings.WidthCells;
			EffectiveSettings.DepthCells = ValidationSettings.DepthCells;
		}
		EffectiveSettings.Seed = static_cast<int32>(static_cast<uint32>(Requested.Seed) + 0x9E3779B9u * static_cast<uint32>(Attempt));
		if (Generate(EffectiveSettings, OutLayout, OutError))
		{
			OutError.Reset();
			return true;
		}
	}
	OutError = FString::Printf(TEXT("No suitable house found after %d seed attempts. Last reason: %s"), Attempts, *OutError);
	return false;
}

bool FHouseLayoutGenerator::Generate(const FHouseGenerationSettings& S, FHouseLayout& OutLayout, FString& OutError)
{
	OutLayout = FHouseLayout();
	if (!ValidateSettings(S, OutError)) return false;
	if (S.Navigation.Pattern != EHouseLayoutPattern::LegacyRandom)
	{
		FHouseLayout Layout;
		TArray<FIntPoint> Connections;
		if (!FHouseNavigation::BuildPlan(S, Layout, Connections, OutError)) return false;
		FHouseGenerationSettings Bounds = S;
		Bounds.WidthCells = Layout.FootprintCells.X; Bounds.DepthCells = Layout.FootprintCells.Y;
		Layout.Walls = BuildBoundaries(Bounds, Layout.Rooms);
		int32 Connected = 0;
		for (FHouseWall& Wall : Layout.Walls)
		{
			const bool bEntrance = Wall.RoomA == 0 && Wall.RoomB == INDEX_NONE && Wall.Start.Y == 0 && Wall.End.Y == 0;
			const bool bPlannedDoor = Connections.Contains(FIntPoint(Wall.RoomA, Wall.RoomB)) || Connections.Contains(FIntPoint(Wall.RoomB, Wall.RoomA));
			if (!bEntrance && !bPlannedDoor) continue;
			if (!FitsOpening(Wall, S.DoorWidth, S)) { OutError = TEXT("A planned door does not fit its shared boundary."); return false; }
			Wall.Opening = EHouseOpening::Door;
			if (bPlannedDoor) ++Connected;
		}
		if (Connected != Connections.Num()) { OutError = TEXT("A planned room connection has no shared wall."); return false; }
		if (!FHouseHousehold::Assign(S, Layout, OutError)) return false;
		GenerateWindows(S, Layout);
		if (!FHouseFurnishing::Generate(S, Layout, OutError)) return false;
		Layout.Navigation = FHouseNavigation::Measure(Layout);
		Layout.Navigation.Summary = FHouseNavigation::Describe(S, Layout.Navigation);
		const FIntPoint Actual = FHouseSizing::Footprint(S, Layout);
		Layout.SizeSummary = FString::Printf(TEXT("%.1f x %.1f m | %.1f m2 footprint | envelope %.1f x %.1f m. Room labels show clear internal dimensions."),
			Actual.X * S.CellSize / 100.f, Actual.Y * S.CellSize / 100.f, Actual.X * Actual.Y * FMath::Square(S.CellSize / 100.f), S.WidthCells * S.CellSize / 100.f, S.DepthCells * S.CellSize / 100.f);
		if (!Validate(S, Layout, OutError)) return false;
		OutLayout = MoveTemp(Layout);
		return true;
	}
	FRandomStream Random(S.Seed);
	// Some random partitions leave short shared boundaries. Retry with the same stream,
	// with a hard limit; never expose an incomplete or disconnected layout.
	for (int32 Attempt = 0; Attempt < 64; ++Attempt)
	{
		FHouseLayout Layout;
		FHouseRoom Root;
		Root.Max = FIntPoint(S.WidthCells, S.DepthCells);
		Layout.Rooms.Add(Root);
		while (Layout.Rooms.Num() < S.RoomCount)
		{
			struct FSplit { int32 Room; bool bAlongX; int32 Cut; };
			TArray<FSplit> Candidates;
			auto Capacity = [&](FIntPoint Size) { return (Size.X / S.MinRoomCells) * (Size.Y / S.MinRoomCells); };
			int32 TotalCapacity = 0;
			for (const FHouseRoom& Room : Layout.Rooms) TotalCapacity += Capacity(Room.Max - Room.Min);
			for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
			{
				const FIntPoint Size = Layout.Rooms[Index].Max - Layout.Rooms[Index].Min;
				// Preserve enough capacity to reach the requested count, even in tightly packed footprints.
				for (int32 Cut = S.MinRoomCells; Cut <= Size.X - S.MinRoomCells; ++Cut)
					if (TotalCapacity - Capacity(Size) + Capacity(FIntPoint(Cut, Size.Y)) + Capacity(FIntPoint(Size.X - Cut, Size.Y)) >= S.RoomCount)
						Candidates.Add({Index, true, Layout.Rooms[Index].Min.X + Cut});
				for (int32 Cut = S.MinRoomCells; Cut <= Size.Y - S.MinRoomCells; ++Cut)
					if (TotalCapacity - Capacity(Size) + Capacity(FIntPoint(Size.X, Cut)) + Capacity(FIntPoint(Size.X, Size.Y - Cut)) >= S.RoomCount)
						Candidates.Add({Index, false, Layout.Rooms[Index].Min.Y + Cut});
			}
			if (Candidates.IsEmpty()) break;
			const FSplit Split = Candidates[Random.RandRange(0, Candidates.Num() - 1)];
			FHouseRoom& First = Layout.Rooms[Split.Room];
			FHouseRoom Second = First;
			if (Split.bAlongX)
			{
				First.Max.X = Split.Cut;
				Second.Min.X = Split.Cut;
			}
			else
			{
				First.Max.Y = Split.Cut;
				Second.Min.Y = Split.Cut;
			}
			Layout.Rooms.Add(Second);
		}
		if (Layout.Rooms.Num() != S.RoomCount) continue;
		Layout.Walls = BuildBoundaries(S, Layout.Rooms);
		TArray<int32> Entrances;
		for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
		{
			const FHouseWall& Wall = Layout.Walls[Index];
			if (Wall.RoomB == INDEX_NONE && Wall.Start.Y == 0 && Wall.End.Y == 0 && FitsOpening(Wall, S.DoorWidth, S))
				Entrances.Add(Index);
		}
		if (Entrances.IsEmpty()) continue;
		FHouseWall& Entrance = Layout.Walls[Entrances[Random.RandRange(0, Entrances.Num() - 1)]];
		Entrance.Opening = EHouseOpening::Door;
		TSet<int32> Reached;
		Reached.Add(Entrance.RoomA);
		while (Reached.Num() < Layout.Rooms.Num())
		{
			TArray<int32> Frontier;
			for (int32 Index = 0; Index < Layout.Walls.Num(); ++Index)
			{
				const FHouseWall& Wall = Layout.Walls[Index];
				if (Wall.RoomB != INDEX_NONE && Reached.Contains(Wall.RoomA) != Reached.Contains(Wall.RoomB) &&
					FitsOpening(Wall, S.DoorWidth, S)) Frontier.Add(Index);
			}
			if (Frontier.IsEmpty()) break;
			FHouseWall& Door = Layout.Walls[Frontier[Random.RandRange(0, Frontier.Num() - 1)]];
			Door.Opening = EHouseOpening::Door;
			Reached.Add(Door.RoomA);
			Reached.Add(Door.RoomB);
		}
		if (Reached.Num() != Layout.Rooms.Num()) continue;
		for (FHouseWall& Wall : Layout.Walls)
		{
			if (Wall.Opening != EHouseOpening::None) continue;
			if (Wall.RoomB != INDEX_NONE && FitsOpening(Wall, S.DoorWidth, S) && Random.FRand() < S.ExtraConnectionChance)
				Wall.Opening = EHouseOpening::Door;
		}
		AssignRoomTypes(Layout, Entrance.RoomA);
		if (!FHouseHousehold::Assign(S, Layout, OutError)) return false;
		GenerateWindows(S, Layout);
		if (!FHouseFurnishing::Generate(S, Layout, OutError)) return false;
		Layout.Navigation = FHouseNavigation::Measure(Layout);
		Layout.Navigation.Summary = FHouseNavigation::Describe(S, Layout.Navigation);
		const FIntPoint Actual = FHouseSizing::Footprint(S, Layout);
		Layout.SizeSummary = FString::Printf(TEXT("%.1f x %.1f m | %.1f m2 footprint | envelope %.1f x %.1f m. Room labels show clear internal dimensions."),
			Actual.X * S.CellSize / 100.f, Actual.Y * S.CellSize / 100.f, Actual.X * Actual.Y * FMath::Square(S.CellSize / 100.f), S.WidthCells * S.CellSize / 100.f, S.DepthCells * S.CellSize / 100.f);
		if (Validate(S, Layout, OutError))
		{
			OutLayout = MoveTemp(Layout);
			return true;
		}
	}
	OutError = TEXT("Could not fit a connected layout in 64 attempts. Reduce room count/opening width or enlarge the footprint.");
	return false;
}

bool FHouseLayoutGenerator::Validate(const FHouseGenerationSettings& S, const FHouseLayout& Layout, FString& OutError)
{
	if (!ValidateSettings(S, OutError)) return false;
	if (Layout.Rooms.Num() != S.RoomCount)
	{
		OutError = TEXT("Room count does not match settings.");
		return false;
	}
	const FIntPoint Actual = FHouseSizing::Footprint(S, Layout);
	if (Actual.X < 1 || Actual.Y < 1 || Actual.X > S.WidthCells || Actual.Y > S.DepthCells)
	{ OutError = TEXT("Generated footprint is outside the requested envelope."); return false; }
	FHouseGenerationSettings Bounds = S;
	Bounds.WidthCells = Actual.X; Bounds.DepthCells = Actual.Y;
	TSet<FIntPoint> Occupied;
	for (const FHouseRoom& Room : Layout.Rooms)
	{
		if (Room.Min.X < 0 || Room.Min.Y < 0 || Room.Max.X <= Room.Min.X || Room.Max.Y <= Room.Min.Y ||
			Room.Max.X > Actual.X || Room.Max.Y > Actual.Y ||
			(S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom && (Room.Max.X - Room.Min.X < S.MinRoomCells || Room.Max.Y - Room.Min.Y < S.MinRoomCells)))
		{
			OutError = TEXT("Room is out of bounds or below minimum size.");
			return false;
		}
		for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
			for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			{
				const FIntPoint Cell(X, Y);
				if (Occupied.Contains(Cell)) { OutError = TEXT("Rooms overlap."); return false; }
				Occupied.Add(Cell);
			}
	}
	if (Occupied.Num() != Actual.X * Actual.Y)
	{
		OutError = TEXT("Rooms do not cover the footprint.");
		return false;
	}
	const TArray<FHouseWall> Expected = BuildBoundaries(Bounds, Layout.Rooms);
	if (Expected.Num() != Layout.Walls.Num()) { OutError = TEXT("Missing or duplicated walls."); return false; }
	int32 EntranceCount = 0;
	TSet<int32> Reached;
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		const FHouseWall& Wall = Layout.Walls[Index];
		const FHouseWall& Boundary = Expected[Index];
		if (Wall.Start != Boundary.Start || Wall.End != Boundary.End || Wall.RoomA != Boundary.RoomA || Wall.RoomB != Boundary.RoomB)
		{
			OutError = TEXT("Wall boundaries are missing, duplicated, or not in canonical order.");
			return false;
		}
		if (Wall.Opening != EHouseOpening::None && Wall.Opening != EHouseOpening::Door && Wall.Opening != EHouseOpening::Window)
		{
			OutError = TEXT("Unknown opening type.");
			return false;
		}
		if (Wall.Opening == EHouseOpening::Door && !FitsOpening(Wall, S.DoorWidth, S))
		{
			OutError = TEXT("Opening is too close to a wall junction.");
			return false;
		}
		if (Wall.Opening == EHouseOpening::Window && Wall.RoomB != INDEX_NONE)
		{
			OutError = TEXT("Windows must be on exterior walls.");
			return false;
		}
		if ((Wall.RoomB != INDEX_NONE && !Wall.Windows.IsEmpty()) || Wall.Windows.Num() > S.MaxWindowsPerWall ||
			(Wall.Opening == EHouseOpening::None && !Wall.Windows.IsEmpty()) || (Wall.Opening == EHouseOpening::Window && Wall.Windows.IsEmpty()))
		{
			OutError = TEXT("Invalid window list or interior window.");
			return false;
		}
		float PreviousRight = -S.WindowSpacing;
		for (const FHouseWindow& Window : Wall.Windows)
		{
			const float Left = Window.Offset - Window.Width * 0.5f, Right = Window.Offset + Window.Width * 0.5f;
			const float Length = WallLength(Wall, S);
			const bool HitsDoor = Wall.Opening == EHouseOpening::Door &&
				Right > (Length - S.DoorWidth) * 0.5f - S.WindowSpacing + 0.01f && Left < (Length + S.DoorWidth) * 0.5f + S.WindowSpacing - 0.01f;
			if (!FMath::IsFinite(Window.Offset) || !FMath::IsFinite(Window.Width) || !FMath::IsFinite(Window.Height) || !FMath::IsFinite(Window.SillHeight) ||
				Window.Width < 49.99f || Window.Height < 49.99f || Window.SillHeight < 49.99f ||
				Window.SillHeight + Window.Height > S.WallHeight - S.WallThickness + 0.01f ||
				Left < S.WallThickness - 0.01f || Right > Length - S.WallThickness + 0.01f || Left < PreviousRight + S.WindowSpacing - 0.01f || HitsDoor)
			{
				OutError = TEXT("Window is out of bounds or overlaps another opening/junction.");
				return false;
			}
			PreviousRight = Right;
		}
		if (Wall.Opening == EHouseOpening::Door && Wall.RoomB == INDEX_NONE)
		{
			if (Layout.Rooms[Wall.RoomA].RoomType != EHouseRoomType::LivingRoom) { OutError = TEXT("Entrance must lead to the living room."); return false; }
			++EntranceCount;
			Reached.Add(Wall.RoomA);
		}
	}
	if (EntranceCount != 1) { OutError = TEXT("House must have exactly one entrance."); return false; }
	for (int32 Pass = 0; Pass < Layout.Rooms.Num(); ++Pass)
		for (const FHouseWall& Wall : Layout.Walls)
			if (Wall.Opening == EHouseOpening::Door && Wall.RoomB != INDEX_NONE &&
				(Reached.Contains(Wall.RoomA) || Reached.Contains(Wall.RoomB)))
			{
				Reached.Add(Wall.RoomA);
				Reached.Add(Wall.RoomB);
			}
	if (Reached.Num() != Layout.Rooms.Num()) { OutError = TEXT("A room is unreachable from the entrance."); return false; }
	int32 Counts[5] = {};
	for (const FHouseRoom& Room : Layout.Rooms)
	{
		const int32 Type = static_cast<int32>(Room.RoomType);
		if (Type < 0 || Type > 4) { OutError = TEXT("Unknown room type."); return false; }
		++Counts[Type];
	}
	if (Counts[0] != 1 || Counts[1] != (Layout.Rooms.Num() >= 2 ? 1 : 0) || Counts[3] != (Layout.Rooms.Num() >= 4 ? 1 : 0))
	{
		OutError = TEXT("Room type counts do not match the house program.");
		return false;
	}
	if (S.Navigation.Pattern != EHouseLayoutPattern::LegacyRandom && Counts[2] == 0)
	{
		OutError = TEXT("Planned houses require at least one bedroom.");
		return false;
	}
	if (!FHouseNavigation::ValidatePlan(S, Layout, OutError)) return false;
	return FHouseSizing::Validate(S, Layout, OutError) && FHouseHousehold::Validate(S, Layout, OutError) && FHouseFurnishing::Validate(S, Layout, OutError);
}
