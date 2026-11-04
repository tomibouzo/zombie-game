#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Core/Characters/prototype3Character.h"
#include "Core/Settings/PrototypeDebugSettings.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
#include "Gameplay/AI/Zombies/ZombieAIController.h"
#include "Gameplay/Player/PlayerNoiseComponent.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/Pause/SPauseMenu.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"

namespace ZombieDebugTest
{
struct FRestorePreference
{
	bool bPrevious = GetDefault<UPrototypeDebugSettings>()->bInvisibleToZombies;
	FRestorePreference()
	{
		auto* Settings = GetMutableDefault<UPrototypeDebugSettings>();
		Settings->bInvisibleToZombies = false;
		Settings->SaveConfig();
	}
	~FRestorePreference()
	{
		auto* Settings = GetMutableDefault<UPrototypeDebugSettings>();
		Settings->bInvisibleToZombies = bPrevious;
		Settings->SaveConfig();
	}
};
void CheckSavedChoice(FAutomationTestBase& Test, bool bExpected)
{
	// Read disk directly rather than accepting an in-memory config cache update.
	const FConfigBranch* Branch = GConfig->FindBranch(FName(TEXT("GameUserSettings")),GGameUserSettingsIni);
	if (!Test.TestNotNull(TEXT("User settings config branch exists"),Branch)) return;
	FConfigFile Saved;
	Saved.Read(Branch->IniPath);
	bool bActual = !bExpected;
	Test.TestTrue(TEXT("Debug preference is written to user settings on disk"),
		Saved.GetBool(TEXT("/Script/prototype3.PrototypeDebugSettings"),TEXT("bInvisibleToZombies"),bActual));
	Test.TestEqual(TEXT("Saved debug choice matches menu"),bActual,bExpected);
}
bool ContainsLabel(const TSharedRef<SWidget>& Widget)
{
	if (Widget->GetType() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == TEXT("Invisible to zombies")) return true;
	for (int32 I=0; I<Widget->GetChildren()->Num(); ++I)
		if (ContainsLabel(Widget->GetChildren()->GetChildAt(I))) return true;
	return false;
}
TSharedPtr<SCheckBox> FindToggle(const TSharedRef<SWidget>& Widget)
{
	if (Widget->GetType() == TEXT("SCheckBox") && ContainsLabel(Widget)) return StaticCastSharedRef<SCheckBox>(Widget);
	for (int32 I=0; I<Widget->GetChildren()->Num(); ++I)
		if (auto Toggle=FindToggle(Widget->GetChildren()->GetChildAt(I))) return Toggle;
	return nullptr;
}
}

class FZombieDebugScenario : public IAutomationLatentCommand
{
public:
	FZombieDebugScenario(FAutomationTestBase* InTest, TSharedRef<ZombieDebugTest::FRestorePreference> InPreference)
		: Test(InTest), Started(FPlatformTime::Seconds()), Preference(InPreference) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Debug test startup timed out.")); return true; }
		UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC=World ? World->GetFirstPlayerController() : nullptr;
		auto* Player=PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		auto* Zombie=World->SpawnActor<AZombieCharacter>(FVector(0,-1300,90),FRotator::ZeroRotator);
		auto* AI=Zombie ? Cast<AZombieAIController>(Zombie->GetController()) : nullptr;
		if (!Test->TestNotNull(TEXT("Real zombie controller"),AI)) return true;
		Player->TeleportTo(FVector(100,-1300,98),FRotator::ZeroRotator,false,true);
		Player->GetCharacterMovement()->StopMovementImmediately();
		Test->TestFalse(TEXT("Saved off choice applies at startup"),Player->IsDebugInvisibleToZombies());
		Test->TestTrue(TEXT("Normal player visible"),AI->CanSee(Player));
		AI->NotifyDamaged(Player); AI->Tick(1);
		Test->TestTrue(TEXT("Normal player can be acquired"),AI->IsAlerted());
		Test->TestTrue(TEXT("Normal attack can start"),Zombie->BeginAttack(Player));
		const auto Menu=SNew(SPauseMenu).DebugPlayer(Player).SaveControls(false);
		Menu->ShowOptions(); Menu->ShowCategory(EOptionsCategory::Debug);
		const auto Toggle=ZombieDebugTest::FindToggle(Menu);
		if (!Test->TestTrue(TEXT("Debug category has invisibility toggle"),Toggle.IsValid())) return true;
		const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(1100,760),FSlateLayoutTransform());
		const FKeyEvent Accept(EKeys::Enter,FModifierKeysState(),0,false,0,0);
		auto Click=[&] { Toggle->OnKeyDown(Geometry,Accept); Toggle->OnKeyUp(Geometry,Accept); };
		Click();
		Test->TestTrue(TEXT("Menu enables debug mode immediately"),Player->IsDebugInvisibleToZombies());
		ZombieDebugTest::CheckSavedChoice(*Test,true);
		Test->TestFalse(TEXT("Active attack cancelled"),Zombie->IsAttacking());
		Test->TestFalse(TEXT("Existing pursuit forgotten"),AI->IsAlerted());
		Test->TestFalse(TEXT("Debug player invisible to sight"),AI->CanSee(Player));
		Test->TestFalse(TEXT("Debug player cannot be struck"),Zombie->CanHit(Player));
		AI->NotifyDamaged(Player); AI->Tick(1);
		Test->TestFalse(TEXT("Attacking while invisible does not reacquire"),AI->IsAlerted());
		const float Health=Player->GetPlayerVitalsComponent()->GetCurrentHealth();
		FZombieDamageEvent Damage;
		Test->TestEqual(TEXT("Zombie damage blocked"),Player->TakeDamage(8,Damage,AI,Zombie),0.f);
		Test->TestEqual(TEXT("Health preserved"),Player->GetPlayerVitalsComponent()->GetCurrentHealth(),Health);
		auto* Noise=Player->FindComponentByClass<UPlayerNoiseComponent>();
		const int32 Steps=Noise->EmittedSteps;
		for (int32 I=0; I<4; ++I)
		{
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Player->AddActorWorldOffset(FVector(50,0,0));
			Noise->TickComponent(.1f,LEVELTICK_All,nullptr);
		}
		Test->TestEqual(TEXT("Invisible walking emits no hearing clues"),Noise->EmittedSteps,Steps);
		Click();
		Test->TestFalse(TEXT("Same menu toggle disables debug mode"),Player->IsDebugInvisibleToZombies());
		ZombieDebugTest::CheckSavedChoice(*Test,false);
		Test->TestTrue(TEXT("Normal sight returns"),AI->CanSee(Player));
		Test->TestEqual(TEXT("Normal zombie damage returns"),Player->TakeDamage(8,Damage,AI,Zombie),8.f);
		Test->TestFalse(TEXT("Debug choice does not create unsaved control preferences"),Menu->HasUnsavedChanges());
		// Leave a saved on choice for a fresh Play world, after reloading it from config.
		Click();
		auto* Settings = GetMutableDefault<UPrototypeDebugSettings>();
		Settings->bInvisibleToZombies = false;
		Settings->ReloadConfig();
		Test->TestTrue(TEXT("Enabled choice reloads from saved configuration"),Settings->bInvisibleToZombies);
		return true;
	}
private:
	FAutomationTestBase* Test;
	double Started;
	TSharedRef<ZombieDebugTest::FRestorePreference> Preference;
};

class FZombieDebugStartupScenario : public IAutomationLatentCommand
{
public:
	FZombieDebugStartupScenario(FAutomationTestBase* InTest, bool bInExpected,
		TSharedRef<ZombieDebugTest::FRestorePreference> InPreference)
		: Test(InTest), bExpected(bInExpected), Preference(InPreference) {}
	bool Update() override
	{
		if (!Started) Started = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Debug restart timed out.")); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		auto* PC = World ? World->GetFirstPlayerController() : nullptr;
		auto* Player = PC ? Cast<Aprototype3Character>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->HasActorBegunPlay()) return false;
		Test->TestEqual(TEXT("Fresh player restores saved debug choice"),Player->IsDebugInvisibleToZombies(),bExpected);
		const auto Menu = SNew(SPauseMenu).DebugPlayer(Player).SaveControls(false);
		Menu->ShowOptions(); Menu->ShowCategory(EOptionsCategory::Debug);
		const auto Toggle = ZombieDebugTest::FindToggle(Menu);
		if (!Test->TestTrue(TEXT("Reopened Debug menu has toggle"),Toggle.IsValid())) return true;
		Test->TestEqual(TEXT("Reopened toggle reflects restored choice"),Toggle->IsChecked(),bExpected);
		if (bExpected)
		{
			const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(1100,760),FSlateLayoutTransform());
			const FKeyEvent Accept(EKeys::Enter,FModifierKeysState(),0,false,0,0);
			Toggle->OnKeyDown(Geometry,Accept); Toggle->OnKeyUp(Geometry,Accept);
			ZombieDebugTest::CheckSavedChoice(*Test,false);
			auto* Settings = GetMutableDefault<UPrototypeDebugSettings>();
			Settings->bInvisibleToZombies = true;
			Settings->ReloadConfig();
			Test->TestFalse(TEXT("Disabled choice reloads from saved configuration"),Settings->bInvisibleToZombies);
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	bool bExpected;
	TSharedRef<ZombieDebugTest::FRestorePreference> Preference;
	double Started = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZombieDebugTest,"Prototype.Encounter.DebugInvisibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FZombieDebugTest::RunTest(const FString&)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Run debug test outside Play.")); return false; }
	const auto Preference = MakeShared<ZombieDebugTest::FRestorePreference>();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Zombies/Lvl_ZombieLab")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieDebugScenario(this,Preference));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieDebugStartupScenario(this,true,Preference));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FZombieDebugStartupScenario(this,false,Preference));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
