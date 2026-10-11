#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerInput.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/HittestGrid.h"
#include "Types/PaintArgs.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"

namespace
{
struct FBackpackFixture
{
	UWorld* World;
	ACharacter* Player;
	UInventoryComponent* Inventory;
	UPlayerItemUseComponent* Use;
	UPlayerVitalsComponent* Vitals;
	FInventoryItemProfile Profile;
	FGuid Id;
	explicit FBackpackFixture(bool bProjectCharacter = false)
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		UClass* Type = bProjectCharacter ? LoadClass<Aprototype3Character>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")) : ACharacter::StaticClass();
		Player = World->SpawnActor<ACharacter>(Type, FVector(1000,1000,200), FRotator::ZeroRotator);
		Inventory = Player->FindComponentByClass<UInventoryComponent>();
		if (!Inventory) { Inventory = NewObject<UInventoryComponent>(Player); Inventory->RegisterComponent(); }
		Vitals = Player->FindComponentByClass<UPlayerVitalsComponent>();
		if (!Vitals) { Vitals = NewObject<UPlayerVitalsComponent>(Player); Vitals->RegisterComponent(); }
		Vitals->InitializeVitals(100,100,50,.25f);
		Use = Player->FindComponentByClass<UPlayerItemUseComponent>();
		if (!Use) { Use = NewObject<UPlayerItemUseComponent>(Player); Use->RegisterComponent(); }
		Use->bSpawnTestSpikes = false;
		Player->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
		Player->DispatchBeginPlay();
		for (const auto& Entry : Inventory->GetEntries())
		{
			if (Entry.Item.Definition->ItemId == TEXT("Bandage")) Profile.Definition = Entry.Item.Definition;
			FItemInstance Removed; Inventory->RemoveItem(Entry.Item.InstanceId, Removed);
		}
		Profile.Id = TEXT("BackpackTestBandage");
		FInventoryShapePart Shape; Shape.Vertices = {{-20,-10},{20,-10},{20,10},{-20,10}}; Profile.ShapeParts = {Shape};
		Inventory->RegisterProfile(Profile);
		const auto Item = FItemInstance::Create(Profile.Definition); Id = Item.InstanceId;
		Inventory->AddItem(Item, Profile.Id, TEXT("Quick"), FVector2D(80), 0);
	}
	~FBackpackFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	void Slow() { Use->BeginOpenBackpack(true); Use->Advance(2.5f); Use->ReleaseBackpackInput(); }
	FInventoryEntry Entry() const { FInventoryEntry Result; Inventory->GetItem(Id, Result); return Result; }
	FInventoryArrangement Request(EInventoryArrangement Kind, FName Pocket = TEXT("Quick2"), FVector2D Position = FVector2D(80)) const
	{
		FInventoryArrangement Result; Result.Kind = Kind; Result.ItemId = Id; Result.Pocket = Pocket; Result.Position = Position; return Result;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackpackModesTest, "Prototype.Inventory.BackpackModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBackpackModesTest::RunTest(const FString&)
{
	FBackpackFixture F(true);
	auto* Character = CastChecked<Aprototype3Character>(F.Player);
	auto Move = [Character](float Right, float Forward)
	{
		Character->DoMove(Right, Forward);
	};
	TestEqual(TEXT("Runtime character uses the agreed gait threshold"), Character->GetGaitHoldThreshold(), .25f);
	F.Use->BeginOpenBackpack(true); F.Use->Advance(.1f); F.Use->ReleaseBackpackInput();
	TestTrue(TEXT("Tap selects quick mode"), F.Use->GetBackpackMode() == EBackpackMode::Quick);
	TestTrue(TEXT("Quick requests crouch"), F.Player->GetCharacterMovement()->bWantsToCrouch);
	F.Use->Advance(.399f); TestFalse(TEXT("Quick cannot open before .5 seconds from press"), F.Use->IsBackpackOpen());
	F.Use->Advance(.002f); TestTrue(TEXT("Quick opens at .5 seconds"), F.Use->IsBackpackOpen());
	Move(1,0);
	TestFalse(TEXT("Movement closes quick and proceeds"), F.Use->IsBackpackActive());
	TestEqual(TEXT("Closing movement survives"), Character->GetMovementIntent(), FVector2D(1,0));
	TestFalse(TEXT("Prior standing request restored"), F.Player->GetCharacterMovement()->bWantsToCrouch);
	Move(0,0); F.Player->Crouch();
	F.Use->BeginOpenBackpack(); F.Use->CloseBackpack();
	TestTrue(TEXT("Prior crouch request preserved"), F.Player->GetCharacterMovement()->bWantsToCrouch);
	F.Player->UnCrouch();
	F.Use->BeginOpenBackpack(true); F.Use->Advance(.25f);
	TestTrue(TEXT("Same hold threshold selects slow before quick opens"), F.Use->GetBackpackMode() == EBackpackMode::Slow);
	F.Use->ReleaseBackpackInput(); TestFalse(TEXT("Release cancels slow opening"), F.Use->IsBackpackActive());
	F.Slow(); TestTrue(TEXT("Stationary slow opening includes recognition time"), F.Use->IsBackpackOpen());
	TestFalse(TEXT("Slow leaves prior standing stance"), F.Player->GetCharacterMovement()->bWantsToCrouch);
	F.Use->CloseBackpack();
	F.Player->GetCharacterMovement()->Velocity = FVector(100,0,0);
	F.Use->BeginOpenBackpack(true); F.Use->Advance(4.f);
	TestFalse(TEXT("Walking at 60 percent is still charging at four seconds"), F.Use->IsBackpackOpen());
	F.Use->Advance(.17f); TestTrue(TEXT("Continuous walking opens at about 4.17 seconds"), F.Use->IsBackpackOpen());
	F.Use->CloseBackpack();
	F.Use->BeginOpenBackpack(true); F.Use->Advance(1.f);
	F.Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	F.Use->Advance(1.89f); TestFalse(TEXT("Mixed walking/still charge accumulates work"), F.Use->IsBackpackOpen());
	F.Use->Advance(.02f); TestTrue(TEXT("Stopping restores full charging speed"), F.Use->IsBackpackOpen());
	F.Vitals->ApplyDamage(1); TestFalse(TEXT("Damage closes slow inventory"), F.Use->IsBackpackActive());
	Move(0,1); F.Use->BeginOpenBackpack(true); F.Use->Advance(.05f); F.Use->ReleaseBackpackInput();
	TestFalse(TEXT("Tap with movement held cancels quick selection"), F.Use->IsBackpackActive());
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	FString Error;
	TestFalse(TEXT("Walking bindings conflict with inventory gestures now"), Settings->AssignKey(EInventoryControl::MoveForward,1,EKeys::LeftMouseButton,false,Error));
	TestTrue(TEXT("Backpack alternate binding remains supported"), Settings->AssignKey(EInventoryControl::Toggle,1,EKeys::J,false,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandPickupRulesTest, "Prototype.Inventory.HandPickupRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHandPickupRulesTest::RunTest(const FString&)
{
	FBackpackFixture F;
	auto* Settings = GetMutableDefault<UInventoryInputSettings>();
	TGuardValue<bool> ReplaceSetting(Settings->bAllowItemReplace, false);
	auto MakeFloor = [&]() -> ADroppedItem*
	{
		const auto Item = FItemInstance::Create(F.Profile.Definition);
		if (F.Inventory->AddItem(Item, F.Profile.Id, TEXT("Quick2"), FVector2D(80), 0) != EInventoryResult::Success) return nullptr;
		FString Error;
		return ADroppedItem::DropFromInventory(F.Inventory, Item.InstanceId, F.Player, Error);
	};
	TestTrue(TEXT("Quick item enters hands"), F.Use->EquipToHands(F.Id));
	TestTrue(TEXT("Quick item has a stow home"), F.Use->HasStowHome());
	auto* First = MakeFloor();
	if (!TestNotNull(TEXT("First floor item exists"), First)) return false;
	const FGuid FirstId = First->GetItem().InstanceId;
	TestTrue(TEXT("Ground pickup stows a quick item with replacement disabled"), F.Use->PickUpToHands(First));
	TestEqual(TEXT("Ground identity reaches hands"), F.Use->GetHeldId(), FirstId);
	TestFalse(TEXT("Ground item has no stow home"), F.Use->HasStowHome());
	TestFalse(TEXT("Ground item cannot be stowed with X"), F.Use->Stow());
	TestFalse(TEXT("Quick item returned to storage"), F.Inventory->IsReserved(F.Id));
	FInventoryEntry Held;
	TestTrue(TEXT("Held ground item stays owned"), F.Inventory->GetItem(FirstId, Held));
	TestEqual(TEXT("Ground item occupies hidden hands storage"), Held.PocketId, UPlayerItemUseComponent::HandsPocketId());
	auto* Second = MakeFloor();
	if (!TestNotNull(TEXT("Second floor item exists"), Second)) return false;
	const FGuid SecondId = Second->GetItem().InstanceId;
	TestFalse(TEXT("Replacing an unstowable item is blocked by default"), F.Use->PickUpToHands(Second));
	TestEqual(TEXT("Blocked pickup preserves hands"), F.Use->GetHeldId(), FirstId);
	TestEqual(TEXT("Blocked pickup preserves floor actor"), Second->GetItem().InstanceId, SecondId);
	Settings->bAllowItemReplace = true;
	TestTrue(TEXT("Preference enables replacement"), F.Use->CanReplaceWithFloorItem());
	TestTrue(TEXT("Second floor item remains interactable"), Second->CanInteract(F.Player));
	TestFalse(TEXT("Second floor identity is absent from inventory before transfer"), F.Inventory->GetItem(SecondId, Held));
	const bool bReplaced = F.Use->PickUpToHands(Second);
	TestTrue(FString::Printf(TEXT("Enabled replacement commits: %s"), *F.Use->Status), bReplaced);
	TestEqual(TEXT("New item is held"), F.Use->GetHeldId(), SecondId);
	TestFalse(TEXT("Old item no longer in inventory"), F.Inventory->GetItem(FirstId, Held));
	const auto Nearby = ADroppedItem::FindNearby(F.Player);
	TestTrue(TEXT("Old held item is now on the floor"), Nearby.ContainsByPredicate([FirstId](const auto& Actor)
		{ return Actor.IsValid() && Actor->GetItem().InstanceId == FirstId; }));
	TestTrue(TEXT("P drops the held item"), F.Use->DropHeld());
	TestFalse(TEXT("Hands clear after dropping"), F.Use->HasHeldItem());

	F.Use->BeginOpenBackpack(); F.Use->Advance(.5f);
	const auto BagItem = FItemInstance::Create(F.Profile.Definition);
	if (!TestEqual(TEXT("Bag fixture added"), F.Inventory->AddItem(BagItem, F.Profile.Id, TEXT("Backpack"), FVector2D(80), 0), EInventoryResult::Success)) return false;
	TestTrue(TEXT("Bag item can be taken"), F.Use->EquipToHands(BagItem.InstanceId));
	TestTrue(TEXT("Open bag preserves its stow home"), F.Use->HasStowHome());
	TestTrue(TEXT("Stow works while bag is open"), F.Use->Stow());
	F.Use->EquipToHands(BagItem.InstanceId);
	F.Use->CloseBackpack();
	TestTrue(TEXT("Closing bag keeps item held"), F.Use->GetHeldId() == BagItem.InstanceId);
	TestFalse(TEXT("Closing bag clears stow home"), F.Use->HasStowHome());
	TestFalse(TEXT("X does nothing after closure"), F.Use->Stow());
	F.Inventory->GetItem(BagItem.InstanceId, Held);
	TestEqual(TEXT("Closed bag has its former item in hands storage"), Held.PocketId, UPlayerItemUseComponent::HandsPocketId());
	F.Use->BeginOpenBackpack(); F.Use->Advance(.5f);
	TestFalse(TEXT("Reopening does not restore old reservation"), F.Use->HasStowHome());
	FInventoryArrangement Place;
	Place.Kind = EInventoryArrangement::Move; Place.ItemId = BagItem.InstanceId;
	Place.Pocket = TEXT("Backpack"); Place.Position = FVector2D(80);
	TestTrue(TEXT("Manual placement into bag succeeds"), F.Use->RequestArrangement(Place));
	TestFalse(TEXT("Manual placement clears hands"), F.Use->HasHeldItem());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandPickupPanelTest, "Prototype.Inventory.HandPickupPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHandPickupPanelTest::RunTest(const FString&)
{
	FBackpackFixture F;
	F.Use->BeginOpenBackpack(); F.Use->Advance(.5f);
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	const auto Panel = SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Player(F.Player)
		.Controls(Settings.Get()).SaveControls(false).PocketsOnly(false);
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940), FSlateLayoutTransform());
	auto Mouse = [](FVector2D P) { return FPointerEvent(0,P,P,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState()); };
	auto Down = [&](FVector2D P) { Panel->OnMouseButtonDown(G,Mouse(P)); };
	auto Up = [&](FVector2D P) { Panel->OnMouseButtonUp(G,Mouse(P)); };
	auto Move = [&](FVector2D P) { Panel->OnMouseMove(G,Mouse(P)); };
	auto Click = [&](FVector2D P) { Down(P); Up(P); };
	const FVector2D Pocket(580,350), HeldRow(550,180), Bag(150,260), FloorRow(780,168);
	Click(Pocket); Click(Pocket);
	TestEqual(TEXT("Full view takes stored quick item"), F.Use->GetHeldId(), F.Id);
	Click(HeldRow); Click(HeldRow);
	TestFalse(TEXT("Held row double-click stows quick item"), F.Use->HasHeldItem());
	const auto FloorItem = FItemInstance::Create(F.Profile.Definition);
	if (!TestEqual(TEXT("Floor fixture starts in storage"),
		F.Inventory->AddItem(FloorItem,F.Profile.Id,TEXT("Quick2"),FVector2D(80),0), EInventoryResult::Success)) return false;
	FString Error;
	auto* Floor = ADroppedItem::DropFromInventory(F.Inventory, FloorItem.InstanceId, F.Player, Error);
	if (!TestNotNull(TEXT("Floor item exists"), Floor)) return false;
	Panel->Tick(G,0,.21f);
	Click(FloorRow); Click(FloorRow);
	TestEqual(TEXT("Floor row double-click takes item into hands"), F.Use->GetHeldId(), FloorItem.InstanceId);
	Click(HeldRow); Click(HeldRow);
	TestEqual(TEXT("No-home held row double-click leaves item held"), F.Use->GetHeldId(), FloorItem.InstanceId);
	Down(HeldRow); Move(Bag); Up(Bag);
	FInventoryEntry Entry;
	TestTrue(TEXT("Dragging held item creates a manual bag placement"), F.Inventory->GetItem(FloorItem.InstanceId, Entry));
	TestEqual(TEXT("Held item reaches chosen bag position"), Entry.PocketId, FName(TEXT("Backpack")));
	TestFalse(TEXT("Successful manual placement clears hands"), F.Use->HasHeldItem());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackpackArrangementsTest, "Prototype.Inventory.BackpackArrangements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBackpackArrangementsTest::RunTest(const FString&)
{
	FBackpackFixture F;
	F.Slow();
	TestFalse(TEXT("Unchanged placement starts no timer"), F.Use->RequestArrangement(F.Request(EInventoryArrangement::Move,TEXT("Quick"))));
	TestFalse(TEXT("Invalid placement starts no timer"), F.Use->RequestArrangement(F.Request(EInventoryArrangement::Move,TEXT("Quick2"),FVector2D(-20))));
	TestFalse(TEXT("No pending operation after invalid requests"), F.Use->IsArranging());
	F.Player->GetCharacterMovement()->Velocity = FVector(100,0,0);
	TestTrue(TEXT("Changed valid destination queues"), F.Use->RequestArrangement(F.Request(EInventoryArrangement::Move)));
	TestTrue(TEXT("Queue immediately stops walking"), F.Player->GetVelocity().IsNearlyZero());
	TestEqual(TEXT("Source remains owned before commit"), F.Entry().PocketId, FName(TEXT("Quick")));
	TestEqual(TEXT("Destination presentation points at planned location"), F.Use->GetArrangement().Pocket, FName(TEXT("Quick2")));
	F.Use->Advance(.299f); TestTrue(TEXT("Still pending for full .3 seconds"), F.Use->IsArranging());
	F.Use->Advance(.002f); TestEqual(TEXT("Commit moves original identity"), F.Entry().PocketId, FName(TEXT("Quick2")));
	TestEqual(TEXT("No duplicate owned instance"), F.Inventory->GetEntries().Num(), 1);
	F.Use->RequestArrangement(F.Request(EInventoryArrangement::Move,TEXT("Quick3")));
	const auto Blocker = FItemInstance::Create(F.Profile.Definition);
	F.Inventory->AddItem(Blocker,F.Profile.Id,TEXT("Quick3"),FVector2D(80),0);
	F.Use->Advance(.3f);
	TestFalse(TEXT("Changed destination cancels pending shades"), F.Use->IsArranging());
	TestEqual(TEXT("Commit revalidation preserves source"), F.Entry().PocketId, FName(TEXT("Quick2")));
	TestTrue(TEXT("Take queues"), F.Use->RequestArrangement(F.Request(EInventoryArrangement::Take)));
	F.Use->CloseBackpack(); TestFalse(TEXT("Close cancels taking without equipping"), F.Use->HasHeldItem());
	F.Slow(); F.Use->RequestArrangement(F.Request(EInventoryArrangement::Take)); F.Use->Advance(.3f);
	TestEqual(TEXT("Take commits to hands once"), F.Use->GetHeldId(), F.Id);
	TestTrue(TEXT("Take preserves reserved storage home"), F.Inventory->IsReserved(F.Id));
	F.Use->RequestArrangement(F.Request(EInventoryArrangement::Stow)); F.Use->Advance(.1f); F.Vitals->ApplyDamage(1);
	TestEqual(TEXT("Damage cancels stow and keeps hands"), F.Use->GetHeldId(), F.Id);
	TestTrue(TEXT("Canceled stow retains reservation"), F.Inventory->IsReserved(F.Id));
	TestFalse(TEXT("Damage clears both presentation shades"), F.Use->IsArranging());
	F.Slow(); F.Use->RequestArrangement(F.Request(EInventoryArrangement::Stow)); F.Use->Advance(.3f);
	TestFalse(TEXT("Completed stow releases reservation"), F.Inventory->IsReserved(F.Id));
	F.Use->RequestArrangement(F.Request(EInventoryArrangement::Drop)); F.Use->CloseBackpack();
	TestEqual(TEXT("Canceled drop spawns nothing"), ADroppedItem::FindNearby(F.Player).Num(), 0);
	F.Slow(); F.Use->RequestArrangement(F.Request(EInventoryArrangement::Drop));
	TestEqual(TEXT("Queued drop spawns nothing early"), ADroppedItem::FindNearby(F.Player).Num(), 0);
	F.Use->Advance(.3f);
	const auto Floor = ADroppedItem::FindNearby(F.Player);
	if (!TestEqual(TEXT("Completed drop creates one world owner"), Floor.Num(), 1)) return false;
	TestEqual(TEXT("Dropped instance removed from storage"), F.Inventory->GetEntries().Num(), 1); // blocker only
	auto Pickup = F.Request(EInventoryArrangement::PickUp,TEXT("Quick")); Pickup.FloorItem = Floor[0];
	F.Use->RequestArrangement(Pickup); F.Use->Advance(.1f);
	Floor[0]->SetActorLocation(F.Player->GetActorLocation()+FVector(500,0,0),false,nullptr,ETeleportType::TeleportPhysics);
	F.Use->Advance(.3f);
	TestFalse(TEXT("Range loss cancels pickup"), F.Use->IsArranging());
	TestTrue(TEXT("Range loss preserves floor identity"), Floor[0]->GetItem().InstanceId == F.Id);
	Floor[0]->SetActorLocation(F.Player->GetActorLocation(),false,nullptr,ETeleportType::TeleportPhysics);
	F.Use->RequestArrangement(Pickup); F.Use->Advance(.3f);
	TestEqual(TEXT("Delayed pickup preserves original identity"), F.Entry().Item.InstanceId, F.Id);
	TestFalse(TEXT("World source consumed only at successful commit"), Floor[0].IsValid());
	F.Use->CloseBackpack(); F.Use->BeginOpenBackpack(); F.Use->Advance(.5f);
	TestTrue(TEXT("Quick move succeeds instantly"), F.Use->RequestArrangement(F.Request(EInventoryArrangement::Move,TEXT("Quick2"))));
	TestFalse(TEXT("Quick move has no timer"), F.Use->IsArranging());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackpackPanelDelayTest, "Prototype.Inventory.BackpackPanelDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBackpackPanelDelayTest::RunTest(const FString&)
{
	FBackpackFixture F; F.Slow();
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	const auto Panel = SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Player(F.Player).Controls(Settings.Get()).SaveControls(false);
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940), FSlateLayoutTransform(1.f));
	auto Mouse = [&](FVector2D P) { return FPointerEvent(0,P,P,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState()); };
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(580,350)));
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(140,250)));
	TestTrue(TEXT("Slate placement queues in walking mode"), F.Use->IsArranging());
	TestEqual(TEXT("Slate does not mutate source on release"), F.Entry().PocketId,FName(TEXT("Quick")));
	Panel->OnKeyDown(G,FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0));
	TestEqual(TEXT("Pending arrangement blocks pocket switching"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	F.Use->Advance(.3f);
	TestEqual(TEXT("Slate placement commits after delay"), F.Entry().PocketId,FName(TEXT("Backpack")));
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(140,250)));
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(580,350)));
	Panel->CancelInteraction();
	TestFalse(TEXT("Interface cancellation clears pending state"),F.Use->IsArranging());
	TestEqual(TEXT("Canceled gesture keeps committed source"),F.Entry().PocketId,FName(TEXT("Backpack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackpackPresentationTest, "Prototype.Inventory.BackpackPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBackpackPresentationTest::RunTest(const FString&)
{
	FBackpackFixture F;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	const auto Panel = SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Player(F.Player).Controls(Settings.Get()).SaveControls(false);
	const auto Window = SNew(SWindow).ClientSize(FVector2D(1540,940));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940), FSlateLayoutTransform(1.f));
	FHittestGrid HitTest;
	auto Paint = [&]()
	{
		FSlateWindowElementList Elements(Window);
		return Panel->OnPaint(FPaintArgs(&Panel.Get(), HitTest, FVector2D::ZeroVector, 0, .016f),
			G, FSlateRect(0,0,1540,940), Elements, 0, FWidgetStyle(), true);
	};
	F.Use->BeginOpenBackpack(true);
	TestEqual(TEXT("Initial press paints no preliminary screen"), Paint(), 0);
	F.Use->Advance(.1f);
	TestEqual(TEXT("Tap/hold recognition remains invisible"), Paint(), 0);
	F.Use->ReleaseBackpackInput();
	TestTrue(TEXT("Tap displays only quick opening"), F.Use->GetBackpackMode() == EBackpackMode::Quick && Paint() > 0);
	F.Use->Advance(.4f);
	TestTrue(TEXT("Quick inventory opens on its existing timer"), F.Use->IsBackpackOpen() && Paint() > 0);
	F.Use->CloseBackpack();
	TestEqual(TEXT("Closing paints no leftover loading panel"), Paint(), 0);
	F.Use->BeginOpenBackpack(true); F.Use->Advance(.25f);
	TestTrue(TEXT("Recognized hold displays only slow opening"), F.Use->GetBackpackMode() == EBackpackMode::Slow && Paint() > 0);
	F.Use->ReleaseBackpackInput();
	TestEqual(TEXT("Releasing during charge removes the loading panel"), Paint(), 0);
	F.Slow();
	TestTrue(TEXT("Releasing after completion keeps loaded inventory visible"), F.Use->IsBackpackOpen() && Paint() > 0);
	Panel->SetPocketsOnly(true);
	TestTrue(TEXT("Immediate pockets view remains visible"), Paint() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackpackInputRoutingTest, "Prototype.Inventory.BackpackInputRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBackpackInputRoutingTest::RunTest(const FString&)
{
	FBackpackFixture F;
	auto* Type = LoadClass<Aprototype3PlayerController>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"));
	if (!TestNotNull(TEXT("Player controller class"), Type)) return false;
	auto* Controller = F.World->SpawnActor<Aprototype3PlayerController>(Type);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Player->PlayerController = Controller;
	auto* Input = NewObject<UEnhancedPlayerInput>(Controller);
	Controller->PlayerInput = Input;
	auto* MoveAction = NewObject<UInputAction>(Controller); MoveAction->ValueType = EInputActionValueType::Axis2D;
	auto* Mapping = NewObject<UInputMappingContext>(Controller);
	Mapping->MapKey(MoveAction, EKeys::W);
	auto* Subsystem = NewObject<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	FModifyContextOptions MappingOptions; MappingOptions.bForceImmediately = true;
	Subsystem->AddMappingContext(Mapping, 0, MappingOptions);
	Controller->Possess(F.Player);
	TStrongObjectPtr<UInventoryInputSettings> Saved(DuplicateObject<UInventoryInputSettings>(GetMutableDefault<UInventoryInputSettings>(), GetTransientPackage()));
	auto* Settings = GetMutableDefault<UInventoryInputSettings>(); Settings->ResetDefaults();
	FString Error; Settings->AssignKey(EInventoryControl::Toggle,1,EKeys::J,false,Error);
	auto BeginPress = [&]()
	{
		Controller->CloseInventoryDemo();
		F.Use->BeginOpenBackpack(true);
		Controller->bInventoryDemoOpen = true;
		Controller->BackpackKeysDown.Add(EKeys::I);
		// Reproduce the viewport focus flush queued by opening the Slate interface.
		Controller->FlushPressedKeys();
	};
	BeginPress(); F.Use->Advance(.1f);
	TestTrue(TEXT("Focus flush preserves the physical opener"),Controller->BackpackKeysDown.Contains(EKeys::I));
	TestTrue(TEXT("Release routes through the inventory controller"),Controller->HandleInventoryControlKey(EKeys::I,IE_Released));
	TestTrue(TEXT("Release after focus change selects quick mode"),F.Use->GetBackpackMode()==EBackpackMode::Quick);
	F.Use->Advance(.4f); TestTrue(TEXT("Tap completes quick opening"),F.Use->IsBackpackOpen());
	BeginPress(); F.Use->Advance(.3f);
	Controller->HandleInventoryControlKey(EKeys::I,IE_Released);
	TestFalse(TEXT("Release while slow charging cancels opening"),F.Use->IsBackpackActive());
	BeginPress(); F.Use->Advance(.3f);
	Controller->HandleInventoryControlKey(EKeys::J,IE_Pressed);
	Controller->HandleInventoryControlKey(EKeys::I,IE_Released);
	TestTrue(TEXT("Alternate binding keeps the same hold alive"),F.Use->IsOpeningBackpack());
	Controller->HandleInventoryControlKey(EKeys::J,IE_Released);
	TestFalse(TEXT("Last held binding release cancels charging"),F.Use->IsBackpackActive());
	BeginPress(); F.Use->Advance(2.5f);
	Controller->HandleInventoryControlKey(EKeys::I,IE_Released);
	TestTrue(TEXT("Release after loading leaves slow inventory open"),F.Use->IsBackpackOpen());
	Controller->HandleInventoryControlKey(EKeys::I,IE_Pressed);
	TestFalse(TEXT("A fresh press closes loaded inventory"),F.Use->IsBackpackActive());
	BeginPress();
	Controller->HandleInventoryControlKey(EKeys::W,IE_Pressed);
	TArray<UInputComponent*> Stack;
	Controller->PlayerInput->ProcessInputStack(Stack,.016f,false);
	TestTrue(TEXT("Walking press reaches player input"),Controller->IsInputKeyDown(EKeys::W));
	Controller->FlushPressedKeys();
	Controller->PlayerInput->ProcessInputStack(Stack,.016f,false);
	TestTrue(TEXT("UI focus flush retains held walking"),Controller->IsInputKeyDown(EKeys::W));
	TestTrue(TEXT("Enhanced movement action is not ignored after flush"),Input->GetActionValue(MoveAction).GetMagnitude() > 0);
	Controller->CloseInventoryDemo();
	Input->ProcessInputStack(Stack,.016f,false);
	TestTrue(TEXT("Closing also preserves held walking"),Controller->IsInputKeyDown(EKeys::W));
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0.f));
	Controller->PlayerInput->ProcessInputStack(Stack,.016f,false);
	TestFalse(TEXT("Walking release is not stuck"),Controller->IsInputKeyDown(EKeys::W));
	Controller->CloseInventoryDemo();
	Settings->CopySettingsFrom(*Saved.Get());
	return true;
}
#endif
