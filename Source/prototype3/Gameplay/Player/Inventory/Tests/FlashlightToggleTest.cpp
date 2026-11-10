#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlashlightToggleTest, "Prototype.Inventory.FlashlightToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFlashlightToggleTest::RunTest(const FString&)
{
	using A = EInventoryControl;
	TStrongObjectPtr<UInventoryInputSettings> Settings(NewObject<UInventoryInputSettings>(GetTransientPackage()));
	Settings->ResetDefaults();
	TestEqual(TEXT("Flashlight key default"), Settings->GetKey(A::ToggleFlashlight), EKeys::F);
	TestTrue(TEXT("Flashlight key is rebindable"), UInventoryInputSettings::IsBindable(A::ToggleFlashlight));
	TestTrue(TEXT("Flashlight key is an item action"), UInventoryInputSettings::Section(A::ToggleFlashlight) == EControlSection::ItemActions);
	TestTrue(TEXT("F during gameplay does not collide with the inventory drop on F"), Settings->Conflicts(A::ToggleFlashlight, EKeys::F).IsEmpty());
	TestEqual(TEXT("Inventory drop keeps F"), Settings->GetKey(A::Drop), EKeys::F);
	TestTrue(TEXT("Appended after the build controls"), static_cast<int32>(A::ToggleFlashlight) > static_cast<int32>(A::BuildExit));

	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	ACharacter* Player = World->SpawnActor<ACharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
	auto* Inventory = NewObject<UInventoryComponent>(Player); Inventory->RegisterComponent();
	auto* Vitals = NewObject<UPlayerVitalsComponent>(Player); Vitals->RegisterComponent();
	Vitals->InitializeVitals(100, 100, 50, .25f);
	auto* Use = NewObject<UPlayerItemUseComponent>(Player);
	Use->bSpawnTestSpikes = false; Use->RegisterComponent();
	Player->DispatchBeginPlay();

	FGuid Flashlight, Bandage;
	for (const FInventoryEntry& Entry : Inventory->GetEntries())
	{
		if (Entry.Item.Definition->ItemId == TEXT("Flashlight")) Flashlight = Entry.Item.InstanceId;
		if (Entry.Item.Definition->ItemId == TEXT("Bandage") && UPlayerItemUseComponent::IsQuickPocket(Entry.PocketId)) Bandage = Entry.Item.InstanceId;
	}
	if (!TestTrue(TEXT("Starting inventory holds a flashlight and a pocket bandage"), Flashlight.IsValid() && Bandage.IsValid())) return false;

	TestFalse(TEXT("Empty hands: nothing to toggle"), Use->ToggleHeldLight());
	TestTrue(TEXT("Bandage taken in hands"), Use->EquipToHands(Bandage));
	TestFalse(TEXT("A bandage is not a light source"), Use->IsHeldLightSource());
	TestFalse(TEXT("The flashlight key does nothing with a bandage"), Use->ToggleHeldLight());
	TestTrue(TEXT("Bandage stowed"), Use->Stow());

	Use->BeginOpenBackpack(false);
	Use->Advance(1.f);
	if (!TestTrue(TEXT("Backpack open"), Use->IsBackpackOpen()) || !TestTrue(TEXT("Flashlight taken in hands"), Use->EquipToHands(Flashlight))) return false;
	Use->CloseBackpack();
	TestTrue(TEXT("Flashlight stays in hands after the backpack closes"), Use->GetHeldId() == Flashlight);
	TestTrue(TEXT("A held flashlight is a light source"), Use->IsHeldLightSource());
	TestFalse(TEXT("The light starts off"), Use->IsHeldLightOn());

	TestTrue(TEXT("Primary action is consumed"), Use->PressItemAction(false));
	TestTrue(TEXT("Primary action switches the light on"), Use->IsHeldLightOn());
	Use->ReleaseItemAction(false);
	TestTrue(TEXT("Releasing the button keeps the light on"), Use->IsHeldLightOn());
	Use->PressItemAction(false);
	TestFalse(TEXT("Primary action switches it off again"), Use->IsHeldLightOn());
	TestTrue(TEXT("The flashlight key toggles too"), Use->ToggleHeldLight());
	TestTrue(TEXT("The flashlight key switched it on"), Use->IsHeldLightOn());
	Use->PressItemAction(true);
	TestTrue(TEXT("Secondary action leaves the light alone"), Use->IsHeldLightOn());
	TestFalse(TEXT("Toggling the light is not an item use in progress"), Use->IsUsing());
	Use->Advance(.5f);
	TestTrue(TEXT("The light stays on over time"), Use->IsHeldLightOn());

	Use->ClearHeld();
	TestFalse(TEXT("Putting the flashlight away switches the light off"), Use->IsHeldLightOn());
	TestTrue(TEXT("Flashlight taken again"), Use->EquipToHands(Flashlight));
	Use->Advance(.1f);
	TestFalse(TEXT("It does not come back on by itself"), Use->IsHeldLightOn());
	Use->ToggleHeldLight();
	TestTrue(TEXT("It can be switched on again"), Use->IsHeldLightOn());

	Vitals->ApplyDamage(1000);
	TestFalse(TEXT("Death switches the light off"), Use->IsHeldLightOn());
	TestFalse(TEXT("A dead player cannot toggle a light"), Use->ToggleHeldLight());
	return true;
}
#endif
