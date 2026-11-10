#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Editor.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Build/ShelterBuildComponent.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/Inventory/InventoryInputSettings.h"
#include "UI/HUD/HorrorHUD.h"

/**
 * Plays the build laboratory as saved, with the player's saved bindings, and drives build mode
 * through the controller's real input entry point. Catches problems the isolated checks cannot:
 * a missing build area in the map, undiscovered piece assets, or bindings lost on load.
 */
class FShelterBuildLabPlayCheck : public IAutomationLatentCommand
{
public:
	explicit FShelterBuildLabPlayCheck(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* Controller = World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
		auto* Player = Controller ? Cast<Aprototype3Character>(Controller->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay() || FPlatformTime::Seconds() - Started < 2)
		{
			if (FPlatformTime::Seconds() - Started < 60) return false;
			APlayerController* Any = World ? World->GetFirstPlayerController() : nullptr;
			Test->AddError(FString::Printf(TEXT("Play did not create the expected player within 60 seconds. World=%s controller=%s pawn=%s begun=%d"),
				*GetNameSafe(World), Any ? *Any->GetClass()->GetPathName() : TEXT("none"),
				Any && Any->GetPawn() ? *Any->GetPawn()->GetClass()->GetPathName() : TEXT("none"), Player && Player->HasActorBegunPlay()));
			return true;
		}
		using A = EInventoryControl;
		const auto* Controls = GetDefault<UInventoryInputSettings>();
		const FKey Menu = Controls->GetKey(A::BuildMenu), Next = Controls->GetKey(A::BuildNext), Exit = Controls->GetKey(A::BuildExit);
		Test->AddInfo(FString::Printf(TEXT("Bindings: menu=%s previous=%s next=%s rotate=%s exit=%s"), *Menu.ToString(),
			*Controls->GetKey(A::BuildPrevious).ToString(), *Next.ToString(), *Controls->GetKey(A::BuildRotate).ToString(), *Exit.ToString()));
		Test->TestTrue(TEXT("Build menu has a key with the saved bindings"), Menu.IsValid());
		Test->TestTrue(TEXT("Next piece has a key with the saved bindings"), Next.IsValid());
		Test->TestTrue(TEXT("Exit build mode has a key with the saved bindings"), Exit.IsValid());
		Test->TestNotNull(TEXT("The lab uses the HUD that draws the build panel"), Cast<AHorrorHUD>(Controller->GetHUD()));
		UShelterBuildComponent* Build = Controller->GetBuildComponent();
		if (!Test->TestNotNull(TEXT("The played controller has a build component"), Build)) return true;
		AShelterBuildArea* Area = AShelterBuildArea::FindContaining(World, Player->GetActorLocation());
		Test->AddInfo(FString::Printf(TEXT("Player at %s; local controller=%d; paused=%d"), *Player->GetActorLocation().ToCompactString(),
			Controller->IsLocalController(), Controller->IsPaused()));
		if (!Test->TestNotNull(TEXT("The player starts inside the lab's build area"), Area)) return true;
		auto Tap = [&](FKey Key)
		{
			const bool bConsumed = Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
			return bConsumed;
		};
		Test->TestTrue(TEXT("The build menu key is consumed"), Tap(Menu));
		Test->AddInfo(FString::Printf(TEXT("Categories discovered: %d"), Build->GetCategories().Num()));
		Test->TestTrue(TEXT("The lab offers wall, doorway and window categories"), Build->GetCategories().Num() >= 3);
		if (!Test->TestTrue(TEXT("The build menu key opens the category menu in play"), Build->GetState() == EShelterBuildState::Menu)) return true;
		Tap(Controls->GetKey(A::Primary));
		Test->TestTrue(TEXT("The place button enters placement"), Build->GetState() == EShelterBuildState::Placing);
		Controller->SetControlRotation(FRotator(-40, Player->GetActorRotation().Yaw, 0));
		Controller->PlayerCameraManager->UpdateCamera(0.f);
		Build->RefreshTarget();
		Test->AddInfo(TEXT("Target status: ") + Build->GetStatus());
		Test->TestTrue(TEXT("Looking at the floor ahead gives a buildable slot"), Build->HasValidTarget());
		Tap(Controls->GetKey(A::Primary));
		Test->TestEqual(TEXT("A piece is built in play"), Area->GetModel().Placements.Num(), 1);
		Tap(Exit);
		Test->TestFalse(TEXT("The exit key leaves build mode"), Build->IsActive());
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterBuildLabPlayTest, "Prototype.ShelterPlay.BuildLab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterBuildLabPlayTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ShelterPIE")))
	{ AddInfo(TEXT("Opt in with -ShelterPIE to play the build lab.")); return true; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Shelter/Lvl_BuildLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FShelterBuildLabPlayCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
