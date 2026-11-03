#include "Gameplay/Procedural/HouseResidentialPlan.h"
#include "Gameplay/Procedural/HouseNavigation.h"
#include "Gameplay/Procedural/HouseHousehold.h"
#include "Gameplay/Procedural/HouseSizing.h"
#include "Math/RandomStream.h"

namespace
{
	struct FSide
	{
		TArray<int32> Rooms;
		TArray<TArray<int32>> Heights;
		TArray<TArray<uint8>> Reach;
		void Solve(const FHouseGenerationSettings& S, const FHouseLayout& L, int32 Width, int32 Primary)
		{
			Heights.SetNum(Rooms.Num());
			for (int32 I = 0; I < Rooms.Num(); ++I)
				for (int32 H = 1; H <= S.DepthCells; ++H)
					if (H * S.CellSize >= S.DoorWidth + 2 * S.WallThickness &&
						FHouseSizing::Fits(S, L.Rooms[Rooms[I]], FIntPoint(Width, H), Rooms[I] == Primary) &&
						// A centred doorway needs enough width opposite it for a double bed and its approach.
						!((L.Rooms[Rooms[I]].Purpose == EHouseRoomPurpose::PrimaryBedroom ||
						  (L.Rooms[Rooms[I]].Purpose == EHouseRoomPurpose::Default && Rooms[I] == Primary)) &&
						  FHouseSizing::ClearSize(S, FIntPoint(Width, H)).X < 3.6 &&
						  FHouseSizing::ClearSize(S, FIntPoint(Width, H)).Y < 5.6) &&
						!(L.Rooms[Rooms[I]].Purpose == EHouseRoomPurpose::ChildBedroom &&
						  FHouseSizing::ClearSize(S, FIntPoint(Width, H)).X < 3.5 &&
						  FHouseSizing::ClearSize(S, FIntPoint(Width, H)).Y < 3.2)) Heights[I].Add(H);
			Reach.SetNum(Rooms.Num() + 1);
			for (auto& Row : Reach) Row.Init(0, S.DepthCells + 1);
			Reach.Last()[0] = 1;
			for (int32 I = Rooms.Num() - 1; I >= 0; --I)
				for (int32 Sum = 1; Sum <= S.DepthCells; ++Sum)
					for (int32 H : Heights[I]) if (H <= Sum && Reach[I + 1][Sum - H]) { Reach[I][Sum] = 1; break; }
		}
		void Place(FHouseLayout& L, int32 X, int32 Width, int32 Y, int32 Depth, FRandomStream& Random) const
		{
			for (int32 I = 0; I < Rooms.Num(); ++I)
			{
				TArray<int32> Choices;
				for (int32 H : Heights[I]) if (H <= Depth && Reach[I + 1][Depth - H]) Choices.Add(H);
				const int32 H = Choices[Random.RandRange(0, Choices.Num() - 1)];
				L.Rooms[Rooms[I]].Min = FIntPoint(X, Y);
				L.Rooms[Rooms[I]].Max = FIntPoint(X + Width, Y + H);
				Y += H; Depth -= H;
			}
		}
	};
	struct FSection { FSide Left, Right; TArray<int32> Depths; };
	struct FFront { int32 Left, Right, Depth; };
}

bool FHouseResidentialPlan::Build(const FHouseGenerationSettings& S, FHouseLayout& Out, TArray<FIntPoint>& Connections, FString& Error)
{
	const int32 HallCount = FHouseNavigation::TargetDepth(S.Navigation) - 1;
	const int32 PrivateCount = S.RoomCount - 2 - HallCount;
	const int32 HallWidth = FMath::CeilToInt(FMath::Max(S.RoomSizing.MinHallwayWidth * 100 + S.WallThickness,
		S.DoorWidth + 2 * S.WallThickness) / S.CellSize);
	if ((HallWidth * S.CellSize - S.WallThickness) / 100.0 > S.RoomSizing.MaxHallwayWidth + 0.001)
	{ Error = TEXT("Cell Size / Door Width cannot fit the hallway width range. Try Cell Size = 100 cm, Door Width = 140 cm, or adjust Room Sizing."); return false; }
	TArray<FFront> Fronts;
	FHouseRoom Living, Kitchen, Hall;
	Living.RoomType = EHouseRoomType::LivingRoom; Kitchen.RoomType = EHouseRoomType::Kitchen; Hall.RoomType = EHouseRoomType::Hallway;
	for (int32 Left = 1; Left < S.WidthCells - HallWidth; ++Left)
		for (int32 Right = 1; Right <= S.WidthCells - HallWidth - Left; ++Right)
			for (int32 Depth = 1; Depth < S.DepthCells; ++Depth)
				if (Depth * S.CellSize >= S.DoorWidth + 2 * S.WallThickness &&
					FHouseSizing::Fits(S, Living, FIntPoint(Left + HallWidth, Depth)) &&
					FHouseSizing::Fits(S, Kitchen, FIntPoint(Right, Depth))) Fronts.Add({Left, Right, Depth});
	if (Fronts.IsEmpty())
	{ Error = TEXT("The footprint envelope/grid cannot fit the living room and kitchen size limits. Use 100 cm cells and a larger envelope, or adjust Room Sizing."); return false; }
	FRandomStream Random(S.Seed);
	FString ProfileError;
	for (int32 Attempt = 0; Attempt < 512; ++Attempt)
	{
		const FFront Front = Fronts[Random.RandRange(0, Fronts.Num() - 1)];
		FHouseLayout L;
		L.Rooms.Add(Living); L.Rooms.Add(Kitchen);
		for (int32 I = 0; I < HallCount; ++I) L.Rooms.Add(Hall);
		TArray<FIntPoint> Edges{FIntPoint(0, 1), FIntPoint(0, 2)};
		for (int32 I = 1; I < HallCount; ++I) Edges.Add(FIntPoint(1 + I, 2 + I));
		TArray<int32> Counts; Counts.Init(2, HallCount);
		for (int32 Extra = PrivateCount - 2 * HallCount; Extra > 0; --Extra)
			++Counts[S.Navigation.Pattern == EHouseLayoutPattern::CentralHub ? 0 : Random.RandRange(0, HallCount - 1)];
		TArray<FSection> Sections; Sections.SetNum(HallCount);
		for (int32 I = 0; I < HallCount; ++I)
		{
			const int32 LeftCount = Random.RandRange(1, Counts[I] - 1);
			TArray<int32> IDs;
			for (int32 N = 0; N < Counts[I]; ++N) IDs.Add(L.Rooms.Add(FHouseRoom()));
			for (int32 N = IDs.Num() - 1; N > 0; --N) IDs.Swap(N, Random.RandRange(0, N));
			for (int32 N = 0; N < IDs.Num(); ++N)
			{
				(N < LeftCount ? Sections[I].Left.Rooms : Sections[I].Right.Rooms).Add(IDs[N]);
				Edges.Add(FIntPoint(2 + I, IDs[N]));
			}
		}
		// Match the canonical boundary enumeration used by the final door graph.
		Edges.Sort([](FIntPoint A, FIntPoint B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
		L.Rooms[Sections[0].Right.Rooms[0]].RoomType = EHouseRoomType::Bathroom;
		// Temporary topology lets household assignment reserve room purposes before dimensions are chosen.
		FHouseWall Entrance; Entrance.RoomA = 0; Entrance.Opening = EHouseOpening::Door; L.Walls.Add(Entrance);
		for (FIntPoint Edge : Edges)
		{
			FHouseWall Door; Door.RoomA = Edge.X; Door.RoomB = Edge.Y; Door.Opening = EHouseOpening::Door; L.Walls.Add(Door);
		}
		ProfileError.Reset();
		if (!FHouseHousehold::Assign(S, L, ProfileError)) continue;
		const int32 Primary = FHouseNavigation::Measure(L).DeepestRoom;
		int32 MinimumDepth = Front.Depth;
		bool bFits = true;
		for (FSection& Section : Sections)
		{
			Section.Left.Solve(S, L, Front.Left, Primary); Section.Right.Solve(S, L, Front.Right, Primary);
			for (int32 D = HallWidth; D <= S.DepthCells - Front.Depth; ++D)
				if (Section.Left.Reach[0][D] && Section.Right.Reach[0][D] && FHouseSizing::Fits(S, Hall, FIntPoint(HallWidth, D))) Section.Depths.Add(D);
			if (Section.Depths.IsEmpty()) { bFits = false; break; }
			MinimumDepth += Section.Depths[0];
		}
		if (!bFits || MinimumDepth > S.DepthCells) continue;
		L.Rooms[0].Min = FIntPoint(0, 0); L.Rooms[0].Max = FIntPoint(Front.Left + HallWidth, Front.Depth);
		L.Rooms[1].Min = FIntPoint(Front.Left + HallWidth, 0); L.Rooms[1].Max = FIntPoint(Front.Left + HallWidth + Front.Right, Front.Depth);
		int32 Y = Front.Depth;
		// Keep the house compact: at most two spare grid rows beyond the shortest valid packing.
		int32 Spare = FMath::Min(2, S.DepthCells - MinimumDepth);
		for (int32 I = 0; I < Sections.Num(); ++I)
		{
			FSection& Section = Sections[I];
			TArray<int32> Options;
			for (int32 D : Section.Depths) if (D <= Section.Depths[0] + Spare) Options.Add(D);
			const int32 Depth = Options[Random.RandRange(0, Options.Num() - 1)];
			Spare -= Depth - Section.Depths[0];
			L.Rooms[2 + I].Min = FIntPoint(Front.Left, Y); L.Rooms[2 + I].Max = FIntPoint(Front.Left + HallWidth, Y + Depth);
			Section.Left.Place(L, 0, Front.Left, Y, Depth, Random);
			Section.Right.Place(L, Front.Left + HallWidth, Front.Right, Y, Depth, Random);
			Y += Depth;
		}
		L.FootprintCells = FIntPoint(Front.Left + HallWidth + Front.Right, Y);
		if (!FHouseSizing::Validate(S, L, Error)) continue;
		if (Random.RandRange(0, 1))
			for (FHouseRoom& Room : L.Rooms)
			{
				const int32 OldMin = Room.Min.X;
				Room.Min.X = L.FootprintCells.X - Room.Max.X; Room.Max.X = L.FootprintCells.X - OldMin;
			}
		L.Walls.Reset();
		Out = MoveTemp(L); Connections = MoveTemp(Edges); Error.Reset();
		return true;
	}
	Error = ProfileError.IsEmpty() ? TEXT("No residential packing found within the size limits and footprint envelope (512 attempts). Try 100 cm cells and a 24 x 24 envelope; check purpose overrides, lower Room Count/difficulty, or adjust Room Sizing. The house is never stretched to fill spare space.") : ProfileError;
	return false;
}
