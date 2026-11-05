#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterGameMode.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/HouseOfficePlan.h"
#include "Gameplay/Procedural/HouseSpatial.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "FileHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AI/NavigationSystemConfig.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"

namespace EncounterTests
{
UNavigationSystemV1* SetNavigation(UWorld* World, FNavigationSystemRunMode Mode)
{
	auto* Config = World->GetWorldSettings()->GetNavigationSystemConfig();
	Config->NavigationSystemClass = FSoftClassPath(UHouseEncounterNavigationSystem::StaticClass());
	FNavigationSystem::AddNavigationSystemToWorld(*World, Mode, Config, false, true);
	return FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
}

struct FWorld
{
	UWorld* World;
	UNavigationSystemV1* Nav;
	AHouseEncounterNavMesh* Mesh;
	ACharacter* Player;
	UInventoryComponent* Inventory;
	UPlayerItemUseComponent* Use;
	FWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Nav = SetNavigation(World, FNavigationSystemRunMode::GameMode);
		World->SpawnActor<AHouseEncounterNavBounds>(FVector(1400,1400,100), FRotator::ZeroRotator);
		Mesh = World->SpawnActor<AHouseEncounterNavMesh>();
		Nav->OnWorldInitDone(FNavigationSystemRunMode::GameMode);
		Player = World->SpawnActor<ACharacter>(FVector(-400,-400,100), FRotator::ZeroRotator);
		Player->GetCapsuleComponent()->SetCapsuleSize(34,96);
		Inventory = NewObject<UInventoryComponent>(Player); Inventory->RegisterComponent();
		auto* Vitals = NewObject<UPlayerVitalsComponent>(Player); Vitals->RegisterComponent();
		Vitals->InitializeVitals(100,100,50,.25f);
		Use = NewObject<UPlayerItemUseComponent>(Player); Use->bSpawnTestSpikes = false; Use->RegisterComponent();
		Player->DispatchBeginPlay();
	}
	~FWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	AHouseEncounter* Create(int32 Seed)
	{
		auto* E = World->SpawnActor<AHouseEncounter>();
		E->bStartOnBeginPlay = false;
		E->HouseSettings.Seed = Seed;
		E->DispatchBeginPlay();
		return E;
	}
	bool Activate(AHouseEncounter* Encounter)
	{
		for (int32 Step=0; Step<9 && Encounter->State==EHouseEncounterState::Preparing; ++Step)
		{
			Build();
			Encounter->Activate();
		}
		return Encounter->State==EHouseEncounterState::Active;
	}
	void Build()
	{
		Nav->ProcessPendingOctreeUpdates();
		Nav->Build();
		for (TActorIterator<ANavigationData> It(World); It; ++It) It->EnsureBuildCompletion();
	}
};

void CheckStartingRoom(FAutomationTestBase& Test, const AHouseGenerator* House,
	const FHouseEncounterPlacement& Placement, float ZombieRadius)
{
	const FHouseWall* Entrance = House->Layout.Walls.FindByPredicate([](const FHouseWall& Wall)
		{ return Wall.RoomB == INDEX_NONE && Wall.Opening == EHouseOpening::Door; });
	if (!Test.TestNotNull(TEXT("House has an entrance room"), Entrance)) return;
	const FHouseRoom& Room = House->Layout.Rooms[Entrance->RoomA];
	const float Cell = House->GeneratedSettings.CellSize;
	const FBox2D Bounds(FVector2D(Room.Min) * Cell, FVector2D(Room.Max) * Cell);
	const FTransform Transform = House->GetActorTransform();
	const FVector LocalPlayer = Transform.InverseTransformPosition(Placement.PlayerFeet);
	Test.TestTrue(TEXT("Player starts in entrance room"), Bounds.IsInsideOrOn(FVector2D(LocalPlayer.X, LocalPlayer.Y)));
	for (const FVector& Feet : Placement.ZombieFeet)
	{
		const FVector LocalZombie = Transform.InverseTransformPosition(Feet);
		Test.TestFalse(TEXT("Zombie body stays outside whole player starting room"),
			Bounds.ExpandBy(ZombieRadius).IsInsideOrOn(FVector2D(LocalZombie.X, LocalZombie.Y)));
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterChoicesTest, "Prototype.Encounter.ExitChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterChoicesTest::RunTest(const FString&)
{
	FHouseExitDecision Decision;
	TestFalse(TEXT("Cannot choose before leaving"), Decision.Choose(TEXT("Street")));
	TestFalse(TEXT("Duplicate choices rejected atomically"), Decision.Offer({TEXT("Street"),TEXT("Street")}));
	TestFalse(TEXT("Failed offer leaves closed state"), Decision.bOffered);
	TestTrue(TEXT("Leave offers all choices"), Decision.Offer({TEXT("Street"),TEXT("Another house"),TEXT("Shelter")}));
	TestTrue(TEXT("Choice initially available"), Decision.IsAvailable(TEXT("Shelter")));
	TestFalse(TEXT("Unknown option rejected"), Decision.Choose(TEXT("Unknown")));
	TestTrue(TEXT("Selection commits"), Decision.Choose(TEXT("Street")));
	TestEqual(TEXT("Selection recorded"), Decision.Selected, FName(TEXT("Street")));
	TestFalse(TEXT("Unchosen option unavailable"), Decision.IsAvailable(TEXT("Shelter")));
	TestFalse(TEXT("Cannot choose unchosen option later"), Decision.Choose(TEXT("Shelter")));
	TestFalse(TEXT("Cannot double-submit"), Decision.Choose(TEXT("Street")));
	TestFalse(TEXT("Cannot reopen completed decision"), Decision.Offer({TEXT("Street"),TEXT("Shelter")}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterIntegrationTest, "Prototype.Encounter.PlacementNavigationAndPickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterIntegrationTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	for (int32 Seed : {12345, 7, 42, 93, -19, 217})
	{
		Fixture.Player->SetActorLocation(FVector(-400,-400,100));
		auto* Encounter = Fixture.Create(Seed);
		if (Seed == 93 || Seed == 217)
		{
			Encounter->HouseSettings.Navigation.Pattern = Seed == 93 ? EHouseLayoutPattern::CentralHub : EHouseLayoutPattern::DeepBranches;
			Encounter->HouseSettings.WidthCells = Encounter->HouseSettings.DepthCells = 24;
			Encounter->HouseSettings.RoomCount = 8;
		}
		// Exercise adjustable counts in the compact Deep Branches case as well as the default 3/6.
		const int32 Zombies = Seed == 217 ? 1 : 3, Items = Seed == 217 ? 3 : 6;
		Encounter->ZombieCount = Zombies; Encounter->ItemCount = Items;
		const int32 OwnedBefore = Fixture.Inventory->GetEntries().Num();
		if (!TestTrue(TEXT("Prepare seed ") + FString::FromInt(Seed), Encounter->Prepare(Fixture.Player)))
		{ AddError(Encounter->LastError); return false; }
		Fixture.Build();
		FHouseEncounterPlacement First, Second;
		FString Error;
		if (!TestTrue(TEXT("Collision and Recast plan: ") + FString::FromInt(Seed), HouseEncounterPlacement::Plan(
			Encounter->House, Fixture.Player,34,96,Zombies,Items,Seed,First,Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Same request plans twice"), HouseEncounterPlacement::Plan(Encounter->House, Fixture.Player,34,96,Zombies,Items,Seed,Second,Error));
		TestTrue(TEXT("Spawn positions deterministic"), First.ZombieFeet == Second.ZombieFeet && First.ItemFeet == Second.ItemFeet
			&& First.PlayerFeet == Second.PlayerFeet && First.ExitFeet == Second.ExitFeet);
		TestTrue(TEXT("Player reaches outside exit"), HouseEncounterPlacement::Reachable(Fixture.World,First.PlayerFeet,First.ExitFeet,34,96,Fixture.Player));
		for (const FVector& Feet : First.ZombieFeet)
		{
			TestTrue(TEXT("Zombie capsule clears furnished geometry"), HouseEncounterPlacement::ClearCapsule(Fixture.World,Feet,34,96,Fixture.Player));
			TestTrue(TEXT("Zombie has complete collision-checked navigation path to player"), HouseEncounterPlacement::Reachable(Fixture.World,Feet,First.PlayerFeet,34,96,Fixture.Player));
		}
		if (!TestTrue(TEXT("Entire encounter activates"), Fixture.Activate(Encounter))) { AddError(Encounter->LastError); return false; }
		EncounterTests::CheckStartingRoom(*this, Encounter->House, Encounter->Placement,
			Encounter->Zombies[0]->GetSimpleCollisionRadius());
		TestEqual(TEXT("Exact zombie count"), Encounter->Zombies.Num(),Zombies);
		TestEqual(TEXT("Exact item count"), Encounter->Items.Num(),Items);
		for (int32 Index = 0; Index < Encounter->Zombies.Num(); ++Index)
		{
			auto* Zombie = Encounter->Zombies[Index].Get();
			// Activation plans with actual selected loot sizes; the count-only helper above uses conservative boxes.
			const FVector ActualFeet = Encounter->Placement.ZombieFeet[Index];
			TestTrue(TEXT("Spawned capsule is clear of other occupants"), HouseEncounterPlacement::ClearCapsule(Fixture.World,ActualFeet,34,96,Zombie));
			auto* Path = Fixture.Nav->FindPathToLocationSynchronously(Fixture.World,ActualFeet,Encounter->Placement.PlayerFeet,Zombie);
			TestTrue(TEXT("Spawned NPC queries a complete path to player"), Path && Path->IsValid() && !Path->IsPartial());
		}
		TestEqual(TEXT("Spawning does not add owned player items"), Fixture.Inventory->GetEntries().Num(),OwnedBefore);
		TestFalse(TEXT("Start cannot duplicate encounter"), Encounter->Prepare(Fixture.Player));
		TestFalse(TEXT("Spawn does not offer exit immediately"), Encounter->CheckExit());
		TSet<FGuid> Unique;
		for (ADroppedItem* Item : Encounter->Items)
		{
			Unique.Add(Item->GetItem().InstanceId);
			FInventoryEntry Entry;
			TestFalse(TEXT("World instance not also in player inventory"), Fixture.Inventory->GetItem(Item->GetItem().InstanceId, Entry));
			FInventoryItemProfile Profile;
			TestTrue(TEXT("Existing pickup profile matches type"), Fixture.Inventory->GetProfile(Item->GetInventoryProfileId(), Profile)
				&& Profile.Definition == Item->GetItem().Definition);
		}
		TestEqual(TEXT("Unique world ownership"), Unique.Num(),Items);
		ADroppedItem* Item = Encounter->Items[0];
		const FGuid PickedId = Item->GetItem().InstanceId;
		Fixture.Player->SetActorLocation(Encounter->Placement.ItemAccessFeet[0]+FVector(0,0,98));
		TestTrue(TEXT("World loot uses current direct hand pickup"), Fixture.Use->PickUpToHands(Item));
		TestEqual(TEXT("Pickup preserves instance identity"), Fixture.Use->GetHeldId(), PickedId);
		TestFalse(TEXT("Picked actor cannot be picked twice"), Fixture.Use->PickUpToHands(Item));
		Fixture.Use->ClearHeld();
		FItemInstance Removed; Fixture.Inventory->RemoveItem(PickedId, Removed, true);
		Fixture.Player->SetActorLocation(First.PlayerFeet+FVector(0,0,96));
		Encounter->CheckExit();
		Fixture.Player->SetActorLocation(First.ExitFeet+FVector(0,0,96));
		TestTrue(TEXT("Leaving through the door offers choices"), Encounter->CheckExit());
		TestTrue(TEXT("Section choice records"), Encounter->ChooseNextSection(TEXT("Shelter")));
		TestFalse(TEXT("Alternative section remains unavailable"), Encounter->ChooseNextSection(TEXT("Street")));
		TestFalse(TEXT("Exit cannot reopen"), Encounter->CheckExit());
		Encounter->Destroy();
		TestNull(TEXT("Destroy clears the owned house even before full world startup"), Encounter->House.Get());
		for (TActorIterator<AHouseGenerator> It(Fixture.World); It; ++It)
			TestTrue(TEXT("No house geometry leaks between encounters"), It->IsActorBeingDestroyed());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterReproTest, "Prototype.Encounter.Reproducibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterReproTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	FHouseEncounterPlacement First;
	TSet<FGuid> Ids;
	for (int32 Run = 0; Run < 2; ++Run)
	{
		Fixture.Player->SetActorLocation(FVector(-400,-400,100));
		auto* Encounter = Fixture.Create(12345);
		Encounter->SetActorLocationAndRotation(FVector(400,100,0),FRotator(0,37,0));
		if (!TestTrue(TEXT("Translated/rotated preparation"), Encounter->Prepare(Fixture.Player))) { AddError(Encounter->LastError); return false; }
		// Blueprint/Details edits during preparation must not corrupt the in-flight seeded transaction.
		Encounter->ItemSelection.Empty(); Encounter->ZombieClass = nullptr;
		Encounter->HouseSettings.Seed = 999; Encounter->ItemCount = 0; Encounter->ZombieCount = 0;
		Fixture.Build();
		if (!TestTrue(TEXT("Prepared settings remain stable"),Fixture.Activate(Encounter))) { AddError(Encounter->LastError); return false; }
		const auto& Current = Encounter->Placement;
		EncounterTests::CheckStartingRoom(*this, Encounter->House, Current,
			Encounter->Zombies[0]->GetSimpleCollisionRadius());
		if (Run == 0) First = Current;
		else TestTrue(TEXT("Regenerated house repeats spawn transforms and selected loot types"),
			Current.PlayerFeet == First.PlayerFeet && Current.ExitFeet == First.ExitFeet && Current.ZombieFeet == First.ZombieFeet
			&& Current.ItemFeet == First.ItemFeet && Current.ItemTypes == First.ItemTypes);
		for (ADroppedItem* Item : Encounter->Items) Ids.Add(Item->GetItem().InstanceId);
		Encounter->Destroy();
	}
	TestEqual(TEXT("Repeatable loot uses fresh ownership identities across encounters"),Ids.Num(),12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterStartingRoomTest, "Prototype.Encounter.StartingRoomExclusion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterStartingRoomTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	const FVector Original = Fixture.Player->GetActorLocation();
	auto* Encounter = Fixture.Create(12345);
	Encounter->SetActorLocationAndRotation(FVector(400,100,0), FRotator(0,37,0));
	Encounter->HouseSettings.RoomCount = 1;
	Encounter->HouseSettings.bGenerateFurniture = false;
	Encounter->ZombieCount = 0;
	Encounter->ItemCount = 6;
	if (!TestTrue(TEXT("Large single-room house prepares"), Encounter->Prepare(Fixture.Player)))
	{ AddError(Encounter->LastError); return false; }
	Fixture.Build();
	FHouseEncounterPlacement Plan;
	FString Error;
	if (!TestTrue(TEXT("Loot still fits in the player room without zombies"), HouseEncounterPlacement::Plan(
		Encounter->House, Fixture.Player,34,96,0,6,12345,Plan,Error))) { AddError(Error); return false; }
	TestEqual(TEXT("All requested loot remains available"), Plan.ItemFeet.Num(),6);
	EncounterTests::CheckStartingRoom(*this, Encounter->House, Plan,34);
	for (const FVector& Feet : Plan.ItemFeet)
		TestTrue(TEXT("Starting-room loot may be beyond the old player exclusion radius"), FVector::Dist2D(Feet,Plan.PlayerFeet) >= 250);
	TestFalse(TEXT("Even one zombie cannot spawn elsewhere in the same large room"), HouseEncounterPlacement::Plan(
		Encounter->House, Fixture.Player,34,96,1,6,12345,Plan,Error));
	TestTrue(TEXT("Failed plan clears prior loot and zombie positions"), Plan.ItemFeet.IsEmpty() && Plan.ZombieFeet.IsEmpty());
	TestTrue(TEXT("Room exclusion failure reports zero valid zombies"), Error.Contains(TEXT("0/1 zombies")));
	const FHouseGenerationSettings SingleRoomSettings = Encounter->HouseSettings;
	Encounter->Destroy();
	Encounter = Fixture.Create(12345);
	Encounter->HouseSettings = SingleRoomSettings;
	Encounter->ZombieCount = 1;
	TestFalse(TEXT("Impossible one-room zombie request fails before building"), Encounter->Prepare(Fixture.Player));
	TestEqual(TEXT("Failure is terminal"), Encounter->State,EHouseEncounterState::Failed);
	TestTrue(TEXT("Room exclusion failure removes house and all partial occupants"), !Encounter->House && Encounter->Zombies.IsEmpty() && Encounter->Items.IsEmpty());
	TestTrue(TEXT("Room exclusion failure preserves player position"), Fixture.Player->GetActorLocation().Equals(Original));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterRollbackTest, "Prototype.Encounter.FailureRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterRollbackTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	const FVector Original = Fixture.Player->GetActorLocation();
	auto* Encounter = Fixture.Create(12345);
	if (!TestTrue(TEXT("Prepare"),Encounter->Prepare(Fixture.Player))) { AddError(Encounter->LastError); return false; }
	// Occupied world geometry covering the entire house and landing must invalidate all placements.
	auto* Blocker = Fixture.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(2000,2000,110));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation(FVector(600,500,100));
	Box->RegisterComponent();
	Fixture.Build();
	TestFalse(TEXT("Blocked encounter fails"), Fixture.Activate(Encounter));
	TestEqual(TEXT("Terminal failed state"), Encounter->State,EHouseEncounterState::Failed);
	TestTrue(TEXT("Failure reports reason"), !Encounter->LastError.IsEmpty());
	TestNull(TEXT("House rolled back"), Encounter->House.Get());
	TestTrue(TEXT("No partial zombies/items/placements"), Encounter->Zombies.IsEmpty() && Encounter->Items.IsEmpty() && Encounter->Placement.ZombieFeet.IsEmpty());
	TestTrue(TEXT("Original player location preserved"), Fixture.Player->GetActorLocation().Equals(Original));
	TestFalse(TEXT("Failure cannot offer choices"), Encounter->CheckExit());
	TestTrue(TEXT("Unrelated blocker survives rollback"), IsValid(Blocker) && !Blocker->IsActorBeingDestroyed());
	Blocker->Destroy();
	auto* Crowded = Fixture.Create(12345);
	Crowded->HouseSettings.WidthCells = Crowded->HouseSettings.DepthCells = 3;
	Crowded->HouseSettings.RoomCount = 1;
	Crowded->ZombieCount = 32; Crowded->ItemCount = 64;
	TestFalse(TEXT("Impossible one-room population rejects before building"), Crowded->Prepare(Fixture.Player));
	TestTrue(TEXT("Capacity failure removes house and all spawned actors"), !Crowded->House && Crowded->Zombies.IsEmpty() && Crowded->Items.IsEmpty());
	TestTrue(TEXT("Capacity failure preserves player position"), Fixture.Player->GetActorLocation().Equals(Original));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterLootTest, "Prototype.Encounter.AllCollectibleOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterLootTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	auto* Catalog = NewObject<UInventoryComponent>(Fixture.Player);
	if (!TestTrue(TEXT("Existing item catalog"),InventoryDemo::Populate(Catalog))) return false;
	TSet<FName> Types;
	TSet<FGuid> Ids;
	const int32 InitialCount = Fixture.Inventory->GetEntries().Num();
	for (const FInventoryEntry& Entry : Catalog->GetEntries())
	{
		const FName Type = Entry.Item.Definition->ItemId;
		if (Type.ToString().StartsWith(TEXT("Demo_")) || Types.Contains(Type)) continue;
		Types.Add(Type);
		FInventoryItemProfile Profile, Existing;
		Catalog->GetProfile(Entry.ProfileId, Profile);
		if (!Fixture.Inventory->GetProfile(Profile.Id, Existing)) Fixture.Inventory->RegisterProfile(Profile);
		FVector Extent;
		TestTrue(TEXT("World representation exists"),ADroppedItem::GetWorldHalfExtent(Profile.Definition,Extent));
		const FVector Feet = Fixture.Player->GetActorLocation()-FVector(0,0,96);
		auto* WorldItem = ADroppedItem::SpawnWorldItem(Fixture.World,Profile,FTransform(Feet+FVector(60,0,Extent.Z+2)));
		if (!TestNotNull(TEXT("Each existing collectible spawns"), WorldItem)) return false;
		const FGuid Id = WorldItem->GetItem().InstanceId;
		Ids.Add(Id);
		TestNotEqual(TEXT("World factory never copies the catalog identity"),Id,Entry.Item.InstanceId);
		TestTrue(TEXT("Invalid destination refuses pickup"),WorldItem->PickUp(Fixture.Inventory,Fixture.Player,TEXT("MissingPocket"),FVector2D(50),0) != EInventoryResult::Success);
		TestEqual(TEXT("Failed pickup retains world ownership"),WorldItem->GetItem().InstanceId,Id);
		TestTrue(TEXT("Current hand pickup accepts collectible"),Fixture.Use->PickUpToHands(WorldItem));
		TestEqual(TEXT("Hand ownership preserves identity"),Fixture.Use->GetHeldId(),Id);
		TestFalse(TEXT("Duplicate pickup rejected"),Fixture.Use->PickUpToHands(WorldItem));
		Fixture.Use->ClearHeld();
		FItemInstance Removed;
		Fixture.Inventory->RemoveItem(Id,Removed,true);
	}
	TestEqual(TEXT("All nine collectible types"),Types.Num(),9);
	TestEqual(TEXT("Unique world identities"),Ids.Num(),9);
	TestEqual(TEXT("Original loadout is preserved"),Fixture.Inventory->GetEntries().Num(),InitialCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterMapTest, "Prototype.Encounter.ExampleMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterMapTest::RunTest(const FString&)
{
	const FString MapPath = TEXT("/Game/Procedural/Lvl_HouseEncounter");
	if (FParse::Param(FCommandLine::Get(), TEXT("CreateHouseEncounterMap")))
	{
		if (FPackageName::DoesPackageExist(MapPath)) { AddError(TEXT("Refusing to overwrite existing encounter map.")); return false; }
		UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
		auto* Nav = EncounterTests::SetNavigation(World, FNavigationSystemRunMode::EditorMode);
		World->GetWorldSettings()->DefaultGameMode = AHouseEncounterGameMode::StaticClass();
		auto* Encounter = World->SpawnActor<AHouseEncounter>();
		Encounter->SetActorLabel(TEXT("House Encounter - Seed and Spawn Settings"));
		World->SpawnActor<AHouseEncounterNavBounds>(FVector(1400,1400,100), FRotator::ZeroRotator);
		World->SpawnActor<AHouseEncounterNavMesh>();
		Nav->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
		auto* Start = World->SpawnActor<APlayerStart>(FVector(-450,-450,100), FRotator(0,45,0));
		Start->SetActorLabel(TEXT("Safe staging position - encounter places player at entrance"));
		auto* Ground = World->SpawnActor<AStaticMeshActor>(FVector(-450,-450,-20), FRotator::ZeroRotator);
		Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Ground->SetActorScale3D(FVector(6,6,.4));
		Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0,0,1500), FRotator(-55,-35,0));
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(5);
		Nav->Build();
		for (TActorIterator<ANavigationData> It(World); It; ++It) It->EnsureBuildCompletion();
		if (!TestTrue(TEXT("Encounter map saves"), UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))) return false;
	}
	if (!FPackageName::DoesPackageExist(MapPath)) { AddError(TEXT("Missing encounter map. Create with -CreateHouseEncounterMap.")); return false; }
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!TestNotNull(TEXT("Encounter map reloads"), World)) return false;
	if (FParse::Param(FCommandLine::Get(), TEXT("UpgradeHouseEncounterZombies")))
	{
		// Explicit opt-in: change only the legacy spawn class, preserving seed/counts and all other authored settings.
		for (TActorIterator<AHouseEncounter> It(World); It; ++It)
			if (It->ZombieClass == AHouseZombiePlaceholder::StaticClass())
			{ It->Modify(); It->ZombieClass = AZombieCharacter::StaticClass(); }
		if (!TestTrue(TEXT("Upgraded map saves"), UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))) return false;
		World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	}
	TestEqual(TEXT("Dedicated game mode"), World->GetWorldSettings()->DefaultGameMode.Get(), AHouseEncounterGameMode::StaticClass());
	int32 Encounters = 0, DynamicMeshes = 0, Bounds = 0;
	for (TActorIterator<AHouseEncounter> It(World); It; ++It)
	{
		++Encounters;
		TestTrue(TEXT("Runtime start enabled"),It->bStartOnBeginPlay);
		TestTrue(TEXT("Saved encounter uses real zombie class"),It->ZombieClass && It->ZombieClass->IsChildOf(AZombieCharacter::StaticClass()));
	}
	for (TActorIterator<AHouseEncounterNavMesh> It(World); It; ++It)
	{ ++DynamicMeshes; TestEqual(TEXT("Dynamic navigation survives save/reload"),It->GetRuntimeGenerationMode(),ERuntimeGenerationType::Dynamic); }
	for (TActorIterator<AHouseEncounterNavBounds> It(World); It; ++It) ++Bounds;
	TestEqual(TEXT("One encounter"),Encounters,1);
	TestEqual(TEXT("One dynamic navmesh"),DynamicMeshes,1);
	TestEqual(TEXT("One navigation bounds actor"),Bounds,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterCompactLootTest, "Prototype.Encounter.CompactLootAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterCompactLootTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	auto* Encounter = Fixture.Create(217);
	Encounter->HouseSettings.Navigation.Pattern = EHouseLayoutPattern::DeepBranches;
	Encounter->HouseSettings.WidthCells = Encounter->HouseSettings.DepthCells = 24;
	Encounter->HouseSettings.RoomCount = 8;
	Encounter->ZombieCount = 3;
	Encounter->ItemCount = 6;
	if (!TestTrue(TEXT("Compact furnished house prepares"), Encounter->Prepare(Fixture.Player)))
	{ AddError(Encounter->LastError); return false; }
	Fixture.Build();
	if (!TestTrue(TEXT("Compact house accommodates all requested zombies and loot"), Fixture.Activate(Encounter)))
	{ AddError(Encounter->LastError); return false; }
	TestEqual(TEXT("No zombie count reduction"), Encounter->Zombies.Num(), 3);
	TestEqual(TEXT("No item count reduction"), Encounter->Items.Num(), 6);
	if (!TestEqual(TEXT("Every item has a recorded pickup approach"), Encounter->Placement.ItemAccessFeet.Num(), 6)) return false;
	EncounterTests::CheckStartingRoom(*this, Encounter->House, Encounter->Placement,
		Encounter->Zombies[0]->GetSimpleCollisionRadius());
	const int32 OwnedBefore = Fixture.Inventory->GetEntries().Num();
	for (int32 I = 0; I < Encounter->Items.Num(); ++I)
	{
		auto* Item = Encounter->Items[I].Get();
		const FVector Access = Encounter->Placement.ItemAccessFeet[I];
		TestTrue(TEXT("Pickup approach is not occupied by furniture or another spawn"),
			HouseEncounterPlacement::ClearCapsule(Fixture.World, Access, 34, 96, Fixture.Player));
		Fixture.Player->SetActorLocation(Access + FVector(0,0,98));
		TestTrue(TEXT("Actual item accepts interaction from planned approach"), Item->CanInteract(Fixture.Player));
		const FGuid Id = Item->GetItem().InstanceId;
		TestTrue(TEXT("Compact loot uses ordinary hand pickup"), Fixture.Use->PickUpToHands(Item));
		TestEqual(TEXT("Compact pickup retains ownership identity"), Fixture.Use->GetHeldId(), Id);
		Fixture.Use->ClearHeld();
		FItemInstance Removed;
		Fixture.Inventory->RemoveItem(Id, Removed, true);
	}
	TestEqual(TEXT("Checking pickups leaves original loadout intact"), Fixture.Inventory->GetEntries().Num(), OwnedBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterItemBodiesTest, "Prototype.Encounter.ItemBodiesAndAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterItemBodiesTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	auto* Encounter = Fixture.Create(12345);
	Encounter->SetActorLocationAndRotation(FVector(400,100,0), FRotator(0,37,0));
	if (!TestTrue(TEXT("Rotated furnished house prepares"), Encounter->Prepare(Fixture.Player)))
	{ AddError(Encounter->LastError); return false; }
	Fixture.Build();
	auto* Catalog = NewObject<UInventoryComponent>(Fixture.Player);
	if (!TestTrue(TEXT("Existing collectible catalog loads"), InventoryDemo::Populate(Catalog))) return false;
	TArray<FInventoryItemProfile> Profiles;
	TArray<FVector> Extents;
	TSet<FName> Types;
	for (const FInventoryEntry& Entry : Catalog->GetEntries())
	{
		const FName Type = Entry.Item.Definition->ItemId;
		if (Type.ToString().StartsWith(TEXT("Demo_")) || Types.Contains(Type)) continue;
		Types.Add(Type);
		FInventoryItemProfile Profile;
		FVector Extent;
		if (!TestTrue(TEXT("Collectible has current physical dimensions"), Catalog->GetProfile(Entry.ProfileId, Profile)
			&& ADroppedItem::GetWorldHalfExtent(Profile.Definition, Extent))) return false;
		Profiles.Add(Profile);
		Extents.Add(Extent);
	}
	TestEqual(TEXT("Plan exercises all nine existing collectible bodies"), Profiles.Num(), 9);
	FHouseEncounterPlacement First, Second;
	FString Error;
	if (!TestTrue(TEXT("Exact item bodies fit with reachable pickup approaches"), HouseEncounterPlacement::Plan(
		Encounter->House, Fixture.Player, 34, 96, 3, Extents, 12345, First, Error)))
	{ AddError(Error); return false; }
	if (!TestTrue(TEXT("Same typed request plans again"), HouseEncounterPlacement::Plan(
		Encounter->House, Fixture.Player, 34, 96, 3, Extents, 12345, Second, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Positions and pickup approaches are reproducible"), First.ItemFeet == Second.ItemFeet
		&& First.ItemAccessFeet == Second.ItemAccessFeet && First.ZombieFeet == Second.ZombieFeet);
	if (!TestEqual(TEXT("One pickup approach per body"), First.ItemAccessFeet.Num(), Extents.Num())) return false;
	EncounterTests::CheckStartingRoom(*this, Encounter->House, First, 34);
	for (const FVector& Access : First.ItemAccessFeet)
	{
		TestTrue(TEXT("Player capsule fits at pickup approach"),
			HouseEncounterPlacement::ClearCapsule(Fixture.World, Access, 34, 96, Fixture.Player));
		TestTrue(TEXT("Pickup approach has a complete collision-checked entrance route"),
			HouseEncounterPlacement::Reachable(Fixture.World, Access, First.PlayerFeet, 34, 96, Fixture.Player));
	}
	TArray<ADroppedItem*> Items;
	TSet<FGuid> Ids;
	const int32 OwnedBefore = Fixture.Inventory->GetEntries().Num();
	for (int32 I = 0; I < Profiles.Num(); ++I)
	{
		const FInventoryItemProfile& Profile = Profiles[I];
		FInventoryItemProfile Existing;
		if (!Fixture.Inventory->GetProfile(Profile.Id, Existing))
			TestEqual(TEXT("Current pickup profile registers"), Fixture.Inventory->RegisterProfile(Profile), EInventoryResult::Success);
		const FTransform Transform(Encounter->GetActorRotation(), First.ItemFeet[I] + FVector(0,0,Extents[I].Z+2));
		FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterTypedLootTest), false, Fixture.Player);
		TestFalse(TEXT("Exact rotated item body clears world geometry and previously spawned loot"),
			Fixture.World->OverlapAnyTestByObjectType(Transform.GetLocation(), Transform.GetRotation(),
				FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeBox(Extents[I]+FVector(1)), Query));
		auto* Item = ADroppedItem::SpawnWorldItem(Fixture.World, Profile, Transform);
		if (!TestNotNull(TEXT("Planned body spawns through current world-item factory"), Item)) return false;
		TestTrue(TEXT("World item uses its planned physical dimensions"), Item->GetBody()->GetUnscaledBoxExtent().Equals(Extents[I]));
		Ids.Add(Item->GetItem().InstanceId);
		Items.Add(Item);
	}
	TestEqual(TEXT("New loot has unique identities"), Ids.Num(), Profiles.Num());
	TestEqual(TEXT("World spawning leaves player ownership unchanged"), Fixture.Inventory->GetEntries().Num(), OwnedBefore);
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		auto* Item = Items[I];
		const FVector Access = First.ItemAccessFeet[I];
		TestTrue(TEXT("Pickup approach stays clear after all loot exists"),
			HouseEncounterPlacement::ClearCapsule(Fixture.World, Access, 34, 96, Fixture.Player));
		Fixture.Player->SetActorLocation(Access + FVector(0,0,98));
		TestTrue(TEXT("Actual interaction radius accepts planned approach"), Item->CanInteract(Fixture.Player));
		const FGuid Id = Item->GetItem().InstanceId;
		TestTrue(TEXT("Failed pickup leaves world ownership"),
			Item->PickUp(Fixture.Inventory, Fixture.Player, TEXT("MissingPocket"), FVector2D(50), 0) != EInventoryResult::Success);
		TestEqual(TEXT("Failed pickup preserves item identity"), Item->GetItem().InstanceId, Id);
		TestTrue(TEXT("Every planned type can be picked up from its approach"), Fixture.Use->PickUpToHands(Item));
		TestEqual(TEXT("Successful pickup preserves identity"), Fixture.Use->GetHeldId(), Id);
		TestFalse(TEXT("Picked actor cannot transfer twice"), Fixture.Use->PickUpToHands(Item));
		Fixture.Use->ClearHeld();
		FItemInstance Removed;
		Fixture.Inventory->RemoveItem(Id, Removed, true);
	}
	TestEqual(TEXT("Original loadout survives the complete pickup test"), Fixture.Inventory->GetEntries().Num(), OwnedBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterInvalidItemBodiesTest, "Prototype.Encounter.InvalidItemBodies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterInvalidItemBodiesTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	auto* Encounter = Fixture.Create(12345);
	if (!TestTrue(TEXT("House prepares"), Encounter->Prepare(Fixture.Player)))
	{ AddError(Encounter->LastError); return false; }
	Fixture.Build();
	FHouseEncounterPlacement Placement;
	Placement.ItemFeet.Add(FVector(10,20,30));
	Placement.ItemAccessFeet.Add(FVector(40,50,60));
	Placement.ZombieFeet.Add(FVector(70,80,90));
	FString Error;
	const TArray<FVector> Invalid = {FVector(5,5,-1)};
	TestFalse(TEXT("Invalid item body dimensions fail safely"), HouseEncounterPlacement::Plan(
		Encounter->House, Fixture.Player, 34, 96, 1, Invalid, 12345, Placement, Error));
	TestTrue(TEXT("Invalid body failure explains the rejected request"), !Error.IsEmpty());
	TestTrue(TEXT("Failed plan clears stale bodies and approaches"), Placement.ItemFeet.IsEmpty()
		&& Placement.ItemAccessFeet.IsEmpty() && Placement.ZombieFeet.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterRecoveryTest, "Prototype.Encounter.LayoutRecovery",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterRecoveryTest::RunTest(const FString&)
{
 EncounterTests::FWorld Fixture;
 FHouseEncounterPlacement RepeatPlan;
 int32 RepeatSeed=0;
 for(int32 Run=0;Run<2;++Run)
 {
  Fixture.Player->SetActorLocation(FVector(-400,-400,100));
  const FVector Original=Fixture.Player->GetActorLocation();
  auto* Encounter=Fixture.Create(12345);
  Encounter->HouseSettings.bUseGenerationRanges=true;
  Encounter->HouseSettings.GenerationRanges.WidthCells=FHouseIntegerRange(12,12);
  Encounter->HouseSettings.GenerationRanges.DepthCells=FHouseIntegerRange(10,10);
  Encounter->HouseSettings.GenerationRanges.RoomCount=FHouseIntegerRange(4,4);
  if(!TestTrue(TEXT("Recovery request prepares"),Encounter->Prepare(Fixture.Player))) return false;
  auto* Blocker=Fixture.World->SpawnActor<AActor>();
  auto* Box=NewObject<UBoxComponent>(Blocker); Blocker->SetRootComponent(Box);
  Box->SetBoxExtent(FVector(2000,2000,110)); Box->SetCollisionProfileName(TEXT("BlockAll"));
  Box->SetWorldLocation(FVector(600,500,100)); Box->RegisterComponent();
  TSet<int32> LayoutSeeds;
  int32 PreviousAttempt=0;
  for(int32 Step=0;Step<9 && Encounter->State==EHouseEncounterState::Preparing;++Step)
  {
   if(Encounter->House)
   {
    const auto& S=Encounter->House->GeneratedSettings;
    TestEqual(TEXT("Recovery preserves exact width"),S.WidthCells,12);
    TestEqual(TEXT("Recovery preserves exact depth"),S.DepthCells,10);
    TestEqual(TEXT("Recovery preserves room count"),S.RoomCount,4);
    LayoutSeeds.Add(S.Seed);
    TestTrue(TEXT("Attempt number advances"),Encounter->LayoutAttempts>PreviousAttempt);
    PreviousAttempt=Encounter->LayoutAttempts;
   }
   Fixture.Build(); Encounter->Activate();
   TestTrue(TEXT("Unsuccessful attempts never move the player"),Fixture.Player->GetActorLocation().Equals(Original));
   TestTrue(TEXT("Unsuccessful attempts leave no partial occupants"),Encounter->Zombies.IsEmpty() && Encounter->Items.IsEmpty());
  }
  TestEqual(TEXT("Search exhausts exactly four layouts"),Encounter->LayoutAttempts,4);
  TestEqual(TEXT("Each layout uses a different seed block"),LayoutSeeds.Num(),4);
  TestEqual(TEXT("Exhaustion is an explicit failure"),Encounter->State,EHouseEncounterState::Failed);
  TestTrue(TEXT("Failure removes owned house"),!Encounter->House);
  TestTrue(TEXT("Failure permits explicit retry"),Encounter->CanRetryGeneration());
  TestTrue(TEXT("Unrelated geometry is preserved"),IsValid(Blocker));
  Blocker->Destroy();
  // Edits while preparing must not change the in-flight or explicit retry request.
  Encounter->ZombieCount=0; Encounter->ItemCount=0; Encounter->HouseSettings.WidthCells=3;
  if(!TestTrue(TEXT("Manual retry starts a fresh request"),Encounter->RetryGeneration())) return false;
  TestFalse(TEXT("Cannot queue a second retry while preparing"),Encounter->RetryGeneration());
  TestNotEqual(TEXT("Manual retry changes the recorded request seed"),Encounter->RequestSeed,12345);
  if(!TestTrue(TEXT("Retry succeeds once obstruction is removed"),Fixture.Activate(Encounter))) { AddError(Encounter->LastError); return false; }
  TestEqual(TEXT("Retry keeps requested zombie count"),Encounter->Zombies.Num(),3);
  TestEqual(TEXT("Retry keeps requested item count"),Encounter->Items.Num(),6);
  TestFalse(TEXT("An active encounter cannot retry"),Encounter->RetryGeneration());
  if(Run==0) { RepeatPlan=Encounter->Placement; RepeatSeed=Encounter->RequestSeed; }
  else
  {
   TestEqual(TEXT("Manual retry seed sequence is deterministic"),Encounter->RequestSeed,RepeatSeed);
   TestTrue(TEXT("Recovery reproduces bodies, access and loot selection"),RepeatPlan.ZombieFeet==Encounter->Placement.ZombieFeet
    && RepeatPlan.ItemFeet==Encounter->Placement.ItemFeet && RepeatPlan.ItemAccessFeet==Encounter->Placement.ItemAccessFeet
    && RepeatPlan.ItemTypes==Encounter->Placement.ItemTypes);
  }
  Encounter->Destroy();
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterNaturalRecoveryTest, "Prototype.Encounter.AlternativeLayoutPlacement",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterNaturalRecoveryTest::RunTest(const FString&)
{
 EncounterTests::FWorld Fixture;
 auto* Encounter=Fixture.Create(2235);
 Encounter->HouseSettings.bUseGenerationRanges=true;
 if(!TestTrue(TEXT("Known awkward layout prepares"),Encounter->Prepare(Fixture.Player))) return false;
 if(!TestTrue(TEXT("Alternative arrangement fits exact population"),Fixture.Activate(Encounter))) { AddError(Encounter->LastError); return false; }
 AddInfo(FString::Printf(TEXT("Seed 2235 accepted after %d layouts."),Encounter->LayoutAttempts));
 TestTrue(TEXT("A different room arrangement was needed"),Encounter->LayoutAttempts>1);
 TestEqual(TEXT("Population unchanged"),Encounter->Items.Num(),6);
 TestEqual(TEXT("Footprint unchanged"),Encounter->House->GeneratedSettings.WidthCells,12);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEncounterRoomRulesTest, "Prototype.Encounter.RoomRulePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEncounterRoomRulesTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	FHouseEncounterPlacement First;
	for (int32 Run=0; Run<2; ++Run)
	{
		Fixture.Player->SetActorLocation(FVector(-400,-400,100));
		auto* E = Fixture.Create(12345);
		E->SetActorLocationAndRotation(FVector(400,100,0), FRotator(0,37,0));
		E->HouseSettings.WidthCells=20; E->HouseSettings.DepthCells=20;
		E->HouseSettings.RoomCount=4;
		E->ZombieCount=2; E->ItemCount=2;
		FHouseRoomRule Rule;
		Rule.RuleName=TEXT("Quiet kitchen");
		Rule.MatchTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Room.Type.Kitchen")));
		Rule.bAllowZombieSpawns=false; Rule.bRequireMatchingRoom=true;
		E->HouseSettings.RoomRules.Add(Rule);
		Rule.RuleName=TEXT("Clean entrance"); Rule.MatchTags.Reset();
		Rule.MatchTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Room.Role.Entrance")));
		Rule.bAllowZombieSpawns=true; Rule.bAllowItemSpawns=false;
		E->HouseSettings.RoomRules.Add(Rule);
		if (!TestTrue(TEXT("Ruled house prepares"), E->Prepare(Fixture.Player))) { AddError(E->LastError); return false; }
		// Changes during navigation must not mutate the prepared rule snapshot.
		E->HouseSettings.RoomRules.Reset();
		if (!TestTrue(TEXT("Ruled encounter activates"), Fixture.Activate(E))) { AddError(E->LastError); return false; }
		TestEqual(TEXT("Exact requested zombies"), E->Zombies.Num(),2);
		TestEqual(TEXT("Exact requested loot"), E->Items.Num(),2);
		const auto* H=E->House.Get();
		EncounterTests::CheckStartingRoom(*this,H,E->Placement,E->Zombies[0]->GetSimpleCollisionRadius());
		for (const FVector& Feet : E->Placement.ZombieFeet)
		{
			const FVector2D Local(H->GetActorTransform().InverseTransformPosition(Feet));
			for (const auto& Room : H->Layout.Rooms)
				if (!Room.bAllowZombieSpawns)
					TestFalse(TEXT("Zombie body avoids every excluded room including doorway margins"),
						FHouseSpatial::RoomBounds(H->GeneratedSettings,Room).ExpandBy(E->Zombies[0]->GetSimpleCollisionRadius()).IsInsideOrOn(Local));
		}
		for (int32 I=0; I<E->Placement.ItemFeet.Num(); ++I)
		{
			const int32 Room=FHouseSpatial::RoomAt(H->GeneratedSettings,H->Layout,FVector2D(H->GetActorTransform().InverseTransformPosition(E->Placement.ItemFeet[I])));
			if (TestTrue(TEXT("Loot stays inside a room"), H->Layout.Rooms.IsValidIndex(Room)))
				TestTrue(TEXT("Loot respects room rule"), H->Layout.Rooms[Room].bAllowItemSpawns);
		}
		if (Run==0) First=E->Placement;
		else TestTrue(TEXT("Rules preserve reproducible placements"), First.ZombieFeet==E->Placement.ZombieFeet && First.ItemFeet==E->Placement.ItemFeet && First.ItemAccessFeet==E->Placement.ItemAccessFeet);
		for (int32 I=0; I<E->Zombies.Num(); ++I)
		{
			auto* Path=Fixture.Nav->FindPathToLocationSynchronously(Fixture.World,E->Placement.ZombieFeet[I],E->Placement.PlayerFeet,E->Zombies[I]);
			TestTrue(TEXT("Allowed zombies have full navigation paths"),Path && Path->IsValid() && !Path->IsPartial());
		}
		E->Destroy();
	}
	// A valid layout with no allowed population locations must not expose a partial encounter.
	Fixture.Player->SetActorLocation(FVector(-400,-400,100));
	const FVector Before=Fixture.Player->GetActorLocation();
	auto* E=Fixture.Create(12345);
	E->ZombieCount=1; E->ItemCount=1;
	FHouseRoomRule Deny;
	Deny.RuleName=TEXT("No initial occupants"); Deny.MatchTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Room")));
	Deny.bAllowZombieSpawns=false; Deny.bAllowItemSpawns=false;
	E->HouseSettings.RoomRules.Add(Deny);
	if (!TestTrue(TEXT("Restricted layout itself is valid"),E->Prepare(Fixture.Player))) { AddError(E->LastError); return false; }
	TestFalse(TEXT("Unsatisfiable population fails after bounded recovery"),Fixture.Activate(E));
	TestEqual(TEXT("Failed state"),E->State,EHouseEncounterState::Failed);
	TestTrue(TEXT("No partial geometry or occupants"),!E->House && E->Zombies.IsEmpty() && E->Items.IsEmpty());
	TestTrue(TEXT("Failure leaves player in original position"),Fixture.Player->GetActorLocation().Equals(Before));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeEncounterTest,"Prototype.Encounter.OfficeSuite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOfficeEncounterTest::RunTest(const FString&)
{
	EncounterTests::FWorld Fixture;
	for (int32 Seed : {12345,7})
	{
		Fixture.Player->SetActorLocation(FVector(-400,-400,100));
		auto* E=Fixture.Create(Seed);
		E->HouseSettings=FHouseOfficePlan::Preset(Seed);
		E->SetActorLocationAndRotation(FVector(200,100,0),FRotator(0,25,0));
		if (!TestTrue(TEXT("Office encounter prepares"),E->Prepare(Fixture.Player))) { AddError(E->LastError); return false; }
		if (!TestTrue(TEXT("Office encounter activates with default population"),Fixture.Activate(E))) { AddError(E->LastError); return false; }
		TestEqual(TEXT("Office zombies"),E->Zombies.Num(),3); TestEqual(TEXT("Office loot"),E->Items.Num(),6);
		EncounterTests::CheckStartingRoom(*this,E->House,E->Placement,E->Zombies[0]->GetSimpleCollisionRadius());
		for (int32 I=0; I<E->Zombies.Num(); ++I)
		{
			auto* Path=Fixture.Nav->FindPathToLocationSynchronously(Fixture.World,E->Placement.ZombieFeet[I],E->Placement.PlayerFeet,E->Zombies[I]);
			TestTrue(TEXT("Office zombie has full route to reception"),Path && Path->IsValid() && !Path->IsPartial());
		}
		Fixture.Player->SetActorLocation(E->Placement.ItemAccessFeet[0]+FVector(0,0,98));
		auto* Item=E->Items[0].Get(); const FGuid Id=Item->GetItem().InstanceId;
		TestTrue(TEXT("Office loot is reachable through actual pickup system"),Fixture.Use->PickUpToHands(Item));
		Fixture.Use->ClearHeld(); FItemInstance Removed; Fixture.Inventory->RemoveItem(Id,Removed,true);
		Fixture.Player->SetActorLocation(E->Placement.PlayerFeet+FVector(0,0,96)); E->CheckExit();
		Fixture.Player->SetActorLocation(E->Placement.ExitFeet+FVector(0,0,96));
		TestTrue(TEXT("Leaving office offers next section"),E->CheckExit());
		E->Destroy();
	}
	return true;
}
#endif
