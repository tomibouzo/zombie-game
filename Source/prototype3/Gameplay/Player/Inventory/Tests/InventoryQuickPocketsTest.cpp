#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
// Isolated component lifecycle, with no map, controller, viewport or player preferences.
struct FQuickPocketsFixture
{
	UWorld* World = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UPlayerItemUseComponent* Use = nullptr;
	FQuickPocketsFixture()
	{
		const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
		auto* Owner = World->SpawnActor<AActor>();
		Inventory = NewObject<UInventoryComponent>(Owner);
		Inventory->RegisterComponent();
		auto* Vitals = NewObject<UPlayerVitalsComponent>(Owner);
		Vitals->RegisterComponent();
		Vitals->InitializeVitals(100, 100, 50, .25f);
		Use = NewObject<UPlayerItemUseComponent>(Owner);
		Use->bSpawnTestSpikes = false;
		Use->bSavePreferences = false;
		Use->RegisterComponent();
		Owner->DispatchBeginPlay();
	}
	~FQuickPocketsFixture() { World->DestroyWorld(false); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThreeQuickPocketsTest, "Prototype.Inventory.ThreeQuickPockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThreeQuickPocketsTest::RunTest(const FString&)
{
	FQuickPocketsFixture Fixture;
	auto* Use = Fixture.Use;
	auto* Inventory = Fixture.Inventory;
	if (!TestNotNull(TEXT("Player lifecycle initialized shortcuts"), Use->Shortcuts.Get())) return false;
	TestTrue(TEXT("Backpack initialized"), Use->Backpack.IsValid());
	int32 Count = 0;
	for (const auto& Pocket : Inventory->GetPockets())
	{
		if (!UPlayerItemUseComponent::IsQuickPocket(Pocket.Id)) continue;
		++Count;
		TestEqual(TEXT("Each pocket has the configured size"), Pocket.Size, Use->QuickSize);
		TestEqual(TEXT("Each pocket has the per-item mass limit"), Pocket.MaxItemMassKg, Use->QuickMaxItemMassKg);
		TestFalse(TEXT("Firearms excluded from every pocket"), Pocket.bAllowFirearms);
		TestTrue(TEXT("Quick pockets accessible with backpack closed"), Use->CanAccess(Pocket.Id));
	}
	TestEqual(TEXT("Exactly three quick pockets"), Count, 3);
	TestFalse(TEXT("Setup is not accessible storage"), Use->CanAccess(TEXT("Setup")));
	FGuid Bandage;
	for (const auto& Entry : Inventory->GetEntries())
	{
		TestTrue(TEXT("Sample population fits real storage"), Entry.PocketId != TEXT("Setup"));
		if (Entry.PocketId == TEXT("Quick") && Entry.Item.Definition->ItemId == TEXT("Bandage")) Bandage = Entry.Item.InstanceId;
	}
	if (!TestTrue(TEXT("Quick bandage exists"), Bandage.IsValid())) return false;
	const int32 InitialCount = Inventory->GetEntries().Num();
	Use->Shortcuts->ItemTypes[0] = TEXT("Bandage");
	for (int32 Index = 1; Index < 3; ++Index)
	{
		TestTrue(TEXT("Transfer to another quick pocket"), Use->Transfer(Bandage, UPlayerItemUseComponent::QuickPocketId(Index)));
		Use->HandleShortcut(0, 10.0 * Index);
		TestEqual(TEXT("Shortcut finds the same instance in each hidden pocket"), Use->GetHeldId(), Bandage);
		TestFalse(TEXT("Held item cannot be transferred"), Use->Transfer(Bandage, TEXT("Quick")));
		Use->Stow();
	}
	TestFalse(TEXT("Closed backpack rejects transfer"), Use->Transfer(Bandage, TEXT("Backpack")));
	TestTrue(TEXT("Opening starts"), Use->BeginOpenBackpack());
	Use->Advance(1.9f);
	TestFalse(TEXT("Delay still gates backpack access"), Use->CanAccess(TEXT("Backpack")));
	Use->Advance(.2f);
	TestTrue(TEXT("Backpack becomes accessible"), Use->CanAccess(TEXT("Backpack")));
	TestTrue(TEXT("Transfer to open backpack"), Use->Transfer(Bandage, TEXT("Backpack")));
	Use->HandleShortcut(0, 40);
	TestFalse(TEXT("Shortcut never takes an item from backpack"), Use->HasHeldItem());
	TestEqual(TEXT("Transfers never duplicate or consume"), Inventory->GetEntries().Num(), InitialCount);
	Use->CloseBackpack();
	TestFalse(TEXT("Closing restores the access gate"), Use->CanAccess(TEXT("Backpack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickPocketPanelTest, "Prototype.Inventory.QuickPocketPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuickPocketPanelTest::RunTest(const FString&)
{
	FQuickPocketsFixture Fixture;
	auto* Inventory = Fixture.Inventory;
	auto* Use = Fixture.Use;
	for (const auto& Entry : Inventory->GetEntries())
	{
		FItemInstance Removed;
		Inventory->RemoveItem(Entry.Item.InstanceId, Removed);
	}
	FInventoryItemProfile Profile;
	Profile.Id = TEXT("PocketFixture");
	Profile.Definition = NewObject<UItemDefinition>(Inventory);
	Profile.Definition->ItemId = Profile.Id;
	Profile.Definition->DisplayName = FText::FromString(TEXT("Pocket fixture"));
	Profile.Definition->Category = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical"));
	Profile.Definition->MassKg = .1;
	FInventoryShapePart Shape; Shape.Vertices = {{-15,-10},{15,-10},{15,10},{-15,10}};
	Profile.ShapeParts = { Shape };
	if (!TestTrue(TEXT("Fixture profile valid"), Inventory->RegisterProfile(Profile) == EInventoryResult::Success)) return false;
	const FItemInstance BagItem = FItemInstance::Create(Profile.Definition);
	const FItemInstance QuickItem = FItemInstance::Create(Profile.Definition);
	Inventory->AddItem(BagItem, Profile.Id, TEXT("Backpack"), FVector2D(50,50), 0);
	Inventory->AddItem(QuickItem, Profile.Id, TEXT("Quick"), FVector2D(50,50), 0);
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>());
	Settings->ResetDefaults();
	FGuid Dropped;
	const auto Panel = SNew(SInventoryPanel).Inventory(Inventory).ItemUse(Use).Controls(Settings.Get()).SaveControls(false)
		.OnDropItem(SInventoryPanel::FOnDropItem::CreateLambda([&](FGuid Id, FString&) { Dropped = Id; return false; }));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1000,800), FSlateLayoutTransform(1.5f));
	const FVector2D BagOrigin(40,150), QuickOrigin(440,150);
	auto Mouse = [&](FVector2D Point, FKey Key) { const auto P = G.LocalToAbsolute(Point); return FPointerEvent(0,P,P,TSet<FKey>(),Key,0,FModifierKeysState()); };
	auto Down = [&](FVector2D P) { Panel->OnMouseButtonDown(G, Mouse(P,EKeys::LeftMouseButton)); };
	auto Up = [&](FVector2D P) { Panel->OnMouseButtonUp(G, Mouse(P,EKeys::LeftMouseButton)); };
	auto Key = [&](FKey K, bool Repeat = false) { Panel->OnKeyDown(G,FKeyEvent(K,FModifierKeysState(),0,Repeat,0,0)); };
	auto Check = [&](FGuid Id, FName Pocket, FVector2D Position, double Angle = 0)
	{
		FInventoryEntry Entry;
		TestTrue(TEXT("Instance still exists"), Inventory->GetItem(Id,Entry));
		TestEqual(TEXT("Destination pocket"), Entry.PocketId, Pocket);
		TestTrue(TEXT("Placement preserved"), Entry.Position.Equals(Position,.001));
		TestTrue(TEXT("Angle preserved"), FMath::IsNearlyEqual(Entry.AngleDegrees,Angle,.001));
	};
	TestEqual(TEXT("Initially pocket one"), Panel->GetVisibleQuickPocket(), FName(TEXT("Quick")));
	Down(BagOrigin+FVector2D(50,50)); Up(QuickOrigin+FVector2D(100,100));
	Check(BagItem.InstanceId,TEXT("Backpack"),FVector2D(50,50));
	Down(QuickOrigin+FVector2D(60,50)); Key(EKeys::Tab);
	TestEqual(TEXT("Cycle while dragging"), Panel->GetVisibleQuickPocket(),FName(TEXT("Quick2")));
	Up(QuickOrigin+FVector2D(110,100));
	Check(QuickItem.InstanceId,TEXT("Quick2"),FVector2D(100,100));
	Key(EKeys::Tab,true);
	TestEqual(TEXT("Held cycle key does not skip pockets"), Panel->GetVisibleQuickPocket(),FName(TEXT("Quick2")));
	// Idle selection disappears when its pocket is hidden, so Drop cannot target it.
	Key(EKeys::Tab); Key(EKeys::Delete);
	TestFalse(TEXT("Hidden idle selection cannot be dropped"), Dropped.IsValid());
	Key(EKeys::Tab);
	TestEqual(TEXT("Third pocket wraps to first"), Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	Down(QuickOrigin+FVector2D(100,100)); Up(QuickOrigin+FVector2D(150,150));
	Check(QuickItem.InstanceId,TEXT("Quick2"),FVector2D(100,100));
	Key(EKeys::Tab);
	Down(QuickOrigin+FVector2D(100,100)); Up(BagOrigin+FVector2D(150,150));
	Check(QuickItem.InstanceId,TEXT("Quick2"),FVector2D(100,100));
	Use->BeginOpenBackpack(); Use->Advance(2);
	Down(QuickOrigin+FVector2D(100,100)); Up(BagOrigin+FVector2D(50,50));
	Check(QuickItem.InstanceId,TEXT("Quick2"),FVector2D(100,100));
	Down(QuickOrigin+FVector2D(100,100)); Key(EKeys::E); Panel->Tick(G,0,.125f);
	Panel->OnKeyUp(G,FKeyEvent(EKeys::E,FModifierKeysState(),0,false,0,0));
	Up(BagOrigin+FVector2D(150.25,150.75));
	Check(QuickItem.InstanceId,TEXT("Backpack"),FVector2D(150.25,150.75),15);
	// Backpack remains visible while cycling, and transfer targets the visible pocket.
	Key(EKeys::Tab); Key(EKeys::T);
	FInventoryEntry Entry; Inventory->GetItem(QuickItem.InstanceId,Entry);
	TestEqual(TEXT("Transfer targets pocket three"), Entry.PocketId,FName(TEXT("Quick3")));
	Down(QuickOrigin+Entry.Position); Up(QuickOrigin+FVector2D(2,2));
	Check(QuickItem.InstanceId,TEXT("Quick3"),Entry.Position,15);
	Down(QuickOrigin+Entry.Position); Key(EKeys::Tab); Key(EKeys::Escape); Up(BagOrigin+FVector2D(250,250));
	Check(QuickItem.InstanceId,TEXT("Quick3"),Entry.Position,15);
	Key(EKeys::Delete);
	TestFalse(TEXT("Cancelled drag cannot leave a hidden drop target"), Dropped.IsValid());
	// Key rebinding must retain cycling behavior.
	FString Error;
	TestTrue(TEXT("Cycle binding editable"), Settings->TrySetKey(EInventoryControl::ShowQuick,EKeys::Z,Error));
	Key(EKeys::Tab);
	TestEqual(TEXT("Old cycle key is inert"), Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	Key(EKeys::Z); Key(EKeys::Z);
	TestEqual(TEXT("New key reaches pocket three"), Panel->GetVisibleQuickPocket(),FName(TEXT("Quick3")));
	Inventory->ReserveItem(QuickItem.InstanceId);
	Down(QuickOrigin+Entry.Position); Up(BagOrigin+FVector2D(250,250));
	Check(QuickItem.InstanceId,TEXT("Quick3"),Entry.Position,15);
	Inventory->ReleaseItem(QuickItem.InstanceId);
	Settings->bToggleGrab = true;
	Down(QuickOrigin+Entry.Position); Up(QuickOrigin+Entry.Position); Key(EKeys::Z);
	Down(QuickOrigin+FVector2D(100,100)); Up(QuickOrigin+FVector2D(100,100));
	Check(QuickItem.InstanceId,TEXT("Quick"),FVector2D(100,100),15);
	TestEqual(TEXT("Gestures preserve instance count"),Inventory->GetEntries().Num(),2);
	return true;
}
#endif
