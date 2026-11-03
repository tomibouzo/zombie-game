#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Combat/CombatImpact.h"
#include "Core/Characters/prototype3Character.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

class FZombiePresentationScenario : public IAutomationLatentCommand
{
public:
	explicit FZombiePresentationScenario(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Zombie presentation timed out")); return true; }
		UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC=World ? World->GetFirstPlayerController() : nullptr;
		auto* Player=PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		if (Zombie.IsValid()) Zombie->TickAttack();
		if (World->GetTimeSeconds()<WaitUntil) return false;
		if (Phase==0)
		{
			for (TActorIterator<AZombieCharacter> It(World);It;++It) It->Destroy();
			Player->SetActorLocation(FVector(100,-1200,96));
			Zombie=World->SpawnActor<AZombieCharacter>(FVector(0,-1200,88),FRotator::ZeroRotator);
			if (!Test->TestTrue(TEXT("Presentation zombie spawned"),Zombie.IsValid())) return true;
			auto* AI=Cast<AZombieAIController>(Zombie->GetController());
			if (!Test->TestNotNull(TEXT("Presentation controller spawned"),AI)) return true;
			AI->GetZombieStateTree()->StopLogic(TEXT("Presentation scenario")); AI->SetActorTickEnabled(false);
			auto* Camera=World->SpawnActor<ACameraActor>(FVector(330,-1570,180),FRotator::ZeroRotator);
			if (!Test->TestNotNull(TEXT("Presentation camera spawned"),Camera)) return true;
			Camera->SetActorRotation((FVector(0,-1200,100)-Camera->GetActorLocation()).Rotation());
			PC->SetViewTarget(Camera);
			Zombie->bUseMannequin=false; Zombie->RefreshAppearance();
			Test->TestFalse(TEXT("Geometry fallback hides skeletal mesh"),Zombie->GetMesh()->IsVisible());
			Zombie->bUseMannequin=true; Zombie->RefreshAppearance();
			Test->TestTrue(TEXT("Humanoid can be restored"),Zombie->GetMesh()->IsVisible());
			Test->TestTrue(TEXT("Presented strike starts"),Zombie->BeginAttack(Player));
			return Wait(World,.35);
		}
		if (!Test->TestTrue(TEXT("Presentation zombie remains valid"),Zombie.IsValid())) return true;
		auto* Instance=Zombie->GetMesh()->GetSingleNodeInstance();
		if (!Test->TestNotNull(TEXT("Presentation has animation instance"),Instance)) return true;
		if (!Test->TestNotNull(TEXT("Presentation has animation asset"),Instance->GetCurrentAsset())) return true;
		if (Phase==1)
		{
			Test->TestTrue(TEXT("Windup displays first strike"),Instance->GetCurrentAsset()->GetName()==TEXT("MM_Attack_01"));
			Test->TestTrue(TEXT("Attack presentation advances"),Instance->GetCurrentTime()>0);
			Test->TestTrue(TEXT("Authored root motion stays at capsule"),FVector::Dist2D(Zombie->GetMesh()->GetSocketLocation(TEXT("root")),Zombie->GetActorLocation())<15);
			Capture(TEXT("mannequin-windup.png"));
			return Wait(World,.55);
		}
		if (Phase==2)
		{
			FCombatImpactDamageEvent Impact; Impact.ImpactStrength=100;
			Zombie->TakeDamage(1,Impact,PC,Player);
			Zombie->RefreshAppearance();
			Test->TestFalse(TEXT("Interruption cancels presentation's attack"),Zombie->IsAttacking());
			Test->TestTrue(TEXT("Interrupted strike returns to idle"),Instance->GetCurrentAsset()->GetName()==TEXT("MM_Idle"));
			return Wait(World,.8);
		}
		if (Phase==3)
		{
			Zombie->TakeDamage(1000,FDamageEvent(),PC,Player);
			return Wait(World,.7);
		}
		if (Phase==4)
		{
			Test->TestTrue(TEXT("Death has its own animation"),Instance->GetCurrentAsset()->GetName()==TEXT("MM_Death_Front_01"));
			Capture(TEXT("mannequin-death.png"));
			return Wait(World,.3);
		}
		return true;
	}
private:
	bool Wait(UWorld* World,double Seconds) { ++Phase; WaitUntil=World->GetTimeSeconds()+Seconds; return false; }
	void Capture(const TCHAR* Filename)
	{
		if (FParse::Param(FCommandLine::Get(),TEXT("ZombieCapture")))
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ZombieVerification")/Filename),false,false);
	}
	FAutomationTestBase* Test;
	double Started,WaitUntil=0;
	int32 Phase=0;
	TWeakObjectPtr<AZombieCharacter> Zombie;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombiePresentationTest,"Prototype.Zombies.Presentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombiePresentationTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run presentation outside Play.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombiePresentationScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
