#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Gameplay/Procedural/HouseGenerator.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

AHouseEncounter::AHouseEncounter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ZombieClass = AHouseZombiePlaceholder::StaticClass();
	ItemSelection = {TEXT("Bandage"), TEXT("CannedBeans"), TEXT("WaterBottle"), TEXT("Knife"),
		TEXT("Pistol"), TEXT("Flashlight"), TEXT("Jacket"), TEXT("SmallBackpack"), TEXT("ScrapMetal")};
	NextSections = {TEXT("Street"), TEXT("Another house"), TEXT("Shelter")};
}

void AHouseEncounter::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(bStartOnBeginPlay);
}

bool AHouseEncounter::Prepare(ACharacter* InPlayer)
{
	if (State != EHouseEncounterState::Idle) return false;
	State = EHouseEncounterState::Preparing;
	Player = InPlayer;
	PreparedNextSections = NextSections;
	FHouseExitDecision ValidateOptions;
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->FindComponentByClass<UInventoryComponent>()
		|| !ZombieClass || !ValidateOptions.Offer(PreparedNextSections) || (ItemCount > 0 && ItemSelection.IsEmpty())
		|| ZombieCount < 0 || ZombieCount > 32 || ItemCount < 0 || ItemCount > 64)
		return Fail(TEXT("Invalid player, spawn counts, zombie class, item selection or next-section choices."));
	// Edits made while navigation builds affect the next encounter, not this transaction.
	PreparedSeed = HouseSettings.Seed;
	PreparedZombieCount = ZombieCount;
	PreparedItemCount = ItemCount;
	PreparedZombieClass = ZombieClass;
	PreparedItemSelection = ItemSelection;
	if (!GetActorScale3D().Equals(FVector::OneVector) || !FMath::IsNearlyZero(GetActorRotation().Pitch)
		|| !FMath::IsNearlyZero(GetActorRotation().Roll)) return Fail(TEXT("Encounter must be upright and use unit scale."));
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	auto* NavData = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	if (!NavData || NavData->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic)
		return Fail(TEXT("Encounter requires navigation bounds and a Dynamic navmesh. Open Lvl_HouseEncounter."));
	Catalog = NewObject<UInventoryComponent>(this);
	if (!InventoryDemo::Populate(Catalog)) return Fail(TEXT("Existing item catalog could not be loaded."));
	// Keep only type profiles, never unused owned sample instances.
	for (const FInventoryEntry& Entry : Catalog->GetEntries())
	{
		FItemInstance Removed;
		Catalog->RemoveItem(Entry.Item.InstanceId, Removed);
	}
	for (FName Type : ItemSelection)
	{
		FInventoryItemProfile Profile;
		FVector Extent;
		if (!Catalog->GetProfile(FName(Type.ToString() + TEXT("_TestOnly")), Profile)
			|| !ADroppedItem::GetWorldHalfExtent(Profile.Definition, Extent))
			return Fail(FString::Printf(TEXT("Unsupported collectible type: %s"), *Type.ToString()));
	}
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	House = GetWorld()->SpawnActor<AHouseGenerator>(GetActorLocation(), GetActorRotation(), Spawn);
	if (!House) return Fail(TEXT("Could not create the house builder."));
	OwnedActors.Add(House);
	House->Settings = HouseSettings;
	House->Style = HouseStyle;
	House->bShowLabels = false;
	House->GenerateHouse();
	if (!House->LastError.IsEmpty()) return Fail(House->LastError);
	House->SetActorHiddenInGame(true);
	const FHouseWall* Entrance = House->Layout.Walls.FindByPredicate([](const FHouseWall& Wall)
		{ return Wall.RoomB == INDEX_NONE && Wall.Opening == EHouseOpening::Door; });
	if (!Entrance) return Fail(TEXT("Generated house has no entrance."));
	const float DoorX = (Entrance->Start.X + Entrance->End.X) * House->GeneratedSettings.CellSize * .5f;
	// A small outside landing guarantees support through the exit; it is owned and rolled back with the house.
	auto* Landing = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorTransform().TransformPosition(FVector(DoorX,-150,-10)), GetActorRotation(), Spawn);
	if (!Landing) return Fail(TEXT("Could not create the exit landing."));
	OwnedActors.Add(Landing);
	Landing->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Landing->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Landing->SetActorScale3D(FVector(5, 4, .2));
	Landing->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Landing->SetActorHiddenInGame(true);
	NavigationWait = 0;
	// Include this frame's generated meshes and removed actors before requesting the full rebuild.
	Nav->ProcessPendingOctreeUpdates();
	Nav->Build();
	return true;
}

bool AHouseEncounter::Activate()
{
	if (State != EHouseEncounterState::Preparing) return false;
	if (!IsValid(Player) || !IsValid(House)) return Fail(TEXT("Player or generated house became unavailable during preparation."));
	auto* ZombieDefaults = PreparedZombieClass.GetDefaultObject();
	const float ZombieRadius = ZombieDefaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float ZombieHeight = ZombieDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FHouseEncounterPlacement Candidate;
	FString Error;
	if (!HouseEncounterPlacement::Plan(House, Player, ZombieRadius, ZombieHeight, PreparedZombieCount, PreparedItemCount,
		PreparedSeed, Candidate, Error)) return Fail(Error);
	UInventoryComponent* Inventory = Player->FindComponentByClass<UInventoryComponent>();
	if (!Inventory) return Fail(TEXT("Player inventory became unavailable."));
	FRandomStream Random(static_cast<int32>(static_cast<uint32>(PreparedSeed) ^ 0x91e10da5u));
	for (const FVector& Feet : Candidate.ItemFeet)
	{
		const FName Type = PreparedItemSelection[Random.RandRange(0,PreparedItemSelection.Num()-1)];
		FInventoryItemProfile Profile, Existing;
		Catalog->GetProfile(FName(Type.ToString() + TEXT("_TestOnly")), Profile);
		if (!Inventory->GetProfile(Profile.Id, Existing))
		{
			if (Inventory->RegisterProfile(Profile) != EInventoryResult::Success) return Fail(TEXT("Could not register collectible pickup profile."));
		}
		else if (Existing.Definition != Profile.Definition) return Fail(TEXT("Player has a conflicting collectible profile."));
		FVector Extent;
		ADroppedItem::GetWorldHalfExtent(Profile.Definition, Extent);
		const FTransform Transform(GetActorRotation(), Feet + FVector(0,0,Extent.Z+2));
		FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterItem), false, Player);
		if (GetWorld()->OverlapAnyTestByObjectType(Transform.GetLocation(), Transform.GetRotation(),
			FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeBox(Extent + FVector(1)), Query))
			return Fail(TEXT("Collectible body is obstructed."));
		auto* Item = ADroppedItem::SpawnWorldItem(GetWorld(), Profile, Transform);
		if (!Item) return Fail(TEXT("Could not create a world-owned collectible."));
		Item->SetOwner(this);
		Items.Add(Item);
		OwnedActors.Add(Item);
		Candidate.ItemTypes.Add(Type);
	}
	for (const FVector& Feet : Candidate.ZombieFeet)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = this;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		auto* Zombie = GetWorld()->SpawnActor<ACharacter>(PreparedZombieClass, Feet + FVector(0,0,ZombieHeight+2), GetActorRotation(), Spawn);
		if (!Zombie) return Fail(TEXT("Zombie class could not spawn at its validated position."));
		Zombies.Add(Zombie);
		OwnedActors.Add(Zombie);
		if (!FMath::IsNearlyEqual(Zombie->GetSimpleCollisionRadius(), ZombieRadius)
			|| !FMath::IsNearlyEqual(Zombie->GetSimpleCollisionHalfHeight(), ZombieHeight))
			return Fail(TEXT("Zombie class changes capsule size on spawn; configure matching class defaults."));
	}
	// All creation can fail without moving the player. Commit the player only after the entire encounter exists.
	if (!HouseEncounterPlacement::ClearCapsule(GetWorld(), Candidate.PlayerFeet, Player->GetSimpleCollisionRadius(),
		Player->GetSimpleCollisionHalfHeight(), Player)) return Fail(TEXT("Entrance became occupied during spawning."));
	const FVector Start = Candidate.PlayerFeet + FVector(0,0,Player->GetSimpleCollisionHalfHeight()+2);
	const FRotator Facing = GetActorRotation() + FRotator(0,90,0);
	if (!Player->TeleportTo(Start, Facing, false, true)) return Fail(TEXT("Could not place the player at the entrance."));
	Player->GetCharacterMovement()->StopMovementImmediately();
	if (Player->GetController()) Player->GetController()->SetControlRotation(Facing);
	Placement = MoveTemp(Candidate);
	PreviousFeet = Placement.PlayerFeet;
	for (AActor* Actor : OwnedActors) if (IsValid(Actor)) Actor->SetActorHiddenInGame(false);
	for (const FHouseRoom& Room : House->Layout.Rooms)
	{
		const float Cell = House->GeneratedSettings.CellSize;
		auto* Light = GetWorld()->SpawnActor<APointLight>(GetActorTransform().TransformPosition(FVector(
			(Room.Min.X+Room.Max.X)*Cell*.5, (Room.Min.Y+Room.Max.Y)*Cell*.5, House->GeneratedSettings.WallHeight-35)), FRotator::ZeroRotator);
		if (Light)
		{
			Light->SetOwner(this);
			Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Light->PointLightComponent->SetIntensity(3500);
			Light->PointLightComponent->SetAttenuationRadius(1400);
			OwnedActors.Add(Light);
		}
	}
	auto* Marker = NewObject<UTextRenderComponent>(House);
	House->AddInstanceComponent(Marker);
	Marker->SetupAttachment(House->GetRootComponent());
	Marker->SetText(FText::FromString(TEXT("EXIT")));
	Marker->SetHorizontalAlignment(EHTA_Center);
	Marker->SetTextRenderColor(FColor::Green);
	Marker->SetWorldSize(24);
	Marker->SetRelativeLocation(GetActorTransform().InverseTransformPosition(Placement.ExitFeet) + FVector(0,65,190));
	Marker->SetRelativeRotation(FRotator(0,90,0));
	Marker->RegisterComponent();
	State = EHouseEncounterState::Active;
	LastError.Empty();
	return true;
}

bool AHouseEncounter::CheckExit()
{
	if (State != EHouseEncounterState::Active || !IsValid(Player) || !IsValid(House)) return false;
	const FVector Feet = Player->GetActorLocation() - FVector(0,0,Player->GetSimpleCollisionHalfHeight());
	const FVector Local = GetActorTransform().InverseTransformPosition(Feet);
	const FVector Previous = GetActorTransform().InverseTransformPosition(PreviousFeet);
	const FVector Exit = GetActorTransform().InverseTransformPosition(Placement.ExitFeet);
	PreviousFeet = Feet;
	const float Threshold = -Player->GetSimpleCollisionRadius()-5;
	if (Previous.Y <= Threshold || Local.Y > Threshold || FMath::Abs(Local.Z) > 40) return false;
	const double Alpha = (Threshold-Previous.Y)/(Local.Y-Previous.Y);
	const double CrossingX = FMath::Lerp(Previous.X, Local.X, Alpha);
	if (FMath::Abs(CrossingX-Exit.X) > House->GeneratedSettings.DoorWidth*.5 - Player->GetSimpleCollisionRadius()) return false;
	if (!ExitDecision.Offer(PreparedNextSections)) return false;
	State = EHouseEncounterState::Choosing;
	if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController())) Controller->ShowHouseExitChoice(this);
	return true;
}

bool AHouseEncounter::ChooseNextSection(FName Choice)
{
	if (State != EHouseEncounterState::Choosing || !ExitDecision.Choose(Choice)) return false;
	State = EHouseEncounterState::Complete;
	return true;
}

void AHouseEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WaitSeconds += DeltaSeconds;
	if (State == EHouseEncounterState::Idle)
	{
		auto* Controller = GetWorld()->GetFirstPlayerController();
		auto* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (Character && Character->HasActorBegunPlay()) Prepare(Character);
		else if (WaitSeconds > 15) Fail(TEXT("No local character was ready within 15 seconds."));
	}
	else if (State == EHouseEncounterState::Preparing)
	{
		NavigationWait += DeltaSeconds;
		auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Nav && NavigationWait >= .5f && !Nav->IsNavigationBuildInProgress()) Activate();
		else if (NavigationWait > 30) Fail(TEXT("Navigation did not finish building within 30 seconds."));
	}
	else if (State == EHouseEncounterState::Active) CheckExit();
}

bool AHouseEncounter::Fail(const FString& Error)
{
	LastError = Error;
	State = EHouseEncounterState::Failed;
	Cleanup();
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Display, TEXT("House encounter failed: %s"), *LastError);
	if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 30, FColor::Red, TEXT("House encounter: ") + LastError);
	return false;
}

void AHouseEncounter::Cleanup()
{
	for (ACharacter* Zombie : Zombies)
		if (IsValid(Zombie) && Zombie->GetController()) Zombie->GetController()->Destroy();
	for (AActor* Actor : OwnedActors) if (IsValid(Actor)) Actor->Destroy();
	OwnedActors.Reset();
	Items.Reset();
	Zombies.Reset();
	House = nullptr;
	Catalog = nullptr;
	Placement = FHouseEncounterPlacement();
}

void AHouseEncounter::EndPlay(const EEndPlayReason::Type Reason)
{
	Cleanup();
	Super::EndPlay(Reason);
}

void AHouseEncounter::Destroyed()
{
	// Also covers cancellation before the world has initialized actors for Play.
	Cleanup();
	Super::Destroyed();
}
