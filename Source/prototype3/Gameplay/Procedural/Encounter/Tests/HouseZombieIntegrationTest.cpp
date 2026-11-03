#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

class FHouseZombieScenario : public IAutomationLatentCommand
{
public:
	explicit FHouseZombieScenario(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started > 60) { Test->AddError(TEXT("House zombie integration timed out.")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC = World ? World->GetFirstPlayerController() : nullptr;
		auto* Player = PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (PC && PC->GetPawn() && !Player)
		{ Test->AddError(TEXT("Unexpected encounter player class: ")+PC->GetPawn()->GetClass()->GetPathName()); return true; }
		if (!Player || !Player->HasActorBegunPlay()) return false;
		if (!Zombie.IsValid())
		{
			AHouseEncounter* Encounter = nullptr;
			for (TActorIterator<AHouseEncounter> It(World); It; ++It) { Encounter = *It; break; }
			if (!Encounter) { Test->AddError(TEXT("Missing house encounter.")); return true; }
			if (World->GetTimeSeconds() >= NextSample)
			{
				auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				Test->AddInfo(FString::Printf(TEXT("Encounter startup t=%.2f state=%d navbuilding=%d player=%s"),World->GetTimeSeconds(),static_cast<int32>(Encounter->State),Nav && Nav->IsNavigationBuildInProgress(),*Player->GetActorLocation().ToString()));
				NextSample = World->GetTimeSeconds()+1;
			}
			if (Encounter->State == EHouseEncounterState::Failed) { Test->AddError(Encounter->LastError); return true; }
			if (Encounter->State != EHouseEncounterState::Active) return false;
			if (!Test->TestTrue(TEXT("Saved map spawns zombies"), !Encounter->Zombies.IsEmpty())) return true;
			for (ACharacter* Actor : Encounter->Zombies)
			{
				auto* NPC = Cast<AZombieCharacter>(Actor);
				auto* AI = NPC ? Cast<AZombieAIController>(NPC->GetController()) : nullptr;
				if (!Test->TestTrue(TEXT("Real living zombie with running behavior tree"), NPC && NPC->IsAlive() && AI && AI->GetZombieStateTree()->IsRunning())) return true;
				auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				auto* Path = Nav->FindPathToLocationSynchronously(World,NPC->GetActorLocation(),Player->GetActorLocation(),NPC);
				Test->TestTrue(TEXT("Spawned zombie can path toward entrance player"),Path && Path->IsValid() && !Path->IsPartial());
			}
			// Find standing space in sight of an actual spawned NPC; preserve normal sensing and movement behavior.
			for (ACharacter* Actor : Encounter->Zombies)
			{
				auto* NPC = Cast<AZombieCharacter>(Actor);
				for (FVector Offset : {FVector(250,0,0),FVector(-250,0,0),FVector(0,250,0),FVector(0,-250,0)})
				{
					FVector Feet = NPC->GetActorLocation()+Offset;
					Feet.Z = Encounter->Placement.PlayerFeet.Z;
					if (!HouseEncounterPlacement::ClearCapsule(World,Feet,34,96,Player)) continue;
					auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
					FNavLocation Projected;
					if (!Nav->ProjectPointToNavigation(Feet,Projected,FVector(12,12,25))) continue;
					Player->TeleportTo(Feet+FVector(0,0,98),FRotator::ZeroRotator,false,true);
					Player->GetCharacterMovement()->StopMovementImmediately();
					NPC->SetActorRotation((Player->GetActorLocation()-NPC->GetActorLocation()).Rotation());
					auto* AI = Cast<AZombieAIController>(NPC->GetController());
					if (!AI->CanSee(Player)) continue;
					AI->NotifyDamaged(Player);
					Zombie = NPC; StartPosition = NPC->GetActorLocation();
					Test->AddInfo(TEXT("Movement check start: ")+StartPosition.ToString()+TEXT(" player: ")+Player->GetActorLocation().ToString());
					return false;
				}
			}
			Test->AddError(TEXT("No clear visible player position for movement check.")); return true;
		}
		if (World->GetTimeSeconds() >= NextSample)
		{
			auto* AI = Cast<AZombieAIController>(Zombie->GetController());
			Test->AddInfo(FString::Printf(TEXT("House movement t=%.2f state=%d desired=%d visible=%d alert=%d tree=%d move=%d distance=%.1f velocity=%.1f delta=%.1f"),
				World->GetTimeSeconds(),static_cast<int32>(Zombie->Behavior),static_cast<int32>(AI->GetDesiredBehavior()),AI->HasVisualContact(),AI->IsAlerted(),AI->GetZombieStateTree()->IsRunning(),static_cast<int32>(AI->GetMoveStatus()),
				FVector::Dist2D(Zombie->GetActorLocation(),Player->GetActorLocation()),Zombie->GetVelocity().Size2D(),FVector::Dist2D(StartPosition,Zombie->GetActorLocation())));
			NextSample = World->GetTimeSeconds()+1;
		}
		if (FVector::Dist2D(StartPosition,Zombie->GetActorLocation()) > 40)
		{
			Test->AddInfo(TEXT("Real zombie moved more than 40 cm through the generated furnished house."));
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	double NextSample = 0;
	TWeakObjectPtr<AZombieCharacter> Zombie;
	FVector StartPosition;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseZombieIntegrationTest,"Prototype.Encounter.RealZombiePlayIntegration",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHouseZombieIntegrationTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run house zombie integration outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Procedural/Lvl_HouseEncounter")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHouseZombieScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
