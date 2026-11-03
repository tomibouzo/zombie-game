#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Core/Characters/prototype3Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavDataGenerator.h"
#include "Misc/Paths.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

// Exercise the placed, moving zombie from a cold Play session. The combat test
// separately tests impact rules with movement disabled and cannot cover this.
class FZombieChaseRecoveryScenario : public IAutomationLatentCommand
{
public:
	explicit FZombieChaseRecoveryScenario(FAutomationTestBase* InTest)
		: Test(InTest), Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 60)
		{ Test->AddError(TEXT("Chase recovery scenario timed out")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC = World ? World->GetFirstPlayerController() : nullptr;
		auto* Player = PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		const double Now = World->GetTimeSeconds();
		if (Phase == 0)
		{
			for (TActorIterator<AZombieCharacter> It(World); It; ++It) { Zombie = *It; break; }
			if (!Test->TestTrue(TEXT("Lab contains its placed zombie"),Zombie.IsValid())) return true;
			for (TActorIterator<AInventoryTestSpikes> It(World); It; ++It) It->Destroy();
			for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
				Test->AddInfo(TEXT("Lab nav bounds: ")+It->GetComponentsBoundingBox(true).ToString());
			if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				for (const auto& Bounds : Nav->GetNavigationBounds()) Test->AddInfo(TEXT("Registered nav bounds: ")+Bounds.AreaBox.ToString());
			}
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
				Test->AddInfo(TEXT("Geometry: ")+It->GetName()+TEXT(" ")+It->GetComponentsBoundingBox(true).ToString());
			// Clear ground, away from the laboratory obstacles. Do not wait for nav
			// readiness or replace the placed actor with a warmed-up spawned one.
			Zombie->SetActorLocation(FVector(-700,-1200,88),false,nullptr,ETeleportType::TeleportPhysics);
			Zombie->SetActorRotation(FRotator::ZeroRotator);
			PlaceAhead(Player,350);
			StartPosition = Zombie->GetActorLocation();
			PhaseStart = Now; Deadline = Now + 20; Phase = 1;
		}
		auto* AI = Cast<AZombieAIController>(Zombie->GetController());
		if (!Test->TestNotNull(TEXT("Placed zombie has an AI controller"),AI)) return true;
		if (Now >= NextSample)
		{
			Test->AddInfo(FString::Printf(TEXT("t=%.2f phase=%d round=%d state=%d desired=%d sight=%d alert=%d attack=%d move=%d speed=%.1f distance=%.1f tree=%d"),
				Now,Phase,Round,static_cast<int32>(Zombie->Behavior),static_cast<int32>(AI->GetDesiredBehavior()),
				AI->HasVisualContact(),AI->IsAlerted(),Zombie->IsAttacking(),static_cast<int32>(AI->GetMoveStatus()),
				Zombie->GetVelocity().Size2D(),FVector::Dist2D(Zombie->GetActorLocation(),Player->GetActorLocation()),AI->GetZombieStateTree()->IsRunning()));
			NextSample = Now + .5;
			auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation Probe;
			const bool bAtPlayer = Nav && Nav->ProjectPointToNavigation(Player->GetActorLocation(),Probe,FVector(100,100,250));
			const bool bAtCenter = Nav && Nav->ProjectPointToNavigation(FVector(0,-900,0),Probe,FVector(100,100,250));
			Test->AddInfo(FString::Printf(TEXT("Nav availability: building=%d locked=%d player=%d central=%d"),
				Nav && Nav->IsNavigationBuildInProgress(),Nav && Nav->IsNavigationBuildingLocked(),bAtPlayer,bAtCenter));
		}
		if (Phase == 1)
		{
			if (FirstMove < 0 && FVector::Dist2D(StartPosition,Zombie->GetActorLocation()) > 30) FirstMove = Now-PhaseStart;
			if (Zombie->IsAttacking())
			{
				Test->TestTrue(TEXT("Cold-session visible approach starts promptly"),FirstMove >= 0 && FirstMove < 2);
				BeginRetreat(Player,Now); return false;
			}
		}
		else if (Phase == 2)
		{
			if (FVector::Dist2D(StartPosition,Zombie->GetActorLocation()) > 80)
			{
				Test->AddInfo(FString::Printf(TEXT("Round %d resumed pursuit after %.2f seconds"),Round,Now-PhaseStart));
				Test->TestEqual(TEXT("Retreat returns to chase"),Zombie->Behavior,EZombieBehavior::Chase);
				if (++Round == 4) return true;
				PlaceAhead(Player,105);
				Deadline = Now + 3; Phase = 3;
				return false;
			}
		}
		else if (Phase == 3 && Zombie->IsAttacking())
		{
			BeginRetreat(Player,Now); return false;
		}
		if (Now > Deadline)
		{
			auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (auto* Recast = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance()) : nullptr)
			{
				Test->AddInfo(TEXT("Built nav bounds: ")+Recast->GetNavMeshBounds().ToString());
				if (auto* Generator = Recast->GetGenerator()) Generator->ExportNavigationData(FPaths::ProjectSavedDir()/TEXT("ZombieVerification/ChaseGeometry"));
			}
			FNavLocation Ground;
			const bool bProjected = Nav && Nav->ProjectPointToNavigation(Player->GetActorLocation(),Ground,FVector(100,100,250));
			auto* CenterPath = UNavigationSystemV1::FindPathToLocationSynchronously(World,AI->GetNavAgentLocation(),Player->GetActorLocation(),Zombie.Get());
			auto* GroundPath = bProjected ? UNavigationSystemV1::FindPathToLocationSynchronously(World,AI->GetNavAgentLocation(),Ground.Location,Zombie.Get()) : nullptr;
			Test->AddInfo(FString::Printf(TEXT("Navigation diagnostic: nav=%d projected=%d player=%s feet=%s navGoal=%s centerPath=%d groundPath=%d"),
				Nav!=nullptr,bProjected,*Player->GetActorLocation().ToString(),*AI->GetNavAgentLocation().ToString(),*Ground.Location.ToString(),
				CenterPath && CenterPath->IsValid(),GroundPath && GroundPath->IsValid()));
			for (float Y : {-1500.f,-900.f,0.f,900.f,1500.f})
				for (float X : {-1500.f,-700.f,0.f,700.f,1500.f})
				{
					FNavLocation Sample;
					const bool bCovered = Nav && Nav->ProjectPointToNavigation(FVector(X,Y,0),Sample,FVector(60,60,250));
					Test->AddInfo(FString::Printf(TEXT("Nav ground %.0f %.0f: %d"),X,Y,bCovered));
				}
			Test->AddError(FString::Printf(TEXT("Zombie failed to resume its encounter: phase %d, round %d, moved %.1f cm"),Phase,Round,
				FVector::Dist2D(StartPosition,Zombie->GetActorLocation())));
			return true;
		}
		return false;
	}
private:
	void PlaceAhead(Aprototype3Character* Player,float Distance)
	{
		Player->ClearControlIntents();
		Player->GetCharacterMovement()->StopMovementImmediately();
		FVector Position = Zombie->GetActorLocation()+Zombie->GetActorForwardVector()*Distance;
		Position.Z = 96;
		Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
	}
	void BeginRetreat(Aprototype3Character* Player,double Now)
	{
		PlaceAhead(Player,450);
		StartPosition = Zombie->GetActorLocation();
		PhaseStart = Now;
		const auto& T = Zombie->Tuning;
		// Allow the full intentional attack pause plus time to walk 80 cm.
		Deadline = Now + FMath::Max(T.StrikeWindup,T.BiteWindup)+T.RecoverySeconds+1;
		Phase = 2;
	}
	FAutomationTestBase* Test;
	double Started,Deadline=0,PhaseStart=0,NextSample=0,FirstMove=-1;
	int32 Phase=0,Round=0;
	FVector StartPosition;
	TWeakObjectPtr<AZombieCharacter> Zombie;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieChaseRecoveryTest,"Prototype.Zombies.ChaseRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombieChaseRecoveryTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run chase recovery outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieChaseRecoveryScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
