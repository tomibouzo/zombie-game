#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UInventoryComponent::FindItem(FGuid Id) const
{
	return Entries.IndexOfByPredicate([Id](const FInventoryEntry& Entry) { return Entry.Item.InstanceId == Id; });
}

EInventoryResult UInventoryComponent::RegisterProfile(const FInventoryItemProfile& Profile)
{
	if (!Profile.IsValid()) return EInventoryResult::InvalidProfile;
	if (Profiles.ContainsByPredicate([&](const FInventoryItemProfile& P) { return P.Id == Profile.Id; })) return EInventoryResult::DuplicateId;
	Profiles.Add(Profile);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::AddPocket(const FInventoryPocket& Pocket)
{
	if (!FMath::IsFinite(Pocket.MaxItemMassKg) || Pocket.MaxItemMassKg < 0) return EInventoryResult::InvalidPocket;
	if (Pocket.Id.IsNone() || !InventoryGeometry::IsFinite(Pocket.Size) || Pocket.Size.X < 1 || Pocket.Size.Y < 1 || Pocket.Size.X > 1000000 || Pocket.Size.Y > 1000000) return EInventoryResult::InvalidPocket;
	if (Pockets.ContainsByPredicate([&](const FInventoryPocket& P) { return P.Id == Pocket.Id; })) return EInventoryResult::DuplicateId;
	Pockets.Add(Pocket);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

bool UInventoryComponent::GetProfile(FName ProfileId, FInventoryItemProfile& OutProfile) const
{
	const FInventoryItemProfile* Profile = Profiles.FindByPredicate([&](const FInventoryItemProfile& P) { return P.Id == ProfileId; });
	OutProfile = Profile ? *Profile : FInventoryItemProfile();
	return Profile != nullptr;
}

bool UInventoryComponent::GetItem(FGuid InstanceId, FInventoryEntry& OutEntry) const
{
	const int32 Index = FindItem(InstanceId);
	OutEntry = Index != INDEX_NONE ? Entries[Index] : FInventoryEntry();
	return Index != INDEX_NONE;
}

EInventoryResult UInventoryComponent::ValidatePlacement(FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees, FGuid IgnoredId) const
{
	FInventoryItemProfile Profile;
	if (!GetProfile(ProfileId, Profile) || !Profile.IsValid()) return EInventoryResult::InvalidProfile;
	if (!FMath::IsFinite(AngleDegrees) || (!Profile.bAllowRotation && InventoryGeometry::NormalizeAngle(AngleDegrees) != 0)) return EInventoryResult::InvalidRotation;
	const FInventoryPocket* Pocket = Pockets.FindByPredicate([&](const FInventoryPocket& P) { return P.Id == PocketId; });
	if (!Pocket) return EInventoryResult::InvalidPocket;
	if (Profile.Definition)
	{
		if (Pocket->MaxItemMassKg > 0 && Profile.Definition->MassKg > Pocket->MaxItemMassKg) return EInventoryResult::TooHeavy;
		const auto Firearm = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Weapon.Firearm"), false);
		if (!Pocket->bAllowFirearms && Firearm.IsValid() && Profile.Definition->Category.MatchesTag(Firearm)) return EInventoryResult::Incompatible;
	}
	if (!InventoryGeometry::IsFinite(Position) || Position.X < 0 || Position.Y < 0 || Position.X > Pocket->Size.X || Position.Y > Pocket->Size.Y) return EInventoryResult::OutOfBounds;
	const auto Candidate = Profile.GetTransformedParts(Position, AngleDegrees);
	for (const auto& Part : Candidate)
	{
		for (FVector2D P : Part.Vertices)
			if (P.X < -InventoryGeometry::Tolerance || P.Y < -InventoryGeometry::Tolerance || P.X > Pocket->Size.X + InventoryGeometry::Tolerance || P.Y > Pocket->Size.Y + InventoryGeometry::Tolerance) return EInventoryResult::OutOfBounds;
	}
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.PocketId != PocketId || Entry.Item.InstanceId == IgnoredId) continue;
		FInventoryItemProfile Other;
		if (!GetProfile(Entry.ProfileId, Other)) return EInventoryResult::InvalidProfile;
		for (const auto& OtherPart : Other.GetTransformedParts(Entry.Position, Entry.AngleDegrees))
			for (const auto& Part : Candidate) if (Part.Overlaps(OtherPart)) return EInventoryResult::Occupied;
	}
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::CheckPlacement(FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees) const
{
	return ValidatePlacement(ProfileId, PocketId, Position, AngleDegrees, FGuid());
}

EInventoryResult UInventoryComponent::CheckMove(FGuid InstanceId, FName PocketId, FVector2D Position, double AngleDegrees) const
{
	if (IsReserved(InstanceId)) return EInventoryResult::InUse;
	const int32 Index = FindItem(InstanceId);
	if (Index == INDEX_NONE) return EInventoryResult::NotFound;
	if (!Entries[Index].Item.IsValid()) return EInventoryResult::InvalidItem;
	return ValidatePlacement(Entries[Index].ProfileId, PocketId, Position, AngleDegrees, InstanceId);
}

EInventoryResult UInventoryComponent::AddItem(const FItemInstance& Item, FName ProfileId, FName PocketId, FVector2D Position, double AngleDegrees)
{
	if (!Item.IsValid()) return EInventoryResult::InvalidItem;
	if (FindItem(Item.InstanceId) != INDEX_NONE) return EInventoryResult::DuplicateId;
	FInventoryItemProfile Profile;
	if (!GetProfile(ProfileId, Profile) || Profile.Definition != Item.Definition) return EInventoryResult::InvalidProfile;
	const EInventoryResult Result = CheckPlacement(ProfileId, PocketId, Position, AngleDegrees);
	if (Result != EInventoryResult::Success) return Result;
	FInventoryEntry Entry;
	Entry.Item = Item;
	Entry.ProfileId = ProfileId;
	Entry.PocketId = PocketId;
	Entry.Position = Position;
	Entry.AngleDegrees = InventoryGeometry::NormalizeAngle(AngleDegrees);
	Entries.Add(Entry);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::MoveItem(FGuid InstanceId, FName PocketId, FVector2D Position, double AngleDegrees)
{
	const EInventoryResult Result = CheckMove(InstanceId, PocketId, Position, AngleDegrees);
	if (Result != EInventoryResult::Success) return Result;
	FInventoryEntry& Entry = Entries[FindItem(InstanceId)];
	AngleDegrees = InventoryGeometry::NormalizeAngle(AngleDegrees);
	if (Entry.PocketId == PocketId && Entry.Position == Position && Entry.AngleDegrees == AngleDegrees) return EInventoryResult::Success;
	Entry.PocketId = PocketId;
	Entry.Position = Position;
	Entry.AngleDegrees = AngleDegrees;
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::RemoveItem(FGuid InstanceId, FItemInstance& OutItem)
{
	OutItem = FItemInstance();
	if (IsReserved(InstanceId)) return EInventoryResult::InUse;
	const int32 Index = FindItem(InstanceId);
	if (Index == INDEX_NONE) return EInventoryResult::NotFound;
	OutItem = Entries[Index].Item;
	Entries.RemoveAt(Index);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

bool UInventoryComponent::ReserveItem(FGuid Id)
{
	if (FindItem(Id) == INDEX_NONE || IsReserved(Id)) return false;
	ReservedItems.Add(Id);
	return true;
}

bool UInventoryComponent::ConsumeReservedItem(FGuid Id)
{
	const int32 Index = FindItem(Id);
	if (Index == INDEX_NONE || !IsReserved(Id) || !Entries[Index].Item.IsValid()) return false;
	if (--Entries[Index].Item.Quantity == 0) Entries.RemoveAt(Index);
	ReservedItems.Remove(Id);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::FindSpace(FGuid Id, FName PocketId, FVector2D& OutPosition) const
{
	const auto* Pocket = Pockets.FindByPredicate([&](const auto& P) { return P.Id == PocketId; });
	FInventoryEntry Entry;
	if (!Pocket || !GetItem(Id, Entry)) return false;
	// Prototype auto-transfer samples positions; ordinary dragging remains continuous.
	for (double Y = 0; Y <= Pocket->Size.Y; Y += 4)
		for (double X = 0; X <= Pocket->Size.X; X += 4)
			if (CheckMove(Id, PocketId, FVector2D(X, Y), Entry.AngleDegrees) == EInventoryResult::Success)
			{
				OutPosition = FVector2D(X, Y);
				return true;
			}
	return false;
}
