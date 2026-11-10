#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Character.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Build/ShelterBuildComponent.h"
#include "Gameplay/Shelter/Build/ShelterBuildModel.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "InputKeyEventArgs.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/ScopeExit.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FShelterSlot Edge(int32 X, int32 Y, bool bAlongX) { FShelterSlot S; S.Coord = FIntPoint(X, Y); S.bAlongX = bAlongX; return S; }
FShelterSlot Cell(int32 X, int32 Y) { FShelterSlot S; S.Kind = EShelterSlotKind::Cell; S.Coord = FIntPoint(X, Y); S.bAlongX = false; return S; }

UShelterPieceDefinition* MakePiece(FName Id, const TCHAR* Category, EShelterSlotKind Kind, int32 Order, float OpeningWidth = 0, float OpeningHeight = 0)
{
	auto* Piece = NewObject<UShelterPieceDefinition>(GetTransientPackage());
	Piece->PieceId = Id;
	Piece->DisplayName = FText::FromName(Id);
	Piece->Category = FText::FromString(Category);
	Piece->SlotKind = Kind;
	Piece->SortOrder = Order;
	Piece->OpeningWidth = OpeningWidth;
	Piece->OpeningHeight = OpeningHeight;
	return Piece;
}

struct FShelterWorld
{
	UWorld* World;
	AShelterBuildArea* Area;
	FShelterWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Area = World->SpawnActor<AShelterBuildArea>(FVector::ZeroVector, FRotator::ZeroRotator);
		Area->DispatchBeginPlay();
	}
	~FShelterWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterSlotModelTest, "Prototype.Shelter.SlotModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterSlotModelTest::RunTest(const FString&)
{
	using R = EShelterBuildResult;
	FShelterBuildModel Model;
	Model.SizeCells = FIntPoint(3, 2);
	const FName Wall(TEXT("Wall")), Door(TEXT("Door")), Crate(TEXT("Crate"));
	TestEqual(TEXT("Place on an empty edge"), Model.Place(Edge(0, 0, true), Wall, EShelterSlotKind::Edge, 0), R::Success);
	TestEqual(TEXT("Occupied slot is refused, not replaced"), Model.Place(Edge(0, 0, true), Door, EShelterSlotKind::Edge, 0), R::Occupied);
	TestEqual(TEXT("Refusal keeps the original piece"), Model.Find(Edge(0, 0, true))->PieceId, Wall);
	TestEqual(TEXT("Same corner, other orientation is a different slot"), Model.Place(Edge(0, 0, false), Door, EShelterSlotKind::Edge, 3), R::Success);
	TestEqual(TEXT("Edge turns normalise to a flip"), Model.Find(Edge(0, 0, false))->Turns, 1);
	TestEqual(TEXT("Far boundary edge along X is inside"), Model.CanPlace(Edge(2, 2, true), Wall, EShelterSlotKind::Edge), R::Success);
	TestEqual(TEXT("Edge past the width is outside"), Model.CanPlace(Edge(3, 0, true), Wall, EShelterSlotKind::Edge), R::OutOfBounds);
	TestEqual(TEXT("Far boundary edge along Y is inside"), Model.CanPlace(Edge(3, 1, false), Wall, EShelterSlotKind::Edge), R::Success);
	TestEqual(TEXT("Edge past the depth is outside"), Model.CanPlace(Edge(0, 2, false), Wall, EShelterSlotKind::Edge), R::OutOfBounds);
	TestEqual(TEXT("Negative coordinates are outside"), Model.CanPlace(Edge(-1, 0, true), Wall, EShelterSlotKind::Edge), R::OutOfBounds);
	TestEqual(TEXT("An edge piece cannot enter a cell slot"), Model.CanPlace(Cell(0, 0), Wall, EShelterSlotKind::Edge), R::WrongSlotKind);
	TestEqual(TEXT("A cell piece enters a cell slot"), Model.Place(Cell(2, 1), Crate, EShelterSlotKind::Cell, 5), R::Success);
	TestEqual(TEXT("Cell turns normalise to quarter turns"), Model.Find(Cell(2, 1))->Turns, 1);
	TestEqual(TEXT("Cell past the grid is outside"), Model.CanPlace(Cell(3, 0), Crate, EShelterSlotKind::Cell), R::OutOfBounds);
	TestEqual(TEXT("Unnamed pieces are refused"), Model.CanPlace(Edge(1, 0, true), NAME_None, EShelterSlotKind::Edge), R::InvalidPiece);
	FString Error;
	TestTrue(TEXT("Model validates"), Model.Validate(Error));
	TestEqual(TEXT("Three pieces placed"), Model.Placements.Num(), 3);

	FShelterPlacement Removed;
	TestEqual(TEXT("Remove returns the piece"), Model.Remove(Edge(0, 0, true), &Removed), R::Success);
	TestEqual(TEXT("Removed piece identity"), Removed.PieceId, Wall);
	TestEqual(TEXT("Removing an empty slot reports nothing found"), Model.Remove(Edge(0, 0, true)), R::NotFound);
	TestEqual(TEXT("The slot is free again"), Model.Place(Edge(0, 0, true), Door, EShelterSlotKind::Edge, 0), R::Success);

	TestEqual(TEXT("Edge along X centre"), FShelterBuildModel::SlotCenter(Edge(1, 2, true), 200), FVector2D(300, 400));
	TestEqual(TEXT("Edge along Y centre"), FShelterBuildModel::SlotCenter(Edge(1, 0, false), 200), FVector2D(200, 100));
	TestEqual(TEXT("Cell centre"), FShelterBuildModel::SlotCenter(Cell(2, 1), 200), FVector2D(500, 300));
	TestEqual(TEXT("Edge along Y is turned a quarter"), FShelterBuildModel::SlotYaw(Edge(0, 0, false), 0), 90.0);
	TestEqual(TEXT("Flipping an edge turns it around"), FShelterBuildModel::SlotYaw(Edge(0, 0, true), 1), 180.0);

	FShelterSlot Near;
	TestTrue(TEXT("Nearest edge found"), Model.NearestSlot(FVector2D(290, 15), EShelterSlotKind::Edge, 200, Near));
	TestTrue(TEXT("Point beside a horizontal edge selects it"), Near == Edge(1, 0, true));
	Model.NearestSlot(FVector2D(390, 110), EShelterSlotKind::Edge, 200, Near);
	TestTrue(TEXT("Point beside a vertical edge selects it"), Near == Edge(2, 0, false));
	Model.NearestSlot(FVector2D(-900, 5000), EShelterSlotKind::Edge, 200, Near);
	TestTrue(TEXT("Far points clamp to a valid slot"), Model.IsValidSlot(Near));
	Model.NearestSlot(FVector2D(450, 250), EShelterSlotKind::Cell, 200, Near);
	TestTrue(TEXT("Nearest cell"), Near == Cell(2, 1));

	FShelterBuildModel Broken;
	Broken.SizeCells = FIntPoint(0, 4);
	TestEqual(TEXT("Invalid grid refuses placement"), Broken.CanPlace(Edge(0, 0, true), Wall, EShelterSlotKind::Edge), R::InvalidGrid);
	TestFalse(TEXT("Invalid grid has no nearest slot"), Broken.NearestSlot(FVector2D::ZeroVector, EShelterSlotKind::Edge, 200, Near));
	FShelterBuildModel Duplicated;
	Duplicated.Placements.SetNum(2);
	Duplicated.Placements[0].PieceId = Duplicated.Placements[1].PieceId = Wall;
	TestFalse(TEXT("Two pieces in one slot fail validation"), Duplicated.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterBuildControlsTest, "Prototype.Shelter.BuildControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterBuildControlsTest::RunTest(const FString&)
{
	using A = EInventoryControl;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>(GetTransientPackage()));
	Settings->ResetDefaults();
	TestEqual(TEXT("Build menu default"), Settings->GetKey(A::BuildMenu), EKeys::B);
	TestEqual(TEXT("Previous default"), Settings->GetKey(A::BuildPrevious), EKeys::Q);
	TestEqual(TEXT("Next default"), Settings->GetKey(A::BuildNext), EKeys::E);
	TestEqual(TEXT("Rotate default"), Settings->GetKey(A::BuildRotate), EKeys::R);
	TestEqual(TEXT("Exit default"), Settings->GetKey(A::BuildExit), EKeys::V);
	for (A Action : { A::BuildMenu, A::BuildPrevious, A::BuildNext, A::BuildRotate, A::BuildExit })
	{
		TestTrue(TEXT("Build controls are rebindable"), UInventoryInputSettings::IsBindable(Action));
		TestTrue(TEXT("Build controls live in the Building section"), UInventoryInputSettings::Section(Action) == EControlSection::Building);
		TestTrue(TEXT("Build defaults do not collide with other defaults"), Settings->Conflicts(Action, Settings->GetKey(Action)).IsEmpty());
		TestFalse(TEXT("Build controls have a label"), UInventoryInputSettings::Label(Action) == TEXT("Retired action"));
	}
	// Existing defaults must be unchanged: these shared keys are legal because their contexts never overlap.
	TestEqual(TEXT("Pickup keeps E"), Settings->GetKey(A::PickUpWorld), EKeys::E);
	TestEqual(TEXT("Inventory rotate-left keeps Q"), Settings->GetKey(A::TurnLeft), EKeys::Q);
	TestEqual(TEXT("Backpack keeps I"), Settings->GetKey(A::Toggle), EKeys::I);
	TestTrue(TEXT("A build key cannot take a movement key"), Settings->Conflicts(A::BuildNext, EKeys::W).Contains(A::MoveForward));
	TestTrue(TEXT("A build key cannot take the place button"), Settings->Conflicts(A::BuildRotate, EKeys::LeftMouseButton).Contains(A::Primary));
	TestTrue(TEXT("The build menu key cannot take a gameplay key"), Settings->Conflicts(A::BuildMenu, EKeys::G).Contains(A::Bandage));
	TestTrue(TEXT("Build keys are appended after every earlier control"), static_cast<int32>(A::BuildMenu) > static_cast<int32>(A::DropHeld));
	FString Error;
	TestTrue(TEXT("Reset of the Building section completes"), Settings->ResetSection(EControlSection::Building));
	TestEqual(TEXT("Reset restores the build menu key"), Settings->GetKey(A::BuildMenu), EKeys::B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterAreaPlacementTest, "Prototype.Shelter.AreaPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterAreaPlacementTest::RunTest(const FString&)
{
	using R = EShelterBuildResult;
	FShelterWorld F;
	TStrongObjectPtr<UShelterPieceDefinition> Wall(MakePiece(TEXT("TestWall"), TEXT("Walls"), EShelterSlotKind::Edge, 0));
	TStrongObjectPtr<UShelterPieceDefinition> Door(MakePiece(TEXT("TestDoor"), TEXT("Doorways"), EShelterSlotKind::Edge, 10, 140, 220));
	TStrongObjectPtr<UShelterPieceDefinition> Crate(MakePiece(TEXT("TestCrate"), TEXT("Objects"), EShelterSlotKind::Cell, 20));
	TStrongObjectPtr<UShelterPieceDefinition> Unnamed(MakePiece(NAME_None, TEXT("Walls"), EShelterSlotKind::Edge, 0));
	TestTrue(TEXT("Placeholder definitions are valid"), Wall->IsValidDefinition() && Door->IsValidDefinition() && Crate->IsValidDefinition());
	TestFalse(TEXT("A piece without an identity is invalid"), Unnamed->IsValidDefinition());
	TestEqual(TEXT("Invalid definitions are refused"), F.Area->PlacePiece(Edge(0, 0, true), Unnamed.Get(), 0), R::InvalidPiece);

	const FShelterSlot Slot = Edge(1, 1, true);
	TestEqual(TEXT("Wall placed"), F.Area->PlacePiece(Slot, Wall.Get(), 0), R::Success);
	AShelterPiece* Placed = F.Area->GetPieceAt(Slot);
	if (!TestNotNull(TEXT("Placed piece has an actor"), Placed)) return false;
	TestTrue(TEXT("Actor sits at the slot centre"), Placed->GetActorLocation().Equals(FVector(300, 200, 0), .1));
	TestTrue(TEXT("Actor knows its definition"), Placed->GetDefinition() == Wall.Get());
	TestFalse(TEXT("A placed piece is not a preview"), Placed->IsPreview());
	FHitResult Hit;
	TestTrue(TEXT("A placed wall blocks"), F.World->LineTraceSingleByChannel(Hit, FVector(300, 100, 150), FVector(300, 300, 150), ECC_Visibility)
		&& Hit.GetActor() == Placed);
	TestEqual(TEXT("Occupied slot is refused"), F.Area->PlacePiece(Slot, Door.Get(), 0), R::Occupied);
	TestEqual(TEXT("Refusal spawns nothing"), F.Area->GetModel().Placements.Num(), 1);

	const FShelterSlot DoorSlot = Edge(2, 1, false);
	TestEqual(TEXT("Doorway placed"), F.Area->PlacePiece(DoorSlot, Door.Get(), 1), R::Success);
	AShelterPiece* DoorActor = F.Area->GetPieceAt(DoorSlot);
	if (!TestNotNull(TEXT("Doorway actor"), DoorActor)) return false;
	TestTrue(TEXT("Doorway is turned to its edge and flipped"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(DoorActor->GetActorRotation().Yaw - 270.0), 0.0, .1));
	TestFalse(TEXT("A doorway's opening is passable"), F.World->LineTraceSingleByChannel(Hit, FVector(300, 300, 100), FVector(500, 300, 100), ECC_Visibility));
	TestTrue(TEXT("A doorway's frame blocks"), F.World->LineTraceSingleByChannel(Hit, FVector(300, 215, 100), FVector(500, 215, 100), ECC_Visibility));

	TestEqual(TEXT("Cell object placed"), F.Area->PlacePiece(Cell(4, 4), Crate.Get(), 1), R::Success);
	TestEqual(TEXT("An edge piece cannot enter a cell"), F.Area->PlacePiece(Cell(3, 3), Wall.Get(), 0), R::WrongSlotKind);

	ACharacter* Body = F.World->SpawnActor<ACharacter>(FVector(700, 0, 100), FRotator::ZeroRotator);
	TestEqual(TEXT("A character standing in the slot blocks placement"), F.Area->CanPlace(Edge(3, 0, true), Wall.Get()), R::Blocked);
	TestEqual(TEXT("A neighbouring slot stays free"), F.Area->CanPlace(Edge(5, 0, true), Wall.Get()), R::Success);
	Body->Destroy();

	AShelterPiece* Preview = F.Area->SpawnPreview(Wall.Get());
	if (!TestNotNull(TEXT("Preview"), Preview)) return false;
	Preview->SetActorTransform(F.Area->SlotTransform(Edge(0, 5, true), 0));
	TestTrue(TEXT("Preview is flagged"), Preview->IsPreview());
	TestFalse(TEXT("A preview never blocks"), F.World->LineTraceSingleByChannel(Hit, FVector(100, 900, 150), FVector(100, 1100, 150), ECC_Visibility));
	TestEqual(TEXT("A preview is not part of the grid"), F.Area->GetModel().Placements.Num(), 3);
	Preview->Destroy();

	FShelterSlot Aimed;
	TestTrue(TEXT("Looking down at an edge finds it"), F.Area->FindSlotFromView(FVector(300, 20, 160), FVector(0, 1, -1), 600, EShelterSlotKind::Edge, Aimed));
	TestTrue(TEXT("Aimed slot"), Aimed == Edge(1, 1, true));

	TestTrue(TEXT("Inside the grid"), F.Area->ContainsLocation(FVector(600, 600, 90)));
	TestTrue(TEXT("Just outside, within the margin"), F.Area->ContainsLocation(FVector(-150, 600, 90)));
	TestFalse(TEXT("Beyond the margin"), F.Area->ContainsLocation(FVector(-400, 600, 90)));
	TestTrue(TEXT("Area is found by location"), AShelterBuildArea::FindContaining(F.World, FVector(600, 600, 90)) == F.Area);
	TestNull(TEXT("No area far away"), AShelterBuildArea::FindContaining(F.World, FVector(9000, 9000, 90)));

	TestEqual(TEXT("Remove by actor"), F.Area->RemovePiece(Placed), R::Success);
	TestNull(TEXT("Slot is empty after removal"), F.Area->GetPieceAt(Slot));
	TestFalse(TEXT("The removed actor is gone"), IsValid(Placed));
	TestTrue(TEXT("Other pieces keep their actors"), F.Area->GetPieceAt(DoorSlot) == DoorActor);
	TestEqual(TEXT("Remove by slot"), F.Area->RemoveAt(DoorSlot), R::Success);
	TestEqual(TEXT("Nothing left to remove there"), F.Area->RemoveAt(DoorSlot), R::NotFound);
	TestEqual(TEXT("The slot can be built again"), F.Area->PlacePiece(Slot, Door.Get(), 0), R::Success);
	FString Error;
	TestTrue(TEXT("Grid validates after edits"), F.Area->GetModel().Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterBuildModeInputTest, "Prototype.Shelter.BuildModeInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterBuildModeInputTest::RunTest(const FString&)
{
	FShelterWorld F;
	UClass* CharacterType = LoadClass<Aprototype3Character>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	UClass* ControllerType = LoadClass<Aprototype3PlayerController>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Player character class"), CharacterType) || !TestNotNull(TEXT("Player controller class"), ControllerType)) return false;
	// Centre of cell (1,1): clear of every edge slot, so the player never blocks the aimed slot.
	auto* Player = F.World->SpawnActor<Aprototype3Character>(CharacterType, FVector(300, 300, 100), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Player"), Player)) return false;
	Player->DispatchBeginPlay();
	auto* Vitals = Player->GetPlayerVitalsComponent();
	auto* Controller = F.World->SpawnActor<Aprototype3PlayerController>(ControllerType);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Player->PlayerController = Controller;
	Controller->PlayerInput = NewObject<UEnhancedPlayerInput>(Controller);
	Controller->Possess(Player);
	// No frame is rendered here, so the camera manager's cached view is stale. Without it the
	// controller reports the pawn's eyes and control rotation, which is what this check steers.
	if (Controller->PlayerCameraManager) { Controller->PlayerCameraManager->Destroy(); Controller->PlayerCameraManager = nullptr; }
	Controller->SetControlRotation(FRotator(-45, 90, 0));
	TStrongObjectPtr<UInventoryInputSettings> Saved(DuplicateObject<UInventoryInputSettings>(GetMutableDefault<UInventoryInputSettings>(), GetTransientPackage()));
	GetMutableDefault<UInventoryInputSettings>()->ResetDefaults();
	ON_SCOPE_EXIT { GetMutableDefault<UInventoryInputSettings>()->CopySettingsFrom(*Saved.Get()); };

	UShelterBuildComponent* Build = Controller->GetBuildComponent();
	if (!TestNotNull(TEXT("Controller owns a build component"), Build)) return false;
	TStrongObjectPtr<UShelterPieceDefinition> Wall(MakePiece(TEXT("TestWall"), TEXT("Walls"), EShelterSlotKind::Edge, 0));
	TStrongObjectPtr<UShelterPieceDefinition> Half(MakePiece(TEXT("TestHalfWall"), TEXT("Walls"), EShelterSlotKind::Edge, 5));
	TStrongObjectPtr<UShelterPieceDefinition> Door(MakePiece(TEXT("TestDoor"), TEXT("Doorways"), EShelterSlotKind::Edge, 10, 140, 220));
	Build->SetCatalog({ Door.Get(), Half.Get(), Wall.Get() });
	TestEqual(TEXT("Categories follow sort order"), Build->GetCategories().Num(), 2);
	TestTrue(TEXT("Walls come first"), Build->GetCategories()[0].ToString() == TEXT("Walls"));
	TestEqual(TEXT("Pieces group by category"), Build->GetPiecesInCategory(0).Num(), 2);

	auto Key = [&](FKey K, EInputEvent Event = IE_Pressed)
	{
		return Controller->InputKey(FInputKeyEventArgs::CreateSimulated(K, Event, Event == IE_Released ? 0.f : 1.f));
	};
	auto Tap = [&](FKey K) { const bool bConsumed = Key(K); Key(K, IE_Released); return bConsumed; };

	Player->SetActorLocation(FVector(9000, 9000, 100));
	TestTrue(TEXT("The build key is consumed outside a build area"), Tap(EKeys::B));
	TestFalse(TEXT("Outside a build area the key does nothing"), Build->IsActive());
	Player->SetActorLocation(FVector(300, 300, 100));

	Tap(EKeys::B);
	TestTrue(TEXT("B opens the category menu"), Build->GetState() == EShelterBuildState::Menu);
	TestTrue(TEXT("E is consumed while building"), Tap(EKeys::E));
	TestEqual(TEXT("E moves to the next category"), Build->GetCategoryIndex(), 1);
	Tap(EKeys::Q);
	TestEqual(TEXT("Q moves back"), Build->GetCategoryIndex(), 0);
	TestTrue(TEXT("Number keys are consumed in the menu"), Tap(EKeys::Two));
	TestTrue(TEXT("A number key picks its category"), Build->GetState() == EShelterBuildState::Placing && Build->GetCategoryIndex() == 1);
	Tap(EKeys::B);
	TestTrue(TEXT("B returns to the category menu"), Build->GetState() == EShelterBuildState::Menu);
	Tap(EKeys::Q);
	TestTrue(TEXT("The place button is consumed in the menu"), Tap(EKeys::LeftMouseButton));
	TestTrue(TEXT("The place button confirms the category"), Build->GetState() == EShelterBuildState::Placing && Build->GetCategoryIndex() == 0);
	TestTrue(TEXT("Placing starts on the first piece"), Build->GetSelectedPiece() == Wall.Get());
	Tap(EKeys::E);
	TestTrue(TEXT("E cycles to the next piece"), Build->GetSelectedPiece() == Half.Get());
	Tap(EKeys::E);
	TestTrue(TEXT("Cycling wraps within the category"), Build->GetSelectedPiece() == Wall.Get());
	Tap(EKeys::R);
	TestEqual(TEXT("R rotates the piece"), Build->GetTurns(), 1);

	Build->RefreshTarget();
	TestTrue(TEXT("Looking at the floor ahead gives a buildable slot"), Build->HasValidTarget());
	const FShelterSlot Target = Build->GetTargetSlot();
	Tap(EKeys::LeftMouseButton);
	TestEqual(TEXT("The place button builds"), F.Area->GetModel().Placements.Num(), 1);
	TestTrue(TEXT("It builds the aimed slot with the chosen piece"), F.Area->GetModel().Find(Target) && F.Area->GetModel().Find(Target)->PieceId == Wall->PieceId);
	TestEqual(TEXT("Rotation is stored"), F.Area->GetModel().Find(Target)->Turns, 1);
	Tap(EKeys::LeftMouseButton);
	TestEqual(TEXT("Building the same slot again is refused"), F.Area->GetModel().Placements.Num(), 1);
	TestFalse(TEXT("The occupied slot shows as not buildable"), Build->HasValidTarget());
	TestTrue(TEXT("The remove button is consumed"), Tap(EKeys::RightMouseButton));
	TestEqual(TEXT("The remove button clears the piece"), F.Area->GetModel().Placements.Num(), 0);

	TestTrue(TEXT("Suspended item keys are consumed"), Tap(EKeys::P) && Tap(EKeys::X) && Tap(EKeys::G));
	TestFalse(TEXT("Movement keys pass through"), Build->HandleKey(EKeys::W, IE_Pressed));
	TestFalse(TEXT("Run passes through"), Build->HandleKey(EKeys::LeftShift, IE_Pressed));
	TestTrue(TEXT("Build mode stayed open"), Build->GetState() == EShelterBuildState::Placing);

	Tap(EKeys::V);
	TestFalse(TEXT("V leaves build mode"), Build->IsActive());
	TestFalse(TEXT("E is not a build key once build mode is closed"), Build->HandleKey(EKeys::E, IE_Pressed));
	Tap(EKeys::B);
	TestFalse(TEXT("The backpack key is not consumed"), Build->HandleKey(EKeys::I, IE_Pressed));
	TestFalse(TEXT("Opening the backpack leaves build mode"), Build->IsActive());
	Tap(EKeys::B);
	TestFalse(TEXT("The pockets key is not consumed"), Build->HandleKey(EKeys::Tab, IE_Pressed));
	TestFalse(TEXT("Opening the pockets leaves build mode"), Build->IsActive());
	Tap(EKeys::B);
	Controller->HandleInterfaceEscape();
	TestFalse(TEXT("Esc leaves build mode"), Build->IsActive());
	Tap(EKeys::B);
	Tap(EKeys::LeftMouseButton);
	Vitals->ApplyDamage(5);
	TestFalse(TEXT("Damage leaves build mode"), Build->IsActive());
	Tap(EKeys::B);
	TestTrue(TEXT("Build mode reopens after damage"), Build->IsActive());
	Player->SetActorLocation(FVector(9000, 9000, 100));
	Build->TickComponent(.016f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Leaving the build area closes build mode"), Build->IsActive());
	Player->SetActorLocation(FVector(300, 300, 100));
	Tap(EKeys::B);
	Vitals->ApplyDamage(10000);
	TestFalse(TEXT("Death leaves build mode"), Build->IsActive());
	Tap(EKeys::B);
	TestFalse(TEXT("A dead player cannot open build mode"), Build->IsActive());
	Controller->UnPossess();
	return true;
}

// The build menu finds pieces by asset type. This guards the scan rule in DefaultGame.ini:
// without it the build key would silently do nothing in every map.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterPieceCatalogTest, "Prototype.Shelter.PieceCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterPieceCatalogTest::RunTest(const FString&)
{
	UAssetManager* Assets = UAssetManager::GetIfInitialized();
	if (!TestNotNull(TEXT("Asset Manager"), Assets)) return false;
	TArray<FPrimaryAssetId> Ids;
	Assets->GetPrimaryAssetIdList(UShelterPieceDefinition::PieceAssetType, Ids);
	TestTrue(TEXT("Shelter pieces are discovered by type"), Ids.Num() >= 3);
	TSet<FString> Categories;
	for (const FPrimaryAssetId& Id : Ids)
	{
		auto* Piece = Cast<UShelterPieceDefinition>(Assets->GetPrimaryAssetPath(Id).TryLoad());
		if (!TestNotNull(*FString::Printf(TEXT("Piece %s loads"), *Id.ToString()), Piece)) continue;
		TestTrue(*FString::Printf(TEXT("Piece %s is valid"), *Id.ToString()), Piece->IsValidDefinition());
		TestTrue(*FString::Printf(TEXT("Piece %s is registered under its PieceId"), *Id.ToString()), Id.PrimaryAssetName == Piece->PieceId);
		TestNotNull(*FString::Printf(TEXT("Piece %s has an actor class"), *Id.ToString()), Piece->ResolveActorClass().Get());
		Categories.Add(Piece->Category.ToString());
	}
	for (const TCHAR* Expected : { TEXT("Walls"), TEXT("Doorways"), TEXT("Windows") })
		TestTrue(*FString::Printf(TEXT("Category %s has at least one piece"), Expected), Categories.Contains(Expected));
	return true;
}
#endif
