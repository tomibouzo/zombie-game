#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterGameMode.h"
#include "Gameplay/Procedural/HouseGenerator.h"
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
	void Build()
	{
		Nav->ProcessPendingOctreeUpdates();
		Nav->Build();
		for (TActorIterator<ANavigationData> It(World); It; ++It) It->EnsureBuildCompletion();
	}
};
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
		if (!TestTrue(TEXT("Entire encounter activates"), Encounter->Activate())) { AddError(Encounter->LastError); return false; }
		TestEqual(TEXT("Exact zombie count"), Encounter->Zombies.Num(),Zombies);
		TestEqual(TEXT("Exact item count"), Encounter->Items.Num(),Items);
		for (int32 Index = 0; Index < Encounter->Zombies.Num(); ++Index)
		{
			auto* Zombie = Encounter->Zombies[Index].Get();
			TestTrue(TEXT("Spawned capsule is clear of other occupants"), HouseEncounterPlacement::ClearCapsule(Fixture.World,First.ZombieFeet[Index],34,96,Zombie));
			auto* Path = Fixture.Nav->FindPathToLocationSynchronously(Fixture.World,First.ZombieFeet[Index],First.PlayerFeet,Zombie);
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
		Fixture.Player->SetActorLocation(Item->GetActorLocation()+FVector(60,0,96));
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
		if (!TestTrue(TEXT("Prepared settings remain stable"),Encounter->Activate())) { AddError(Encounter->LastError); return false; }
		const auto& Current = Encounter->Placement;
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
	TestFalse(TEXT("Blocked encounter fails"), Encounter->Activate());
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
	if (!TestTrue(TEXT("Valid small house prepares"), Crowded->Prepare(Fixture.Player))) { AddError(Crowded->LastError); return false; }
	Fixture.Build();
	TestFalse(TEXT("Insufficient capacity rejects the complete encounter"), Crowded->Activate());
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
	TestEqual(TEXT("Dedicated game mode"), World->GetWorldSettings()->DefaultGameMode.Get(), AHouseEncounterGameMode::StaticClass());
	int32 Encounters = 0, DynamicMeshes = 0, Bounds = 0;
	for (TActorIterator<AHouseEncounter> It(World); It; ++It) { ++Encounters; TestTrue(TEXT("Runtime start enabled"),It->bStartOnBeginPlay); }
	for (TActorIterator<AHouseEncounterNavMesh> It(World); It; ++It)
	{ ++DynamicMeshes; TestEqual(TEXT("Dynamic navigation survives save/reload"),It->GetRuntimeGenerationMode(),ERuntimeGenerationType::Dynamic); }
	for (TActorIterator<AHouseEncounterNavBounds> It(World); It; ++It) ++Bounds;
	TestEqual(TEXT("One encounter"),Encounters,1);
	TestEqual(TEXT("One dynamic navmesh"),DynamicMeshes,1);
	TestEqual(TEXT("One navigation bounds actor"),Bounds,1);
	return true;
}
#endif
