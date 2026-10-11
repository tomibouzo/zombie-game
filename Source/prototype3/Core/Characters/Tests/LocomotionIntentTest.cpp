#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Characters/prototype3Character.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "HAL/PlatformTime.h"

namespace LocomotionTests
{
struct FCharacterFixture
{
	UWorld* World;
	Aprototype3Character* Character;

	FCharacterFixture()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		UClass* Type = LoadClass<Aprototype3Character>(nullptr,
			TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
		Character = World->SpawnActor<Aprototype3Character>(Type, FVector(1000, 1000, 200), FRotator::ZeroRotator);
		if (Character)
		{
			Character->FindComponentByClass<UPlayerItemUseComponent>()->bSpawnTestSpikes = false;
			Character->DispatchBeginPlay();
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Character->GetCharacterMovement()->Velocity = FVector(100, 0, 0);
		}
	}

	~FCharacterFixture()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLocomotionIntentTest, "Prototype.Movement.IntentReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLocomotionIntentTest::RunTest(const FString&)
{
	LocomotionTests::FCharacterFixture Fixture;
	auto* Character = Fixture.Character;
	if (!TestNotNull(TEXT("Project character loads"), Character)) return false;
	Character->DoMove(0, 1);

	auto Start = [Character](EPlayerLocomotionGait Gait) { Character->GaitInputStarted(Gait); };
	auto Release = [Character](EPlayerLocomotionGait Gait, bool bTap)
	{
		if (auto* Intent = Character->FindGaitInputIntent(Gait); Intent && Intent->bHeld)
			Intent->InputStartTime = FPlatformTime::Seconds() - (bTap ? 0.01 : Character->GaitHoldThreshold + 0.1);
		Character->GaitInputCompleted(Gait);
	};
	auto Reset = [Character]()
	{
		Character->ClearControlIntents();
		Character->DoMove(0, 1);
		Character->GetCharacterMovement()->Velocity = FVector(100, 0, 0);
		Character->ResolveLocomotionState();
	};

	// Exercise both replacement directions and all tap/hold combinations.
	for (const auto Previous : {EPlayerLocomotionGait::Running, EPlayerLocomotionGait::Sprinting})
	{
		const auto New = Previous == EPlayerLocomotionGait::Running
			? EPlayerLocomotionGait::Sprinting : EPlayerLocomotionGait::Running;
		for (const bool bPreviousTap : {false, true})
		{
			for (const bool bNewTap : {false, true})
			{
				Reset();
				const FString Case = FString::Printf(TEXT("%s %s -> %s %s"),
					Previous == EPlayerLocomotionGait::Running ? TEXT("Run") : TEXT("Sprint"),
					bPreviousTap ? TEXT("tap") : TEXT("hold"),
					New == EPlayerLocomotionGait::Running ? TEXT("Run") : TEXT("Sprint"),
					bNewTap ? TEXT("tap") : TEXT("hold"));
				Start(Previous);
				if (bPreviousTap) Release(Previous, true);
				TestEqual(Case + TEXT(": previous gait acts"), Character->GetActiveGait(), Previous);
				Start(New);
				TestEqual(Case + TEXT(": newest gait acts immediately"), Character->GetActiveGait(), New);
				if (!bPreviousTap)
				{
					Release(Previous, true);
					Character->GaitInputCanceled(Previous);
					TestEqual(Case + TEXT(": old release/cancel cannot change newest gait"), Character->GetActiveGait(), New);
				}
				Release(New, bNewTap);
				TestEqual(Case + TEXT(": new release follows tap/hold"), Character->GetActiveGait(),
					bNewTap ? New : EPlayerLocomotionGait::Walking);
				if (bNewTap) { Start(New); Release(New, true); }
				TestEqual(Case + TEXT(": ending newest gait returns to walk"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
				Start(Previous);
				TestEqual(Case + TEXT(": fresh previous press works"), Character->GetActiveGait(), Previous);
				Release(Previous, false);
			}
		}
		Reset();
		Character->ToggleGaitRequest(Previous);
		Character->ToggleGaitRequest(New);
		TestEqual(TEXT("Blueprint toggle also replaces previous gait"), Character->GetActiveGait(), New);
		Character->ToggleGaitRequest(New);
		TestEqual(TEXT("Blueprint toggle off cannot restore old gait"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	}

	Reset();
	Start(EPlayerLocomotionGait::Running);
	Character->DoMove(0, -1);
	Start(EPlayerLocomotionGait::Sprinting);
	TestEqual(TEXT("Backward sprint replaces run but waits"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	Release(EPlayerLocomotionGait::Running, true);
	Character->DoMove(0, 1);
	Character->ResolveLocomotionState();
	TestEqual(TEXT("Pending sprint starts when direction allows"), Character->GetActiveGait(), EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Sprinting, false);
	TestEqual(TEXT("Pending sprint never remembers run"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);

	Reset();
	Start(EPlayerLocomotionGait::Running);
	Character->DoMove(0, -1);
	Start(EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Sprinting, false);
	Character->DoMove(0, 1);
	Character->ResolveLocomotionState();
	TestEqual(TEXT("Released pending hold cannot start later or restore run"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);

	Reset();
	Start(EPlayerLocomotionGait::Running);
	Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	Start(EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Sprinting, true);
	TestEqual(TEXT("Stationary tap stays pending"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	Character->GetCharacterMovement()->Velocity = FVector(100, 0, 0);
	Character->ResolveLocomotionState();
	TestEqual(TEXT("Movement starts only the newest latched gait"), Character->GetActiveGait(), EPlayerLocomotionGait::Sprinting);
	Character->ToggleGaitRequest(EPlayerLocomotionGait::Sprinting);
	TestEqual(TEXT("Pending tap off returns to walk"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);

	Reset();
	Start(EPlayerLocomotionGait::Running);
	auto* Vitals = Character->GetPlayerVitalsComponent();
	Vitals->DrainStamina(Vitals->GetCurrentStamina());
	Start(EPlayerLocomotionGait::Sprinting);
	TestEqual(TEXT("Exhaustion blocks newest gait without restoring old one"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	Vitals->InitializeVitals(100, 100, 50, .25f);
	Character->ResolveLocomotionState();
	TestEqual(TEXT("Newest request acts once stamina gate permits it"), Character->GetActiveGait(), EPlayerLocomotionGait::Sprinting);
	Character->GaitInputCanceled(EPlayerLocomotionGait::Sprinting);
	TestEqual(TEXT("Canceling newest request cannot revive consumed run"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);

	Reset();
	Start(EPlayerLocomotionGait::Running);
	Character->GaitInputStarted(EPlayerLocomotionGait::Walking);
	Character->ToggleGaitRequest(EPlayerLocomotionGait::Walking);
	TestEqual(TEXT("Unsupported gait request does not consume a valid request"), Character->GetActiveGait(), EPlayerLocomotionGait::Running);

	// Crouch also consumes held/toggled gaits; standing consumes the crouch press.
	auto* Movement = Character->GetCharacterMovement();
	Movement->bCrouchMaintainsBaseLocation = true;
	auto ReleaseCrouch = [Character](bool bTap)
	{
		if (Character->bCrouchInputHeld)
			Character->CrouchInputStartTime = FPlatformTime::Seconds() - (bTap ? .01 : Character->CrouchHoldThreshold + .1);
		Character->CrouchInputCompleted();
	};
	for (const auto Gait : {EPlayerLocomotionGait::Running, EPlayerLocomotionGait::Sprinting})
	{
		for (const bool bGaitTap : {false, true})
		{
			Reset();
			Start(Gait);
			if (bGaitTap) Release(Gait, true);
			Character->CrouchInputStarted();
			TestTrue(TEXT("New crouch request consumes even a held gait"), Movement->bWantsToCrouch);
			Movement->Crouch(false);
			TestEqual(TEXT("New crouch changes physical stance"), Character->GetActiveStance(), EPlayerLocomotionStance::Crouching);
			Release(Gait, true);
			Character->GaitInputCanceled(Gait);
			TestTrue(TEXT("Consumed gait release/cancel cannot undo crouch"), Movement->bWantsToCrouch);
			ReleaseCrouch(true);
			TestTrue(TEXT("Crouch tap still latches"), Movement->bWantsToCrouch);
			Start(Gait);
			TestFalse(TEXT("Gait consumes latched crouch"), Movement->bWantsToCrouch);
			Movement->UnCrouch(false);
			TestEqual(TEXT("New gait acts after standing"), Character->GetActiveGait(), Gait);
			Release(Gait, false);
			TestEqual(TEXT("Ending new gait cannot restore crouch"), Character->GetActiveStance(), EPlayerLocomotionStance::Standing);
		}
		Reset();
		Character->CrouchInputStarted();
		Movement->Crouch(false);
		Start(Gait);
		Movement->UnCrouch(false);
		ReleaseCrouch(true);
		Character->CrouchInputCanceled();
		TestEqual(TEXT("Superseded held crouch release/cancel cannot change gait"), Character->GetActiveGait(), Gait);
		Release(Gait, false);
		Character->CrouchInputStarted();
		TestTrue(TEXT("Fresh crouch press works after consumption"), Movement->bWantsToCrouch);
		Movement->Crouch(false);
		ReleaseCrouch(false);
		TestFalse(TEXT("Crouch hold ends on release"), Movement->bWantsToCrouch);
		Movement->UnCrouch(false);
	}

	Reset();
	Character->CrouchInputStarted();
	Movement->Crouch(false);
	auto* CeilingActor = Fixture.World->SpawnActor<AActor>();
	auto* Ceiling = NewObject<UBoxComponent>(CeilingActor);
	CeilingActor->SetRootComponent(Ceiling);
	Ceiling->SetBoxExtent(FVector(500, 500, 10));
	Ceiling->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ceiling->SetCollisionObjectType(ECC_WorldStatic);
	Ceiling->SetCollisionResponseToAllChannels(ECR_Block);
	Ceiling->SetWorldLocation(Character->GetActorLocation()
		+ FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20));
	Ceiling->RegisterComponent();
	Start(EPlayerLocomotionGait::Running);
	Movement->UnCrouch(false);
	TestEqual(TEXT("Ceiling physically blocks standing"), Character->GetActiveStance(), EPlayerLocomotionStance::Crouching);
	Start(EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Running, true);
	Movement->UnCrouch(false);
	TestEqual(TEXT("Replacing a blocked request cannot bypass clearance"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement->UnCrouch(false);
	TestEqual(TEXT("Clearance unlocks only newest request"), Character->GetActiveGait(), EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Sprinting, false);
	TestEqual(TEXT("Ending new request never restores blocked run/crouch"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);
	TestFalse(TEXT("Previous crouch remains consumed"), Movement->bWantsToCrouch);

	// Releasing a hold while still blocked also leaves the old crouch consumed.
	Character->CrouchInputStarted();
	Movement->Crouch(false);
	Ceiling->SetWorldLocation(Character->GetActorLocation()
		+ FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20));
	Ceiling->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Start(EPlayerLocomotionGait::Running);
	Movement->UnCrouch(false);
	Release(EPlayerLocomotionGait::Running, false);
	TestFalse(TEXT("Ending blocked gait cannot re-latch previous crouch"), Movement->bWantsToCrouch);
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement->UnCrouch(false);
	TestEqual(TEXT("After blocked hold ends, clearance permits standing walk"), Character->GetActiveStance(), EPlayerLocomotionStance::Standing);
	TestEqual(TEXT("Released blocked hold stays ended"), Character->GetActiveGait(), EPlayerLocomotionGait::Walking);

	Reset();
	auto* Use = Character->FindComponentByClass<UPlayerItemUseComponent>();
	FGuid Bandage;
	for (const auto& Entry : Use->Inventory->GetEntries())
		if (Entry.Item.Definition->ItemId == TEXT("Bandage") && Entry.PocketId == TEXT("Quick")) Bandage = Entry.Item.InstanceId;
	if (!TestTrue(TEXT("Quick bandage equips for interruption check"), Use->EquipToHands(Bandage))) return false;
	const int32 ItemCount = Use->Inventory->GetEntries().Num();
	Vitals->ApplyDamage(25);
	Use->PressItemAction(true);
	TestTrue(TEXT("Healing starts"), Use->IsUsing());
	Character->DoMove(0, -1);
	Start(EPlayerLocomotionGait::Sprinting);
	TestTrue(TEXT("Blocked pending sprint preserves healing until it can act"), Use->IsUsing());
	Character->DoMove(0, 1);
	Character->ResolveLocomotionState();
	TestFalse(TEXT("Newest pending gait interrupts healing once allowed"), Use->IsUsing());
	TestEqual(TEXT("Allowed pending gait starts"), Character->GetActiveGait(), EPlayerLocomotionGait::Sprinting);
	TestEqual(TEXT("Interruption preserves held bandage"), Use->GetHeldId(), Bandage);
	TestEqual(TEXT("Interruption does not consume an item"), Use->Inventory->GetEntries().Num(), ItemCount);
	Release(EPlayerLocomotionGait::Sprinting, false);
	TestFalse(TEXT("Ending gait does not resume consumed use"), Use->IsUsing());
	Use->PressItemAction(true);
	Character->DoMove(0, -1);
	Start(EPlayerLocomotionGait::Sprinting);
	Release(EPlayerLocomotionGait::Sprinting, false);
	Character->DoMove(0, 1);
	Character->ResolveLocomotionState();
	TestTrue(TEXT("Ending blocked hold before applicability preserves healing"), Use->IsUsing());
	Character->ToggleGaitRequest(EPlayerLocomotionGait::Running);
	TestFalse(TEXT("Blueprint gait request follows the same interruption rule"), Use->IsUsing());
	TestEqual(TEXT("Blueprint gait starts after interruption"), Character->GetActiveGait(), EPlayerLocomotionGait::Running);
	return true;
}
#endif
