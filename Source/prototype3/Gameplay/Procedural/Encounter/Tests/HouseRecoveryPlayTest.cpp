#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformTime.h"

namespace HouseRecoveryPlay
{
bool HasText(const TSharedRef<SWidget>& Widget,const FString& Text)
{
 if(Widget->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==Text) return true;
 for(int32 I=0;I<Widget->GetChildren()->Num();++I) if(HasText(Widget->GetChildren()->GetChildAt(I),Text)) return true;
 return false;
}
TSharedPtr<SButton> FindButton(const TSharedRef<SWidget>& Widget,const FString& Text)
{
 if(Widget->GetType()==TEXT("SButton") && HasText(Widget,Text)) return StaticCastSharedRef<SButton>(Widget);
 for(int32 I=0;I<Widget->GetChildren()->Num();++I) if(auto Button=FindButton(Widget->GetChildren()->GetChildAt(I),Text)) return Button;
 return nullptr;
}
class FSetup : public IAutomationLatentCommand
{
 bool Update() override
 {
  for(TActorIterator<AHouseEncounter> It(GEditor->GetEditorWorldContext().World());It;++It) It->bStartOnBeginPlay=false;
  return true; // Test-world setup only, never save the authored map.
 }
};
class FScenario : public IAutomationLatentCommand
{
public:
 explicit FScenario(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if(!Started) Started=FPlatformTime::Seconds();
  if(FPlatformTime::Seconds()-Started>60) { Test->AddError(TEXT("Recovery UI timed out.")); return true; }
  UWorld* World=GEditor ? GEditor->PlayWorld : nullptr;
  auto* PC=World ? Cast<Aprototype3PlayerController>(World->GetFirstPlayerController()) : nullptr;
  auto* Player=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
  if(!Player || !Player->HasActorBegunPlay()) return false;
  if(Stage==0)
  {
   for(TActorIterator<AHouseEncounter> It(World);It;++It) { Encounter=*It; break; }
   if(!Test->TestTrue(TEXT("Test encounter exists"),Encounter.IsValid())) return true;
   Encounter->HouseSettings=FHouseGenerationSettings(); Encounter->ZombieCount=1; Encounter->ItemCount=3;
   Blocker=World->SpawnActor<AActor>();
   auto* Box=NewObject<UBoxComponent>(Blocker.Get()); Blocker->SetRootComponent(Box);
   Box->SetBoxExtent(FVector(2000,2000,110)); Box->SetCollisionProfileName(TEXT("BlockAll"));
   Box->SetWorldLocation(FVector(600,500,100)); Box->RegisterComponent();
   if(!Test->TestTrue(TEXT("UI request starts"),Encounter->Prepare(Player))) return true;
   Encounter->SetActorTickEnabled(true);
   Test->TestTrue(TEXT("Preparation owns movement/look focus"),PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
   auto Widget=PC->GetHouseGenerationWidgetForTest();
   if(!Test->TestTrue(TEXT("Preparation is visible in viewport"),Widget.IsValid())) return true;
   Test->TestTrue(TEXT("Prompt provides End Play"),FindButton(Widget.ToSharedRef(),TEXT("End Play")).IsValid());
   Stage=1; return false;
  }
  if(Stage==1)
  {
   if(Encounter->State!=EHouseEncounterState::Failed) return false;
   Test->TestEqual(TEXT("UI exhausts bounded automatic recovery"),Encounter->LayoutAttempts,4);
   auto Widget=PC->GetHouseGenerationWidgetForTest();
   if(!Test->TestTrue(TEXT("Failure keeps a visible prompt"),Widget.IsValid())) return true;
   auto Button=FindButton(Widget.ToSharedRef(),TEXT("Try another seed"));
   if(Button.IsValid()) Button->UpdateAllAttributes();
   if(!Test->TestTrue(TEXT("Failure enables retry button"),Button.IsValid() && Button->IsEnabled())) return true;
   Blocker->Destroy();
   const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(900,700),FSlateLayoutTransform());
   const FKeyEvent Enter(EKeys::Enter,FModifierKeysState(),0,false,0,0);
   Button->OnKeyDown(Geometry,Enter); Button->OnKeyUp(Geometry,Enter);
   Test->TestEqual(TEXT("Real button starts preparation"),Encounter->State,EHouseEncounterState::Preparing);
   Test->TestFalse(TEXT("Retry is rejected while preparing"),Encounter->CanRetryGeneration());
   RetryButton=Button;
   Stage=2; return false;
  }
  if(!bCheckedButton)
  {
   // NullRHI does not paint the viewport: explicitly evaluate the same bound attributes that normal Slate updates.
   RetryButton->UpdateAllAttributes();
   Test->TestFalse(TEXT("Retry button disables on the next UI frame"),RetryButton->IsEnabled());
   bCheckedButton=true;
  }
  if(Encounter->State==EHouseEncounterState::Failed) { Test->AddError(Encounter->LastError); return true; }
  if(Encounter->State!=EHouseEncounterState::Active) return false;
  Test->TestFalse(TEXT("Success removes status prompt"),PC->GetHouseGenerationWidgetForTest().IsValid());
  Test->TestFalse(TEXT("Success restores movement"),PC->IsMoveInputIgnored());
  Test->TestFalse(TEXT("Success restores look"),PC->IsLookInputIgnored());
  Test->TestEqual(TEXT("Retry retains exact item count"),Encounter->Items.Num(),3);
  Test->TestEqual(TEXT("Retry retains exact zombie count"),Encounter->Zombies.Num(),1);
  return true;
 }
private:
 FAutomationTestBase* Test;
 int32 Stage=0;
 double Started=0;
 bool bCheckedButton=false;
 TSharedPtr<SButton> RetryButton;
 TWeakObjectPtr<AHouseEncounter> Encounter;
 TWeakObjectPtr<AActor> Blocker;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRecoveryMenuTest,"Prototype.Encounter.RecoveryPrompt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHouseRecoveryMenuTest::RunTest(const FString&)
{
 if(GEditor->PlayWorld) { AddError(TEXT("Run outside Play.")); return false; }
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Procedural/Lvl_HouseEncounter")));
 ADD_LATENT_AUTOMATION_COMMAND(HouseRecoveryPlay::FSetup());
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(HouseRecoveryPlay::FScenario(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
