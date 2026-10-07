#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FFloorFixture
{
	UWorld* World;
	ACharacter* Player;
	UInventoryComponent* Inventory;
	UPlayerItemUseComponent* Use;
	FInventoryItemProfile Profile;
	FFloorFixture()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Player = World->SpawnActor<ACharacter>(FVector(1000,1000,200), FRotator::ZeroRotator);
		Inventory = NewObject<UInventoryComponent>(Player); Inventory->RegisterComponent();
		auto* Vitals = NewObject<UPlayerVitalsComponent>(Player); Vitals->RegisterComponent();
		Vitals->InitializeVitals(100,100,50,.25f);
		Use = NewObject<UPlayerItemUseComponent>(Player);
		Use->bSpawnTestSpikes = false; Use->RegisterComponent();
		Player->DispatchBeginPlay();
		for (const auto& Entry : Inventory->GetEntries())
		{
			if (Entry.Item.Definition->ItemId == TEXT("Bandage")) Profile.Definition = Entry.Item.Definition;
			FItemInstance Removed; Inventory->RemoveItem(Entry.Item.InstanceId, Removed);
		}
		Profile.Id = TEXT("FloorTestBandage");
		FInventoryShapePart Shape; Shape.Vertices = {{-40,-20},{40,-20},{40,20},{-40,20}};
		Profile.ShapeParts = {Shape};
		Inventory->RegisterProfile(Profile);
	}
	~FFloorFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	ADroppedItem* Drop()
	{
		const auto Item = FItemInstance::Create(Profile.Definition);
		if (Inventory->AddItem(Item, Profile.Id, TEXT("Quick"), FVector2D(60,60),0) != EInventoryResult::Success) return nullptr;
		FString Error;
		return ADroppedItem::DropFromInventory(Inventory, Item.InstanceId, Player, Error);
	}
	FVector Feet() const { return Player->GetActorLocation() - FVector(0,0,Player->GetSimpleCollisionHalfHeight()); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorTransferTest, "Prototype.Inventory.FloorTransfers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloorTransferTest::RunTest(const FString&)
{
	FFloorFixture F;
	auto* Actor = F.Drop();
	if (!TestNotNull(TEXT("Existing drop prepares a physical actor"), Actor)) return false;
	const FItemInstance Item = Actor->GetItem();
	const FName Profile = Actor->GetInventoryProfileId();
	const FVector Original = Actor->GetActorLocation();
	TestTrue(TEXT("Nearby actor discoverable"), ADroppedItem::FindNearby(F.Player).Contains(Actor));
	TestTrue(TEXT("Invalid pocket rejected"), Actor->PickUp(F.Inventory,F.Player,TEXT("Missing"),FVector2D(100),0) != EInventoryResult::Success);
	TestTrue(TEXT("Failed pickup keeps actor and location"), Actor->CanInteract(F.Player) && Actor->GetActorLocation().Equals(Original));
	const auto Blocker = FItemInstance::Create(F.Profile.Definition);
	F.Inventory->AddItem(Blocker,Profile,TEXT("Quick"),FVector2D(100),0);
	TestTrue(TEXT("Collision leaves source on floor"), Actor->PickUp(F.Inventory,F.Player,TEXT("Quick"),FVector2D(100),0) == EInventoryResult::Occupied);
	FItemInstance Removed; F.Inventory->RemoveItem(Blocker.InstanceId,Removed);
	// Corrupt/competing duplicate admission must fail without consuming the world source.
	F.Inventory->AddItem(Item,Profile,TEXT("Quick"),FVector2D(100),0);
	TestTrue(TEXT("Duplicate identity rejected"), Actor->PickUp(F.Inventory,F.Player,TEXT("Quick2"),FVector2D(100),0) == EInventoryResult::DuplicateId);
	TestTrue(TEXT("Duplicate rejection preserves source"), Actor->CanInteract(F.Player));
	F.Inventory->RemoveItem(Item.InstanceId,Removed);
	Actor->SetActorLocation(F.Feet()+FVector(251,0,0),false,nullptr,ETeleportType::TeleportPhysics);
	FString Error;
	TestFalse(TEXT("Beyond 2.5m excluded"), Actor->CanInteract(F.Player));
	TestTrue(TEXT("Out of range pickup fails"), Actor->PickUp(F.Inventory,F.Player,TEXT("Quick"),FVector2D(100),0) != EInventoryResult::Success);
	TestFalse(TEXT("Out of range relocation fails"), Actor->DropAtFeet(F.Player,Error));
	TestTrue(TEXT("Failed relocation leaves source in place"), Actor->GetActorLocation().Equals(F.Feet()+FVector(251,0,0)));
	Actor->SetActorLocation(F.Feet()+FVector(250,0,0),false,nullptr,ETeleportType::TeleportPhysics);
	TestTrue(TEXT("Radius boundary accepted"), Actor->CanInteract(F.Player));
	Actor->GetBody()->SetPhysicsLinearVelocity(FVector(50,20,0));
	TestTrue(TEXT("Floor to floor drop succeeds"), Actor->DropAtFeet(F.Player,Error));
	TestTrue(TEXT("Existing actor moves to the same feet placement"), Actor->GetActorLocation().Equals(Original));
	TestEqual(TEXT("Floor relocation preserves identity"),Actor->GetItem().InstanceId,Item.InstanceId);
	TestTrue(TEXT("Relocation clears old momentum"),Actor->GetBody()->GetPhysicsLinearVelocity().IsNearlyZero());
	TestTrue(TEXT("Pickup succeeds"),Actor->PickUp(F.Inventory,F.Player,TEXT("Quick2"),FVector2D(110,100),35) == EInventoryResult::Success);
	FInventoryEntry Entry;
	TestTrue(TEXT("Original identity in inventory"),F.Inventory->GetItem(Item.InstanceId,Entry));
	TestEqual(TEXT("Quantity preserved"),Entry.Item.Quantity,Item.Quantity);
	TestEqual(TEXT("Definition preserved"),Entry.Item.Definition.Get(),Item.Definition.Get());
	TestEqual(TEXT("Profile preserved"),Entry.ProfileId,Profile);
	TestEqual(TEXT("Rotation applied"),Entry.AngleDegrees,35.0);
	TestTrue(TEXT("Source actor destroyed"),Actor->IsActorBeingDestroyed());
	TestTrue(TEXT("Repeated pickup rejected"),Actor->PickUp(F.Inventory,F.Player,TEXT("Quick3"),FVector2D(100),0) != EInventoryResult::Success);
	TestEqual(TEXT("Exactly one owned instance"),F.Inventory->GetEntries().Num(),1);
	TestEqual(TEXT("No remaining nearby world copy"),ADroppedItem::FindNearby(F.Player).Num(),0);
	F.Inventory->ReserveItem(Item.InstanceId);
	TestNull(TEXT("Reserved item cannot drop"),ADroppedItem::DropFromInventory(F.Inventory,Item.InstanceId,F.Player,Error));
	F.Inventory->ReleaseItem(Item.InstanceId);
	Actor = ADroppedItem::DropFromInventory(F.Inventory,Item.InstanceId,F.Player,Error);
	if (!TestNotNull(TEXT("Picked-up item can be dropped again"),Actor)) return false;
	TestEqual(TEXT("Round trip keeps identity"),Actor->GetItem().InstanceId,Item.InstanceId);
	TestEqual(TEXT("Round trip leaves inventory empty"),F.Inventory->GetEntries().Num(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorPanelTest, "Prototype.Inventory.FloorPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloorPanelTest::RunTest(const FString&)
{
	FFloorFixture F;
	auto* Actor = F.Drop();
	if (!TestNotNull(TEXT("Floor fixture exists"),Actor)) return false;
	const FGuid Id = Actor->GetItem().InstanceId;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	int32 Drops = 0;
	const auto Panel = SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Player(F.Player).Controls(Settings.Get()).SaveControls(false).PocketsOnly(true)
		.OnDropItem(SInventoryPanel::FOnDropItem::CreateLambda([&](FGuid Item,FString& Error)
		{
			++Drops;
			return ADroppedItem::DropFromInventory(F.Inventory,Item,F.Player,Error) != nullptr;
		}));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940),FSlateLayoutTransform(1.5f,FVector2f(120,90)));
	auto Mouse = [&](FVector2D P,FKey K,float Wheel=0.f) { auto Screen=G.LocalToAbsolute(P); return FPointerEvent(0,Screen,Screen,TSet<FKey>(),K,Wheel,FModifierKeysState()); };
	auto Down = [&](FVector2D P,FKey K=EKeys::LeftMouseButton) { Panel->OnMouseButtonDown(G,Mouse(P,K)); };
	auto Up = [&](FVector2D P) { Panel->OnMouseButtonUp(G,Mouse(P,EKeys::LeftMouseButton)); };
	auto Key = [&](FKey K) { Panel->OnKeyDown(G,FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	auto Tick = [&]() { Panel->Tick(G,0,.21f); };
	auto Present = [&]() { FInventoryEntry E; return F.Inventory->GetItem(Id,E); };
	const FVector2D Floor(780,168), Quick(610,250), Bag(150,260);
	Down(Floor); Panel->OnKeyDown(G,FKeyEvent(EKeys::C,FModifierKeysState(),0,false,0,0)); Up(Quick);
	TestFalse(TEXT("Cancel never admits floor item"),Present());
	Down(Floor); Up(FVector2D(750,250));
	TestTrue(TEXT("Invalid gap release preserves world source"),Actor->CanInteract(F.Player));
	Panel->SetPocketsOnly(false);
	Down(Floor); Up(Bag);
	TestFalse(TEXT("Opening full interface blocks floor input"),Present());
	F.Use->BeginOpenBackpack(); F.Use->Advance(1); Tick();
	Down(Floor); Up(Bag);
	TestFalse(TEXT("Opening delay still rejects pickup"),Present());
	F.Use->Advance(1); Tick();
	Down(Floor); Key(EKeys::Escape); Up(Bag);
	TestFalse(TEXT("Closing mid-drag cancels"),Present());
	Down(Floor); Panel->OnMouseCaptureLost(FCaptureLostEvent()); Up(Quick);
	TestFalse(TEXT("Capture loss cancels"),Present());
	Down(Floor); Actor->SetActorLocation(F.Feet()+FVector(400,0,0),false,nullptr,ETeleportType::TeleportPhysics); Up(Quick);
	TestFalse(TEXT("Range checked again at release without waiting for refresh"),Present());
	Actor->SetActorLocation(F.Feet()+FVector(100,0,5),false,nullptr,ETeleportType::TeleportPhysics); Tick();
	const FVector Original=Actor->GetActorLocation();
	Down(Floor); Up(FVector2D(759,250));
	TestTrue(TEXT("Partly outside storage restores floor position"),Actor->GetActorLocation().Equals(Original));
	Down(Floor); Up(FVector2D(760,250));
	TestTrue(TEXT("Touching storage edge still cancels"),Actor->GetActorLocation().Equals(Original));
	Down(Floor); Up(FVector2D(761,250));
	TestTrue(TEXT("Whole object clear of grids can drop over floor list"),Actor->CanInteract(F.Player) && !Actor->GetActorLocation().Equals(Original));
	TestEqual(TEXT("Floor to floor retains identity"),Actor->GetItem().InstanceId,Id);
	TestEqual(TEXT("Floor to floor never enters storage"),F.Inventory->GetEntries().Num(),0);
	Key(EKeys::Tab); Key(EKeys::Tab); Down(Floor); Up(Quick);
	FInventoryEntry Entry;
	TestTrue(TEXT("Floor pickup into third pocket succeeds"),F.Inventory->GetItem(Id,Entry));
	TestEqual(TEXT("Uses cycled third pocket"),Entry.PocketId,FName(TEXT("Quick3")));
	Down(Quick); Up(FVector2D(759,250));
	TestTrue(TEXT("Partial inventory drag retains source"),Present());
	TestEqual(TEXT("Partial drag never calls drop"),Drops,0);
	Down(Quick); Up(FVector2D(480,250));
	TestTrue(TEXT("Overlap with either storage container cancels drop"),Present());
	TestEqual(TEXT("Cross-container overlap never calls drop"),Drops,0);
	Down(Quick); F.Inventory->ReserveItem(Id); Up(FVector2D(761,250));
	TestTrue(TEXT("Reservation acquired mid-drag prevents world drop"),Present());
	F.Inventory->ReleaseItem(Id);
	// Rotated footprint is 40 wide: cursor alone cannot decide whether the item cleared the edge.
	Down(Quick); Key(EKeys::E); Panel->Tick(G,0,.75f);
	Panel->OnKeyUp(G,FKeyEvent(EKeys::E,FModifierKeysState(),0,false,0,0));
	Up(FVector2D(739,250));
	TestEqual(TEXT("Rotated partial overlap does not drop"),Drops,0);
	Settings->bToggleGrab=true;
	Down(Quick); Up(Quick); Down(FVector2D(610,391)); Up(FVector2D(610,391));
	TestEqual(TEXT("Click mode drop below pocket inside panel succeeds once"),Drops,1);
	TestFalse(TEXT("World drop removes source entry"),Present());
	Down(Floor); Up(Floor); Down(Bag); Up(Bag);
	TestTrue(TEXT("Click mode pickup into open backpack succeeds"),F.Inventory->GetItem(Id,Entry));
	TestEqual(TEXT("Backpack destination"),Entry.PocketId,FName(TEXT("Backpack")));
	Key(EKeys::F);
	TestEqual(TEXT("Mapped Drop still works on selected item"),Drops,2);
	Settings->bToggleGrab=false;
	Down(Floor);
	const auto Nearby=ADroppedItem::FindNearby(F.Player);
	if (Nearby.Num()==1) Nearby[0]->Destroy();
	Tick(); Up(Quick);
	TestFalse(TEXT("Destroyed source cancels safely"),Present());
	TestEqual(TEXT("No duplicated inventory items"),F.Inventory->GetEntries().Num(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorScrollTest, "Prototype.Inventory.FloorScroll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloorScrollTest::RunTest(const FString&)
{
	FFloorFixture F;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	const auto Panel=SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Player(F.Player).Controls(Settings.Get()).SaveControls(false).PocketsOnly(true);
	for (int32 I=0; I<20; ++I) if (!TestNotNull(TEXT("Scroll fixture spawned"),F.Drop())) return false;
	const auto Nearby=ADroppedItem::FindNearby(F.Player);
	const FGeometry G=FGeometry::MakeRoot(FVector2D(1540,940),FSlateLayoutTransform(1));
	auto Mouse=[&](FVector2D P,FKey K,float Wheel=0.f) { return FPointerEvent(0,P,P,TSet<FKey>(),K,Wheel,FModifierKeysState()); };
	Panel->Tick(G,0,.21f);
	Panel->OnMouseWheel(G,Mouse(FVector2D(780,168),EKeys::MouseWheelAxis,-1));
	TestEqual(TEXT("List scrolling does not alter rotation speed"),Settings->TurnSpeed,120.f);
	const FGuid Expected=Nearby[3]->GetItem().InstanceId;
	FString Error;
	Settings->AssignKey(EInventoryControl::Grab,1,EKeys::J,false,Error);
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(780,168),EKeys::LeftMouseButton));
	Panel->OnKeyDown(G,FKeyEvent(EKeys::J,FModifierKeysState(),0,false,0,0));
	Panel->OnMouseWheel(G,Mouse(FVector2D(780,168),EKeys::MouseWheelAxis,1));
	TestEqual(TEXT("Wheel during drag adjusts rotation speed"),Settings->TurnSpeed,135.f);
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(150,260),EKeys::LeftMouseButton));
	FInventoryEntry Entry;
	TestFalse(TEXT("Floor drag waits for last grab binding release"),F.Inventory->GetItem(Expected,Entry));
	Panel->OnKeyUp(G,FKeyEvent(EKeys::J,FModifierKeysState(),0,false,0,0));
	TestTrue(TEXT("Scrolled row picks the displayed instance"),F.Inventory->GetItem(Expected,Entry));
	TestEqual(TEXT("Duplicate names do not duplicate items"),ADroppedItem::FindNearby(F.Player).Num(),19);
	Panel->SetPocketsOnly(false); F.Use->Advance(2); Panel->Tick(G,0,.21f);
	const auto Remaining=ADroppedItem::FindNearby(F.Player);
	const FGuid Last=Remaining.Last()->GetItem().InstanceId;
	Settings->TrySetKey(EInventoryControl::FasterRotation,EKeys::Invalid,Error);
	Settings->TrySetKey(EInventoryControl::SlowerRotation,EKeys::Invalid,Error);
	Panel->OnMouseWheel(G,Mouse(FVector2D(780,708),EKeys::MouseWheelAxis,-100));
	TestEqual(TEXT("Unbound speed controls preserve fixed rotation speed"),Settings->TurnSpeed,135.f);
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(780,708),EKeys::LeftMouseButton));
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(150,550),EKeys::LeftMouseButton));
	TestTrue(TEXT("Extended floor list scrolls and selects near backpack bottom"),F.Inventory->GetItem(Last,Entry));
	TestEqual(TEXT("Extended list pickup reaches backpack"),Entry.PocketId,FName(TEXT("Backpack")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeldItemDropTest, "Prototype.Inventory.HeldItemDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeldItemDropTest::RunTest(const FString&)
{
	FFloorFixture F;
	const FItemInstance Item = FItemInstance::Create(F.Profile.Definition);
	if (!TestTrue(TEXT("Fixture placed"),F.Inventory->AddItem(Item,F.Profile.Id,TEXT("Quick"),FVector2D(60),0)==EInventoryResult::Success)) return false;
	if (!TestTrue(TEXT("Take fixture in hands"),F.Use->EquipToHands(Item.InstanceId))) return false;
	FString Error;
	TestNull(TEXT("Invalid world drop fails"),ADroppedItem::DropFromInventory(F.Inventory,Item.InstanceId,nullptr,Error));
	TestEqual(TEXT("Failed drop retains hands"),F.Use->GetHeldId(),Item.InstanceId);
	TestTrue(TEXT("Failed drop retains reservation"),F.Inventory->IsReserved(Item.InstanceId));
	auto* Dropped = ADroppedItem::DropFromInventory(F.Inventory,Item.InstanceId,F.Player,Error);
	if (!TestNotNull(TEXT("Owned held item drops successfully"),Dropped)) return false;
	TestEqual(TEXT("World actor has original identity"),Dropped->GetItem().InstanceId,Item.InstanceId);
	TestFalse(TEXT("Successful drop clears hands"),F.Use->HasHeldItem());
	TestFalse(TEXT("Successful drop clears reservation"),F.Inventory->IsReserved(Item.InstanceId));
	FInventoryEntry Entry;
	TestFalse(TEXT("No inventory duplicate after held drop"),F.Inventory->GetItem(Item.InstanceId,Entry));
	return true;
}
#endif
