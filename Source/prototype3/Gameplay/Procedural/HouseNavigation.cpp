#include "Gameplay/Procedural/HouseNavigation.h"

#include "Algo/Reverse.h"
#include "Gameplay/Procedural/HouseResidentialPlan.h"
#include "Gameplay/Procedural/HouseHousehold.h"
#include "Math/RandomStream.h"

namespace
{
	TArray<TArray<int32>> DoorGraph(const FHouseLayout& L)
	{
		TArray<TArray<int32>> Graph;
		Graph.SetNum(L.Rooms.Num());
		for (const FHouseWall& Wall : L.Walls)
		{
			if (Wall.Opening != EHouseOpening::Door || !Graph.IsValidIndex(Wall.RoomA) || !Graph.IsValidIndex(Wall.RoomB)) continue;
			Graph[Wall.RoomA].AddUnique(Wall.RoomB);
			Graph[Wall.RoomB].AddUnique(Wall.RoomA);
		}
		return Graph;
	}
}

int32 FHouseNavigation::TargetDepth(const FHouseNavigationSettings& S)
{
	if (S.Difficulty == EHouseNavigationDifficulty::Custom) return S.CustomRouteDepth;
	return 2 + static_cast<int32>(S.Difficulty);
}

int32 FHouseNavigation::MinimumRoomCount(const FHouseGenerationSettings& S)
{
	if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom) return 1;
	const int32 Halls = TargetDepth(S.Navigation) - 1;
	return FMath::Max(2 + 3 * Halls, Halls + 3 + FHouseHousehold::RequiredPrivateRooms(S.Household));
}

bool FHouseNavigation::ValidateSettings(const FHouseGenerationSettings& S, FString& OutError)
{
	if (S.Navigation.Pattern > EHouseLayoutPattern::DeepBranches || S.Navigation.Difficulty > EHouseNavigationDifficulty::Custom)
	{
		OutError = TEXT("Unknown navigation pattern or difficulty.");
		return false;
	}
	if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom) return true;
	if (S.MinRoomCells < 3 || S.RoomCount < 1 || S.RoomCount > 32 || S.WidthCells > 64 || S.DepthCells > 64)
	{
		OutError = TEXT("Validate the footprint, room count and minimum room size before planning navigation.");
		return false;
	}
	const int32 Depth = TargetDepth(S.Navigation);
	if (Depth < 2 || Depth > 12 || (S.Navigation.Difficulty == EHouseNavigationDifficulty::Custom &&
		(S.Navigation.CustomMaxDeadEnds < 3 || S.Navigation.CustomMaxDeadEnds > 31)))
	{
		OutError = TEXT("Custom route depth must be 2..12 and maximum dead ends must be 3..31 (kitchen, bathroom and bedroom).");
		return false;
	}
	const int32 Halls = Depth - 1;
	const int32 PrivateRooms = S.RoomCount - 2 - Halls;
	const int32 MinimumRooms = MinimumRoomCount(S);
	if (S.RoomCount < MinimumRooms)
	{
		OutError = FString::Printf(TEXT("This difficulty needs at least %d rooms: living room, kitchen and %d hallways, each serving at least two private rooms. This minimum includes the household room requirements. Increase Room Count or lower difficulty."), MinimumRooms, Halls);
		return false;
	}
	if (S.Navigation.Difficulty == EHouseNavigationDifficulty::Custom && PrivateRooms + 1 > S.Navigation.CustomMaxDeadEnds)
	{
		OutError = TEXT("Custom Max Dead Ends is too small for the requested room count and useful hallway sections (private rooms plus kitchen). Increase the limit or reduce Room Count.");
		return false;
	}
	return true;
}

bool FHouseNavigation::BuildPlan(const FHouseGenerationSettings& S, FHouseLayout& L,
	TArray<FIntPoint>& Connections, FString& OutError)
{
	if (!ValidateSettings(S, OutError)) return false;
	return FHouseResidentialPlan::Build(S, L, Connections, OutError);
}

FHouseNavigationReport FHouseNavigation::Measure(const FHouseLayout& L)
{
	FHouseNavigationReport R;
	const TArray<TArray<int32>> Graph = DoorGraph(L);
	R.RoomDoorDepths.Init(INDEX_NONE, L.Rooms.Num());
	for (const FHouseWall& Wall : L.Walls)
		if (Wall.Opening == EHouseOpening::Door && Wall.RoomB == INDEX_NONE && Graph.IsValidIndex(Wall.RoomA)) { R.EntranceRoom = Wall.RoomA; break; }
	if (R.EntranceRoom == INDEX_NONE) return R;
	TArray<int32> Parent;
	Parent.Init(INDEX_NONE, Graph.Num());
	TArray<int32> Queue{R.EntranceRoom};
	R.RoomDoorDepths[R.EntranceRoom] = 0;
	R.DeepestRoom = R.EntranceRoom;
	int32 DegreeSum = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 ID = Queue[Head], Depth = R.RoomDoorDepths[ID];
		DegreeSum += Graph[ID].Num();
		R.AverageDoorDepth += Depth;
		if (Graph[ID].Num() >= 3) ++R.JunctionCount;
		if (ID != R.EntranceRoom && Graph[ID].Num() == 1) R.DeadEndRooms.Add(ID);
		if (Depth > R.MaxDoorDepth || (Depth == R.MaxDoorDepth && L.Rooms[ID].RoomType == EHouseRoomType::Bedroom &&
			L.Rooms[R.DeepestRoom].RoomType != EHouseRoomType::Bedroom))
		{
			R.MaxDoorDepth = Depth;
			R.DeepestRoom = ID;
		}
		for (int32 Other : Graph[ID])
		{
			if (R.RoomDoorDepths[Other] != INDEX_NONE) continue;
			R.RoomDoorDepths[Other] = Depth + 1;
			Parent[Other] = ID;
			Queue.Add(Other);
		}
	}
	R.AverageDoorDepth /= Queue.Num();
	R.LoopCount = DegreeSum / 2 - Queue.Num() + 1;
	for (int32 ID = R.DeepestRoom; ID != INDEX_NONE; ID = Parent[ID]) R.MainRoute.Add(ID);
	Algo::Reverse(R.MainRoute);
	return R;
}

bool FHouseNavigation::IsResidentialConnectionAllowed(EHouseRoomType A, EHouseRoomType B)
{
	if (A == EHouseRoomType::LivingRoom) return B == EHouseRoomType::Kitchen || B == EHouseRoomType::Hallway;
	if (B == EHouseRoomType::LivingRoom) return A == EHouseRoomType::Kitchen || A == EHouseRoomType::Hallway;
	if (A == EHouseRoomType::Hallway) return B == EHouseRoomType::Hallway || B == EHouseRoomType::Bedroom || B == EHouseRoomType::Bathroom;
	if (B == EHouseRoomType::Hallway) return A == EHouseRoomType::Bedroom || A == EHouseRoomType::Bathroom;
	return false;
}

bool FHouseNavigation::ValidatePlan(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& OutError)
{
	if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom) return true;
	const FHouseNavigationReport R = Measure(L);
	const TArray<TArray<int32>> Graph = DoorGraph(L);
	if (R.EntranceRoom == INDEX_NONE || R.RoomDoorDepths.Contains(INDEX_NONE) || R.MaxDoorDepth != TargetDepth(S.Navigation) ||
		R.LoopCount != 0 || R.DeadEndRooms.Num() < 3 ||
		(S.Navigation.Difficulty == EHouseNavigationDifficulty::Custom && R.DeadEndRooms.Num() > S.Navigation.CustomMaxDeadEnds))
	{
		OutError = TEXT("Door graph does not meet the requested route depth, connectivity or dead-end limit.");
		return false;
	}
	if (Graph[R.EntranceRoom].Num() != 2)
	{
		OutError = TEXT("The entrance living room must connect only to one kitchen and one hallway.");
		return false;
	}
	int32 EntryHallway = INDEX_NONE;
	for (int32 Other : Graph[R.EntranceRoom])
		if (L.Rooms[Other].RoomType == EHouseRoomType::Hallway) EntryHallway = Other;
	if (EntryHallway == INDEX_NONE)
	{
		OutError = TEXT("The living room needs a distribution hallway.");
		return false;
	}
	for (int32 ID = 0; ID < L.Rooms.Num(); ++ID)
	{
		const EHouseRoomType Type = L.Rooms[ID].RoomType;
		if (Type == EHouseRoomType::Hallway)
		{
			int32 ServedRooms = 0;
			for (int32 Other : Graph[ID])
				if (L.Rooms[Other].RoomType == EHouseRoomType::Bedroom || L.Rooms[Other].RoomType == EHouseRoomType::Bathroom) ++ServedRooms;
			if (ServedRooms < 2)
			{ OutError = FString::Printf(TEXT("Hallway %d must directly serve at least two private rooms; other hallways and the entrance living room do not count."), ID); return false; }
		}
		for (int32 Other : Graph[ID])
			if (!IsResidentialConnectionAllowed(Type, L.Rooms[Other].RoomType))
			{
				OutError = FString::Printf(TEXT("Invalid room doorway %d -> %d: living rooms connect to kitchens/hallways; bedrooms and bathrooms require hallway access."), ID, Other);
				return false;
			}
		if ((Type == EHouseRoomType::Kitchen && (Graph[ID].Num() != 1 || !Graph[ID].Contains(R.EntranceRoom))) ||
			((Type == EHouseRoomType::Bedroom || Type == EHouseRoomType::Bathroom) && Graph[ID].Num() != 1) ||
			(Type == EHouseRoomType::Hallway && Graph[ID].Num() < 3))
		{
			OutError = TEXT("Private rooms need one hallway door, the kitchen needs one living-room door, and hallways must serve at least two rooms.");
			return false;
		}
	}
	return true;
}

FString FHouseNavigation::Describe(const FHouseGenerationSettings& S, const FHouseNavigationReport& R)
{
	const FString Pattern = StaticEnum<EHouseLayoutPattern>()->GetDisplayNameTextByValue(static_cast<int64>(S.Navigation.Pattern)).ToString();
	const FString Difficulty = S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom ? TEXT("unrated") :
		StaticEnum<EHouseNavigationDifficulty>()->GetDisplayNameTextByValue(static_cast<int64>(S.Navigation.Difficulty)).ToString();
	const FString Target = S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom ? TEXT("") :
		FString::Printf(TEXT(" | Target: %d (%s)"), TargetDepth(S.Navigation), R.MaxDoorDepth == TargetDepth(S.Navigation) ? TEXT("met") : TEXT("not met"));
	return FString::Printf(TEXT("%s / %s | Deepest: %d interior doors%s | Average: %.1f | Dead ends: %d | Junctions: %d | Loops: %d | Deepest room: %d. Graph distances, not walking distances."),
		*Pattern, *Difficulty, R.MaxDoorDepth, *Target, R.AverageDoorDepth, R.DeadEndRooms.Num(), R.JunctionCount, R.LoopCount, R.DeepestRoom);
}
