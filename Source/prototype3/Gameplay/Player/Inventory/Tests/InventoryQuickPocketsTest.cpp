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
	for (int32 Index = 1; Index < 3; ++Index)
	{
		TestTrue(TEXT("Transfer to another quick pocket"), Use->Transfer(Bandage, UPlayerItemUseComponent::QuickPocketId(Index)));
		Use->PressQuickItem(0); Use->ReleaseQuickItem(0);
		TestEqual(TEXT("Shortcut finds the same instance in each hidden pocket"), Use->GetHeldId(), Bandage);
		TestFalse(TEXT("Held item cannot be transferred"), Use->Transfer(Bandage, TEXT("Quick")));
		Use->Stow();
	}
	TestFalse(TEXT("Closed backpack rejects transfer"), Use->Transfer(Bandage, TEXT("Backpack")));
	TestTrue(TEXT("Opening starts"), Use->BeginOpenBackpack());
	Use->Advance(.4f);
	TestFalse(TEXT("Delay still gates backpack access"), Use->CanAccess(TEXT("Backpack")));
	Use->Advance(.2f);
	TestTrue(TEXT("Backpack becomes accessible"), Use->CanAccess(TEXT("Backpack")));
	TestTrue(TEXT("Transfer to open backpack"), Use->Transfer(Bandage, TEXT("Backpack")));
	Use->PressQuickItem(0); Use->ReleaseQuickItem(0);
	TestFalse(TEXT("Shortcut never takes an item from backpack"), Use->HasHeldItem());
	TestEqual(TEXT("Transfers never duplicate or consume"), Inventory->GetEntries().Num(), InitialCount);
	Use->CloseBackpack();
	TestFalse(TEXT("Closing restores the access gate"), Use->CanAccess(TEXT("Backpack")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickItemHoldTest, "Prototype.Inventory.QuickItemHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuickItemHoldTest::RunTest(const FString&)
{
	FQuickPocketsFixture F;
	auto* Use = F.Use;
	auto* Vitals = Use->GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	Vitals->ApplyDamage(50);
	const int32 Count = F.Inventory->GetEntries().Num();
	Use->PressQuickItem(0); Use->Advance(.1f); Use->ReleaseQuickItem(0);
	const FGuid Bandage = Use->GetHeldId();
	TestTrue(TEXT("Tap takes bandage into hands"),Bandage.IsValid());
	Use->Advance(5);
	TestFalse(TEXT("Released tap never begins use"),Use->IsUsing());
	TestEqual(TEXT("Tap never consumes"),F.Inventory->GetEntries().Num(),Count);
	Use->PressQuickItem(0); Use->Advance(.4f);
	TestTrue(TEXT("Holding begins use after the threshold"),Use->IsUsing());
	Use->Advance(1); Use->ReleaseQuickItem(0);
	TestFalse(TEXT("Release cancels incomplete use"),Use->IsUsing());
	TestEqual(TEXT("Release leaves the same item in hands"),Use->GetHeldId(),Bandage);
	TestEqual(TEXT("Cancelled hold does not heal"),Vitals->GetCurrentHealth(),50.f);
	Use->Advance(5);
	TestEqual(TEXT("Cancelled hold never consumes later"),F.Inventory->GetEntries().Num(),Count);
	Use->PressQuickItem(0); Use->Advance(.5f);
	Use->ReleaseQuickItem(1);
	TestTrue(TEXT("Releasing a different key does not cancel active use"),Use->IsUsing());
	Vitals->ApplyDamage(1); Use->Advance(5);
	TestFalse(TEXT("Damage cancels held use without auto-restarting"),Use->IsUsing());
	Use->ReleaseQuickItem(0);
	Use->PressQuickItem(0); Use->Advance(1);
	Use->CancelQuickItemHold(); Use->Advance(5);
	TestFalse(TEXT("Interface/focus cancellation ends held use"),Use->IsUsing());
	TestEqual(TEXT("Interrupted holds preserve ownership"),F.Inventory->GetEntries().Num(),Count);
	Use->PressQuickItem(0); Use->Advance(Use->QuickUseHoldSeconds + 3.1f);
	TestFalse(TEXT("Complete hold spends its held item"),Use->HasHeldItem());
	TestEqual(TEXT("Complete hold consumes exactly one"),F.Inventory->GetEntries().Num(),Count-1);
	TestEqual(TEXT("Complete hold heals once"),Vitals->GetCurrentHealth(),74.f);
	Use->Advance(8);
	TestEqual(TEXT("Continued hold does not chain another use"),F.Inventory->GetEntries().Num(),Count-1);
	Use->ReleaseQuickItem(0);
	Use->PressQuickItem(1); Use->ReleaseQuickItem(1);
	const FGuid Food = Use->GetHeldId();
	Use->PressQuickItem(1); Use->Advance(5); Use->ReleaseQuickItem(1);
	TestEqual(TEXT("Repeated key stays on the named food item"),Use->GetHeldId(),Food);
	TestEqual(TEXT("Unimplemented food effect cannot consume"),F.Inventory->GetEntries().Num(),Count-1);
	Use->PressQuickItem(2); Use->ReleaseQuickItem(2);
	FInventoryEntry Water;
	TestTrue(TEXT("Water key selects its fixed type"),F.Inventory->GetItem(Use->GetHeldId(),Water) && Water.Item.Definition->ItemId==TEXT("WaterBottle"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickPocketPanelTest, "Prototype.Inventory.QuickPocketPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FQuickPocketPanelTest::RunTest(const FString&)
{
	FQuickPocketsFixture F;
	auto* Inventory = F.Inventory;
	auto* Use = F.Use;
	for (const auto& Entry : Inventory->GetEntries()) { FItemInstance Removed; Inventory->RemoveItem(Entry.Item.InstanceId,Removed); }
	FInventoryItemProfile Profile;
	Profile.Id = TEXT("PocketFixture");
	Profile.Definition = NewObject<UItemDefinition>(Inventory);
	Profile.Definition->ItemId = Profile.Id;
	Profile.Definition->DisplayName = FText::FromString(TEXT("Pocket fixture"));
	Profile.Definition->Category = FGameplayTag::RequestGameplayTag(TEXT("Item.Category.Consumable.Medical"));
	Profile.Definition->MassKg = .1;
	FInventoryShapePart Shape; Shape.Vertices = {{-15,-10},{15,-10},{15,10},{-15,10}};
	Profile.ShapeParts = {Shape};
	if (!TestTrue(TEXT("Valid fixture profile"),Inventory->RegisterProfile(Profile)==EInventoryResult::Success)) return false;
	TArray<FGuid> Ids;
	for (int32 I=0; I<3; ++I)
	{
		const auto Item=FItemInstance::Create(Profile.Definition); Ids.Add(Item.InstanceId);
		Inventory->AddItem(Item,Profile.Id,UPlayerItemUseComponent::QuickPocketId(I),FVector2D(50),0);
	}
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	int32 Closed=0;
	const auto Panel=SNew(SInventoryPanel).Inventory(Inventory).ItemUse(Use).Controls(Settings.Get()).SaveControls(false).PocketsOnly(true)
		.OnClose(FSimpleDelegate::CreateLambda([&](){++Closed;}));
	const FGeometry G=FGeometry::MakeRoot(FVector2D(1540,940),FSlateLayoutTransform(1.5f));
	auto Mouse=[&](FVector2D P,FKey K){auto S=G.LocalToAbsolute(P);return FPointerEvent(0,S,S,TSet<FKey>(),K,0,FModifierKeysState());};
	auto Down=[&](FVector2D P){Panel->OnMouseButtonDown(G,Mouse(P,EKeys::LeftMouseButton));};
	auto Up=[&](FVector2D P){Panel->OnMouseButtonUp(G,Mouse(P,EKeys::LeftMouseButton));};
	auto Key=[&](FKey K){Panel->OnKeyDown(G,FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
	TestTrue(TEXT("Pockets interface ready immediately"),Panel->IsInterfaceReady());
	for (int32 I=0; I<3; ++I)
	{
		const FVector2D Origin(40+240*I,150);
		Down(Origin+FVector2D(50)); Up(Origin+FVector2D(100));
		FInventoryEntry Entry; Inventory->GetItem(Ids[I],Entry);
		TestEqual(TEXT("All three pockets interact without cycling"),Entry.Position,FVector2D(100));
	}
	Key(EKeys::I);
	TestFalse(TEXT("Full inventory begins loading"),Panel->IsInterfaceReady());
	Use->Advance(.4f); Panel->Tick(G,0,.1f);
	Down(FVector2D(600,370)); Up(FVector2D(200,350));
	FInventoryEntry Entry; Inventory->GetItem(Ids[0],Entry);
	TestEqual(TEXT("All storage input gated during delay"),Entry.PocketId,FName(TEXT("Quick")));
	Use->Advance(.1f); Panel->Tick(G,0,.1f);
	TestTrue(TEXT("Full inventory available after delay"),Panel->IsInterfaceReady());
	TestFalse(TEXT("Full inventory includes backpack"),Panel->IsPocketsOnly());
	for (int32 I=0; I<3; ++I)
	{
		TestEqual(TEXT("Full view cycles one pocket at a time"),Panel->GetVisibleQuickPocket(),UPlayerItemUseComponent::QuickPocketId(I));
		Down(FVector2D(600,370)); Up(FVector2D(600,400));
		Inventory->GetItem(Ids[I],Entry);
		TestEqual(TEXT("Visible pocket remains interactive"),Entry.Position,FVector2D(100,130));
		Key(EKeys::Tab);
		TestFalse(TEXT("Tab never changes full view to pockets-only"),Panel->IsPocketsOnly());
		TestTrue(TEXT("Tab preserves backpack access"),Use->IsBackpackOpen());
	}
	TestEqual(TEXT("Third pocket wraps to first"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	Down(FVector2D(600,400)); Up(FVector2D(430,720));
	Inventory->GetItem(Ids[0],Entry);
	TestEqual(TEXT("Enlarged backpack fits below former boundary"),Entry.PocketId,FName(TEXT("Backpack")));
	TestEqual(TEXT("Expanded backpack placement"),Entry.Position,FVector2D(390,570));
	Key(EKeys::Tab);
	Down(FVector2D(600,400)); Key(EKeys::Tab); Up(FVector2D(660,450));
	Inventory->GetItem(Ids[1],Entry);
	TestEqual(TEXT("Carried item can move across cycling"),Entry.PocketId,FName(TEXT("Quick3")));
	Panel->SetPocketsOnly(true);
	Key(EKeys::Tab); TestEqual(TEXT("Tab still closes pockets-only interface"),Closed,1);
	Panel->SetPocketsOnly(false);
	auto* Vitals=Use->GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	Vitals->ApplyDamage(1); Panel->Tick(G,0,.1f);
	TestFalse(TEXT("Damage interrupts opening"),Use->IsOpeningBackpack());
	TestEqual(TEXT("Interrupted opening closes UI"),Closed,2);
	Panel->SetPocketsOnly(false); Use->Advance(2);
	Vitals->ApplyDamage(1); Panel->Tick(G,0,.1f);
	TestFalse(TEXT("Damage closes open backpack"),Use->IsBackpackOpen());
	TestEqual(TEXT("Damage closes complete interface"),Closed,3);
	Panel->SetPocketsOnly(false); Use->Advance(1);
	Vitals->ApplyDamage(1,false); Use->Advance(1);
	TestTrue(TEXT("Noninterrupting damage allows opening to finish"),Panel->IsInterfaceReady());
	Vitals->ApplyDamage(1,false);
	TestTrue(TEXT("Noninterrupting damage leaves backpack open"),Use->IsBackpackOpen());
	FString Error; Settings->TrySetKey(EInventoryControl::CyclePocket,EKeys::Z,Error);
	Key(EKeys::Tab); TestEqual(TEXT("Old cycle binding inert"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	Key(EKeys::Z); TestEqual(TEXT("Rebound cycle key works"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick2")));
	TestFalse(TEXT("Rebound cycle preserves full view"),Panel->IsPocketsOnly());
	TestEqual(TEXT("Mode changes preserve ownership"),Inventory->GetEntries().Num(),3);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageInterruptionTest, "Prototype.Inventory.DamageInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDamageInterruptionTest::RunTest(const FString&)
{
	FQuickPocketsFixture F;
	auto* Vitals=F.Use->GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	FGuid Bandage;
	for (const auto& Entry:F.Inventory->GetEntries())
		if (Entry.PocketId==TEXT("Quick") && Entry.Item.Definition->ItemId==TEXT("Bandage")) Bandage=Entry.Item.InstanceId;
	Vitals->ApplyDamage(40);
	if (!TestTrue(TEXT("Bandage equipped"),F.Use->EquipToHands(Bandage))) return false;
	F.Use->PressItemAction(true); F.Use->Advance(.5f);
	const float Progress=F.Use->GetProgress();
	Vitals->ApplyDamage(1,false);
	TestTrue(TEXT("Future damage-over-time can preserve bandage use"),F.Use->IsUsing());
	TestEqual(TEXT("Noninterrupting damage preserves timer"),F.Use->GetProgress(),Progress);
	Vitals->ApplyDamage(1);
	TestFalse(TEXT("Direct hit cancels bandage"),F.Use->IsUsing());
	TestEqual(TEXT("Direct hit clears progress"),F.Use->GetProgress(),0.f);
	FInventoryEntry Entry;
	TestTrue(TEXT("Interrupted bandage remains owned"),F.Inventory->GetItem(Bandage,Entry));
	F.Use->PressItemAction(true); Vitals->ApplyDamage(1,false); F.Use->Advance(10);
	TestFalse(TEXT("Bandage completes through noninterrupting damage"),F.Inventory->GetItem(Bandage,Entry));
	F.Use->BeginOpenBackpack(); F.Use->Advance(1);
	Vitals->ApplyDamage(1000,false);
	TestFalse(TEXT("Death always interrupts opening"),F.Use->IsOpeningBackpack());
	TestFalse(TEXT("Death leaves backpack closed"),F.Use->IsBackpackOpen());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemActionContractTest, "Prototype.Inventory.ItemActionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FItemActionContractTest::RunTest(const FString&)
{
	FQuickPocketsFixture F;
	auto* Vitals = F.Use->GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	Vitals->ApplyDamage(50);
	F.Use->PressQuickItem(0); F.Use->ReleaseQuickItem(0);
	const FGuid Bandage = F.Use->GetHeldId();
	const int32 Count = F.Inventory->GetEntries().Num();
	TestTrue(TEXT("Held item consumes missing primary without fallback"),F.Use->PressItemAction(false));
	TestFalse(TEXT("Primary never uses secondary healing contract"),F.Use->IsUsing());
	F.Use->ReleaseItemAction(false);
	F.Use->PressItemAction(true); F.Use->Advance(.5f);
	TestTrue(TEXT("Secondary begins the declared healing action"),F.Use->IsUsing());
	F.Use->ReleaseItemAction(false);
	TestTrue(TEXT("Other slot release does not cancel use"),F.Use->IsUsing());
	F.Use->ReleaseItemAction(true); F.Use->Advance(5);
	TestFalse(TEXT("Secondary release cancels unfinished use"),F.Use->IsUsing());
	TestEqual(TEXT("Cancelled action has no healing effect"),Vitals->GetCurrentHealth(),50.f);
	TestEqual(TEXT("Cancelled action preserves item and reservation"),F.Use->GetHeldId(),Bandage);
	TestTrue(TEXT("Reservation remains"),F.Inventory->IsReserved(Bandage));
	F.Use->PressItemAction(true);
	TestFalse(TEXT("Invalid equip rejected"),F.Use->EquipToHands(FGuid::NewGuid()));
	TestTrue(TEXT("Failed conflicting request preserves healing"),F.Use->IsUsing());
	// Remove the only pocket food before requesting its fixed shortcut.
	for (const auto& Entry : F.Inventory->GetEntries())
		if (UPlayerItemUseComponent::IsQuickPocket(Entry.PocketId) && Entry.Item.Definition->ItemId == TEXT("CannedBeans"))
		{ FItemInstance Removed; F.Inventory->RemoveItem(Entry.Item.InstanceId,Removed); }
	F.Use->PressQuickItem(1);
	TestTrue(TEXT("Missing quick item leaves healing underway"),F.Use->IsUsing());
	F.Use->PressQuickItem(2); F.Use->ReleaseQuickItem(2);
	TestFalse(TEXT("Available different item interrupts healing"),F.Use->IsUsing());
	TestTrue(TEXT("Valid switch changes hands"),F.Use->GetHeldId() != Bandage);
	FInventoryEntry Entry;
	TestTrue(TEXT("Interrupted bandage still owned"),F.Inventory->GetItem(Bandage,Entry));
	F.Use->Advance(5);
	TestEqual(TEXT("Interrupted use cannot restart from old hold"),Vitals->GetCurrentHealth(),50.f);
	TestEqual(TEXT("Only fixture food was removed"),F.Inventory->GetEntries().Num(),Count-1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeldItemPanelTest, "Prototype.Inventory.HeldItemPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHeldItemPanelTest::RunTest(const FString&)
{
	FQuickPocketsFixture F;
	FInventoryEntry Original;
	for (const auto& Entry : F.Inventory->GetEntries())
		if (Entry.PocketId == TEXT("Quick") && Entry.Item.Definition->ItemId == TEXT("Bandage")) Original = Entry;
	const FGuid Id = Original.Item.InstanceId;
	if (!TestTrue(TEXT("Bandage fixture exists"),Id.IsValid())) return false;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>()); Settings->ResetDefaults();
	int32 Closed = 0;
	const auto Panel = SNew(SInventoryPanel).Inventory(F.Inventory).ItemUse(F.Use).Controls(Settings.Get()).SaveControls(false).PocketsOnly(true)
		.OnClose(FSimpleDelegate::CreateLambda([&](){++Closed;}));
	const FGeometry G = FGeometry::MakeRoot(FVector2D(1540,940),FSlateLayoutTransform());
	auto Mouse = [](FVector2D P,FKey K) { return FPointerEvent(0,P,P,TSet<FKey>(),K,0,FModifierKeysState()); };
	auto Move = [&](FVector2D P) { Panel->OnMouseMove(G,Mouse(P,EKeys::Invalid)); };
	auto Down = [&](FVector2D P) { Panel->OnMouseButtonDown(G,Mouse(P,EKeys::LeftMouseButton)); };
	auto Up = [&](FVector2D P) { Panel->OnMouseButtonUp(G,Mouse(P,EKeys::LeftMouseButton)); };
	auto Key = [&](FKey K) { Panel->OnKeyDown(G,FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	auto KeyUp = [&](FKey K) { Panel->OnKeyUp(G,FKeyEvent(K,FModifierKeysState(),0,false,0,0)); };
	auto Click = [&](FVector2D P) { Down(P); Up(P); };
	const FVector2D Start = FVector2D(40,150) + Original.Position;
	Click(Start);
	TestFalse(TEXT("Single click selects without taking into hands"),F.Use->HasHeldItem());
	Click(Start);
	TestEqual(TEXT("Double click takes selected stored item"),F.Use->GetHeldId(),Id);
	TestEqual(TEXT("Take leaves inventory open"),Closed,0);
	Click(Start); Click(Start);
	TestFalse(TEXT("Double click held item stows"),F.Use->HasHeldItem());
	TestEqual(TEXT("Stow leaves inventory open"),Closed,0);
	FString Error;
	Settings->AssignKey(EInventoryControl::Grab,1,EKeys::J,false,Error);
	Move(Start); Key(EKeys::J); KeyUp(EKeys::J); Key(EKeys::J); KeyUp(EKeys::J);
	TestEqual(TEXT("Double tap alternate keyboard bind takes item"),F.Use->GetHeldId(),Id);
	Down(Start); Move(Start+FVector2D(50,100)); Key(EKeys::C); Up(Start+FVector2D(50,100));
	FInventoryEntry Entry; F.Inventory->GetItem(Id,Entry);
	TestEqual(TEXT("Cancel retains hands"),F.Use->GetHeldId(),Id);
	TestEqual(TEXT("Cancel retains original location"),Entry.Position,Original.Position);
	TestTrue(TEXT("Cancel retains reservation"),F.Inventory->IsReserved(Id));
	Down(Start); Up(FVector2D(41,151));
	F.Inventory->GetItem(Id,Entry);
	TestEqual(TEXT("Invalid boundary placement retains hands"),F.Use->GetHeldId(),Id);
	TestEqual(TEXT("Invalid placement retains source"),Entry.Position,Original.Position);
	Down(Start); Key(EKeys::J); Move(FVector2D(330,230)); Up(FVector2D(330,230));
	TestEqual(TEXT("Releasing one of two grab keys does not commit"),F.Use->GetHeldId(),Id);
	KeyUp(EKeys::J);
	F.Inventory->GetItem(Id,Entry);
	TestEqual(TEXT("Last grab release places into second pocket"),Entry.PocketId,FName(TEXT("Quick2")));
	TestFalse(TEXT("Successful manual placement clears hands"),F.Use->HasHeldItem());
	TestFalse(TEXT("Successful placement clears reservation"),F.Inventory->IsReserved(Id));
	// Optional click-grab mode obeys the same unmoved double-activation rule.
	Settings->bToggleGrab = true;
	Click(FVector2D(330,230)); Click(FVector2D(330,230));
	TestEqual(TEXT("Click mode double click takes without placing"),F.Use->GetHeldId(),Id);
	TestEqual(TEXT("No take/stow/drag operation closes UI"),Closed,0);
	return true;
}
#endif
