#include "Gameplay/Procedural/HouseHousehold.h"
#include "Gameplay/Procedural/HouseNavigation.h"
#include "Math/RandomStream.h"

int32 FHouseHousehold::RequiredPrivateRooms(const FHouseholdSettings& H)
{
	if (H.Profile == EHouseholdProfile::Family) return 1 + H.ChildrenBedrooms;
	if (H.Profile == EHouseholdProfile::WorkingFromHomeCouple) return 3;
	return 1;
}

bool FHouseHousehold::ValidateSettings(const FHouseGenerationSettings& S, FString& Error)
{
	const FHouseholdSettings& H = S.Household;
	if (H.Profile > EHouseholdProfile::WorkingFromHomeCouple || H.FurnitureStyle > EHouseFurnitureStyle::MixedSecondhand ||
		H.Density > EHouseFurnishingDensity::Full || H.ChildrenBedrooms < 1 || H.ChildrenBedrooms > 4)
	{ Error = TEXT("Unknown household category or Children Bedrooms outside 1..4."); return false; }
	if (H.Profile != EHouseholdProfile::None && S.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom)
	{ Error = TEXT("Household profiles require Central Hub or Deep Branches for private hallway access."); return false; }
	TSet<int32> IDs;
	for (const FHouseRoomOverride& O : H.RoomOverrides)
	{
		if (H.Profile == EHouseholdProfile::None || O.RoomIndex < 0 || O.RoomIndex >= S.RoomCount || IDs.Contains(O.RoomIndex) ||
			O.Purpose > EHouseRoomPurpose::Storage || O.Density > EHouseFurnishingDensity::Full)
		{ Error = TEXT("Room overrides need an enabled household, unique valid room IDs, and valid categories."); return false; }
		IDs.Add(O.RoomIndex);
	}
	return true;
}

bool FHouseHousehold::Assign(const FHouseGenerationSettings& S, FHouseLayout& L, FString& Error)
{
	L.Household = S.Household;
	L.Household.Occupancy = EHouseOccupancy::Settled;
	if (S.Household.Profile == EHouseholdProfile::None) return true;
	TArray<int32> Private;
	for (int32 I = 0; I < L.Rooms.Num(); ++I)
	{
		L.Rooms[I].Density = S.Household.Density;
		if (L.Rooms[I].RoomType == EHouseRoomType::Bedroom) Private.Add(I);
	}
	if (Private.Num() < RequiredPrivateRooms(S.Household))
	{ Error = TEXT("Household needs more private rooms. Increase Room Count/footprint or reduce navigation depth."); return false; }
	// Keep the main navigation destination a real bedroom, even for office-heavy households.
	const int32 Deepest = FHouseNavigation::Measure(L).DeepestRoom;
	Private.Remove(Deepest);
	Private.Insert(Deepest, 0);
	for (int32 I = 0; I < Private.Num(); ++I)
	{
		FHouseRoom& Room = L.Rooms[Private[I]];
		Room.Purpose = I == 0 ? EHouseRoomPurpose::PrimaryBedroom : EHouseRoomPurpose::GuestBedroom;
		if (I > 0 && S.Household.Profile == EHouseholdProfile::Family && I <= S.Household.ChildrenBedrooms)
			Room.Purpose = EHouseRoomPurpose::ChildBedroom;
		if (I > 0 && I <= 2 && S.Household.Profile == EHouseholdProfile::WorkingFromHomeCouple)
			Room.Purpose = EHouseRoomPurpose::Office;
		if (I > 1 && S.Household.Profile == EHouseholdProfile::RetiredCouple) Room.Purpose = EHouseRoomPurpose::Storage;
		if (I > 0 && S.Household.Profile == EHouseholdProfile::RetiredCouple) Room.Density = EHouseFurnishingDensity::Sparse;
	}
	for (const FHouseRoomOverride& O : S.Household.RoomOverrides)
	{
		FHouseRoom& Room = L.Rooms[O.RoomIndex];
		if (O.Purpose != EHouseRoomPurpose::Default)
		{
			if (Room.RoomType != EHouseRoomType::Bedroom)
			{ Error = TEXT("Purpose overrides apply only to private rooms; living room, kitchen, bathroom and hallway access must be preserved."); return false; }
			Room.Purpose = O.Purpose;
		}
		Room.bUnfurnished = O.bUnfurnished;
		if (O.bOverrideDensity) Room.Density = O.Density;
	}
	return Validate(S, L, Error);
}

bool FHouseHousehold::Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error)
{
	if (S.Household.Profile == EHouseholdProfile::None) return true;
	int32 Primary = 0, Children = 0, Offices = 0;
	for (const FHouseRoom& R : L.Rooms)
	{
		if (R.Purpose > EHouseRoomPurpose::Storage || R.Density > EHouseFurnishingDensity::Full ||
			(R.RoomType == EHouseRoomType::Bedroom) == (R.Purpose == EHouseRoomPurpose::Default))
		{ Error = TEXT("Invalid room purpose or furnishing density."); return false; }
		Primary += R.Purpose == EHouseRoomPurpose::PrimaryBedroom;
		Children += R.Purpose == EHouseRoomPurpose::ChildBedroom;
		Offices += R.Purpose == EHouseRoomPurpose::Office;
	}
	if (Primary != 1 || (S.Household.Profile == EHouseholdProfile::Family && Children < S.Household.ChildrenBedrooms) ||
		(S.Household.Profile == EHouseholdProfile::WorkingFromHomeCouple && Offices < 2))
	{ Error = TEXT("Household requires exactly one primary bedroom; Family needs its requested children's bedrooms and Working From Home Couple needs two offices. Check room overrides."); return false; }
	return true;
}

bool FHouseHousehold::WantsFurniture(const FHouseGenerationSettings& S, const FHouseRoom& Room, int32 RoomIndex, int32 RuleIndex)
{
	const FHouseFurnitureRule& Rule = S.FurnitureRules[RuleIndex];
	if (!Rule.AllowedRooms.Contains(Room.RoomType)) return false;
	const FHouseholdSettings& H = S.Household;
	if (Room.bUnfurnished) return false;
	if (H.Profile == EHouseholdProfile::None && Room.Purpose == EHouseRoomPurpose::Default) return !Rule.bHouseholdOnly;
	if ((!Rule.AllowedPurposes.IsEmpty() && !Rule.AllowedPurposes.Contains(Room.Purpose)) ||
		(!Rule.AllowedStyles.IsEmpty() && !Rule.AllowedStyles.Contains(H.FurnitureStyle))) return false;
	if (!Rule.bEssential && Room.Density < Rule.MinimumDensity) return false;
	return true;
}

FString FHouseHousehold::RoomLabel(const FHouseRoom& Room)
{
	return Room.Purpose == EHouseRoomPurpose::Default ? StaticEnum<EHouseRoomType>()->GetDisplayNameTextByValue(static_cast<int64>(Room.RoomType)).ToString() :
		StaticEnum<EHouseRoomPurpose>()->GetDisplayNameTextByValue(static_cast<int64>(Room.Purpose)).ToString();
}
