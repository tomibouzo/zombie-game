#include "Gameplay/Player/Inventory/InventoryComponent.h"

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
	if (Pocket.Id.IsNone() || Pocket.Size.X < 1 || Pocket.Size.Y < 1 || Pocket.Size.X > 256 || Pocket.Size.Y > 256) return EInventoryResult::InvalidPocket;
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

EInventoryResult UInventoryComponent::ValidatePlacement(FName ProfileId, FName PocketId, FIntPoint Position, int32 QuarterTurns, FGuid IgnoredId) const
{
	FInventoryItemProfile Profile;
	if (!GetProfile(ProfileId, Profile) || !Profile.IsValid()) return EInventoryResult::InvalidProfile;
	if (QuarterTurns < 0 || QuarterTurns > 3 || (!Profile.bAllowRotation && QuarterTurns != 0)) return EInventoryResult::InvalidRotation;
	const FInventoryPocket* Pocket = Pockets.FindByPredicate([&](const FInventoryPocket& P) { return P.Id == PocketId; });
	if (!Pocket) return EInventoryResult::InvalidPocket;
	// Check anchor before adding offsets, including extreme caller-provided coordinates.
	if (Position.X < 0 || Position.Y < 0 || Position.X >= Pocket->Size.X || Position.Y >= Pocket->Size.Y) return EInventoryResult::OutOfBounds;
	TSet<FIntPoint> Candidate;
	for (FIntPoint Cell : Profile.GetRotatedCells(QuarterTurns))
	{
		Cell += Position;
		if (Cell.X >= Pocket->Size.X || Cell.Y >= Pocket->Size.Y) return EInventoryResult::OutOfBounds;
		Candidate.Add(Cell);
	}
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.PocketId != PocketId || Entry.Item.InstanceId == IgnoredId) continue;
		FInventoryItemProfile Other;
		if (!GetProfile(Entry.ProfileId, Other)) return EInventoryResult::InvalidProfile;
		for (FIntPoint Cell : Other.GetRotatedCells(Entry.QuarterTurns))
			if (Candidate.Contains(Cell + Entry.Position)) return EInventoryResult::Occupied;
	}
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::CheckPlacement(FName ProfileId, FName PocketId, FIntPoint Position, int32 QuarterTurns) const
{
	return ValidatePlacement(ProfileId, PocketId, Position, QuarterTurns, FGuid());
}

EInventoryResult UInventoryComponent::CheckMove(FGuid InstanceId, FName PocketId, FIntPoint Position, int32 QuarterTurns) const
{
	const int32 Index = FindItem(InstanceId);
	if (Index == INDEX_NONE) return EInventoryResult::NotFound;
	if (!Entries[Index].Item.IsValid()) return EInventoryResult::InvalidItem;
	return ValidatePlacement(Entries[Index].ProfileId, PocketId, Position, QuarterTurns, InstanceId);
}

EInventoryResult UInventoryComponent::AddItem(const FItemInstance& Item, FName ProfileId, FName PocketId, FIntPoint Position, int32 QuarterTurns)
{
	if (!Item.IsValid()) return EInventoryResult::InvalidItem;
	if (FindItem(Item.InstanceId) != INDEX_NONE) return EInventoryResult::DuplicateId;
	FInventoryItemProfile Profile;
	if (!GetProfile(ProfileId, Profile) || Profile.Definition != Item.Definition) return EInventoryResult::InvalidProfile;
	const EInventoryResult Result = CheckPlacement(ProfileId, PocketId, Position, QuarterTurns);
	if (Result != EInventoryResult::Success) return Result;
	FInventoryEntry Entry;
	Entry.Item = Item;
	Entry.ProfileId = ProfileId;
	Entry.PocketId = PocketId;
	Entry.Position = Position;
	Entry.QuarterTurns = QuarterTurns;
	Entries.Add(Entry);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::MoveItem(FGuid InstanceId, FName PocketId, FIntPoint Position, int32 QuarterTurns)
{
	const EInventoryResult Result = CheckMove(InstanceId, PocketId, Position, QuarterTurns);
	if (Result != EInventoryResult::Success) return Result;
	FInventoryEntry& Entry = Entries[FindItem(InstanceId)];
	if (Entry.PocketId == PocketId && Entry.Position == Position && Entry.QuarterTurns == QuarterTurns) return EInventoryResult::Success;
	Entry.PocketId = PocketId;
	Entry.Position = Position;
	Entry.QuarterTurns = QuarterTurns;
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::RemoveItem(FGuid InstanceId, FItemInstance& OutItem)
{
	OutItem = FItemInstance();
	const int32 Index = FindItem(InstanceId);
	if (Index == INDEX_NONE) return EInventoryResult::NotFound;
	OutItem = Entries[Index].Item;
	Entries.RemoveAt(Index);
	OnInventoryChanged.Broadcast();
	return EInventoryResult::Success;
}
