#include "Gameplay/Player/Inventory/InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	for (const FInventoryContainerDefinition& Definition : InitialContainers)
	{
		if (AddContainer(Definition) != EInventoryResult::Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("Inventory: could not initialize container '%s' on %s"),
				*Definition.Id.ToString(), *GetNameSafe(GetOwner()));
		}
	}
}

int32 UInventoryComponent::FindContainer(FName Id) const
{
	return Containers.IndexOfByPredicate([Id](const FInventoryContainer& Container)
	{
		return Container.Definition.Id == Id;
	});
}

bool UInventoryComponent::FindItem(FGuid Id, int32& OutContainer, int32& OutEntry) const
{
	for (int32 C = 0; C < Containers.Num(); ++C)
	{
		const int32 E = Containers[C].Entries.IndexOfByPredicate([Id](const FInventoryEntry& Entry)
		{
			return Entry.Id == Id;
		});
		if (E != INDEX_NONE)
		{
			OutContainer = C;
			OutEntry = E;
			return true;
		}
	}
	OutContainer = OutEntry = INDEX_NONE;
	return false;
}

bool UInventoryComponent::IsAccessible(const FInventoryContainer& Container)
{
	return Container.Definition.Access == EInventoryAccess::Quick || Container.bOpen;
}

void UInventoryComponent::NotifyChange(UInventoryComponent* Other)
{
	// Both sides are already committed before any listener can inspect their state.
	OnInventoryChanged.Broadcast();
	if (IsValid(Other) && Other != this)
	{
		Other->OnInventoryChanged.Broadcast();
	}
}

EInventoryResult UInventoryComponent::AddContainer(const FInventoryContainerDefinition& Definition)
{
	if (Definition.Id.IsNone() || FindContainer(Definition.Id) != INDEX_NONE || Definition.Compartments.IsEmpty()
		|| (Definition.Access != EInventoryAccess::Quick && Definition.Access != EInventoryAccess::RequiresOpening))
	{
		return EInventoryResult::InvalidContainer;
	}
	TSet<FName> Ids;
	for (const FInventoryCompartment& Compartment : Definition.Compartments)
	{
		if (Compartment.Id.IsNone() || Ids.Contains(Compartment.Id) || Compartment.Size.X < 1
			|| Compartment.Size.Y < 1 || Compartment.Size.X > 256 || Compartment.Size.Y > 256)
		{
			return EInventoryResult::InvalidCompartment;
		}
		Ids.Add(Compartment.Id);
	}
	FInventoryContainer Container;
	Container.Definition = Definition;
	Containers.Add(MoveTemp(Container));
	NotifyChange();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::TransferContainer(FName ContainerId, UInventoryComponent* Target)
{
	const int32 Index = FindContainer(ContainerId);
	if (!IsValid(Target) || Target == this || Index == INDEX_NONE || Target->FindContainer(ContainerId) != INDEX_NONE)
	{
		return EInventoryResult::InvalidContainer;
	}
	// Keep all entries and their IDs, but require slow storage to be opened again.
	FInventoryContainer Moved = Containers[Index];
	Moved.bOpen = false;
	Target->Containers.Add(MoveTemp(Moved));
	Containers.RemoveAt(Index);
	NotifyChange(Target);
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::SetContainerOpen(FName ContainerId, bool bOpen)
{
	const int32 Index = FindContainer(ContainerId);
	if (Index == INDEX_NONE)
	{
		return EInventoryResult::InvalidContainer;
	}
	if (Containers[Index].bOpen != bOpen)
	{
		Containers[Index].bOpen = bOpen;
		NotifyChange();
	}
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::CheckPlacement(UItemDefinition* Definition, FName ContainerId,
	FName CompartmentId, FIntPoint Position, int32 QuarterTurns, FGuid IgnoredItemId) const
{
	if (!IsValid(Definition) || !Definition->IsValidDefinition() || QuarterTurns < 0 || QuarterTurns > 3
		|| (!Definition->bAllowRotation && QuarterTurns != 0))
	{
		return EInventoryResult::InvalidItem;
	}
	const int32 Index = FindContainer(ContainerId);
	if (Index == INDEX_NONE)
	{
		return EInventoryResult::InvalidContainer;
	}
	const FInventoryContainer& Container = Containers[Index];
	if (!IsAccessible(Container))
	{
		return EInventoryResult::Closed;
	}
	const FInventoryCompartment* Compartment = Container.Definition.Compartments.FindByPredicate(
		[CompartmentId](const FInventoryCompartment& Candidate) { return Candidate.Id == CompartmentId; });
	if (!Compartment)
	{
		return EInventoryResult::InvalidCompartment;
	}
	// Check the anchor first, before adding cell offsets, to avoid integer overflow.
	if (Position.X < 0 || Position.Y < 0 || Position.X >= Compartment->Size.X || Position.Y >= Compartment->Size.Y)
	{
		return EInventoryResult::OutOfBounds;
	}
	TSet<FIntPoint> Blocked;
	for (const FInventoryEntry& Entry : Container.Entries)
	{
		if (Entry.Id == IgnoredItemId || Entry.CompartmentId != CompartmentId)
		{
			continue;
		}
		for (const FIntPoint& Cell : Entry.Definition->GetRotatedCells(Entry.QuarterTurns))
		{
			Blocked.Add(Entry.Position + Cell);
		}
	}
	const TArray<FIntPoint> Cells = Definition->GetRotatedCells(QuarterTurns);
	for (const FIntPoint& Cell : Cells)
	{
		const FIntPoint Destination = Position + Cell;
		if (Destination.X >= Compartment->Size.X || Destination.Y >= Compartment->Size.Y)
		{
			return EInventoryResult::OutOfBounds;
		}
	}
	for (const FIntPoint& Cell : Cells)
	{
		if (Blocked.Contains(Position + Cell))
		{
			return EInventoryResult::Occupied;
		}
	}
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::AddItem(UItemDefinition* Definition, int32 Quantity,
	FName ContainerId, FName CompartmentId, FIntPoint Position, int32 QuarterTurns, FGuid& OutItemId)
{
	OutItemId.Invalidate();
	if (!IsValid(Definition) || !Definition->IsValidDefinition())
	{
		return EInventoryResult::InvalidItem;
	}
	if (Quantity < 1 || Quantity > Definition->MaxStackSize)
	{
		return EInventoryResult::InvalidQuantity;
	}
	const EInventoryResult Result = CheckPlacement(Definition, ContainerId, CompartmentId, Position, QuarterTurns, FGuid());
	if (Result != EInventoryResult::Success)
	{
		return Result;
	}
	FInventoryEntry Entry;
	Entry.Id = FGuid::NewGuid();
	Entry.Definition = Definition;
	Entry.Quantity = Quantity;
	Entry.CompartmentId = CompartmentId;
	Entry.Position = Position;
	Entry.QuarterTurns = QuarterTurns;
	OutItemId = Entry.Id;
	Containers[FindContainer(ContainerId)].Entries.Add(Entry);
	NotifyChange();
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::TransferItem(FGuid ItemId, int32 Quantity, UInventoryComponent* Target,
	FName ContainerId, FName CompartmentId, FIntPoint Position, int32 QuarterTurns, FGuid& OutItemId)
{
	OutItemId.Invalidate();
	int32 SourceContainer, SourceEntry;
	if (!FindItem(ItemId, SourceContainer, SourceEntry))
	{
		return EInventoryResult::InvalidItem;
	}
	if (!IsValid(Target))
	{
		return EInventoryResult::InvalidContainer;
	}
	if (!IsAccessible(Containers[SourceContainer]))
	{
		return EInventoryResult::Closed;
	}
	FInventoryEntry Moved = Containers[SourceContainer].Entries[SourceEntry];
	if (Quantity < 1 || Quantity > Moved.Quantity)
	{
		return EInventoryResult::InvalidQuantity;
	}
	const bool bFullTransfer = Quantity == Moved.Quantity;
	const FGuid Ignored = Target == this && bFullTransfer ? ItemId : FGuid();
	const EInventoryResult Result = Target->CheckPlacement(Moved.Definition, ContainerId,
		CompartmentId, Position, QuarterTurns, Ignored);
	if (Result != EInventoryResult::Success)
	{
		return Result;
	}
	// Validate first, then commit. A rejected transfer never removes the original.
	if (bFullTransfer)
	{
		Containers[SourceContainer].Entries.RemoveAt(SourceEntry);
	}
	else
	{
		Containers[SourceContainer].Entries[SourceEntry].Quantity -= Quantity;
		Moved.Id = FGuid::NewGuid();
	}
	Moved.Quantity = Quantity;
	Moved.CompartmentId = CompartmentId;
	Moved.Position = Position;
	Moved.QuarterTurns = QuarterTurns;
	OutItemId = Moved.Id;
	Target->Containers[Target->FindContainer(ContainerId)].Entries.Add(Moved);
	NotifyChange(Target);
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::StackItems(FGuid SourceItemId, UInventoryComponent* Target,
	FGuid TargetItemId, int32 Quantity)
{
	int32 SourceContainer, SourceEntry, TargetContainer, TargetEntry;
	if (!IsValid(Target) || (Target == this && SourceItemId == TargetItemId)
		|| !FindItem(SourceItemId, SourceContainer, SourceEntry)
		|| !Target->FindItem(TargetItemId, TargetContainer, TargetEntry))
	{
		return EInventoryResult::InvalidItem;
	}
	if (!IsAccessible(Containers[SourceContainer]) || !IsAccessible(Target->Containers[TargetContainer]))
	{
		return EInventoryResult::Closed;
	}
	FInventoryEntry& Source = Containers[SourceContainer].Entries[SourceEntry];
	FInventoryEntry& Destination = Target->Containers[TargetContainer].Entries[TargetEntry];
	if (Source.Definition != Destination.Definition || Source.Definition->MaxStackSize <= 1)
	{
		return EInventoryResult::IncompatibleStack;
	}
	if (Quantity < 1 || Quantity > Source.Quantity)
	{
		return EInventoryResult::InvalidQuantity;
	}
	if (Quantity > Destination.Definition->MaxStackSize - Destination.Quantity)
	{
		return EInventoryResult::StackFull;
	}
	Destination.Quantity += Quantity;
	Source.Quantity -= Quantity;
	if (Source.Quantity == 0)
	{
		Containers[SourceContainer].Entries.RemoveAt(SourceEntry);
	}
	NotifyChange(Target);
	return EInventoryResult::Success;
}

EInventoryResult UInventoryComponent::RemoveItem(FGuid ItemId, int32 Quantity)
{
	int32 C, E;
	if (!FindItem(ItemId, C, E))
	{
		return EInventoryResult::InvalidItem;
	}
	if (!IsAccessible(Containers[C]))
	{
		return EInventoryResult::Closed;
	}
	FInventoryEntry& Entry = Containers[C].Entries[E];
	if (Quantity < 1 || Quantity > Entry.Quantity)
	{
		return EInventoryResult::InvalidQuantity;
	}
	Entry.Quantity -= Quantity;
	if (Entry.Quantity == 0)
	{
		Containers[C].Entries.RemoveAt(E);
	}
	NotifyChange();
	return EInventoryResult::Success;
}

bool UInventoryComponent::GetItem(FGuid ItemId, FInventoryEntry& OutItem, FName& OutContainerId) const
{
	int32 C, E;
	if (!FindItem(ItemId, C, E))
	{
		OutItem = FInventoryEntry();
		OutContainerId = NAME_None;
		return false;
	}
	OutItem = Containers[C].Entries[E];
	OutContainerId = Containers[C].Definition.Id;
	return true;
}

void UInventoryComponent::CountCells(const FInventoryContainer& Container, int64& Occupied, int64& Capacity)
{
	for (const FInventoryCompartment& Compartment : Container.Definition.Compartments)
	{
		Capacity += static_cast<int64>(Compartment.Size.X) * Compartment.Size.Y;
	}
	for (const FInventoryEntry& Entry : Container.Entries)
	{
		// A stack occupies its silhouette once, regardless of its quantity.
		Occupied += Entry.Definition->OccupiedCells.Num();
	}
}

float UInventoryComponent::GetContainerFillRatio(FName ContainerId) const
{
	const int32 Index = FindContainer(ContainerId);
	int64 Occupied = 0, Capacity = 0;
	if (Index != INDEX_NONE)
	{
		CountCells(Containers[Index], Occupied, Capacity);
	}
	return Capacity > 0 ? static_cast<float>(Occupied) / Capacity : 0.0f;
}

EInventoryLoad UInventoryComponent::GetContainerLoad(FName ContainerId) const
{
	return ClassifyLoad(GetContainerFillRatio(ContainerId));
}

float UInventoryComponent::GetFillRatio() const
{
	int64 Occupied = 0, Capacity = 0;
	for (const FInventoryContainer& Container : Containers)
	{
		CountCells(Container, Occupied, Capacity);
	}
	return Capacity > 0 ? static_cast<float>(Occupied) / Capacity : 0.0f;
}

EInventoryLoad UInventoryComponent::ClassifyLoad(float Ratio) const
{
	const float Low = FMath::Clamp(FMath::IsFinite(LowLoadMaxRatio) ? LowLoadMaxRatio : 1.0f / 3.0f, 0.0f, 1.0f);
	const float Medium = FMath::Clamp(FMath::IsFinite(MediumLoadMaxRatio) ? MediumLoadMaxRatio : 2.0f / 3.0f, Low, 1.0f);
	return Ratio <= Low ? EInventoryLoad::Low : (Ratio <= Medium ? EInventoryLoad::Medium : EInventoryLoad::High);
}

EInventoryLoad UInventoryComponent::GetLoad() const
{
	return ClassifyLoad(GetFillRatio());
}

float UInventoryComponent::GetMovementSpeedMultiplier() const
{
	const EInventoryLoad Load = GetLoad();
	const float Value = Load == EInventoryLoad::High ? HighSpeedMultiplier
		: (Load == EInventoryLoad::Medium ? MediumSpeedMultiplier : 1.0f);
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.1f, 1.0f) : 1.0f;
}

float UInventoryComponent::GetStaminaDrainMultiplier() const
{
	const EInventoryLoad Load = GetLoad();
	const float Value = Load == EInventoryLoad::High ? HighStaminaMultiplier
		: (Load == EInventoryLoad::Medium ? MediumStaminaMultiplier : 1.0f);
	return FMath::IsFinite(Value) ? FMath::Max(Value, 1.0f) : 1.0f;
}
