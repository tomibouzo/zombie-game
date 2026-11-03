#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Player/PlayerNoiseComponent.h"
#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Core/Characters/prototype3Character.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

class FZombieNoiseScenario : public IAutomationLatentCommand
{
public:
	explicit FZombieNoiseScenario(FAutomationTestBase* InTest) : Test(InTest),Started(FPlatformTime::Seconds()) {}
	~FZombieNoiseScenario()
	{
		if (bInputConfigured)
		{
			ReleaseInputs();
			FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(bPreviousInput);
		}
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Noise integration timed out")); return true; }
		World=GEditor ? GEditor->PlayWorld : nullptr;
		Controller=World ? World->GetFirstPlayerController() : nullptr;
		Player=Controller.IsValid() ? Cast<Aprototype3Character>(Controller->GetPawn()) : nullptr;
		if (!Player.IsValid() || !Player->HasActorBegunPlay()) return false;
		if (World->GetTimeSeconds()<WaitUntil) return false;
		auto* Noise=Player->FindComponentByClass<UPlayerNoiseComponent>();
		auto* AI=Zombie.IsValid() ? Cast<AZombieAIController>(Zombie->GetController()) : nullptr;
		switch(Phase)
		{
		case 0:
			bPreviousInput=FSlateApplication::Get().GetHandleDeviceInputWhenApplicationNotActive();
			FSlateApplication::Get().SetHandleDeviceInputWhenApplicationNotActive(true);
			bInputConfigured=true;
			ForwardKey=GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::MoveForward);
			RunKey=GetDefault<UInventoryInputSettings>()->GetKey(EInventoryControl::Run);
			for(TActorIterator<AZombieCharacter> It(World);It;++It) { if(It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
			for(TActorIterator<AInventoryTestSpikes> It(World);It;++It) It->Destroy();
			ResetPositions();
			return Wait(.5);
		case 1:
			Steps=Noise->EmittedSteps;
			Key(RunKey,true);
			return Wait(.6);
		case 2:
			Test->TestEqual(TEXT("Holding run while stationary emits no noise"),Noise->EmittedSteps,Steps);
			ReleaseInputs();
			WalkStart=Player->GetActorLocation();
			Key(ForwardKey,true);
			return Wait(1.2);
		case 3:
			Test->AddInfo(FString::Printf(TEXT("Walking input travelled %.1f cm at max speed %.1f; step distance %.1f"),
				FVector::Dist2D(WalkStart,Player->GetActorLocation()),Player->GetCharacterMovement()->GetMaxSpeed(),Noise->StepDistance));
			ReleaseInputs();
			Test->TestTrue(TEXT("Actual walking generates steps"),Noise->EmittedSteps>Steps);
			Test->TestEqual(TEXT("Walking outside its smaller radius is not heard"),Zombie->Suspicion,0.f);
			ResetPositions();
			return Wait(.3);
		case 4:
			Key(ForwardKey,true); Key(RunKey,true);
			return Wait(.55);
		case 5:
			Test->TestTrue(TEXT("Input bindings actually produce running"),Player->IsRunning());
			Test->TestTrue(TEXT("Running is heard at distance where walking was silent"),Zombie->Suspicion>0 || Zombie->Behavior==EZombieBehavior::Investigate);
			Test->TestFalse(TEXT("Running noise behind does not grant vision"),AI->HasVisualContact());
			ReleaseInputs();
			ResetPositions();
			Noise->RunNoiseRadius=300;
			return Wait(.3);
		case 6:
			Key(ForwardKey,true); Key(RunKey,true);
			return Wait(.55);
		case 7:
			Test->TestEqual(TEXT("Reducing running noise radius changes detection"),Zombie->Suspicion,0.f);
			ReleaseInputs();
			ResetPositions();
			Wall=World->SpawnActor<AStaticMeshActor>(FVector(60,-1300,140),FRotator::ZeroRotator);
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Wall->SetActorScale3D(FVector(.1,3,2.8));
			return Wait(.3);
		case 8:
			Steps=Noise->EmittedSteps;
			Key(ForwardKey,true); Key(RunKey,true);
			return Wait(1.2);
		case 9:
			Test->TestTrue(TEXT("Wall really blocks player movement"),Player->GetActorLocation().X<40);
			Test->TestEqual(TEXT("Running against a wall emits no footstep noise"),Noise->EmittedSteps,Steps);
			ReleaseInputs();
			return true;
		}
		return true;
	}
private:
	void Key(FKey InKey,bool bPressed)
	{
		if (!Controller.IsValid()) return;
		// Do not queue a release for an untouched key immediately before pressing
		// it in the same input frame: Enhanced Input can consume that as a tap.
		if (bPressed)
		{
			if (PressedKeys.Contains(InKey)) return;
			PressedKeys.Add(InKey);
		}
		else if (PressedKeys.Remove(InKey)==0) return;
		FInputKeyEventArgs Args; Args.Key=InKey; Args.Event=bPressed?IE_Pressed:IE_Released; Args.AmountDepressed=bPressed?1.f:0.f;
		Controller->InputKey(Args);
	}
	void ReleaseInputs()
	{
		Key(ForwardKey,false); Key(RunKey,false);
		if(Player.IsValid()) { Player->ClearControlIntents(); Player->GetCharacterMovement()->StopMovementImmediately(); }
	}
	void ResetPositions()
	{
		Player->SetActorLocation(FVector(0,-1300,96),false,nullptr,ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(FRotator::ZeroRotator);
		if(Zombie.IsValid()) { if(Zombie->GetController()) Zombie->GetController()->Destroy(); Zombie->Destroy(); }
		FTransform Transform(FRotator(0,90,0),FVector(0,-750,88));
		Zombie=World->SpawnActorDeferred<AZombieCharacter>(AZombieCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Zombie->Tuning.WanderRadius=0; Zombie->Tuning.PursuitSpeed=0; Zombie->Tuning.IdleSeconds=100;
		Zombie->FinishSpawning(Transform);
	}
	bool Wait(double Seconds) { ++Phase; WaitUntil=World->GetTimeSeconds()+Seconds; return false; }
	FAutomationTestBase* Test;
	double Started,WaitUntil=0;
	int32 Phase=0,Steps=0;
	bool bPreviousInput=false,bInputConfigured=false;
	FKey ForwardKey,RunKey;
	TSet<FKey> PressedKeys;
	FVector WalkStart=FVector::ZeroVector;
	UWorld* World=nullptr;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<Aprototype3Character> Player;
	TWeakObjectPtr<AZombieCharacter> Zombie;
	AStaticMeshActor* Wall=nullptr;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieNoiseTest,"Prototype.Zombies.NoiseIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombieNoiseTest::RunTest(const FString&)
{
	if(GEditor->PlayWorld) { AddError(TEXT("Run noise verification outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieNoiseScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
