#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/DirectionalLight.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Interaction/ShelterInteractivePieces.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterInteractivePiecesTest, "Prototype.Interaction.ShelterPieces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterInteractivePiecesTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Area = World->SpawnActor<AShelterBuildArea>();
	Area->DispatchBeginPlay();
	auto* Player = World->SpawnActor<ACharacter>(FVector(300, 40, 100), FRotator::ZeroRotator);

	auto* DoorDefinition = LoadObject<UShelterPieceDefinition>(nullptr,
		TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Door.DA_ShelterPiece_Door"));
	auto* LampDefinition = LoadObject<UShelterPieceDefinition>(nullptr,
		TEXT("/Game/Shelter/Pieces/DA_ShelterPiece_Lamp.DA_ShelterPiece_Lamp"));
	if (!TestNotNull(TEXT("Saved door definition"), DoorDefinition)
		|| !TestNotNull(TEXT("Saved lamp definition"), LampDefinition)) return false;
	TArray<FPrimaryAssetId> CatalogIds;
	UAssetManager::Get().GetPrimaryAssetIdList(UShelterPieceDefinition::PieceAssetType, CatalogIds);
	TestTrue(TEXT("Door appears in the catalog"), CatalogIds.Contains(DoorDefinition->GetPrimaryAssetId()));
	TestTrue(TEXT("Lamp appears in the catalog"), CatalogIds.Contains(LampDefinition->GetPrimaryAssetId()));

	FShelterSlot DoorSlot;
	DoorSlot.Coord = FIntPoint(1, 1);
	DoorSlot.bAlongX = true;
	FShelterSlot LampSlot;
	LampSlot.Kind = EShelterSlotKind::Cell;
	LampSlot.Coord = FIntPoint(3, 1);
	using R = EShelterBuildResult;
	TestEqual(TEXT("Place door in edge slot"), Area->PlacePiece(DoorSlot, DoorDefinition, 0), R::Success);
	TestEqual(TEXT("Place lamp in cell slot"), Area->PlacePiece(LampSlot, LampDefinition, 0), R::Success);
	auto* Door = Cast<AShelterDoorPiece>(Area->GetPieceAt(DoorSlot));
	auto* Lamp = Cast<AShelterLampPiece>(Area->GetPieceAt(LampSlot));
	if (!TestNotNull(TEXT("Placed door"), Door) || !TestNotNull(TEXT("Placed lamp"), Lamp)) return false;

	const auto PassageBlocked = [&]
	{
		FHitResult Hit;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(ShelterPassageTest), false, Player);
		return World->SweepSingleByChannel(Hit, FVector(300, 90, 100), FVector(300, 320, 100),
			FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(40, 90), Query);
	};
	TestFalse(TEXT("Door starts closed"), Door->IsOpen());
	TestTrue(TEXT("Closed door blocks passage"), PassageBlocked());
	Door->Interact(Player);
	TestTrue(TEXT("Door opens"), Door->IsOpen());
	TestFalse(TEXT("Open door admits a character-sized capsule"), PassageBlocked());
	Player->SetActorLocation(FVector(300, 200, 100));
	Door->Interact(Player);
	TestTrue(TEXT("Door stays open while passage is occupied"), Door->IsOpen());
	Player->SetActorLocation(FVector(300, 40, 100));
	Door->Interact(Player);
	TestFalse(TEXT("Door closes when clear"), Door->IsOpen());
	TestTrue(TEXT("Collision returns when closed"), PassageBlocked());

	TestFalse(TEXT("Lamp starts dark"), Lamp->IsLit());
	Lamp->Interact(Player);
	TestTrue(TEXT("Lamp turns on"), Lamp->IsLit());
	Lamp->Interact(Player);
	TestFalse(TEXT("Lamp turns off"), Lamp->IsLit());

	for (const auto* Definition : { DoorDefinition, LampDefinition })
	{
		auto* Preview = Area->SpawnPreview(Definition);
		if (!TestNotNull(TEXT("Piece preview"), Preview)) return false;
		auto* Interaction = Cast<IInteractable>(Preview);
		TestTrue(TEXT("Preview disables collision"), !Preview->GetActorEnableCollision());
		TestTrue(TEXT("Preview cannot be interacted with"), Interaction && !Interaction->CanInteract(Player));
		Preview->Destroy();
	}
	TestEqual(TEXT("Remove door"), Area->RemoveAt(DoorSlot), R::Success);
	TestEqual(TEXT("Remove lamp"), Area->RemoveAt(LampSlot), R::Success);
	TestEqual(TEXT("Both slots cleared"), Area->GetModel().Placements.Num(), 0);
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShelterDemonstrationMapTest, "Prototype.Interaction.ShelterMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShelterDemonstrationMapTest::RunTest(const FString&)
{
	auto* Map = LoadObject<UWorld>(nullptr, TEXT("/Game/Shelter/Lvl_Shelter.Lvl_Shelter"));
	if (!TestNotNull(TEXT("Saved shelter map"), Map) || !TestNotNull(TEXT("Persistent shelter level"), Map->PersistentLevel.Get()))
		return false;

	const AShelterDoorPiece* Door = nullptr;
	const AShelterLampPiece* Lamp = nullptr;
	const AShelterBuildArea* Area = nullptr;
	int32 StartCount = 0;
	int32 DirectionalLightCount = 0;
	for (const AActor* Actor : Map->PersistentLevel->Actors)
	{
		if (const auto* FoundDoor = Cast<AShelterDoorPiece>(Actor)) Door = FoundDoor;
		if (const auto* FoundLamp = Cast<AShelterLampPiece>(Actor)) Lamp = FoundLamp;
		if (const auto* FoundArea = Cast<AShelterBuildArea>(Actor)) Area = FoundArea;
		if (Cast<APlayerStart>(Actor)) ++StartCount;
		if (Cast<ADirectionalLight>(Actor)) ++DirectionalLightCount;
	}

	TestEqual(TEXT("One player start"), StartCount, 1);
	TestTrue(TEXT("Level has lighting"), DirectionalLightCount > 0);
	if (!TestNotNull(TEXT("Hand-placed door"), Door) || !TestNotNull(TEXT("Hand-placed lamp"), Lamp)
		|| !TestNotNull(TEXT("Build area"), Area)) return false;
	TestFalse(TEXT("Saved door starts closed"), Door->IsOpen());
	TestFalse(TEXT("Saved lamp starts unlit"), Lamp->IsLit());
	TestTrue(TEXT("Door has its saved piece definition"), Door->GetDefinition() && Door->GetDefinition()->PieceId == TEXT("Door"));
	TestTrue(TEXT("Lamp has its saved piece definition"), Lamp->GetDefinition() && Lamp->GetDefinition()->PieceId == TEXT("Lamp"));
	TestTrue(TEXT("Door is outside the grid footprint"), Door->GetActorLocation().Y < Area->GetActorLocation().Y);
	TestTrue(TEXT("Lamp is outside the grid footprint"), Lamp->GetActorLocation().Y < Area->GetActorLocation().Y);
	return !HasAnyErrors();
}
#endif
