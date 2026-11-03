#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHousePersistenceTest, "Prototype.House.FurnishedSaveReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHousePersistenceTest::RunTest(const FString& Parameters)
{
	const FString Root = TEXT("/HouseVerification/");
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HouseFurnishingVerification/"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FPackageName::RegisterMountPoint(Root, Directory);
	const FString PackageName = Root + TEXT("FurnishedRoundTrip");
	UPackage* Package = CreatePackage(*PackageName);
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FurnishedRoundTrip"), Package, true, ERHIFeatureLevel::Num, &Values);
	if (!World)
	{
		FPackageName::UnRegisterMountPoint(Root, Directory);
		AddError(TEXT("Could not create save/reload test world."));
		return false;
	}
	AHouseGenerator* House = World->SpawnActor<AHouseGenerator>();
	House->Settings.Navigation.Pattern = EHouseLayoutPattern::DeepBranches;
	House->Settings.Navigation.Difficulty = EHouseNavigationDifficulty::Hard;
	House->Settings.RoomCount = 12;
	House->Settings.Household.Profile = EHouseholdProfile::WorkingFromHomeCouple;
	House->Settings.Household.FurnitureStyle = EHouseFurnitureStyle::Traditional;
	House->Settings.WidthCells = House->Settings.DepthCells = 24;
	House->GenerateHouse();
	TestTrue(TEXT("Planned house generated before saving"), House->LastError.IsEmpty());
	const FString Summary = House->NavigationSummary;
	const FIntPoint ActualFootprint = House->Layout.FootprintCells;
	const FString SizeSummary = House->SizeSummary;
	const TArray<int32> MainRoute = House->Layout.Navigation.MainRoute;
	const int32 FurnitureCount = House->Layout.Furniture.Num();
	const int32 LabelCount = House->Layout.Rooms.Num() + FurnitureCount;
	World->SetFlags(RF_Public | RF_Standalone);
	// This test deliberately replaces its complete temporary map on each run.
	// A previous file on disk must not make the new in-memory package partially loaded.
	Package->MarkAsFullyLoaded();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = Directory / TEXT("FurnishedRoundTrip.umap");
	const bool bSaved = TestTrue(TEXT("Furnished map saves"), UPackage::SavePackage(Package, World, *Filename, SaveArgs));
	World->DestroyWorld(false);
	World->ClearFlags(RF_Standalone);
	Package->ClearFlags(RF_Standalone);
	House = nullptr;
	World = nullptr;
	Package = nullptr;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestNull(TEXT("Original map was unloaded before reading the saved file"),
		FindObject<UWorld>(nullptr, *(PackageName + TEXT(".FurnishedRoundTrip"))));
	if (!bSaved)
	{
		FPackageName::UnRegisterMountPoint(Root, Directory);
		return false;
	}
	World = LoadObject<UWorld>(nullptr, *(PackageName + TEXT(".FurnishedRoundTrip")));
	if (TestNotNull(TEXT("Saved map loads"), World))
	{
		House = nullptr;
		for (AActor* Actor : World->PersistentLevel->Actors)
			if (AHouseGenerator* Found = Cast<AHouseGenerator>(Actor)) { House = Found; break; }
		if (TestNotNull(TEXT("Saved builder loads"), House))
		{
			FString Error;
			TestEqual(TEXT("Household persists"), House->Layout.Household.Profile, EHouseholdProfile::WorkingFromHomeCouple);
			TestEqual(TEXT("Furniture style persists"), House->GeneratedSettings.Household.FurnitureStyle, EHouseFurnitureStyle::Traditional);
			TestEqual(TEXT("Pattern persists"), House->GeneratedSettings.Navigation.Pattern, EHouseLayoutPattern::DeepBranches);
			TestEqual(TEXT("Difficulty persists"), House->GeneratedSettings.Navigation.Difficulty, EHouseNavigationDifficulty::Hard);
			TestEqual(TEXT("Actual footprint persists"), House->Layout.FootprintCells, ActualFootprint);
			TestEqual(TEXT("Size summary persists"), House->SizeSummary, SizeSummary);
			TestEqual(TEXT("Navigation summary persists"), House->NavigationSummary, Summary);
			TestEqual(TEXT("Route depth persists"), House->Layout.Navigation.MaxDoorDepth, 4);
			TestTrue(TEXT("Main route persists"), House->Layout.Navigation.MainRoute == MainRoute);
			TestTrue(TEXT("Saved room/window/furniture data validates"), FHouseLayoutGenerator::Validate(House->GeneratedSettings, House->Layout, Error));
			TestEqual(TEXT("Furniture placements persist"), House->Layout.Furniture.Num(), FurnitureCount);
			int32 InstanceCount = 0;
			for (UInstancedStaticMeshComponent* Component : House->FurnitureMeshes) InstanceCount += Component->GetInstanceCount();
			TestEqual(TEXT("Furniture instances persist"), InstanceCount, FurnitureCount);
			TArray<UTextRenderComponent*> Labels;
			House->GetComponents(Labels);
			TestEqual(TEXT("Generated text labels persist"), Labels.Num(), LabelCount);
			for (UTextRenderComponent* Label : Labels)
			{
				TestFalse(TEXT("Saved label has text"), Label->Text.IsEmpty());
				TestEqual(TEXT("Label remains attached to builder root"), Label->GetAttachParent(), House->GetRootComponent());
			}
		}
		World->DestroyWorld(false);
		World->ClearFlags(RF_Standalone);
		World->GetPackage()->ClearFlags(RF_Standalone);
	}
	House = nullptr;
	World = nullptr;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	FPackageName::UnRegisterMountPoint(Root, Directory);
	return true;
}
#endif
