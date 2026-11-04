#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Engine/World.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseRangeToggleTest, "Prototype.House.RangeTogglePreservesSettings",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseRangeToggleTest::RunTest(const FString&)
{
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* House=World->SpawnActor<AHouseGenerator>();
 auto* Encounter=World->SpawnActor<AHouseEncounter>();
 auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
 for (AActor* Owner : {static_cast<AActor*>(House),static_cast<AActor*>(Encounter)})
 {
  auto& S=Owner==House ? House->Settings : Encounter->HouseSettings;
  S.WidthCells=22; S.DepthCells=19; S.RoomCount=15;
  auto Rows=Module.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
  Rows->SetObjects({Owner});
  TSharedPtr<IPropertyHandle> Toggle;
  TFunction<void(TSharedRef<IDetailTreeNode>)> Find=[&](TSharedRef<IDetailTreeNode> Node)
  {
   auto Handle=Node->CreatePropertyHandle();
   if (Handle.IsValid() && Handle->GetProperty() && Handle->GetProperty()->GetFName()==TEXT("bUseGenerationRanges")) Toggle=Handle;
   TArray<TSharedRef<IDetailTreeNode>> Children; Node->GetChildren(Children,true);
   for(auto Child:Children) Find(Child);
  };
  for(auto Root:Rows->GetRootTreeNodes()) Find(Root);
  if(!TestTrue(TEXT("Actor exposes real range checkbox"),Toggle.IsValid())) { World->DestroyWorld(false); return false; }
  Toggle->SetValue(true);
  TestEqual(TEXT("First toggle inherits width"),S.GenerationRanges.WidthCells.Min,22);
  TestEqual(TEXT("First toggle inherits depth"),S.GenerationRanges.DepthCells.Max,19);
  TestEqual(TEXT("First toggle inherits count"),S.GenerationRanges.RoomCount.Min,15);
  TestTrue(TEXT("Initialization marker persists in settings"),S.bGenerationRangesInitialized);
  S.GenerationRanges.WidthCells=FHouseIntegerRange(20,24);
  S.GenerationRanges.RoomCount=FHouseIntegerRange(8,15);
  Toggle->SetValue(false);
  S.WidthCells=30; S.RoomCount=20;
  Toggle->SetValue(true);
  TestEqual(TEXT("Re-enabling keeps edited lower bound"),S.GenerationRanges.WidthCells.Min,20);
  TestEqual(TEXT("Re-enabling keeps edited upper bound"),S.GenerationRanges.WidthCells.Max,24);
  TestEqual(TEXT("Re-enabling keeps authored room range"),S.GenerationRanges.RoomCount.Min,8);
  TestEqual(TEXT("Fixed settings are retained separately"),S.RoomCount,20);
  Rows->SetObjects({});
 }
 World->DestroyWorld(false);
 return true;
}
#endif
