#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "Misc/AutomationTest.h"
#include "InputCoreTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FInventoryFixture
{
	TStrongObjectPtr<UInventoryComponent> Inventory { NewObject<UInventoryComponent>() };
	UItemDefinition* Definition;
	FInventoryFixture()
	{
		Definition = NewObject<UItemDefinition>(Inventory.Get());
		Definition->ItemId = TEXT("Fixture");
		Definition->DisplayName = FText::FromString(TEXT("Fixture"));
		Definition->Category = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical"));
		Definition->MassKg = 1;
		FInventoryPocket Pocket;
		Pocket.Id = TEXT("A"); Pocket.Size = FIntPoint(4, 3);
		Inventory->AddPocket(Pocket);
		Pocket.Id = TEXT("B");
		Inventory->AddPocket(Pocket);
		FInventoryItemProfile Profile;
		Profile.Id = TEXT("Single"); Profile.Definition = Definition;
		Profile.OccupiedCells = { FIntPoint(0, 0) };
		Inventory->RegisterProfile(Profile);
		Profile.Id = TEXT("L");
		Profile.OccupiedCells = { FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(1, 1) };
		Inventory->RegisterProfile(Profile);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryGeometryTest, "Prototype.Inventory.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryGeometryTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	const auto Item = FItemInstance::Create(F.Definition);
	TestTrue(TEXT("L placed"), I.AddItem(Item, TEXT("L"), TEXT("A"), FIntPoint(0, 0), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Hole stays usable"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FIntPoint(1, 0), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Occupied silhouette rejects overlap"), I.CheckPlacement(TEXT("Single"), TEXT("A"), FIntPoint(1, 1), 0) == EInventoryResult::Occupied);
	TestTrue(TEXT("Other pocket is independent"), I.CheckPlacement(TEXT("L"), TEXT("B"), FIntPoint(0, 0), 0) == EInventoryResult::Success);
	TestTrue(TEXT("Right edge"), I.CheckPlacement(TEXT("L"), TEXT("B"), FIntPoint(3, 0), 0) == EInventoryResult::OutOfBounds);
	TestTrue(TEXT("Bottom edge"), I.CheckPlacement(TEXT("L"), TEXT("B"), FIntPoint(0, 2), 0) == EInventoryResult::OutOfBounds);
	TestTrue(TEXT("Negative anchor"), I.CheckPlacement(TEXT("Single"), TEXT("B"), FIntPoint(-1, 0), 0) == EInventoryResult::OutOfBounds);
	TestTrue(TEXT("Extreme anchor does not overflow"), I.CheckPlacement(TEXT("L"), TEXT("B"), FIntPoint(MAX_int32, MAX_int32), 0) == EInventoryResult::OutOfBounds);
	FInventoryItemProfile Profile;
	I.GetProfile(TEXT("L"), Profile);
	const auto Rotated = Profile.GetRotatedCells(1);
	TestEqual(TEXT("Rotation retains 3 cells"), Rotated.Num(), 3);
	TestTrue(TEXT("Rotated cell 1"), Rotated.Contains(FIntPoint(1, 0)));
	TestTrue(TEXT("Rotated cell 2"), Rotated.Contains(FIntPoint(0, 0)));
	TestTrue(TEXT("Rotated cell 3"), Rotated.Contains(FIntPoint(0, 1)));
	TestTrue(TEXT("Rotated hole stays empty"), !Rotated.Contains(FIntPoint(1, 1)));
	TestTrue(TEXT("Four turns restore geometry"), Profile.GetRotatedCells(4) == Profile.OccupiedCells);
	TestTrue(TEXT("Unknown rotation rejected"), I.CheckMove(Item.InstanceId, TEXT("A"), FIntPoint(0, 0), 4) == EInventoryResult::InvalidRotation);
	Profile.Id = TEXT("Fixed"); Profile.bAllowRotation = false;
	I.RegisterProfile(Profile);
	TestTrue(TEXT("Fixed shape cannot rotate"), I.CheckPlacement(Profile.Id, TEXT("B"), FIntPoint(0, 0), 1) == EInventoryResult::InvalidRotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryAtomicMoveTest, "Prototype.Inventory.AtomicMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryAtomicMoveTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	const auto Moving = FItemInstance::Create(F.Definition);
	const auto Obstacle = FItemInstance::Create(F.Definition);
	I.AddItem(Moving, TEXT("L"), TEXT("A"), FIntPoint(0, 0), 0);
	I.AddItem(Obstacle, TEXT("Single"), TEXT("B"), FIntPoint(1, 1), 0);
	TestTrue(TEXT("Collision rejects whole move"), I.MoveItem(Moving.InstanceId, TEXT("B"), FIntPoint(0, 0), 0) == EInventoryResult::Occupied);
	FInventoryEntry Found;
	TestTrue(TEXT("Original still exists"), I.GetItem(Moving.InstanceId, Found));
	TestEqual(TEXT("Original pocket"), Found.PocketId, FName(TEXT("A")));
	TestEqual(TEXT("Original position"), Found.Position, FIntPoint(0, 0));
	TestEqual(TEXT("Count unchanged"), I.GetEntries().Num(), 2);
	TestTrue(TEXT("Self overlap allowed when moving"), I.MoveItem(Moving.InstanceId, TEXT("A"), FIntPoint(1, 0), 1) == EInventoryResult::Success);
	TestTrue(TEXT("Fits exactly in other pocket"), I.MoveItem(Moving.InstanceId, TEXT("B"), FIntPoint(2, 1), 3) == EInventoryResult::Success);
	I.GetItem(Moving.InstanceId, Found);
	TestEqual(TEXT("ID preserved"), Found.Item.InstanceId, Moving.InstanceId);
	TestEqual(TEXT("Quantity preserved"), Found.Item.Quantity, Moving.Quantity);
	TestTrue(TEXT("Definition reference preserved"), Found.Item.Definition == Moving.Definition);
	TestEqual(TEXT("Rotation stored"), Found.QuarterTurns, 3);
	TestTrue(TEXT("Cannot insert same ID again"), I.AddItem(Moving, TEXT("Single"), TEXT("A"), FIntPoint(3, 2), 0) == EInventoryResult::DuplicateId);
	FItemInstance Removed;
	TestTrue(TEXT("Removal succeeds"), I.RemoveItem(Moving.InstanceId, Removed) == EInventoryResult::Success);
	TestEqual(TEXT("Returned whole instance"), Removed.InstanceId, Moving.InstanceId);
	TestEqual(TEXT("Only obstacle remains"), I.GetEntries().Num(), 1);
	TestTrue(TEXT("Missing removal fails"), I.RemoveItem(Moving.InstanceId, Removed) == EInventoryResult::NotFound);
	TestFalse(TEXT("Failed removal clears stale output"), Removed.IsValid());
	TestTrue(TEXT("Missing move fails"), I.MoveItem(Moving.InstanceId, TEXT("A"), FIntPoint(0, 0), 0) == EInventoryResult::NotFound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryValidationTest, "Prototype.Inventory.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryValidationTest::RunTest(const FString&)
{
	FInventoryFixture F;
	auto& I = *F.Inventory;
	TestTrue(TEXT("Invalid instance rejected"), I.AddItem(FItemInstance(), TEXT("Single"), TEXT("A"), FIntPoint(0, 0), 0) == EInventoryResult::InvalidItem);
	FInventoryItemProfile Profile;
	I.GetProfile(TEXT("L"), Profile);
	TestTrue(TEXT("Registered profiles cannot be replaced"), I.RegisterProfile(Profile) == EInventoryResult::DuplicateId);
	Profile.Id = TEXT("Bad"); Profile.OccupiedCells.Empty();
	TestTrue(TEXT("Empty shape rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	Profile.OccupiedCells = { FIntPoint(0, 0), FIntPoint(0, 0) };
	TestTrue(TEXT("Duplicate cells rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	Profile.OccupiedCells = { FIntPoint(-1, 0) };
	TestTrue(TEXT("Negative cells rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	Profile.OccupiedCells = { FIntPoint(256, 0) };
	TestTrue(TEXT("Excessive extent rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	Profile.OccupiedCells = { FIntPoint(1, 1) };
	TestTrue(TEXT("Non-normalized shape rejected"), I.RegisterProfile(Profile) == EInventoryResult::InvalidProfile);
	FInventoryPocket Pocket;
	TestTrue(TEXT("Unnamed pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Id = TEXT("Bad"); Pocket.Size = FIntPoint(0, 2);
	TestTrue(TEXT("Zero size rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Size = FIntPoint(257, 2);
	TestTrue(TEXT("Huge size rejected"), I.AddPocket(Pocket) == EInventoryResult::InvalidPocket);
	Pocket.Id = TEXT("A"); Pocket.Size = FIntPoint(3, 2);
	TestTrue(TEXT("Duplicate pocket rejected"), I.AddPocket(Pocket) == EInventoryResult::DuplicateId);
	const auto Item = FItemInstance::Create(F.Definition);
	TestTrue(TEXT("Missing pocket rejected"), I.AddItem(Item, TEXT("Single"), TEXT("Missing"), FIntPoint(0, 0), 0) == EInventoryResult::InvalidPocket);
	TestTrue(TEXT("Missing profile rejected"), I.AddItem(Item, TEXT("Missing"), TEXT("A"), FIntPoint(0, 0), 0) == EInventoryResult::InvalidProfile);
	UItemDefinition* Other = DuplicateObject<UItemDefinition>(F.Definition, F.Inventory.Get());
	TestTrue(TEXT("Wrong definition for profile rejected"), I.AddItem(FItemInstance::Create(Other), TEXT("Single"), TEXT("A"), FIntPoint(0, 0), 0) == EInventoryResult::InvalidProfile);
	TestEqual(TEXT("Failures added nothing"), I.GetEntries().Num(), 0);
	// Read access returns copies; editing a snapshot must not bypass validation.
	auto Pockets = I.GetPockets(); Pockets[0].Size = FIntPoint(100, 100);
	TestEqual(TEXT("Pocket snapshot is independent"), I.GetPockets()[0].Size, FIntPoint(4, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryBandageTest, "Prototype.Inventory.BandageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryBandageTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> I(NewObject<UInventoryComponent>());
	if (!TestTrue(TEXT("Real asset and demo populate"), InventoryDemo::Populate(I.Get()))) return false;
	const auto Entries = I->GetEntries();
	TestEqual(TEXT("Two bandages and two fixtures"), Entries.Num(), 4);
	FInventoryItemProfile Profile;
	I->GetProfile(TEXT("Bandage_TestOnly"), Profile);
	TestTrue(TEXT("Bandage geometry explicitly provisional"), Profile.bProvisional);
	TestEqual(TEXT("Temporary one cell"), Profile.OccupiedCells.Num(), 1);
	TestEqual(TEXT("Bandage stack limit unchanged"), Profile.Definition->MaxStackSize, 1);
	TestEqual(TEXT("Bandage mass unchanged"), Profile.Definition->MassKg, 0.05);
	TestFalse(TEXT("Two bandages cannot become one stack"), FItemInstance::Create(Profile.Definition, 2).IsValid());
	TestTrue(TEXT("Distinct bandage identities"), Entries[0].Item.InstanceId != Entries[1].Item.InstanceId);
	TestTrue(TEXT("Bandage rotates without replacing identity"), I->MoveItem(Entries[0].Item.InstanceId, TEXT("Side"), FIntPoint(3, 2), 1) == EInventoryResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryPanelInteractionTest, "Prototype.Inventory.PanelInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryPanelInteractionTest::RunTest(const FString&)
{
	TStrongObjectPtr<UInventoryComponent> I(NewObject<UInventoryComponent>());
	if (!TestTrue(TEXT("Demo fixture"), InventoryDemo::Populate(I.Get()))) return false;
	bool bClosed = false;
	const auto Panel = SNew(SInventoryPanel).Inventory(I.Get()).OnClose(FSimpleDelegate::CreateLambda([&]() { bClosed = true; }));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1000, 650), FSlateLayoutTransform());
	auto Mouse = [](FVector2D Point, FKey Button)
	{
		return FPointerEvent(0, Point, Point, TSet<FKey>(), Button, 0, FModifierKeysState());
	};
	auto Down = [&](FVector2D P) { Panel->OnMouseButtonDown(G, Mouse(P, EKeys::LeftMouseButton)); };
	auto Up = [&](FVector2D P) { Panel->OnMouseButtonUp(G, Mouse(P, EKeys::LeftMouseButton)); };
	auto Key = [&](FKey K) { Panel->OnKeyDown(G, FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0)); };
	const FGuid BandageId = I->GetEntries()[0].Item.InstanceId;
	Down(FVector2D(66, 216)); Up(FVector2D(762, 320));
	FInventoryEntry Entry;
	I->GetItem(BandageId, Entry);
	TestEqual(TEXT("Drag to other pocket"), Entry.PocketId, FName(TEXT("Side")));
	TestEqual(TEXT("Drag position"), Entry.Position, FIntPoint(3, 2));
	Key(EKeys::R);
	I->GetItem(BandageId, Entry);
	TestEqual(TEXT("Keyboard rotation"), Entry.QuarterTurns, 1);
	Down(FVector2D(762, 320)); Up(FVector2D(658, 268));
	I->GetItem(BandageId, Entry);
	TestEqual(TEXT("Occupied drop leaves original position"), Entry.Position, FIntPoint(3, 2));
	Down(FVector2D(762, 320)); Up(FVector2D(960, 510));
	I->GetItem(BandageId, Entry);
	TestEqual(TEXT("Outside drop preserves original"), Entry.Position, FIntPoint(3, 2));
	Down(FVector2D(762, 320));
	Panel->OnMouseButtonDown(G, Mouse(FVector2D(66, 216), EKeys::RightMouseButton));
	Up(FVector2D(66, 216));
	I->GetItem(BandageId, Entry);
	TestEqual(TEXT("Cancel restores original"), Entry.Position, FIntPoint(3, 2));
	Key(EKeys::Delete);
	TestEqual(TEXT("Remove button/shortcut"), I->GetEntries().Num(), 3);
	Down(FVector2D(100, 580)); Up(FVector2D(100, 580));
	Down(FVector2D(66, 216)); Up(FVector2D(66, 216));
	TestEqual(TEXT("Add bandage through screen"), I->GetEntries().Num(), 4);
	Key(EKeys::I);
	TestTrue(TEXT("Close shortcut"), bClosed);
	return true;
}
#endif
