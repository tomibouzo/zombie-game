#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"
#include "Gameplay/Player/Inventory/InventoryTestPile.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryTestPileTest, "Prototype.Inventory.TestPile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryTestPileTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Player = World->SpawnActor<ACharacter>(FVector(1000,1000,200), FRotator::ZeroRotator);
	auto* Inventory = NewObject<UInventoryComponent>(Player); Inventory->RegisterComponent();
	auto* Vitals = NewObject<UPlayerVitalsComponent>(Player); Vitals->RegisterComponent();
	Vitals->InitializeVitals(100,100,50,.25f);
	auto* Use = NewObject<UPlayerItemUseComponent>(Player); Use->bSpawnTestSpikes = false; Use->RegisterComponent();
	Player->DispatchBeginPlay();
	auto* Ground = World->SpawnActor<AActor>();
	auto* Floor = NewObject<UBoxComponent>(Ground); Ground->SetRootComponent(Floor);
	Floor->SetBoxExtent(FVector(500,500,10));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionObjectType(ECC_WorldStatic);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	Floor->SetWorldLocation(Player->GetActorLocation()-FVector(0,0,Player->GetSimpleCollisionHalfHeight()+10));
	Floor->RegisterComponent();
	const auto Original = Inventory->GetEntries();
	auto* Pile = World->SpawnActor<AInventoryTestPile>();
	if (!TestTrue(TEXT("Mixed pile spawns on nearby static floor"), Pile->SpawnForPlayer(Player))) return false;
	TestEqual(TEXT("Enough instances for several scroll pages"), Pile->GetSpawnedItems().Num(), 36);
	TestEqual(TEXT("All instances start in pickup range"), ADroppedItem::FindNearby(Player).Num(), 36);
	TSet<FGuid> Ids;
	TSet<FName> Types;
	for (const auto& Weak : Pile->GetSpawnedItems())
	{
		if (!TestTrue(TEXT("Spawned world item is valid"), Weak.IsValid())) return false;
		Ids.Add(Weak->GetItem().InstanceId); Types.Add(Weak->GetItem().Definition->ItemId);
		FInventoryItemProfile Profile;
		TestTrue(TEXT("Every world item has a compatible pickup profile"),
			Inventory->GetProfile(Weak->GetInventoryProfileId(), Profile) && Profile.Definition == Weak->GetItem().Definition);
	}
	TestEqual(TEXT("Every floor item has unique ownership"), Ids.Num(), 36);
	TestEqual(TEXT("All nine existing item types represented"), Types.Num(), 9);
	TestEqual(TEXT("Player loadout count is unchanged"), Inventory->GetEntries().Num(), Original.Num());
	for (const auto& Entry : Original)
	{
		FInventoryEntry Current;
		TestTrue(TEXT("Player loadout identity and placement preserved"), Inventory->GetItem(Entry.Item.InstanceId, Current)
			&& Current.PocketId == Entry.PocketId && Current.Position == Entry.Position && Current.AngleDegrees == Entry.AngleDegrees);
	}
	TestFalse(TEXT("Same pile cannot spawn twice"), Pile->SpawnForPlayer(Player));
	TestEqual(TEXT("Repeated request creates no extra world items"), ADroppedItem::FindNearby(Player).Num(), 36);
	// Exercise the last row of the real mixed pile through the existing floor UI.
	for (const auto& Entry : Original) if (Entry.PocketId == TEXT("Quick2"))
	{
		FItemInstance Removed; Inventory->RemoveItem(Entry.Item.InstanceId, Removed);
	}
	Use->BeginOpenBackpack(); Use->Advance(2);
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	const auto Panel = SNew(SInventoryPanel).Inventory(Inventory).ItemUse(Use).Player(Player).Controls(Settings.Get()).SaveControls(false);
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940), FSlateLayoutTransform(1));
	auto Mouse = [](FVector2D P, float Wheel = 0.f) { return FPointerEvent(0,P,P,TSet<FKey>(),EKeys::LeftMouseButton,Wheel,FModifierKeysState()); };
	const FGuid LastId = ADroppedItem::FindNearby(Player).Last()->GetItem().InstanceId;
	Panel->OnKeyDown(G, FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0));
	Panel->OnMouseWheel(G, Mouse(FVector2D(780,708),-100));
	Panel->OnMouseButtonDown(G, Mouse(FVector2D(780,708)));
	Panel->OnMouseButtonUp(G, Mouse(FVector2D(580,230)));
	FInventoryEntry PickedUp;
	TestTrue(TEXT("Last scrolled mixed item transfers into storage"), Inventory->GetItem(LastId, PickedUp));
	TestEqual(TEXT("Pickup uses the selected quick pocket"), PickedUp.PocketId, FName(TEXT("Quick2")));
	TestEqual(TEXT("Pickup consumes exactly one floor instance"), ADroppedItem::FindNearby(Player).Num(), 35);
	return true;
}
#endif
