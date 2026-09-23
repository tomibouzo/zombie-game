#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Core/Characters/prototype3Character.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace InventoryTests
{
	FInventoryContainerDefinition Storage(FIntPoint Size, EInventoryAccess Access = EInventoryAccess::Quick)
	{
		FInventoryContainerDefinition Result;
		Result.Access = Access;
		Result.Compartments[0].Size = Size;
		return Result;
	}

	UInventoryComponent* Inventory(FIntPoint Size)
	{
		UInventoryComponent* Result = NewObject<UInventoryComponent>();
		Result->AddContainer(Storage(Size));
		return Result;
	}

	UItemDefinition* Item(std::initializer_list<FIntPoint> Cells = { FIntPoint(0, 0) }, int32 StackLimit = 1)
	{
		UItemDefinition* Result = NewObject<UItemDefinition>();
		Result->OccupiedCells = Cells;
		Result->MaxStackSize = StackLimit;
		return Result;
	}

	EInventoryResult Add(UInventoryComponent* Inventory, UItemDefinition* Item, FGuid& Id,
		FIntPoint Position = FIntPoint::ZeroValue, int32 Quantity = 1, int32 Rotation = 0)
	{
		return Inventory->AddItem(Item, Quantity, TEXT("Storage"), TEXT("Main"), Position, Rotation, Id);
	}

	FInventoryEntry Read(UInventoryComponent* Inventory, FGuid Id)
	{
		FInventoryEntry Entry;
		FName Container;
		Inventory->GetItem(Id, Entry, Container);
		return Entry;
	}
}

using namespace InventoryTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryShapeTest, "Prototype.Inventory.ShapeAndRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryShapeTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inv = Inventory(FIntPoint(4, 2));
	UItemDefinition* Rifle = Item({ {0,0}, {1,0}, {2,0}, {3,0}, {0,1}, {2,1} });
	FGuid RifleId, SmallId, FailedId;
	TestEqual(TEXT("Place irregular rifle"), Add(Inv, Rifle, RifleId), EInventoryResult::Success);
	TestEqual(TEXT("Use gap between protruding parts"), Add(Inv, Item(), SmallId, {1,1}), EInventoryResult::Success);
	TestEqual(TEXT("Occupied cell rejects overlap"), Add(Inv, Item(), FailedId, {2,1}), EInventoryResult::Occupied);
	TestFalse(TEXT("Failed add returns no ID"), FailedId.IsValid());
	TestEqual(TEXT("Silhouette counts six, not eight cells"), Inv->GetFillRatio(), 7.0f / 8.0f);
	TestEqual(TEXT("Negative anchor rejected"), Add(Inv, Item(), FailedId, {-1,0}), EInventoryResult::OutOfBounds);
	TestEqual(TEXT("Extreme anchor rejected without overflow"), Add(Inv, Item(), FailedId, {MAX_int32,0}), EInventoryResult::OutOfBounds);

	UInventoryComponent* Narrow = Inventory({2,4});
	TestEqual(TEXT("Unrotated rifle cannot fit narrow grid"), Add(Narrow, Rifle, FailedId), EInventoryResult::OutOfBounds);
	TestEqual(TEXT("90 degree rotation fits"), Add(Narrow, Rifle, RifleId, {0,0}, 1, 1), EInventoryResult::Success);
	TestEqual(TEXT("Rotated gap remains usable"), Add(Narrow, Item(), SmallId, {0,1}), EInventoryResult::Success);
	Rifle->bAllowRotation = false;
	TestEqual(TEXT("Rotation can be disabled per definition"), Add(Inventory({4,4}), Rifle, FailedId, {0,0}, 1, 1), EInventoryResult::InvalidItem);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryTransferTest, "Prototype.Inventory.AtomicTransfers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryTransferTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Source = Inventory({3,1});
	UInventoryComponent* Target = Inventory({1,1});
	FGuid Id, Blocker, Moved;
	UItemDefinition* Bandage = Item();
	Add(Source, Bandage, Id);
	Add(Target, Bandage, Blocker);
	TestEqual(TEXT("Occupied destination fails"), Source->TransferItem(Id, 1, Target, TEXT("Storage"), TEXT("Main"), {0,0}, 0, Moved), EInventoryResult::Occupied);
	TestEqual(TEXT("Failed transfer preserves source"), Read(Source, Id).Id, Id);
	TestEqual(TEXT("Failed transfer preserves destination"), Read(Target, Blocker).Id, Blocker);
	TestEqual(TEXT("Self-move ignores own cells"), Source->TransferItem(Id, 1, Source, TEXT("Storage"), TEXT("Main"), {0,0}, 0, Moved), EInventoryResult::Success);
	TestEqual(TEXT("Self-move preserves ID"), Moved, Id);
	TestEqual(TEXT("Move into empty space"), Source->TransferItem(Id, 1, Source, TEXT("Storage"), TEXT("Main"), {2,0}, 0, Moved), EInventoryResult::Success);
	TestEqual(TEXT("New anchor recorded"), Read(Source, Id).Position, FIntPoint(2,0));
	Target->RemoveItem(Blocker, 1);
	TestEqual(TEXT("Transfer after freeing destination"), Source->TransferItem(Id, 1, Target, TEXT("Storage"), TEXT("Main"), {0,0}, 0, Moved), EInventoryResult::Success);
	TestFalse(TEXT("Source no longer contains item"), Read(Source, Id).Id.IsValid());
	TestEqual(TEXT("Destination preserves ID"), Read(Target, Id).Id, Id);
	TestEqual(TEXT("Source occupancy released"), Source->GetFillRatio(), 0.0f);
	TestEqual(TEXT("Destination occupancy counted"), Target->GetFillRatio(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryStackTest, "Prototype.Inventory.StackingAndSplitting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryStackTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inv = Inventory({6,1});
	UItemDefinition* Ammo = Item({{0,0}}, 30);
	UItemDefinition* Bandage = Item();
	FGuid A, B, C, D, Split;
	TestEqual(TEXT("Bandages cannot enter as a stack"), Add(Inv, Bandage, A, {0,0}, 2), EInventoryResult::InvalidQuantity);
	Add(Inv, Ammo, A, {0,0}, 20);
	Add(Inv, Ammo, B, {1,0}, 15);
	TestEqual(TEXT("Reject exceeding stack cap"), Inv->StackItems(A, Inv, B, 20), EInventoryResult::StackFull);
	TestEqual(TEXT("Rejected merge unchanged"), Read(Inv, A).Quantity, 20);
	TestEqual(TEXT("Partial merge"), Inv->StackItems(A, Inv, B, 10), EInventoryResult::Success);
	TestEqual(TEXT("Source reduced"), Read(Inv, A).Quantity, 10);
	TestEqual(TEXT("Destination increased"), Read(Inv, B).Quantity, 25);
	TestEqual(TEXT("Split stack"), Inv->TransferItem(B, 5, Inv, TEXT("Storage"), TEXT("Main"), {2,0}, 0, Split), EInventoryResult::Success);
	TestTrue(TEXT("Split has a new identity"), Split.IsValid() && Split != B);
	TestEqual(TEXT("Split quantity"), Read(Inv, Split).Quantity, 5);
	TestEqual(TEXT("Remainder quantity"), Read(Inv, B).Quantity, 20);
	TestEqual(TEXT("Merge entire remainder"), Inv->StackItems(A, Inv, B, 10), EInventoryResult::Success);
	TestFalse(TEXT("Empty source removed"), Read(Inv, A).Id.IsValid());
	TestEqual(TEXT("Same stack cannot merge with itself"), Inv->StackItems(B, Inv, B, 1), EInventoryResult::InvalidItem);
	Add(Inv, Bandage, C, {0,0});
	Add(Inv, Bandage, D, {3,0});
	TestEqual(TEXT("Two separate bandages cannot merge"), Inv->StackItems(C, Inv, D, 1), EInventoryResult::IncompatibleStack);
	UInventoryComponent* Other = Inventory({2,1});
	FGuid OtherAmmo;
	Add(Other, Item({{0,0}}, 30), OtherAmmo, {0,0}, 2);
	TestEqual(TEXT("Different ammo definitions cannot merge"), Inv->StackItems(B, Other, OtherAmmo, 1), EInventoryResult::IncompatibleStack);
	Other->RemoveItem(OtherAmmo, 2);
	Add(Other, Ammo, OtherAmmo, {0,0}, 2);
	TestEqual(TEXT("Merge across inventories"), Inv->StackItems(Split, Other, OtherAmmo, 5), EInventoryResult::Success);
	TestEqual(TEXT("External stack grows"), Read(Other, OtherAmmo).Quantity, 7);
	TestFalse(TEXT("External merge removes exhausted source"), Read(Inv, Split).Id.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryContainerTest, "Prototype.Inventory.AccessAndLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryContainerTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inv = NewObject<UInventoryComponent>();
	FInventoryContainerDefinition Bag = Storage({2,1}, EInventoryAccess::RequiresOpening);
	FInventoryCompartment Layer;
	Layer.Id = TEXT("Front");
	Layer.Size = {1,1};
	Bag.Compartments.Add(Layer);
	TestEqual(TEXT("Add layered bag"), Inv->AddContainer(Bag), EInventoryResult::Success);
	FGuid Id, OtherId;
	UItemDefinition* Small = Item();
	TestEqual(TEXT("Slow inventory starts closed"), Add(Inv, Small, Id), EInventoryResult::Closed);
	Inv->SetContainerOpen(TEXT("Storage"), true);
	TestEqual(TEXT("Open bag allows placement"), Add(Inv, Small, Id), EInventoryResult::Success);
	TestEqual(TEXT("Same coordinate in a separate layer is free"), Inv->AddItem(Small, 1, TEXT("Storage"), TEXT("Front"), {0,0}, 0, OtherId), EInventoryResult::Success);
	TestEqual(TEXT("Layers do not combine width"), Add(Inv, Item({{0,0},{1,0},{2,0}}), OtherId), EInventoryResult::OutOfBounds);
	TestEqual(TEXT("Two cells across three-cell capacity"), Inv->GetFillRatio(), 2.0f / 3.0f);
	Inv->SetContainerOpen(TEXT("Storage"), false);
	TestEqual(TEXT("Closed bag prevents consuming"), Inv->RemoveItem(Id, 1), EInventoryResult::Closed);
	UInventoryComponent* WorldStorage = NewObject<UInventoryComponent>();
	TestEqual(TEXT("Drop entire closed bag"), Inv->TransferContainer(TEXT("Storage"), WorldStorage), EInventoryResult::Success);
	TestTrue(TEXT("Contents stay in bag"), Read(WorldStorage, Id).Id.IsValid());
	TestEqual(TEXT("Player no longer carries bag capacity"), Inv->GetContainers().Num(), 0);
	TestEqual(TEXT("Player load reset"), Inv->GetFillRatio(), 0.0f);
	TestEqual(TEXT("Transferred bag remains closed"), WorldStorage->RemoveItem(Id, 1), EInventoryResult::Closed);
	TestEqual(TEXT("Pick up entire bag"), WorldStorage->TransferContainer(TEXT("Storage"), Inv), EInventoryResult::Success);
	TestTrue(TEXT("Round trip preserves content identity"), Read(Inv, Id).Id.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryLoadTest, "Prototype.Inventory.LoadLevels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryLoadTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inv = Inventory({3,1});
	FGuid A, B, C;
	TestEqual(TEXT("Empty is low"), Inv->GetLoad(), EInventoryLoad::Low);
	TestEqual(TEXT("Empty has normal movement"), Inv->GetMovementSpeedMultiplier(), 1.0f);
	Add(Inv, Item(), A);
	TestEqual(TEXT("Exactly one third is low"), Inv->GetLoad(), EInventoryLoad::Low);
	Add(Inv, Item(), B, {1,0});
	TestEqual(TEXT("Exactly two thirds is medium"), Inv->GetLoad(), EInventoryLoad::Medium);
	TestEqual(TEXT("Medium movement"), Inv->GetMovementSpeedMultiplier(), 0.95f);
	Add(Inv, Item({{0,0}}, 30), C, {2,0}, 30);
	TestEqual(TEXT("Above two thirds is high"), Inv->GetLoad(), EInventoryLoad::High);
	TestEqual(TEXT("High movement"), Inv->GetMovementSpeedMultiplier(), 0.85f);
	TestEqual(TEXT("High stamina drain"), Inv->GetStaminaDrainMultiplier(), 1.35f);
	Inv->RemoveItem(C, 29);
	TestEqual(TEXT("Ammo quantity does not alter occupied cells"), Inv->GetFillRatio(), 1.0f);
	Inv->RemoveItem(C, 1);
	TestEqual(TEXT("Removing final unit releases cell"), Inv->GetLoad(), EInventoryLoad::Medium);
	Inv->RemoveItem(B, 1);
	TestEqual(TEXT("Unload restores normal movement"), Inv->GetMovementSpeedMultiplier(), 1.0f);
	const Aprototype3Character* Character = GetDefault<Aprototype3Character>();
	TestNotNull(TEXT("Player owns an inventory component"), Character->GetInventoryComponent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryValidationTest, "Prototype.Inventory.InvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryValidationTest::RunTest(const FString& Parameters)
{
	UInventoryComponent* Inv = Inventory({2,2});
	FGuid Id;
	TestEqual(TEXT("Null item"), Add(Inv, nullptr, Id), EInventoryResult::InvalidItem);
	TestEqual(TEXT("Empty silhouette"), Add(Inv, Item({}), Id), EInventoryResult::InvalidItem);
	TestEqual(TEXT("Duplicate cells"), Add(Inv, Item({{0,0},{0,0}}), Id), EInventoryResult::InvalidItem);
	TestEqual(TEXT("Negative cell"), Add(Inv, Item({{-1,0}}), Id), EInventoryResult::InvalidItem);
	TestEqual(TEXT("Zero quantity"), Add(Inv, Item(), Id, {0,0}, 0), EInventoryResult::InvalidQuantity);
	UItemDefinition* NotStorable = Item();
	NotStorable->bStorable = false;
	TestEqual(TEXT("Non-storable item"), Add(Inv, NotStorable, Id), EInventoryResult::InvalidItem);
	TestEqual(TEXT("Unknown container"), Inv->AddItem(Item(), 1, TEXT("Missing"), TEXT("Main"), {0,0}, 0, Id), EInventoryResult::InvalidContainer);
	TestEqual(TEXT("Unknown compartment"), Inv->AddItem(Item(), 1, TEXT("Storage"), TEXT("Missing"), {0,0}, 0, Id), EInventoryResult::InvalidCompartment);
	TestEqual(TEXT("Duplicate container"), Inv->AddContainer(Storage({1,1})), EInventoryResult::InvalidContainer);
	FInventoryContainerDefinition Bad = Storage({0,1});
	Bad.Id = TEXT("Invalid");
	TestEqual(TEXT("Zero-sized grid"), Inv->AddContainer(Bad), EInventoryResult::InvalidCompartment);
	Bad = Storage({1,1});
	Bad.Id = TEXT("Invalid");
	const FInventoryCompartment DuplicateCompartment = Bad.Compartments[0];
	Bad.Compartments.Add(DuplicateCompartment);
	TestEqual(TEXT("Duplicate layer IDs"), Inv->AddContainer(Bad), EInventoryResult::InvalidCompartment);
	TestEqual(TEXT("Validation failures add no objects"), Inv->GetFillRatio(), 0.0f);
	Add(Inv, Item(), Id);
	TestEqual(TEXT("Remove too many"), Inv->RemoveItem(Id, 2), EInventoryResult::InvalidQuantity);
	TestEqual(TEXT("Failed removal preserves quantity"), Read(Inv, Id).Quantity, 1);
	TestEqual(TEXT("Missing item"), Inv->RemoveItem(FGuid::NewGuid(), 1), EInventoryResult::InvalidItem);
	return true;
}
#endif
