#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Combat/CombatImpact.h"
#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "Gameplay/Player/PlayerNoiseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "StateTree.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieAssetTest,"Prototype.Zombies.Assets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombieAssetTest::RunTest(const FString&)
{
	auto* Tree = LoadObject<UStateTree>(nullptr,TEXT("/Game/Zombies/ST_ZombieBasic.ST_ZombieBasic"));
	TestTrue(TEXT("Authored StateTree is compiled and ready"),Tree && Tree->IsReadyToRun());
	TestNotNull(TEXT("Zombie has a humanoid presentation"),GetDefault<AZombieCharacter>()->GetMesh()->GetSkeletalMeshAsset());
	return !HasAnyErrors();
}

class FZombiePIEScenario : public IAutomationLatentCommand
{
public:
	explicit FZombiePIEScenario(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>150) { Test->AddError(FString::Printf(TEXT("Zombie PIE timed out in phase %d"),Phase)); return true; }
		World = GEditor ? GEditor->PlayWorld : nullptr;
		PC = World ? World->GetFirstPlayerController() : nullptr;
		Player = PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		if (Phase == 2 || Phase == 3) Player->AddMovementInput(FVector(1,0,0),1);
		if (bManualCombat && Zombie.IsValid()) Zombie->TickAttack();
		if (World->GetTimeSeconds()<WaitUntil) return false;
		auto* Noise = Player->FindComponentByClass<UPlayerNoiseComponent>();
		auto* Vitals = Player->GetPlayerVitalsComponent();
		auto* AI = Zombie.IsValid() ? Cast<AZombieAIController>(Zombie->GetController()) : nullptr;
		switch (Phase)
		{
		case 0:
		{
			auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation Projected;
			if (!Nav || !Nav->ProjectPointToNavigation(FVector(0,-900,0),Projected,FVector(100,100,200))) return false;
			for (TActorIterator<AZombieCharacter> It(World);It;++It) { if(It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
			for (TActorIterator<AInventoryTestSpikes> It(World);It;++It) It->Destroy();
			TeleportPlayer(FVector(-400,-900,96));
			Player->Crouch();
			SpawnZombie(FVector(0,-900,88));
			return Wait(.6);
		}
		case 1:
			Test->TestTrue(TEXT("StateTree actually running in PIE"),AI && AI->GetZombieStateTree()->IsRunning());
			Test->TestFalse(TEXT("Crouched behind cannot be seen"),AI->HasVisualContact());
			Test->TestEqual(TEXT("No initial suspicion behind"),Zombie->Suspicion,0.f);
			StepsBefore = Noise->EmittedSteps;
			return Wait(.8);
		case 2:
			Player->GetCharacterMovement()->StopMovementImmediately();
			Test->TestEqual(TEXT("Real crouched travel emits no steps"),Noise->EmittedSteps,StepsBefore);
			Test->TestEqual(TEXT("Crouched passage behind remains unnoticed"),Zombie->Suspicion,0.f);
			Player->UnCrouch();
			return Wait(.65);
		case 3:
			Player->GetCharacterMovement()->StopMovementImmediately();
			Test->TestTrue(TEXT("Walking emits perception noise"),Noise->EmittedSteps>StepsBefore);
			Test->TestFalse(TEXT("Noise alone does not confirm a target"),AI->IsAlerted());
			Test->TestTrue(TEXT("Heard walk starts investigation"),Zombie->Suspicion>0 || Zombie->Behavior==EZombieBehavior::Investigate);
			TeleportPlayer(FVector(700,-900,96));
			SpawnZombie(FVector(0,-900,88));
			Zombie->Tuning.FarDetectionSeconds=2;
			Zombie->Tuning.SearchSeconds=1.5;
			return Wait(.7);
		case 4:
			Test->TestTrue(TEXT("Far vision accumulates partial suspicion"),Zombie->Suspicion>0 && Zombie->Suspicion<1);
			Capture(TEXT("suspicion.png"));
			return Wait(1.8);
		case 5:
			Test->TestTrue(TEXT("Sustained sight detects player"),AI->IsAlerted() && AI->HasVisualContact());
			if (!bCapturedAlert) { Capture(TEXT("detected.png")); bCapturedAlert=true; WaitUntil=World->GetTimeSeconds()+.25; return false; }
			LastSeen=AI->GetLastKnownLocation();
			Wall=World->SpawnActor<AStaticMeshActor>(FVector(350,-900,140),FRotator::ZeroRotator);
			Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
			Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Wall->SetActorScale3D(FVector(.4,5,2.8));
			TeleportPlayer(FVector(700,-750,96));
			return Wait(.4);
		case 6:
			Test->TestFalse(TEXT("Wall immediately blocks visual contact"),AI->HasVisualContact());
			Test->TestTrue(TEXT("Search retains observed location, not hidden transform"),AI->GetLastKnownLocation().Equals(LastSeen,1));
			Test->TestEqual(TEXT("Lost contact activates Search StateTree state"),Zombie->Behavior,EZombieBehavior::Search);
			UAISense_Hearing::ReportNoiseEvent(World,FVector(100,-1150,0),1,Player,650,TEXT("TestClue"));
			return Wait(.4);
		case 7:
			Test->TestTrue(TEXT("Search follows new event location"),AI->GetLastKnownLocation().Equals(FVector(100,-1150,0),1));
			return Wait(1.8);
		case 8:
			Test->TestFalse(TEXT("Search eventually abandons encounter"),AI->IsAlerted());
			Wall->Destroy(); Wall=nullptr;
			// Use the persistent lab wall to test actual navigation around an occluder.
			TeleportPlayer(FVector(350,250,96));
			SpawnZombie(FVector(-350,250,88));
			Zombie->Tuning.PursuitSpeed=260;
			Zombie->Tuning.InvestigateSeconds=12;
			Zombie->Tuning.HearingRadius=1000;
			Cast<AZombieAIController>(Zombie->GetController())->RefreshSenses();
			UAISense_Hearing::ReportNoiseEvent(World,FVector(350,250,0),1,Player,900,TEXT("NavigationClue"));
			MoveStart=Zombie->GetActorLocation();
			return Wait(3);
		case 9:
		{
			Test->TestTrue(TEXT("Investigation actually moves on navmesh"),FVector::Dist2D(MoveStart,Zombie->GetActorLocation())>120);
			auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,MoveStart,FVector(350,250,88),Zombie.Get());
			Test->TestTrue(TEXT("Navigation produces a full route around wall"),Path && Path->IsValid() && !Path->IsPartial() && Path->GetPathLength()>800);
			TeleportPlayer(FVector(100,-900,96));
			SpawnZombie(FVector(0,-900,88));
			bManualCombat=true;
			AI=Cast<AZombieAIController>(Zombie->GetController());
			AI->GetZombieStateTree()->StopLogic(TEXT("Deterministic combat integration"));
			AI->SetActorTickEnabled(false);
			Zombie->Tuning.StrikeWindup=.15f; Zombie->Tuning.BiteWindup=.15f; Zombie->Tuning.RecoverySeconds=.15f;
			Vitals->Heal(100);
			Test->TestTrue(TEXT("First strike starts"),Zombie->BeginAttack(Player));
			Test->TestEqual(TEXT("First attack is strike"),Zombie->GetAttackKind(),EZombieAttackKind::Strike);
			return Wait(.4);
		}
		case 10:
			Test->TestEqual(TEXT("First strike damages exactly once"),Vitals->GetCurrentHealth(),92.f);
			Test->TestTrue(TEXT("Second strike starts"),Zombie->BeginAttack(Player));
			Test->TestEqual(TEXT("Second attack is strike"),Zombie->GetAttackKind(),EZombieAttackKind::Strike);
			return Wait(.4);
		case 11:
			Test->TestEqual(TEXT("Second strike damages exactly once"),Vitals->GetCurrentHealth(),84.f);
			Test->TestTrue(TEXT("Bite starts"),Zombie->BeginAttack(Player));
			Test->TestEqual(TEXT("Third attack is bite"),Zombie->GetAttackKind(),EZombieAttackKind::Bite);
			return Wait(.4);
		case 12:
			Test->TestEqual(TEXT("Bite uses its independent damage"),Vitals->GetCurrentHealth(),72.f);
			Test->TestEqual(TEXT("Sequence repeats"),Zombie->NextAttackIndex,0);
			Zombie->BeginAttack(Player);
			TeleportPlayer(FVector(400,-900,96));
			return Wait(.4);
		case 13:
			Test->TestEqual(TEXT("Leaving reach avoids pending hit"),Vitals->GetCurrentHealth(),72.f);
			TeleportPlayer(FVector(100,-900,96));
			if (!bCheckedAttackWall)
			{
				Zombie->BeginAttack(Player);
				Wall=World->SpawnActor<AStaticMeshActor>(FVector(50,-900,140),FRotator::ZeroRotator);
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				Wall->SetActorScale3D(FVector(.1,2,2.8));
				bCheckedAttackWall=true; WaitUntil=World->GetTimeSeconds()+.4; return false;
			}
			Test->TestEqual(TEXT("Wall appearing during windup blocks impact"),Vitals->GetCurrentHealth(),72.f);
			Wall->Destroy(); Wall=nullptr;
			Zombie->NextAttackIndex=0;
			Zombie->BeginAttack(Player);
			Impact(5);
			Test->TestTrue(TEXT("Weak hit does not interrupt"),Zombie->IsAttacking());
			return Wait(.4);
		case 14:
			Test->TestEqual(TEXT("Weak hit still allows attack to land"),Vitals->GetCurrentHealth(),64.f);
			Zombie->BeginAttack(Player);
			Impact(30);
			Test->TestFalse(TEXT("Strong hit immediately cancels pending bite"),Zombie->IsAttacking());
			return Wait(.8);
		case 15:
			Test->TestEqual(TEXT("Interrupted attack causes no delayed damage"),Vitals->GetCurrentHealth(),64.f);
			Zombie->Tuning.InterruptThreshold=3;
			Zombie->BeginAttack(Player); Impact(5);
			Test->TestFalse(TEXT("Edited threshold changes interruption"),Zombie->IsAttacking());
			return Wait(.8);
		case 16:
		{
			// Real melee component trace transmits interruption strength independently of damage.
			PC->SetControlRotation(FRotator(0,180,0));
			Zombie->Tuning.InterruptThreshold=25;
			Zombie->BeginAttack(Player);
			auto* Melee=Player->FindComponentByClass<UPlayerMeleeComponent>();
			Melee->ImpactStrength=30; Melee->HitDelay=0; Melee->UnarmedDamage=1;
			Test->TestTrue(TEXT("Player's real melee action starts"),Melee->TryAttack());
			Test->TestFalse(TEXT("Real player punch interrupts with configured strength"),Zombie->IsAttacking());
			return Wait(.8);
		}
		case 17:
			Zombie->BeginAttack(Player);
			Zombie->TakeDamage(1000,FDamageEvent(),PC,Player);
			Test->TestFalse(TEXT("Zombie dies and cancels attack"),Zombie->IsAlive() || Zombie->IsAttacking());
			HealthBefore=Vitals->GetCurrentHealth();
			return Wait(.5);
		case 18:
			Test->TestEqual(TEXT("Dead zombie cannot apply pending damage"),Vitals->GetCurrentHealth(),HealthBefore);
			bManualCombat=false;
			SpawnZombie(FVector(0,-900,88));
			TeleportPlayer(FVector(200,-900,96));
			Player->Crouch();
			return Wait(.8);
		case 19:
			Test->TestTrue(TEXT("Crouched player is still visible from the front"),AI->HasVisualContact() && AI->IsAlerted());
			// Fully autonomous attack cycle must also run, including consecutive attacks.
			Player->UnCrouch();
			TeleportPlayer(FVector(100,-900,96));
			Zombie->Tuning.StrikeWindup=.1f; Zombie->Tuning.BiteWindup=.1f; Zombie->Tuning.RecoverySeconds=.2f;
			Vitals->Heal(100);
			return Wait(1.3);
		case 20:
			Test->TestTrue(TEXT("StateTree autonomously executes multiple attacks"),Vitals->GetCurrentHealth()<=72);
			Vitals->ApplyDamage(1000);
			return Wait(.5);
		case 21:
			Test->TestFalse(TEXT("Death of target cancels attacks"),Zombie->IsAttacking());
			Test->TestFalse(TEXT("Death of target clears alert"),AI->IsAlerted());
			return true;
		}
		return true;
	}
private:
	void TeleportPlayer(FVector Location)
	{
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->ClearControlIntents();
		Player->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(0,180,0));
	}
	void SpawnZombie(FVector Location)
	{
		if (Zombie.IsValid()) { if(Zombie->GetController()) Zombie->GetController()->Destroy(); Zombie->Destroy(); }
		const FTransform Transform(FRotator::ZeroRotator,Location);
		Zombie=World->SpawnActorDeferred<AZombieCharacter>(AZombieCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Zombie->Tuning.WanderRadius=0; Zombie->Tuning.WanderSpeed=0; Zombie->Tuning.PursuitSpeed=0;
		Zombie->Tuning.IdleSeconds=100;
		Zombie->FinishSpawning(Transform);
	}
	void Impact(float Strength)
	{
		FCombatImpactDamageEvent Event; Event.ImpactStrength=Strength;
		Zombie->TakeDamage(1,Event,PC,Player);
	}
	bool Wait(double Seconds) { ++Phase; WaitUntil=World->GetTimeSeconds()+Seconds; return false; }
	void Capture(const TCHAR* Filename)
	{
		if(FParse::Param(FCommandLine::Get(),TEXT("ZombieCapture")))
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ZombieVerification")/Filename),false,false);
	}
	FAutomationTestBase* Test;
	double Started,WaitUntil=0;
	int32 Phase=0,StepsBefore=0;
	bool bManualCombat=false,bCapturedAlert=false,bCheckedAttackWall=false;
	float HealthBefore=0;
	UWorld* World=nullptr;
	APlayerController* PC=nullptr;
	Aprototype3Character* Player=nullptr;
	TWeakObjectPtr<AZombieCharacter> Zombie;
	AStaticMeshActor* Wall=nullptr;
	FVector LastSeen,MoveStart;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombiePlayTest,"Prototype.Zombies.PlayIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombiePlayTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run verification in a fresh editor outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombiePIEScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif

