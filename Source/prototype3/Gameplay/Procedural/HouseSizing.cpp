#include "Gameplay/Procedural/HouseSizing.h"
#include "Gameplay/Procedural/HouseNavigation.h"

bool FHouseSizing::ValidateSettings(const FHouseGenerationSettings& S, FString& Error)
{
	if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom) return true;
	const FHouseSizingSettings& R = S.RoomSizing;
	for (const FHouseRoomSizeRange* T : {&R.LivingRoom, &R.Kitchen, &R.PrimaryBedroom, &R.OtherBedroom, &R.Bathroom, &R.Office, &R.Storage})
	{
		if (!FMath::IsFinite(T->MinArea) || !FMath::IsFinite(T->MaxArea) || !FMath::IsFinite(T->MinWidth) || !FMath::IsFinite(T->MaxAspectRatio) ||
			T->MinArea < 1 || T->MaxArea < T->MinArea || T->MaxArea > 100 || T->MinWidth < 1 || T->MinWidth > 10 || T->MaxAspectRatio < 1 || T->MaxAspectRatio > 4)
		{ Error = TEXT("Room sizing needs finite area ranges of 1..100 m2, minimum widths of 1..10 m, and aspect ratios of 1..4."); return false; }
	}
	if (!FMath::IsFinite(R.MinHallwayWidth) || !FMath::IsFinite(R.MaxHallwayWidth) || !FMath::IsFinite(R.MaxHallwayLength) ||
		R.MinHallwayWidth < 1 || R.MaxHallwayWidth < R.MinHallwayWidth || R.MaxHallwayWidth > 3 || R.MaxHallwayLength < 2 || R.MaxHallwayLength > 15)
	{ Error = TEXT("Check hallway clear width (1..3 m) and section length (2..15 m)."); return false; }
	return true;
}

const FHouseRoomSizeRange& FHouseSizing::Range(const FHouseGenerationSettings& S, const FHouseRoom& Room, bool bPrimary)
{
	const FHouseSizingSettings& R = S.RoomSizing;
	switch (Room.RoomType)
	{
	case EHouseRoomType::LivingRoom: return R.LivingRoom;
	case EHouseRoomType::Kitchen: return R.Kitchen;
	case EHouseRoomType::Bathroom: return R.Bathroom;
	default: break;
	}
	if (Room.Purpose == EHouseRoomPurpose::Office) return R.Office;
	if (Room.Purpose == EHouseRoomPurpose::Storage) return R.Storage;
	if (Room.Purpose == EHouseRoomPurpose::PrimaryBedroom || (Room.Purpose == EHouseRoomPurpose::Default && bPrimary)) return R.PrimaryBedroom;
	return R.OtherBedroom;
}

FVector2D FHouseSizing::ClearSize(const FHouseGenerationSettings& S, FIntPoint Cells)
{
	return FVector2D(Cells.X * S.CellSize - S.WallThickness, Cells.Y * S.CellSize - S.WallThickness) / 100.0;
}

bool FHouseSizing::Fits(const FHouseGenerationSettings& S, const FHouseRoom& Room, FIntPoint Cells, bool bPrimary)
{
	const FVector2D Size = ClearSize(S, Cells);
	const double Short = FMath::Min(Size.X, Size.Y), Long = FMath::Max(Size.X, Size.Y);
	if (Short <= 0) return false;
	if (Room.RoomType == EHouseRoomType::Hallway)
		return Short >= S.RoomSizing.MinHallwayWidth - 0.001 && Short <= S.RoomSizing.MaxHallwayWidth + 0.001 && Long <= S.RoomSizing.MaxHallwayLength + 0.001;
	const FHouseRoomSizeRange& R = Range(S, Room, bPrimary);
	const double Area = Size.X * Size.Y;
	return Area >= R.MinArea - 0.001 && Area <= R.MaxArea + 0.001 && Short >= R.MinWidth - 0.001 && Long / Short <= R.MaxAspectRatio + 0.001;
}

FIntPoint FHouseSizing::Footprint(const FHouseGenerationSettings& S, const FHouseLayout& L)
{
	return L.FootprintCells == FIntPoint::ZeroValue ? FIntPoint(S.WidthCells, S.DepthCells) : L.FootprintCells;
}

bool FHouseSizing::Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error)
{
	if (S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom) return true;
	const int32 Primary = FHouseNavigation::Measure(L).DeepestRoom;
	double BathroomArea = 0, SmallestBedroom = TNumericLimits<double>::Max();
	for (int32 I = 0; I < L.Rooms.Num(); ++I)
	{
		const FHouseRoom& R = L.Rooms[I];
		if (!Fits(S, R, R.Max - R.Min, I == Primary))
		{ Error = FString::Printf(TEXT("Room %d violates its usable area, width or proportion limits. Check Room Sizing or room-purpose overrides."), I); return false; }
		const FVector2D Size = ClearSize(S, R.Max - R.Min);
		if (R.RoomType == EHouseRoomType::Bathroom) BathroomArea = Size.X * Size.Y;
		if (R.RoomType == EHouseRoomType::Bedroom && R.Purpose != EHouseRoomPurpose::Office && R.Purpose != EHouseRoomPurpose::Storage)
			SmallestBedroom = FMath::Min(SmallestBedroom, Size.X * Size.Y);
	}
	if (BathroomArea > SmallestBedroom + 0.001)
	{ Error = TEXT("Bathroom may not be larger than the smallest bedroom."); return false; }
	return true;
}
