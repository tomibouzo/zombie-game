#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Engine/World.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SEditableText.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseEditorControlsTest, "Prototype.House.EditorControls",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseEditorControlsTest::RunTest(const FString& Parameters)
{
 const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
  .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
 AHouseGenerator* House = World->SpawnActor<AHouseGenerator>();
 auto& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
 auto Rows = Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
 Rows->SetObjects({House});
 TMap<FName, TSharedRef<IDetailTreeNode>> Controls;
 int32 SeedRows = 0;
 TFunction<void(TSharedRef<IDetailTreeNode>)> FindControls = [&](TSharedRef<IDetailTreeNode> Node)
 {
  auto Handle = Node->CreatePropertyHandle();
  if (Handle.IsValid() && Handle->GetProperty())
  {
   const FName Name = Handle->GetProperty()->GetFName();
   if (Name == TEXT("Seed")) ++SeedRows;
   if (Name == TEXT("Seed") || Name == TEXT("WallHeight") || Name == TEXT("Difficulty") || Name == TEXT("Pattern")) Controls.Add(Name, Node);
  }
  TArray<TSharedRef<IDetailTreeNode>> Children; Node->GetChildren(Children, true);
  for (auto Child : Children) FindControls(Child);
 };
 bool bHasControlsCategory = false;
 for (auto Root : Rows->GetRootTreeNodes())
 {
  bHasControlsCategory |= Root->GetNodeName().ToString().StartsWith(TEXT("House Controls"));
  FindControls(Root);
 }
 TestTrue(TEXT("Editable inputs are grouped under House Controls"), bHasControlsCategory);
 TestEqual(TEXT("Only one Seed control, no read-only settings duplicate"), SeedRows, 1);
 auto CommitNumber = [&](FName Name, FString Text)
 {
  auto Node = Controls.Find(Name);
  if (!TestTrue(TEXT("Editable control exists"), Node != nullptr)) return;
  TestTrue(TEXT("Property handle allows edits"), (*Node)->CreatePropertyHandle()->IsEditable());
  TSharedPtr<SEditableText> Input;
  TFunction<void(TSharedRef<SWidget>)> FindInput = [&](TSharedRef<SWidget> Widget)
  {
   if (Widget->GetTypeAsString() == TEXT("SEditableText")) { Input = StaticCastSharedRef<SEditableText>(Widget); return; }
   FChildren* Children = Widget->GetChildren();
   for (int32 I = 0; I < Children->Num(); ++I) FindInput(Children->GetChildAt(I));
  };
  auto Widget = (*Node)->CreateNodeWidgets().ValueWidget;
  if (!TestTrue(TEXT("Value widget exists"), Widget.IsValid())) return;
  FindInput(Widget.ToSharedRef());
  if (!TestTrue(TEXT("Numeric field has text input"), Input.IsValid())) return;
  TestTrue(TEXT("Text field is enabled"), Input->IsEnabled());
  Input->SetText(FText::FromString(Text));
  StaticCastSharedPtr<SWidget>(Input)->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
 };
 CommitNumber(TEXT("Seed"), TEXT("7201"));
 TestEqual(TEXT("Typing seed commits through widget"), House->Settings.Seed, 7201);
 CommitNumber(TEXT("WallHeight"), TEXT("320"));
 TestEqual(TEXT("Typing dimension commits through widget"), House->Settings.WallHeight, 320.f);
 if (Controls.Contains(TEXT("Pattern"))) Controls[TEXT("Pattern")]->CreatePropertyHandle()->SetValue(static_cast<uint8>(EHouseLayoutPattern::CentralHub));
 House->Settings.Household.Profile = EHouseholdProfile::Family;
 for (int32 Difficulty = 0; Difficulty < 3; ++Difficulty)
 {
  if (Controls.Contains(TEXT("Difficulty"))) Controls[TEXT("Difficulty")]->CreatePropertyHandle()->SetValue(static_cast<uint8>(Difficulty));
  CommitNumber(TEXT("Seed"), FString::FromInt(7201 + Difficulty));
  House->GenerateHouse();
  TestTrue(*FString::Printf(TEXT("Difficulty %d generates with the original 12x10 inputs: %s"), Difficulty, *House->LastError), House->LastError.IsEmpty());
  TestEqual(TEXT("Input seed survives generation"), House->Settings.Seed, 7201 + Difficulty);
  TestEqual(TEXT("Input room count survives generation"), House->Settings.RoomCount, 4);
  TestEqual(TEXT("Difficulty retained"), House->Layout.Navigation.MaxDoorDepth, Difficulty + 2);
 }
 CommitNumber(TEXT("Seed"), TEXT("8100"));
 TestEqual(TEXT("Seed remains editable after generation"), House->Settings.Seed, 8100);
 House->GenerateNewHouse();
 TestEqual(TEXT("New House button changes input seed"), House->Settings.Seed, 8101);
 TestTrue(TEXT("New House button generates"), House->LastError.IsEmpty());
 Rows->SetObjects({});
 World->DestroyWorld(false);
 return true;
}
#endif
