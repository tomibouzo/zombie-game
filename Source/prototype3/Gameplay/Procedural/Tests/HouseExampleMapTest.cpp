#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHouseExampleMapTest, "Prototype.House.CreateExampleMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHouseExampleMapTest::RunTest(const FString& Parameters)
{
	// Explicit opt-in: never replace an open map or write Content during normal tests.
	if (!FParse::Param(FCommandLine::Get(), TEXT("CreateHouseBuilderExampleMap")))
	{
		AddInfo(TEXT("Example map creation skipped. Use -CreateHouseBuilderExampleMap in a fresh verification editor."));
		return true;
	}
	const FString MapPath = TEXT("/Game/Procedural/Lvl_HouseBuilder");
	if (FPackageName::DoesPackageExist(MapPath))
	{
		AddError(TEXT("Example map already exists; refusing to overwrite it. Edit the existing map in Unreal."));
		return false;
	}
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!TestNotNull(TEXT("Example world"), World)) return false;
	AHouseGenerator* House = World->SpawnActor<AHouseGenerator>();
	House->SetActorLabel(TEXT("House Builder - Select and Generate House"));
	House->GenerateHouse();
	if (!TestTrue(TEXT("Example house generated"), House->LastError.IsEmpty())) return false;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(FVector(600, 500, -40), FRotator::ZeroRotator);
	Ground->SetActorLabel(TEXT("Test Ground"));
	Ground->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Ground->SetActorScale3D(FVector(40, 40, 0.4));
	Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1500), FRotator(-55, -35, 0));
	Sun->SetActorLabel(TEXT("Test Sun"));
	Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sun->GetLightComponent()->SetIntensity(5.0f);
	for (int32 Index = 0; Index < House->Layout.Rooms.Num(); ++Index)
	{
		const FHouseRoom& Room = House->Layout.Rooms[Index];
		APointLight* Light = World->SpawnActor<APointLight>(FVector(
			(Room.Min.X + Room.Max.X) * House->Settings.CellSize * 0.5f,
			(Room.Min.Y + Room.Max.Y) * House->Settings.CellSize * 0.5f, 250), FRotator::ZeroRotator);
		Light->SetActorLabel(FString::Printf(TEXT("Test Room Light %d (move after regeneration)"), Index));
		Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Light->PointLightComponent->SetIntensity(3500.0f);
		Light->PointLightComponent->SetAttenuationRadius(1200.0f);
	}
	for (const FHouseWall& Wall : House->Layout.Walls)
	{
		if (Wall.RoomB != INDEX_NONE || Wall.Opening != EHouseOpening::Door) continue;
		APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(
			(Wall.Start.X + Wall.End.X) * House->Settings.CellSize * 0.5f, -180, 100), FRotator(0, 90, 0));
		Start->SetActorLabel(TEXT("Player Start - Facing Entrance"));
	}
	if (!TestTrue(TEXT("Example map saves"), UEditorLoadingAndSavingUtils::SaveMap(World, MapPath))) return false;
	World = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
	if (!TestNotNull(TEXT("Example map reloads"), World)) return false;
	int32 Builders = 0;
	for (TActorIterator<AHouseGenerator> It(World); It; ++It)
	{
		++Builders;
		FString Error;
		TestTrue(TEXT("Saved layout is valid"), FHouseLayoutGenerator::Validate(It->GeneratedSettings, It->Layout, Error));
		TestEqual(TEXT("Saved floor instances survive reload"), It->Floors->GetInstanceCount(), 4);
		TestTrue(TEXT("Saved wall instances survive reload"), It->Walls->GetInstanceCount() > 0);
		TestEqual(TEXT("Saved roof survives reload"), It->Roof->GetInstanceCount(), 1);
	}
	TestEqual(TEXT("Exactly one saved builder"), Builders, 1);
	return true;
}
#endif
