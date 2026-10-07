#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Characters/prototype3Character.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "UI/Inventory/SInventoryPanel.h"
#include "UObject/StrongObjectPtr.h"

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
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(580,230)));
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(140,250)));
	TestTrue(TEXT("Slate placement queues in walking mode"), F.Use->IsArranging());
	TestEqual(TEXT("Slate does not mutate source on release"), F.Entry().PocketId,FName(TEXT("Quick")));
	Panel->OnKeyDown(G,FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0));
	TestEqual(TEXT("Pending arrangement blocks pocket switching"),Panel->GetVisibleQuickPocket(),FName(TEXT("Quick")));
	F.Use->Advance(.3f);
	TestEqual(TEXT("Slate placement commits after delay"), F.Entry().PocketId,FName(TEXT("Backpack")));
	Panel->OnMouseButtonDown(G,Mouse(FVector2D(140,250)));
	Panel->OnMouseButtonUp(G,Mouse(FVector2D(580,230)));
	Panel->CancelInteraction();
	TestFalse(TEXT("Interface cancellation clears pending state"),F.Use->IsArranging());
	TestEqual(TEXT("Canceled gesture keeps committed source"),F.Entry().PocketId,FName(TEXT("Backpack")));
	return true;
}
#endif
