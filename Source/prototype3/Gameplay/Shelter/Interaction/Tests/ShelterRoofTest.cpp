#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Interaction/ShelterInteractivePieces.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterRoofTest, "Prototype.Interaction.RoofSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterRoofTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Area = World->SpawnActor<AShelterBuildArea>();
	Area->DispatchBeginPlay();

	auto* Roof = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Roof.DA_ShelterPiece_Roof"));
	auto* Wall = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Wall.DA_ShelterPiece_Wall"));
	auto* Doorway = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Doorway.DA_ShelterPiece_Doorway"));
	auto* Lamp = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Lamp.DA_ShelterPiece_Lamp"));
	if (!TestNotNull(TEXT("Roof"), Roof) || !TestNotNull(TEXT("Wall"), Wall)
		|| !TestNotNull(TEXT("Doorway"), Doorway) || !TestNotNull(TEXT("Lamp"), Lamp)) return false;

	const auto Slot = [](EShelterSlotKind Kind, int32 X, int32 Y)
	{
		FShelterSlot Result;
		Result.Kind = Kind;
		Result.Coord = FIntPoint(X, Y);
		return Result;
	};
	using K = EShelterSlotKind;
	using R = EShelterBuildResult;
	const auto WallSlot = Slot(K::Edge, 1, 1);
	const auto RoofSlot = Slot(K::Roof, 1, 1);
	const auto RoofNext = Slot(K::Roof, 1, 2);
	TestEqual(TEXT("No first roof without support"), Area->PlacePiece(RoofSlot, Roof, 0), R::UnsupportedRoof);
	TestEqual(TEXT("Doorway can be placed"), Area->PlacePiece(WallSlot, Doorway, 0), R::Success);
	TestTrue(TEXT("Doorway has its own working door"), Cast<AShelterDoorPiece>(Area->GetPieceAt(WallSlot)) != nullptr);
	TestEqual(TEXT("An opening alone cannot anchor a roof"), Area->PlacePiece(RoofSlot, Roof, 0), R::UnsupportedRoof);
	TestEqual(TEXT("Remove doorway"), Area->RemoveAt(WallSlot), R::Success);
	TestEqual(TEXT("Solid wall can be placed"), Area->PlacePiece(WallSlot, Wall, 0), R::Success);
	TestEqual(TEXT("Wall anchors the first roof"), Area->PlacePiece(RoofSlot, Roof, 0), R::Success);
	TestEqual(TEXT("A corner touch is not support"), Area->PlacePiece(Slot(K::Roof, 2, 2), Roof, 0), R::UnsupportedRoof);
	TestEqual(TEXT("Roof supports the next roof across a side"), Area->PlacePiece(RoofNext, Roof, 0), R::Success);
	TestEqual(TEXT("Floor object fits underneath"), Area->PlacePiece(Slot(K::Cell, 1, 1), Lamp, 0), R::Success);

	FHitResult Hit;
	TestTrue(TEXT("Placed roof blocks a vertical trace"),
		World->LineTraceSingleByChannel(Hit, FVector(300, 300, 260), FVector(300, 300, 400), ECC_Visibility)
		&& Hit.GetActor() == Area->GetPieceAt(RoofSlot));
	FShelterSlot Aimed;
	TestTrue(TEXT("Roof targeting intersects the upper plane"),
		Area->FindSlotFromView(FVector(300, 300, 150), FVector(0, 0, 1), 450, K::Roof, Aimed) && Aimed == RoofSlot);
	TestEqual(TEXT("Cannot remove the only anchoring wall"), Area->RemoveAt(WallSlot), R::SupportsRoof);
	TestEqual(TEXT("Cannot remove a roof that connects another"), Area->RemoveAt(RoofSlot), R::SupportsRoof);
	TestEqual(TEXT("Remove outermost roof first"), Area->RemoveAt(RoofNext), R::Success);
	TestEqual(TEXT("Remove remaining roof"), Area->RemoveAt(RoofSlot), R::Success);
	TestEqual(TEXT("Wall can then be removed"), Area->RemoveAt(WallSlot), R::Success);
	FString Error;
	TestTrue(TEXT("Grid remains valid"), Area->GetModel().Validate(Error));
	return !HasAnyErrors();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterStockAndChestTest, "Prototype.Interaction.StockAndChest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterStockAndChestTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Area = World->SpawnActor<AShelterBuildArea>();
	Area->DispatchBeginPlay();
	auto* Pawn = World->SpawnActor<ACharacter>(FVector(300, 140, 100), FRotator::ZeroRotator);
	auto* Inventory = NewObject<UInventoryComponent>(Pawn);
	Inventory->RegisterComponent();
	auto* Vitals = NewObject<UPlayerVitalsComponent>(Pawn);
	Vitals->RegisterComponent();
	Vitals->InitializeVitals(100, 100, 50, .25f);
	auto* Use = NewObject<UPlayerItemUseComponent>(Pawn);
	Use->bSpawnTestSpikes = false;
	Use->RegisterComponent();
	Pawn->DispatchBeginPlay();
	auto* ChestDefinition = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Chest.DA_ShelterPiece_Chest"));
	auto* Bandage = LoadObject<UShelterStockPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Bandage.DA_ShelterPiece_Bandage"));
	auto* Food = LoadObject<UShelterStockPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_CannedBeans.DA_ShelterPiece_CannedBeans"));
	if (!TestNotNull(TEXT("Chest definition"), ChestDefinition) || !TestNotNull(TEXT("Bandage definition"), Bandage)
		|| !TestNotNull(TEXT("Food definition"), Food)) return false;

	const auto Cell = [](int32 X, int32 Y)
	{
		FShelterSlot Slot;
		Slot.Kind = EShelterSlotKind::Cell;
		Slot.Coord = FIntPoint(X, Y);
		return Slot;
	};
	using R = EShelterBuildResult;
	const FShelterSlot ChestSlot = Cell(0, 1);
	const FShelterSlot ItemSlot = Cell(1, 1);
	TestEqual(TEXT("Place chest"), Area->PlacePiece(ChestSlot, ChestDefinition, 0), R::Success);
	auto* Chest = Cast<AShelterChestPiece>(Area->GetPieceAt(ChestSlot));
	if (!TestNotNull(TEXT("Placed chest"), Chest)) return false;
	const FName Pocket = Chest->GetOrCreateStoragePocket(*Inventory);
	TestFalse(TEXT("Chest has a storage pocket"), Pocket.IsNone());
	TestEqual(TEXT("Reopening the chest keeps the same pocket"), Chest->GetOrCreateStoragePocket(*Inventory), Pocket);
	TestFalse(TEXT("Closed chest contents are inaccessible"), Use->CanAccess(Pocket));
	if (!TestTrue(TEXT("Open chest storage"), Use->OpenChest(Pocket))) return false;
	TestTrue(TEXT("Open chest is available alongside the backpack"), Use->CanAccess(Pocket) && Use->CanAccess(TEXT("Backpack")));

	for (const auto* Stock : { Bandage, Food })
	{
		FInventoryItemProfile Profile;
		if (!TestTrue(TEXT("Player has original inventory profile"), Inventory->GetProfile(Stock->ProfileId, Profile))) return false;
		if (!TestEqual(TEXT("Place existing supply"), Area->PlacePiece(ItemSlot, Stock, 0), R::Success)) return false;
		ADroppedItem* Item = nullptr;
		for (TActorIterator<ADroppedItem> It(World); It; ++It)
			if (It->GetOwner() == Area->GetPieceAt(ItemSlot)) { Item = *It; break; }
		if (!TestNotNull(TEXT("Placed item uses standard dropped actor"), Item)) return false;
		TestTrue(TEXT("Placed item reuses existing definition"), Item->GetItem().Definition == Stock->Item);
		const FGuid Id = Item->GetItem().InstanceId;
		if (!TestTrue(TEXT("Stock is within normal pickup reach"), Item->CanInteract(Pawn))) return false;
		FInventoryArrangement Request;
		Request.Kind = EInventoryArrangement::PickUp;
		Request.ItemId = Id;
		Request.FloorItem = Item;
		Request.Pocket = Pocket;
		Request.Position = FVector2D(440, 430);
		Request.Angle = 33;
		if (!TestTrue(TEXT("Pick up into chest at arbitrary angle using inventory UI operation"), Use->RequestArrangement(Request))) return false;
		TestTrue(TEXT("Pickup frees the construction cell"), Area->GetPieceAt(ItemSlot) == nullptr);
		TestEqual(TEXT("Chest with contents cannot be removed"), Area->RemoveAt(ChestSlot), R::PieceHasContents);
		FInventoryEntry Stored;
		TestTrue(TEXT("Contents preserve instance identity"), Inventory->GetItem(Id, Stored));
		TestEqual(TEXT("Contents preserve free rotation"), Stored.AngleDegrees, 33.0);
		Use->CloseBackpack();
		TestFalse(TEXT("Closing chest revokes access"), Use->CanAccess(Pocket));
		TestTrue(TEXT("Closing chest preserves contents"), Inventory->GetItem(Id, Stored) && Stored.PocketId == Pocket);
		if (!TestTrue(TEXT("Reopen chest storage"), Use->OpenChest(Pocket))) return false;
		Request.Kind = EInventoryArrangement::Move;
		Request.FloorItem.Reset();
		Request.Pocket = TEXT("Backpack");
		TestTrue(TEXT("Move item back to the backpack"), Use->Transfer(Id, Request.Pocket));
		TestTrue(TEXT("Move out empties chest"), Chest->CanRemove());
	}
	TestEqual(TEXT("Remove empty chest"), Area->RemoveAt(ChestSlot), R::Success);
	return !HasAnyErrors();
}
#endif
