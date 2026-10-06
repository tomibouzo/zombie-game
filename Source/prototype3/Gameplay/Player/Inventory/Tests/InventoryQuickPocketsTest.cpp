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
	Use->Advance(1.9f); Panel->Tick(G,0,.1f);
	Down(FVector2D(600,250)); Up(FVector2D(200,350));
	FInventoryEntry Entry; Inventory->GetItem(Ids[0],Entry);
	TestEqual(TEXT("All storage input gated during delay"),Entry.PocketId,FName(TEXT("Quick")));
	Use->Advance(.1f); Panel->Tick(G,0,.1f);
	TestTrue(TEXT("Full inventory available after delay"),Panel->IsInterfaceReady());
	TestFalse(TEXT("Full inventory includes backpack"),Panel->IsPocketsOnly());
	for (int32 I=0; I<3; ++I)
	{
		TestEqual(TEXT("Full view cycles one pocket at a time"),Panel->GetVisibleQuickPocket(),UPlayerItemUseComponent::QuickPocketId(I));
		Down(FVector2D(600,250)); Up(FVector2D(600,280));
		Inventory->GetItem(Ids[I],Entry);
		TestEqual(TEXT("Visible pocket remains interactive"),Entry.Position,FVector2D(100,130));
		Key(EKeys::Tab);
		TestFalse(TEXT("Tab never changes full view to pockets-only"),Panel->IsPocketsOnly());
		TestTrue(TEXT("Tab preserves backpack access"),Use->IsBackpackOpen());
	}
	TestEqual(TEXT("Third pocket wraps to first"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	Down(FVector2D(600,280)); Up(FVector2D(430,720));
	Inventory->GetItem(Ids[0],Entry);
	TestEqual(TEXT("Enlarged backpack fits below former boundary"),Entry.PocketId,FName(TEXT("Backpack")));
	TestEqual(TEXT("Expanded backpack placement"),Entry.Position,FVector2D(390,570));
	Key(EKeys::Tab);
	Down(FVector2D(600,280)); Key(EKeys::Tab); Up(FVector2D(660,330));
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
	FString Error; Settings->TrySetKey(EInventoryControl::ShowQuick,EKeys::Z,Error);
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
	F.Use->HandlePrimaryAction(); F.Use->Advance(.5f);
	const float Progress=F.Use->GetProgress();
	Vitals->ApplyDamage(1,false);
	TestTrue(TEXT("Future damage-over-time can preserve bandage use"),F.Use->IsUsing());
	TestEqual(TEXT("Noninterrupting damage preserves timer"),F.Use->GetProgress(),Progress);
	Vitals->ApplyDamage(1);
	TestFalse(TEXT("Direct hit cancels bandage"),F.Use->IsUsing());
	TestEqual(TEXT("Direct hit clears progress"),F.Use->GetProgress(),0.f);
	FInventoryEntry Entry;
	TestTrue(TEXT("Interrupted bandage remains owned"),F.Inventory->GetItem(Bandage,Entry));
	F.Use->HandlePrimaryAction(); Vitals->ApplyDamage(1,false); F.Use->Advance(10);
	TestFalse(TEXT("Bandage completes through noninterrupting damage"),F.Inventory->GetItem(Bandage,Entry));
	F.Use->BeginOpenBackpack(); F.Use->Advance(1);
	Vitals->ApplyDamage(1000,false);
	TestFalse(TEXT("Death always interrupts opening"),F.Use->IsOpeningBackpack());
	TestFalse(TEXT("Death leaves backpack closed"),F.Use->IsBackpackOpen());
	return true;
}
#endif
