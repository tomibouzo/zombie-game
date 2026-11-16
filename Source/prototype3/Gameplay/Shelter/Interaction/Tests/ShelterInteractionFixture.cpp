#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Interaction/ShelterInteractivePieces.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/SavePackage.h"

namespace
{
UShelterPieceDefinition* LoadOrCreateInteractionPiece(const TCHAR* AssetName, const TCHAR* Id,
	const TCHAR* Category, int32 SortOrder, EShelterSlotKind Slot, TSubclassOf<AShelterPiece> ActorClass, const TCHAR* StockItem = nullptr)
{
	const FString PackageName = FString(TEXT("/Game/Shelter/Pieces/")) + AssetName;
	const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (FPaths::FileExists(File))
		return LoadObject<UShelterPieceDefinition>(nullptr, *(PackageName + TEXT(".") + AssetName));

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	UPackage* Package = CreatePackage(*PackageName);
	UShelterPieceDefinition* Piece;
	if (StockItem)
	{
		const FString ItemPath = FString::Printf(TEXT("/Game/Items/DA_Item_%s.DA_Item_%s"), StockItem, StockItem);
		auto* Item = LoadObject<UItemDefinition>(nullptr, *ItemPath);
		if (!Item) return nullptr;
		auto* Stock = NewObject<UShelterStockPieceDefinition>(Package, AssetName, RF_Public | RF_Standalone);
		Stock->Item = Item;
		Stock->ProfileId = FName(FString(StockItem) + TEXT("_TestOnly"));
		Piece = Stock;
	}
	else
	{
		Piece = NewObject<UShelterPieceDefinition>(Package, AssetName, RF_Public | RF_Standalone);
	}
	Piece->PieceId = Id;
	Piece->DisplayName = FText::FromString(Id);
	Piece->Category = FText::FromString(Category);
	Piece->SortOrder = SortOrder;
	Piece->SlotKind = Slot;
	Piece->ActorClass = ActorClass;
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	if (!Piece->IsValidDefinition() || !UPackage::SavePackage(Package, Piece, *File, Args)) return nullptr;
	FAssetRegistryModule::AssetCreated(Piece);
	return Piece;
}

void SpawnRoomBox(UWorld& World, UStaticMesh& Cube, const TCHAR* Label, const FVector& Center, const FVector& Size)
{
	const FTransform Transform(FRotator::ZeroRotator, Center, Size / 100.0);
	auto* Actor = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	Actor->SetActorLabel(Label);
	auto* Mesh = Actor->GetStaticMeshComponent();
	Mesh->SetStaticMesh(&Cube);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Actor->FinishSpawning(Transform);
}

AShelterPiece* SpawnDemoPiece(UWorld& World, UShelterPieceDefinition& Definition, const FTransform& Transform)
{
	auto* Piece = World.SpawnActorDeferred<AShelterPiece>(Definition.ResolveActorClass(), Transform);
	if (!Piece) return nullptr;
	Piece->InitializePiece(&Definition, FShelterModule());
	Piece->FinishSpawning(Transform);
	return Piece;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterInteractionFixtures, "Prototype.Fixtures.ShelterInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterInteractionFixtures::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ShelterInteractionCreateFixtures")))
	{
		AddInfo(TEXT("Opt in with -ShelterInteractionCreateFixtures."));
		return true;
	}

	auto* Door = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_Door"), TEXT("Door"), TEXT("Doorways"), 11,
		EShelterSlotKind::Edge, AShelterDoorPiece::StaticClass());
	auto* Lamp = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_Lamp"), TEXT("Lamp"), TEXT("Objects"), 30,
		EShelterSlotKind::Cell, AShelterLampPiece::StaticClass());
	auto* Roof = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_Roof"), TEXT("Roof"), TEXT("Roofs"), 25,
		EShelterSlotKind::Roof, AShelterPlaceholderPiece::StaticClass());
	auto* Chest = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_Chest"), TEXT("Chest"), TEXT("Objects"), 31,
		EShelterSlotKind::Cell, AShelterChestPiece::StaticClass());
	auto* Bandage = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_Bandage"), TEXT("Bandage"), TEXT("Supplies"), 40,
		EShelterSlotKind::Cell, AShelterStockPiece::StaticClass(), TEXT("Bandage"));
	auto* Beans = LoadOrCreateInteractionPiece(TEXT("DA_ShelterPiece_CannedBeans"), TEXT("CannedBeans"), TEXT("Supplies"), 41,
		EShelterSlotKind::Cell, AShelterStockPiece::StaticClass(), TEXT("CannedBeans"));
	if (!TestNotNull(TEXT("Roof definition"), Roof) || !TestNotNull(TEXT("Chest definition"), Chest)
		|| !TestNotNull(TEXT("Bandage stock definition"), Bandage) || !TestNotNull(TEXT("Food stock definition"), Beans)) return false;
	if (FParse::Param(FCommandLine::Get(), TEXT("ShelterUpgradeDoorway")))
	{
		const TCHAR* Path = TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Doorway");
		auto* Doorway = LoadObject<UShelterPieceDefinition>(nullptr, TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Doorway.DA_ShelterPiece_Doorway"));
		if (!TestNotNull(TEXT("Existing doorway definition"), Doorway)) return false;
		if (!Doorway->ActorClass)
		{
			Doorway->ActorClass = AShelterDoorPiece::StaticClass();
			const FString File = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			if (!TestTrue(TEXT("Save upgraded doorway definition"), UPackage::SavePackage(Doorway->GetOutermost(), Doorway, *File, Args))) return false;
		}
	}
	if (!TestNotNull(TEXT("Door definition"), Door) || !TestNotNull(TEXT("Lamp definition"), Lamp)) return false;

	const FString MapFile = FPackageName::LongPackageNameToFilename(TEXT("/Game/Shelter/Lvl_Shelter"), FPackageName::GetMapPackageExtension());
	if (FPaths::FileExists(MapFile))
	{
		AddInfo(TEXT("Lvl_Shelter already exists. Keeping the authored map."));
		return true;
	}

	auto* GameMode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
	auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("First person game mode"), GameMode) || !TestNotNull(TEXT("Room mesh"), Cube)) return false;

	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Shelter world"), World)) return false;
	World->GetWorldSettings()->DefaultGameMode = GameMode;
	SpawnRoomBox(*World, *Cube, TEXT("Shelter floor"), FVector(600, 430, -20), FVector(1800, 2000, 40));
	SpawnRoomBox(*World, *Cube, TEXT("Left demonstration wall"), FVector(325, -160, 150), FVector(350, 20, 300));
	SpawnRoomBox(*World, *Cube, TEXT("Right demonstration wall"), FVector(875, -160, 150), FVector(350, 20, 300));

	auto* Area = World->SpawnActor<AShelterBuildArea>(FVector::ZeroVector, FRotator::ZeroRotator);
	Area->SetActorLabel(TEXT("Shelter build area"));
	World->SpawnActor<APlayerStart>(FVector(600, -365, 100), FRotator(0, 90, 0));
	if (!TestNotNull(TEXT("Hand-placed door"), SpawnDemoPiece(*World, *Door, FTransform(FVector(600, -160, 0))))
		|| !TestNotNull(TEXT("Hand-placed lamp"), SpawnDemoPiece(*World, *Lamp, FTransform(FVector(455, -290, 0)))))
		return false;
	auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(600, 600, 800), FRotator(-52, -30, 0));
	Sun->GetLightComponent()->SetIntensity(2.f);
	auto* Sky = World->SpawnActor<ASkyLight>(FVector(600, 600, 900), FRotator::ZeroRotator);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetIntensity(.65f);
	return TestTrue(TEXT("Save shelter level"), FEditorFileUtils::SaveLevel(World->PersistentLevel, MapFile));
}
#endif
