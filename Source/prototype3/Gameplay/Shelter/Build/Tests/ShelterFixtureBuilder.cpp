#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
struct FPieceSeed
{
	const TCHAR* Asset;
	const TCHAR* Id;
	const TCHAR* Name;
	const TCHAR* Category;
	int32 Order;
	float OpeningWidth, OpeningHeight, Sill;
	FLinearColor Color;
};
// Starting values for the user's standalone placement demonstration. After creation the assets
// are the source of truth; edit them in the editor, not here.
const FPieceSeed PieceSeeds[] = {
	{TEXT("DA_ShelterPiece_Wall"), TEXT("Wall"), TEXT("Wall"), TEXT("Walls"), 0, 0, 0, 0, FLinearColor(0.45f, 0.42f, 0.38f)},
	{TEXT("DA_ShelterPiece_Doorway"), TEXT("Doorway"), TEXT("Doorway"), TEXT("Doorways"), 10, 140, 220, 0, FLinearColor(0.38f, 0.30f, 0.22f)},
	{TEXT("DA_ShelterPiece_Window"), TEXT("Window"), TEXT("Window"), TEXT("Windows"), 20, 120, 100, 100, FLinearColor(0.32f, 0.40f, 0.46f)},
};
}

/** Creates the placeholder piece assets and the build laboratory map. Never overwrites existing files. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterBuildFixtures, "Prototype.Fixtures.BuildLab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterBuildFixtures::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ShelterCreateFixtures")))
	{ AddInfo(TEXT("Opt in with -ShelterCreateFixtures to create the shelter piece assets and build lab.")); return true; }
	const FString Folder = TEXT("/Game/Shelter/Pieces/");
	const FString MapPath = TEXT("/Game/Shelter/Lvl_BuildLab");
	const FString MapFile = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
	bool bExisting = FPaths::FileExists(MapFile);
	for (const FPieceSeed& Seed : PieceSeeds)
		bExisting |= FPaths::FileExists(FPackageName::LongPackageNameToFilename(Folder + Seed.Asset, FPackageName::GetAssetPackageExtension()));
	if (bExisting)
	{ AddError(TEXT("Shelter fixture assets already exist. Keep authored edits; this generator only creates new files.")); return false; }

	for (const FPieceSeed& Seed : PieceSeeds)
	{
		const FString PackageName = Folder + Seed.Asset;
		const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		UPackage* Package = CreatePackage(*PackageName);
		auto* Piece = NewObject<UShelterPieceDefinition>(Package, Seed.Asset, RF_Public | RF_Standalone);
		Piece->PieceId = Seed.Id;
		Piece->DisplayName = FText::FromString(Seed.Name);
		Piece->Category = FText::FromString(Seed.Category);
		Piece->SortOrder = Seed.Order;
		Piece->SlotKind = EShelterSlotKind::Edge;
		Piece->OpeningWidth = Seed.OpeningWidth;
		Piece->OpeningHeight = Seed.OpeningHeight;
		Piece->OpeningSillHeight = Seed.Sill;
		Piece->PlaceholderColor = Seed.Color;
		if (!TestTrue(TEXT("Seeded piece is valid"), Piece->IsValidDefinition())) return false;
		FAssetRegistryModule::AssetCreated(Piece);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		if (!TestTrue(TEXT("Save shelter piece"), UPackage::SavePackage(Package, Piece, *File, Args))) return false;
	}

	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Build lab world"), World)) return false;
	World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"));
	auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(600, 600, -20), FRotator::ZeroRotator);
	Floor->SetActorLabel(TEXT("LabFloor"));
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetActorScale3D(FVector(30, 30, .4));
	// Grid minimum corner at the origin: 6 x 6 modules of 200 cm, floor top at Z = 0.
	auto* Area = World->SpawnActor<AShelterBuildArea>(FVector::ZeroVector, FRotator::ZeroRotator);
	Area->SetActorLabel(TEXT("BuildArea_Edit_Size_Here"));
	Area->RerunConstructionScripts();
	// Start just outside the grid, inside its entry margin, facing in.
	World->SpawnActor<APlayerStart>(FVector(600, -120, 100), FRotator(0, 90, 0));
	auto* Light = World->SpawnActor<ADirectionalLight>(FVector(600, 600, 800), FRotator(-50, -30, 0));
	Light->GetLightComponent()->SetIntensity(5);
	auto* Sky = World->SpawnActor<ASkyLight>(FVector(600, 600, 900), FRotator::ZeroRotator);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetIntensity(1);
	TestTrue(TEXT("Save build lab"), FEditorFileUtils::SaveLevel(World->PersistentLevel, MapFile));
	return !HasAnyErrors();
}
#endif
