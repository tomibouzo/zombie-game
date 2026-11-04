#include "Gameplay/Procedural/HouseRoomRules.h"

#include "Gameplay/Procedural/HouseHousehold.h"
#include "NativeGameplayTags.h"

namespace HouseRoomTags
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(LivingRoom, "Room.Type.LivingRoom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Kitchen, "Room.Type.Kitchen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Bedroom, "Room.Type.Bedroom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Bathroom, "Room.Type.Bathroom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Hallway, "Room.Type.Hallway");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Default, "Room.Purpose.Default");
UE_DEFINE_GAMEPLAY_TAG_STATIC(PrimaryBedroom, "Room.Purpose.PrimaryBedroom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(GuestBedroom, "Room.Purpose.GuestBedroom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(ChildBedroom, "Room.Purpose.ChildBedroom");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Office, "Room.Purpose.Office");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Storage, "Room.Purpose.Storage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Entrance, "Room.Role.Entrance");
// Example labels have no behavior until a rule uses them. More may be added in Project Settings.
UE_DEFINE_GAMEPLAY_TAG_STATIC(Safe, "Room.Label.Safe");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Loot, "Room.Label.Loot");

bool Under(const FGameplayTag& Tag, const TCHAR* Root)
{
	const FString Name = Tag.ToString();
	return Tag.IsValid() && (Name == Root || Name.StartsWith(FString(Root) + TEXT(".")));
}

bool ValidTags(const FGameplayTagContainer& Tags, const TCHAR* Root)
{
	if (Tags.Num() > 16) return false;
	for (const FGameplayTag& Tag : Tags) if (!Under(Tag, Root)) return false;
	return true;
}

bool Matches(const FHouseRoom& Room, const FHouseRoomRule& Rule)
{
	return !Rule.MatchTags.IsEmpty() && Room.Tags.HasAll(Rule.MatchTags);
}
}

bool FHouseRoomRules::ValidateSettings(const FHouseGenerationSettings& S, FString& Error)
{
	using namespace HouseRoomTags;
	if (S.RoomTagAssignments.Num() > 32 || S.RoomRules.Num() > 32)
	{
		Error = TEXT("Use at most 32 room tag assignments and 32 room rules."); return false;
	}
	for (int32 I = 0; I < S.RoomTagAssignments.Num(); ++I)
	{
		const auto& A = S.RoomTagAssignments[I];
		bool bValid = A.RoomIndex >= INDEX_NONE && A.RoomIndex < S.RoomCount &&
			ValidTags(A.MatchTags, TEXT("Room")) && !A.AdditionalTags.IsEmpty() && ValidTags(A.AdditionalTags, TEXT("Room.Label"));
		for (const FGameplayTag& Tag : A.MatchTags)
			bValid &= Under(Tag, TEXT("Room.Type")) || Under(Tag, TEXT("Room.Purpose")) || Under(Tag, TEXT("Room.Role"));
		for (const FGameplayTag& Tag : A.AdditionalTags) bValid &= Tag.ToString() != TEXT("Room.Label");
		if (!bValid)
		{
			Error = FString::Printf(TEXT("Room tag assignment %d needs a valid room ID (-1 for all), automatic selector tags, and custom Room.Label child tags (at most 16 each)."), I); return false;
		}
	}
	TSet<FName> Names;
	for (const FHouseRoomRule& Rule : S.RoomRules)
	{
		if (Rule.RuleName.IsNone() || Names.Contains(Rule.RuleName) || Rule.MatchTags.IsEmpty() ||
			!ValidTags(Rule.MatchTags, TEXT("Room")) || !ValidTags(Rule.AllowedNeighborTags, TEXT("Room")) ||
			Rule.RequiredFurniture.Num() > static_cast<int32>(EHouseFurnitureType::Count))
		{
			Error = FString::Printf(TEXT("Room rule '%s' needs a unique name, nonempty Room match tags, and valid bounded requirements."), *Rule.RuleName.ToString()); return false;
		}
		Names.Add(Rule.RuleName);
		TSet<EHouseFurnitureType> Types;
		for (const EHouseFurnitureType Type : Rule.RequiredFurniture)
		{
			if (Type >= EHouseFurnitureType::Count || Types.Contains(Type))
			{
				Error = FString::Printf(TEXT("Room rule '%s' has an unknown or duplicate required furniture type."), *Rule.RuleName.ToString()); return false;
			}
			Types.Add(Type);
		}
	}
	return true;
}

void FHouseRoomRules::Refresh(const FHouseGenerationSettings& S, FHouseLayout& L)
{
	using namespace HouseRoomTags;
	const FGameplayTag Types[] = {LivingRoom, Kitchen, Bedroom, Bathroom, Hallway};
	const FGameplayTag Purposes[] = {Default, PrimaryBedroom, GuestBedroom, ChildBedroom, Office, Storage};
	for (int32 I = 0; I < L.Rooms.Num(); ++I)
	{
		FHouseRoom& Room = L.Rooms[I];
		Room.Tags.Reset();
		if (static_cast<uint8>(Room.RoomType) < UE_ARRAY_COUNT(Types)) Room.Tags.AddTag(Types[static_cast<uint8>(Room.RoomType)]);
		if (static_cast<uint8>(Room.Purpose) < UE_ARRAY_COUNT(Purposes)) Room.Tags.AddTag(Purposes[static_cast<uint8>(Room.Purpose)]);
		const bool bEntrance = L.Walls.ContainsByPredicate([I](const FHouseWall& W)
			{ return W.Opening == EHouseOpening::Door && W.RoomB == INDEX_NONE && W.RoomA == I; });
		if (bEntrance) Room.Tags.AddTag(Entrance);
		const FGameplayTagContainer AutomaticTags = Room.Tags;
		for (const auto& A : S.RoomTagAssignments)
			if ((A.RoomIndex == INDEX_NONE || A.RoomIndex == I) && AutomaticTags.HasAll(A.MatchTags)) Room.Tags.AppendTags(A.AdditionalTags);
		Room.bAllowZombieSpawns = !bEntrance;
		Room.bAllowItemSpawns = true;
		for (const auto& Rule : S.RoomRules)
			if (Matches(Room, Rule))
			{
				Room.bAllowZombieSpawns &= Rule.bAllowZombieSpawns;
				Room.bAllowItemSpawns &= Rule.bAllowItemSpawns;
			}
	}
}

bool FHouseRoomRules::ValidateConnections(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error)
{
	using namespace HouseRoomTags;
	for (const auto& Rule : S.RoomRules)
	{
		bool bMatched = false;
		for (int32 I = 0; I < L.Rooms.Num(); ++I)
		{
			if (!Matches(L.Rooms[I], Rule)) continue;
			bMatched = true;
			if (Rule.AllowedNeighborTags.IsEmpty()) continue;
			for (const FHouseWall& Wall : L.Walls)
			{
				if (Wall.Opening != EHouseOpening::Door || Wall.RoomB == INDEX_NONE) continue;
				const int32 Neighbor = Wall.RoomA == I ? Wall.RoomB : Wall.RoomB == I ? Wall.RoomA : INDEX_NONE;
				if (Neighbor == INDEX_NONE) continue;
				if (!L.Rooms.IsValidIndex(Neighbor) || !L.Rooms[Neighbor].Tags.HasAny(Rule.AllowedNeighborTags))
				{
					Error = FString::Printf(TEXT("Room rule '%s': room %d has a disallowed doorway connection to room %d. Adjust allowed neighbor tags or the layout pattern."), *Rule.RuleName.ToString(), I, Neighbor); return false;
				}
			}
		}
		if (Rule.bRequireMatchingRoom && !bMatched)
		{
			Error = FString::Printf(TEXT("Room rule '%s' matches no room in this house."), *Rule.RuleName.ToString()); return false;
		}
	}
	return true;
}

bool FHouseRoomRules::RequiresFurniture(const FHouseGenerationSettings& S, const FHouseRoom& Room, EHouseFurnitureType Type)
{
	for (const auto& Rule : S.RoomRules)
		if (HouseRoomTags::Matches(Room, Rule) && Rule.RequiredFurniture.Contains(Type)) return true;
	return false;
}

bool FHouseRoomRules::WantsFurniture(const FHouseGenerationSettings& S, const FHouseRoom& Room, int32 RoomIndex, int32 RuleIndex)
{
	if (!RequiresFurniture(S, Room, S.FurnitureRules[RuleIndex].Type))
		return FHouseHousehold::WantsFurniture(S, Room, RoomIndex, RuleIndex);
	FHouseRoom EligibleRoom = Room;
	// Explicit requirements override density only, not type, purpose, style, or an unfurnished override.
	EligibleRoom.Density = EHouseFurnishingDensity::Full;
	return FHouseHousehold::WantsFurniture(S, EligibleRoom, RoomIndex, RuleIndex);
}

bool FHouseRoomRules::ValidateFurniture(const FHouseGenerationSettings& S, const FHouseLayout& L, int32 RoomIndex, FString& Error)
{
	for (const auto& Rule : S.RoomRules)
	{
		if (!HouseRoomTags::Matches(L.Rooms[RoomIndex], Rule)) continue;
		for (const EHouseFurnitureType Type : Rule.RequiredFurniture)
			if (!L.Furniture.ContainsByPredicate([&](const FHouseFurniturePlacement& F) { return F.RoomIndex == RoomIndex && F.Type == Type; }))
			{
				Error = FString::Printf(TEXT("Room rule '%s': room %d requires %s, but no eligible piece fits safely. Check furniture generation, catalogue filters, room dimensions, and unfurnished overrides."),
					*Rule.RuleName.ToString(), RoomIndex, *StaticEnum<EHouseFurnitureType>()->GetNameStringByValue(static_cast<int64>(Type))); return false;
			}
	}
	return true;
}

bool FHouseRoomRules::Validate(const FHouseGenerationSettings& S, const FHouseLayout& L, FString& Error)
{
	// Historical serialized layouts predate tag metadata. Empty rules preserve their validation contract.
	if (S.RoomRules.IsEmpty() && S.RoomTagAssignments.IsEmpty()) return true;
	FHouseLayout Expected = L;
	Refresh(S, Expected);
	for (int32 I = 0; I < L.Rooms.Num(); ++I)
	{
		const auto& Room = L.Rooms[I];
		const auto& Derived = Expected.Rooms[I];
		if (Room.Tags != Derived.Tags || Room.bAllowZombieSpawns != Derived.bAllowZombieSpawns || Room.bAllowItemSpawns != Derived.bAllowItemSpawns)
		{
			Error = FString::Printf(TEXT("Room %d has stale tags or spawn permissions. Regenerate the house."), I); return false;
		}
		if (!ValidateFurniture(S, L, I, Error)) return false;
	}
	return ValidateConnections(S, L, Error);
}
