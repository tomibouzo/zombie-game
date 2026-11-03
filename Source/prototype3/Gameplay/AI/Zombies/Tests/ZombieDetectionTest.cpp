#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

class FZombieDetectionScenario : public IAutomationLatentCommand
{
public:
	explicit FZombieDetectionScenario(FAutomationTestBase* InTest) : Test(InTest),Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Detection PIE did not start within 30 seconds")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC = World ? World->GetFirstPlayerController() : nullptr;
		auto* Player = PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		for (TActorIterator<AZombieCharacter> It(World); It; ++It) It->Destroy();
		auto* Zombie = World->SpawnActor<AZombieCharacter>(FVector(0,-1300,88),FRotator::ZeroRotator);
		auto* AI = Cast<AZombieAIController>(Zombie->GetController());
		if (!Test->TestNotNull(TEXT("Detection has controller"),AI)) return true;
		AI->GetZombieStateTree()->StopLogic(TEXT("Isolated perception geometry"));
		AI->SetActorTickEnabled(false);
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->ClearControlIntents();
		Player->SetActorLocation(FVector(500,-1300,96),false,nullptr,ETeleportType::TeleportPhysics);
		Test->TestTrue(TEXT("Unobstructed player ahead is visible"),AI->CanSee(Player));
		AI->Tick(.1f);
		Test->TestTrue(TEXT("First sight does not wait for a perception notification"),AI->HasVisualContact());
		auto* Wall = World->SpawnActor<AStaticMeshActor>(FVector(250,-1300,75),FRotator::ZeroRotator);
		auto* Mesh = Wall->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(.2,4,1.5));
		Test->TestTrue(TEXT("Visible head above low cover can be detected"),AI->CanSee(Player));
		Wall->SetActorScale3D(FVector(.2,4,4));
		Test->TestFalse(TEXT("Full cover blocks every sight sample"),AI->CanSee(Player));
		Wall->Destroy();
		Player->SetActorLocation(FVector(-300,-1300,96));
		Test->TestFalse(TEXT("Close player behind is outside field of view"),AI->CanSee(Player));
		Player->SetActorLocation(FVector(1300,-1300,96));
		Test->TestFalse(TEXT("Player beyond configured radius is unseen"),AI->CanSee(Player));
		Player->SetActorLocation(FVector(0,-800,96));
		Test->TestFalse(TEXT("Player to side is outside field of view"),AI->CanSee(Player));
		Player->SetActorLocation(FVector(1100,-1300,96));
		AI->NotifyDamaged(Player);
		Zombie->Suspicion=0; AI->Tick(.1f);
		const float FarSuspicion=Zombie->Suspicion;
		Player->SetActorLocation(FVector(600,-1300,96));
		Zombie->Suspicion=0; AI->Tick(.1f);
		const float MidSuspicion=Zombie->Suspicion;
		Player->SetActorLocation(FVector(200,-1300,96));
		Zombie->Suspicion=0; AI->Tick(.1f);
		Test->TestTrue(TEXT("Detection speeds up smoothly with proximity"),Zombie->Suspicion>MidSuspicion && MidSuspicion>FarSuspicion);
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieDetectionTest,"Prototype.Zombies.DetectionGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombieDetectionTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run detection test outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieDetectionScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif

