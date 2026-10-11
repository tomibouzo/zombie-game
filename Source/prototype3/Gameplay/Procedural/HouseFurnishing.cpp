#include "Gameplay/Procedural/HouseFurnishing.h"
#include "Gameplay/Procedural/HouseHousehold.h"
#include "Math/RandomStream.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseNavigation.h"

namespace
{
	FVector2D Facing(int32 Turns)
	{
		switch (Turns)
		{
		case 1: return FVector2D(-1, 0);
		case 2: return FVector2D(0, -1);
		case 3: return FVector2D(1, 0);
		default: return FVector2D(0, 1);
		}
	}

	FBox2D RoomBounds(const FHouseGenerationSettings& S, const FHouseRoom& Room)
	{
		const double Inset = S.WallThickness * 0.5 + 5.0;
		return FBox2D(FVector2D(Room.Min) * S.CellSize + FVector2D(Inset),
			FVector2D(Room.Max) * S.CellSize - FVector2D(Inset));
	}

	bool Overlaps(const FBox2D& A, const FBox2D& B)
	{
		return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
	}

	bool Contains(const FBox2D& Outer, const FBox2D& Inner)
	{
		return Inner.Min.X >= Outer.Min.X && Inner.Min.Y >= Outer.Min.Y &&
			Inner.Max.X <= Outer.Max.X && Inner.Max.Y <= Outer.Max.Y;
	}

	FBox2D AccessArea(const FHouseGenerationSettings& S, const FHouseFurniturePlacement& Item)
	{
		const FBox2D Body = FHouseFurnishing::Footprint(S, Item);
		const FVector2D Front = Facing(Item.QuarterTurns);
		const FVector2D Face = Item.Center + Front * Body.GetExtent();
		const double Depth = S.FurnitureRules[Item.RuleIndex].FrontClearance;
		const FVector2D Size = Item.QuarterTurns % 2 ? FVector2D(Depth, Body.GetSize().Y) : FVector2D(Body.GetSize().X, Depth);
		const FVector2D Center = Face + Front * (Depth * 0.5);
		return FBox2D(Center - Size * 0.5, Center + Size * 0.5);
	}

	FVector2D DoorCenter(const FHouseGenerationSettings& S, const FHouseWall& Wall)
	{
		return (FVector2D(Wall.Start) + FVector2D(Wall.End)) * (S.CellSize * 0.5);
	}

	FVector2D Inward(const FHouseRoom& Room, const FHouseWall& Wall)
	{
		if (Wall.Start.X == Wall.End.X) return FVector2D(Wall.Start.X == Room.Min.X ? 1 : -1, 0);
		return FVector2D(0, Wall.Start.Y == Room.Min.Y ? 1 : -1);
	}

	bool Fits(const FHouseGenerationSettings& S, const FHouseLayout& L, const FHouseFurniturePlacement& Item, int32 IgnoreIndex = INDEX_NONE)
	{
		const FHouseRoom& Room = L.Rooms[Item.RoomIndex];
		if (S.FurnitureRules[Item.RuleIndex].Size.Z > S.WallHeight - 10.0f) return false;
		const FBox2D Body = FHouseFurnishing::Footprint(S, Item);
		const FBox2D Access = AccessArea(S, Item);
		const FBox2D Bounds = RoomBounds(S, Room);
		if (!Contains(Bounds, Body) || !Contains(Bounds, Access)) return false;
		for (int32 I = 0; I < L.Furniture.Num(); ++I)
		{
			const FHouseFurniturePlacement& Other = L.Furniture[I];
			if (I == IgnoreIndex || Other.RoomIndex != Item.RoomIndex) continue;
			const FBox2D OtherBody = FHouseFurnishing::Footprint(S, Other);
			if (Overlaps(Body.ExpandBy(5.0), OtherBody) || Overlaps(Access, OtherBody) || Overlaps(Body, AccessArea(S, Other))) return false;
		}
		for (const FHouseWall& Wall : L.Walls)
		{
			if (Wall.RoomA != Item.RoomIndex && Wall.RoomB != Item.RoomIndex) continue;
			const bool AlongX = Wall.Start.Y == Wall.End.Y;
			const FVector2D Normal = Inward(Room, Wall);
			if (Wall.Opening == EHouseOpening::Door)
			{
				// Reserve the swept body as well as its centre's approach to the threshold.
				// Otherwise a bed just beyond the anchor can clip a player walking through before turning.
				const double Depth = 1.5 * S.WalkwayWidth + S.WallThickness;
				const FVector2D Half = AlongX ? FVector2D(S.DoorWidth * 0.5 + 10, Depth * 0.5) : FVector2D(Depth * 0.5, S.DoorWidth * 0.5 + 10);
				const FVector2D Center = DoorCenter(S, Wall) + Normal * Depth * 0.5;
				if (Overlaps(Body, FBox2D(Center - Half, Center + Half))) return false;
			}
			// Low counters can sit beneath a window; tall pieces cannot cover it.
			for (const FHouseWindow& Window : Wall.Windows)
			{
				if (S.FurnitureRules[Item.RuleIndex].Size.Z <= Window.SillHeight) continue;
				const FVector2D Center = FVector2D(Wall.Start) * S.CellSize +
					(AlongX ? FVector2D(Window.Offset, 0) : FVector2D(0, Window.Offset));
				const double Depth = S.WallThickness * 0.5 + 25;
				const FVector2D Half = AlongX ? FVector2D(Window.Width * 0.5 + 5, Depth) : FVector2D(Depth, Window.Width * 0.5 + 5);
				if (Overlaps(Body, FBox2D(Center - Half, Center + Half))) return false;
			}
		}
		return true;
	}
}

FBox2D FHouseFurnishing::Footprint(const FHouseGenerationSettings& S, const FHouseFurniturePlacement& Item)
{
	const FVector& Size = S.FurnitureRules[Item.RuleIndex].Size;
	const FVector2D Half = (Item.QuarterTurns % 2 ? FVector2D(Size.Y, Size.X) : FVector2D(Size.X, Size.Y)) * 0.5;
	return FBox2D(Item.Center - Half, Item.Center + Half);
}

bool FHouseFurnishing::FindWalkPaths(const FHouseGenerationSettings& S, const FHouseLayout& L,
	int32 RoomIndex, TArray<TArray<FVector2D>>& OutPaths)
{
	OutPaths.Reset();
	const FHouseRoom& Room = L.Rooms[RoomIndex];
	const double Radius = S.WalkwayWidth * 0.5;
	const FBox2D Bounds = RoomBounds(S, Room).ExpandBy(-Radius);
	if (Bounds.GetSize().GetMin() <= 0) return false;
	TArray<FVector2D> Anchors;
	TArray<FBox2D> Obstacles;
	for (const FHouseWall& Wall : L.Walls)
		if (Wall.Opening == EHouseOpening::Door && (Wall.RoomA == RoomIndex || Wall.RoomB == RoomIndex))
			Anchors.Add(DoorCenter(S, Wall) + Inward(Room, Wall) * (S.WallThickness * 0.5 + 5 + Radius));
	for (const FHouseFurniturePlacement& Item : L.Furniture)
	{
		if (Item.RoomIndex != RoomIndex) continue;
		const FBox2D Body = Footprint(S, Item);
		Obstacles.Add(Body.ExpandBy(Radius));
		Anchors.Add(Item.Center + Facing(Item.QuarterTurns) * (Body.GetExtent() + FVector2D(Radius + 1)));
	}
	if (Anchors.IsEmpty()) return false;
	// Coordinate-compressed visibility grid. Its size depends on furniture/doors, not room dimensions.
	TArray<double> Xs{Bounds.Min.X, Bounds.Max.X}, Ys{Bounds.Min.Y, Bounds.Max.Y};
	for (const FVector2D& Point : Anchors)
	{
		if (!Bounds.IsInsideOrOn(Point)) return false;
		Xs.AddUnique(Point.X);
		Ys.AddUnique(Point.Y);
	}
	for (const FBox2D& Box : Obstacles)
	{
		for (const double X : {Box.Min.X - 1, Box.Max.X + 1}) if (X > Bounds.Min.X && X < Bounds.Max.X) Xs.AddUnique(X);
		for (const double Y : {Box.Min.Y - 1, Box.Max.Y + 1}) if (Y > Bounds.Min.Y && Y < Bounds.Max.Y) Ys.AddUnique(Y);
	}
	Xs.Sort();
	Ys.Sort();
	const int32 NX = Xs.Num(), NY = Ys.Num();
	auto PointAt = [&](int32 Index) { return FVector2D(Xs[Index % NX], Ys[Index / NX]); };
	auto IndexAt = [&](const FVector2D& Point) { return Ys.IndexOfByKey(Point.Y) * NX + Xs.IndexOfByKey(Point.X); };
	auto ClearSegment = [&](const FVector2D& A, const FVector2D& B)
	{
		for (const FBox2D& Box : Obstacles)
		{
			if (A.X == B.X && A.X >= Box.Min.X && A.X <= Box.Max.X && FMath::Max(A.Y, B.Y) >= Box.Min.Y && FMath::Min(A.Y, B.Y) <= Box.Max.Y) return false;
			if (A.Y == B.Y && A.Y >= Box.Min.Y && A.Y <= Box.Max.Y && FMath::Max(A.X, B.X) >= Box.Min.X && FMath::Min(A.X, B.X) <= Box.Max.X) return false;
		}
		return true;
	};
	const int32 Root = IndexAt(Anchors[0]);
	if (!ClearSegment(Anchors[0], Anchors[0])) return false;
	TArray<int32> Parents;
	Parents.Init(INDEX_NONE, NX * NY);
	Parents[Root] = Root;
	TArray<int32> Queue{Root};
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Current = Queue[Head];
		for (const FIntPoint Delta : {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)})
		{
			const int32 X = Current % NX + Delta.X, Y = Current / NX + Delta.Y;
			if (X < 0 || X >= NX || Y < 0 || Y >= NY) continue;
			const int32 Next = Y * NX + X;
			if (Parents[Next] != INDEX_NONE || !ClearSegment(PointAt(Current), PointAt(Next))) continue;
			Parents[Next] = Current;
			Queue.Add(Next);
		}
	}
	for (int32 I = 1; I < Anchors.Num(); ++I)
	{
		int32 Current = IndexAt(Anchors[I]);
		if (Parents[Current] == INDEX_NONE) return false;
		TArray<FVector2D>& Path = OutPaths.AddDefaulted_GetRef();
		while (Current != Root) { Path.Add(PointAt(Current)); Current = Parents[Current]; }
		Path.Add(Anchors[0]);
	}
	return true;
}

bool FHouseFurnishing::Generate(const FHouseGenerationSettings& S, FHouseLayout& L, FString& OutError)
{
	L.Furniture.Reset();
	L.SkippedFurnitureCount = 0;
	if (!S.bGenerateFurniture) return true;
	FRandomStream Random(static_cast<int32>(static_cast<uint32>(S.Seed) ^ 0x61C88647u));
	TArray<int32> RuleOrder;
	for (int32 I = 0; I < S.FurnitureRules.Num(); ++I) RuleOrder.Add(I);
	auto IsBed = [](EHouseFurnitureType Type) { return Type == EHouseFurnitureType::Bed || Type == EHouseFurnitureType::SingleBed; };
	RuleOrder.StableSort([&](int32 A, int32 B)
	{
		if (IsBed(S.FurnitureRules[A].Type) != IsBed(S.FurnitureRules[B].Type)) return IsBed(S.FurnitureRules[A].Type);
		return S.Household.Profile != EHouseholdProfile::None && S.FurnitureRules[A].bEssential && !S.FurnitureRules[B].bEssential;
	});
	const int32 PrimaryRoom = FHouseNavigation::Measure(L).DeepestRoom;
	for (int32 RoomIndex = 0; RoomIndex < L.Rooms.Num(); ++RoomIndex)
	{
		const FBox2D Room = RoomBounds(S, L.Rooms[RoomIndex]);
		auto FurnishRoom = [&]()
		{
			for (int32 RuleIndex : RuleOrder)
			{
				const FHouseFurnitureRule& Rule = S.FurnitureRules[RuleIndex];
				if (!FHouseHousehold::WantsFurniture(S, L.Rooms[RoomIndex], RoomIndex, RuleIndex)) continue;
				TArray<FHouseFurniturePlacement> Candidates;
				for (int32 Turn = 0; Turn < 4; ++Turn)
				{
					FHouseFurniturePlacement Item;
					Item.RoomIndex = RoomIndex;
					Item.RuleIndex = RuleIndex;
					Item.Type = Rule.Type;
					Item.QuarterTurns = Turn;
					const FVector2D Half = Footprint(S, Item).GetExtent();
					const FBox2D Centers(Room.Min + Half, Room.Max - Half);
					if (Centers.GetSize().GetMin() < 0) continue;
					if (Rule.bPreferCenter)
					{
						for (const double X : {0.35, 0.5, 0.65})
							for (const double Y : {0.35, 0.5, 0.65})
							{
								Item.Center = Centers.Min + Centers.GetSize() * FVector2D(X, Y);
								Candidates.Add(Item);
							}
					}
					else
					{
						for (int32 Sample = 0; Sample <= 12; ++Sample)
						{
							const double T = Sample / 12.0;
							if (Turn == 0 || Turn == 2) Item.Center = FVector2D(FMath::Lerp(Centers.Min.X, Centers.Max.X, T), Turn == 0 ? Centers.Min.Y : Centers.Max.Y);
							else Item.Center = FVector2D(Turn == 1 ? Centers.Max.X : Centers.Min.X, FMath::Lerp(Centers.Min.Y, Centers.Max.Y, T));
							Candidates.Add(Item);
						}
					}
				}
				for (int32 I = Candidates.Num() - 1; I > 0; --I) Candidates.Swap(I, Random.RandRange(0, I));
				bool bPlaced = false;
				for (const FHouseFurniturePlacement& Item : Candidates)
				{
					if (!Fits(S, L, Item)) continue;
					L.Furniture.Add(Item);
					TArray<TArray<FVector2D>> Paths;
					if (FindWalkPaths(S, L, RoomIndex, Paths)) { bPlaced = true; break; }
					L.Furniture.Pop();
				}
				if (!bPlaced) ++L.SkippedFurnitureCount;
			}
		};
		const int32 SkippedBefore = L.SkippedFurnitureCount;
		FurnishRoom();
		FHouseRoom& R = L.Rooms[RoomIndex];
		const bool bBedroom = R.RoomType == EHouseRoomType::Bedroom && R.Purpose != EHouseRoomPurpose::Office && R.Purpose != EHouseRoomPurpose::Storage;
		if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom || !bBedroom || R.bUnfurnished || L.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& I) { return I.RoomIndex == RoomIndex && IsBed(I.Type); })) continue;
		// An absent/style-filtered bed rule is a catalogue issue, not evidence that the room is too small.
		bool bEligibleBed = false;
		for (int32 I : RuleOrder) bEligibleBed |= IsBed(S.FurnitureRules[I].Type) && FHouseHousehold::WantsFurniture(S, R, RoomIndex, I);
		if (!bEligibleBed)
		{
			OutError = FString::Printf(TEXT("Bedroom %d has no eligible bed rule. Reset Furniture Rules To Defaults or add a bed matching its purpose/style."), RoomIndex);
			return false;
		}
		const bool bRequired = R.Purpose == EHouseRoomPurpose::PrimaryBedroom || R.Purpose == EHouseRoomPurpose::ChildBedroom ||
			(R.Purpose == EHouseRoomPurpose::Default && RoomIndex == PrimaryRoom);
		if (bRequired)
		{
			OutError = FString::Printf(TEXT("Required bedroom %d cannot fit a bed with safe access. Increase its room dimensions or reduce bed size/clearance."), RoomIndex);
			return false;
		}
		R.Purpose = EHouseRoomPurpose::Office;
		if (S.Navigation.Pattern != EHouseLayoutPattern::LegacyRandom && !FHouseSizing::Fits(S, R, R.Max - R.Min, false))
		{
			OutError = FString::Printf(TEXT("Bedroom %d cannot fit a bed and does not meet office size limits."), RoomIndex);
			return false;
		}
		L.Furniture.RemoveAll([&](const FHouseFurniturePlacement& I) { return I.RoomIndex == RoomIndex; });
		L.SkippedFurnitureCount = SkippedBefore;
		FurnishRoom();
		if (!L.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& I) { return I.RoomIndex == RoomIndex && I.Type == EHouseFurnitureType::Desk; }))
		{
			OutError = FString::Printf(TEXT("Room %d cannot fit a bed or an office desk. Check dimensions and furniture rules."), RoomIndex);
			return false;
		}
	}
	return true;
}

bool FHouseFurnishing::Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& OutError)
{
	for (int32 Index = 0; Index < L.Furniture.Num(); ++Index)
	{
		const FHouseFurniturePlacement& Item = L.Furniture[Index];
		if (!S.bGenerateFurniture || !L.Rooms.IsValidIndex(Item.RoomIndex) || !S.FurnitureRules.IsValidIndex(Item.RuleIndex) ||
			Item.QuarterTurns < 0 || Item.QuarterTurns > 3 || Item.Center.ContainsNaN())
		{
			OutError = TEXT("Invalid furniture placement data.");
			return false;
		}
		const FHouseFurnitureRule& Rule = S.FurnitureRules[Item.RuleIndex];
		if (Item.Type != Rule.Type || !FHouseHousehold::WantsFurniture(S, L.Rooms[Item.RoomIndex], Item.RoomIndex, Item.RuleIndex))
		{
			OutError = TEXT("Furniture does not match the room type/rule.");
			return false;
		}
	}
	for (int32 Index = 0; Index < L.Furniture.Num(); ++Index)
		if (!Fits(S, L, L.Furniture[Index], Index)) { OutError = TEXT("Furniture overlaps a wall, door, window, access area, or another item."); return false; }
	for (int32 Room = 0; Room < L.Rooms.Num(); ++Room)
	{
		TArray<TArray<FVector2D>> Paths;
		if (!FindWalkPaths(S, L, Room, Paths)) { OutError = TEXT("Furniture blocks a doorway or a required walking route."); return false; }
	}
	return true;
}
