#include "Gameplay/Procedural/Encounter/HouseEncounter.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterPlacement.h"
#include "Gameplay/Procedural/Encounter/HouseEncounterSupport.h"
#include "Gameplay/AI/Zombies/ZombieCharacter.h"
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
	ZombieClass = AZombieCharacter::StaticClass();
	ItemSelection = {TEXT("Bandage"), TEXT("CannedBeans"), TEXT("WaterBottle"), TEXT("Knife"),
		TEXT("Pistol"), TEXT("Flashlight"), TEXT("Jacket"), TEXT("SmallBackpack"), TEXT("ScrapMetal")};
	NextSections = {TEXT("Street"), TEXT("Another house"), TEXT("Shelter")};
}

void AHouseEncounter::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(bStartOnBeginPlay);
}

void AHouseEncounter::PostLoad()
{
	Super::PostLoad();
	HouseSettings.PreserveLoadedGenerationRanges();
}

#if WITH_EDITOR
void AHouseEncounter::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHouseGenerationSettings, bUseGenerationRanges))
		HouseSettings.InitializeGenerationRanges();
	else if (HouseSettings.bUseGenerationRanges) HouseSettings.bGenerationRangesInitialized = true;
	Super::PostEditChangeProperty(Event);
}
#endif

bool AHouseEncounter::Prepare(ACharacter* InPlayer)
{
	if (State != EHouseEncounterState::Idle) return false;
	State = EHouseEncounterState::Preparing;
	Player = InPlayer;
	if (IsValid(Player))
		if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController())) Controller->ShowHouseGenerationStatus(this);
	PreparedNextSections = NextSections;
	FHouseExitDecision ValidateOptions;
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->FindComponentByClass<UInventoryComponent>()
		|| !ZombieClass || !ValidateOptions.Offer(PreparedNextSections) || (ItemCount > 0 && ItemSelection.IsEmpty())
		|| ZombieCount < 0 || ZombieCount > 32 || ItemCount < 0 || ItemCount > 64)
		return Fail(TEXT("Invalid player, spawn counts, zombie class, item selection or next-section choices."));
	// Edits made while navigation builds affect the next encounter, not this transaction.
	PreparedSeed = HouseSettings.Seed;
	RequestSeed = PreparedSeed;
	RequestedHouseSettings = HouseSettings;
	PreparedStyle = HouseStyle;
	PreparedZombieCount = ZombieCount;
	PreparedItemCount = ItemCount;
	PreparedZombieClass = ZombieClass;
	PreparedItemSelection = ItemSelection;
	FString SettingsError;
	if (!FHouseLayoutGenerator::ResolveRanges(RequestedHouseSettings, PreparedHouseSettings, SettingsError)) return Fail(SettingsError);
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
	for (FName Type : PreparedItemSelection)
	{
		FInventoryItemProfile Profile;
		FVector Extent;
		if (!Catalog->GetProfile(FName(Type.ToString() + TEXT("_TestOnly")), Profile)
			|| !ADroppedItem::GetWorldHalfExtent(Profile.Definition, Extent))
			return Fail(FString::Printf(TEXT("Unsupported collectible type: %s"), *Type.ToString()));
	}
	bRetryAllowed = true;
	if (PreparedHouseSettings.bRequireExactFootprint && !FHouseLayoutGenerator::ValidateSettings(PreparedHouseSettings, SettingsError))
		return Fail(SettingsError);
	return BeginLayoutAttempt();
}

bool AHouseEncounter::BeginLayoutAttempt()
{
	bLayoutRetryPending = false;
	++LayoutAttempts;
	GenerationStatus = FString::Printf(TEXT("Preparing house %d of %d..."), LayoutAttempts, MaxLayoutAttempts);
	if (PreparedHouseSettings.RoomCount == 1 && PreparedZombieCount > 0
		&& (PreparedHouseSettings.bRequireExactFootprint || PreparedHouseSettings.Navigation.Pattern == EHouseLayoutPattern::LegacyRandom))
	{
		bRetryAllowed = RequestedHouseSettings.bUseGenerationRanges && RequestedHouseSettings.GenerationRanges.RoomCount.Max > 1;
		return Fail(TEXT("A one-room house cannot contain zombies: the player's entire starting room is reserved. Increase the room count or set Zombie Count to zero."));
	}
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return Fail(TEXT("Navigation system became unavailable."));
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	House = GetWorld()->SpawnActor<AHouseGenerator>(GetActorLocation(), GetActorRotation(), Spawn);
	if (!House) return Fail(TEXT("Could not create the house builder."));
	OwnedActors.Add(House);
	House->Settings = PreparedHouseSettings;
	// Each builder invocation already tries 16 seeds. Keep the outer retry blocks disjoint.
	House->Settings.Seed = static_cast<int32>(static_cast<uint32>(PreparedSeed) + 0x9e3779b9u * 16u * (LayoutAttempts-1));
	House->Style = PreparedStyle;
	House->bShowLabels = false;
	House->GenerateHouse();
	if (!House->LastError.IsEmpty())
	{
		const FString GenerationError = House->LastError;
		RecoverPlacement(GenerationError);
		return State == EHouseEncounterState::Preparing;
	}
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
	// Editor bounds may no longer cover an enlarged or rotated generated house.
	// Own a runtime volume covering the actual geometry, rather than restricting AI to the old map envelope.
	const FBox NavigationBounds = House->GetComponentsBoundingBox(true) + Landing->GetComponentsBoundingBox(true);
	auto* Bounds = GetWorld()->SpawnActor<AHouseEncounterNavBounds>(NavigationBounds.GetCenter(), FRotator::ZeroRotator, Spawn);
	if (!Bounds) return Fail(TEXT("Could not create navigation bounds for the generated house."));
	OwnedActors.Add(Bounds);
	Bounds->SetExtent(NavigationBounds.GetExtent()+FVector(150,150,250));
	Nav->OnNavigationBoundsUpdated(Bounds);
	NavigationWait = 0;
	// Include this frame's generated meshes and removed actors before requesting the full rebuild.
	Nav->ProcessPendingOctreeUpdates();
	Nav->Build();
	return true;
}

bool AHouseEncounter::Activate()
{
	if (State != EHouseEncounterState::Preparing) return false;
	if (bLayoutRetryPending) { BeginLayoutAttempt(); return false; }
	if (!IsValid(Player) || !IsValid(House)) return Fail(TEXT("Player or generated house became unavailable during preparation."));
	auto* ZombieDefaults = PreparedZombieClass.GetDefaultObject();
	const float ZombieRadius = ZombieDefaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float ZombieHeight = ZombieDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FHouseEncounterPlacement Candidate;
	FString Error;
	TArray<FName> SelectedTypes;
	TArray<FVector> ItemExtents;
	FRandomStream Random(static_cast<int32>(static_cast<uint32>(PreparedSeed) ^ 0x91e10da5u));
	for (int32 Index=0; Index<PreparedItemCount; ++Index)
	{
		const FName Type=PreparedItemSelection[Random.RandRange(0,PreparedItemSelection.Num()-1)];
		FInventoryItemProfile Profile;
		FVector Extent;
		if (!Catalog->GetProfile(FName(Type.ToString()+TEXT("_TestOnly")),Profile)
			|| !ADroppedItem::GetWorldHalfExtent(Profile.Definition,Extent)) return Fail(TEXT("Collectible profile became unavailable."));
		SelectedTypes.Add(Type); ItemExtents.Add(Extent);
	}
	if (!HouseEncounterPlacement::Plan(House, Player, ZombieRadius, ZombieHeight, PreparedZombieCount, ItemExtents,
		PreparedSeed, Candidate, Error, &PlacementReport)) return RecoverPlacement(Error);
	UInventoryComponent* Inventory = Player->FindComponentByClass<UInventoryComponent>();
	if (!Inventory) return Fail(TEXT("Player inventory became unavailable."));
	for (int32 ItemIndex=0; ItemIndex<Candidate.ItemFeet.Num(); ++ItemIndex)
	{
		const FVector Feet=Candidate.ItemFeet[ItemIndex];
		const FName Type = SelectedTypes[ItemIndex];
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
	GenerationStatus = FString::Printf(TEXT("House ready after %d layout attempt(s). Request seed %d, layout seed %d."), LayoutAttempts, RequestSeed, House->GeneratedSeed);
	LastError.Empty();
	if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController())) Controller->CloseHouseGenerationStatus(this);
	return true;
}

bool AHouseEncounter::RecoverPlacement(const FString& Error)
{
	if (LayoutAttempts >= MaxLayoutAttempts)
		return Fail(FString::Printf(TEXT("No complete encounter after %d house layouts. Last attempt: %s"), LayoutAttempts, *Error));
	UE_LOG(LogTemp, Display, TEXT("House encounter layout %d rejected: %s"), LayoutAttempts, *Error);
	ClearLayout();
	bLayoutRetryPending = true;
	GenerationStatus = FString::Printf(TEXT("Trying another arrangement (%d of %d)..."), LayoutAttempts+1, MaxLayoutAttempts);
	return false; // No active encounter yet. Tick builds the next house and waits for its navigation.
}

bool AHouseEncounter::RetryGeneration()
{
	if (!CanRetryGeneration()) return false;
	++RetryNumber;
	PreparedSeed = static_cast<int32>(static_cast<uint32>(RequestedHouseSettings.Seed) + 0x9e3779b9u * 64u * RetryNumber);
	RequestSeed = PreparedSeed;
	LayoutAttempts = 0;
	FHouseGenerationSettings Request = RequestedHouseSettings;
	Request.Seed = PreparedSeed;
	FString Error;
	if (!FHouseLayoutGenerator::ResolveRanges(Request, PreparedHouseSettings, Error)
		|| (PreparedHouseSettings.bRequireExactFootprint && !FHouseLayoutGenerator::ValidateSettings(PreparedHouseSettings, Error))) return Fail(Error);
	State = EHouseEncounterState::Preparing;
	LayoutAttempts = 0;
	PlacementReport = FHouseEncounterPlacementReport();
	LastError.Empty();
	SetActorTickEnabled(true);
	return BeginLayoutAttempt();
}

bool AHouseEncounter::CanRetryGeneration() const
{
	return State == EHouseEncounterState::Failed && bRetryAllowed && IsValid(Player);
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
		if (bLayoutRetryPending) { BeginLayoutAttempt(); return; }
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
	GenerationStatus = TEXT("House could not be prepared.");
	ClearLayout();
	bLayoutRetryPending = false;
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Display, TEXT("House encounter failed: %s"), *LastError);
	// The status prompt owns the visible error, so an old red warning cannot linger after a successful retry.
	if (IsValid(Player))
		if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController())) Controller->ShowHouseGenerationStatus(this);
	return false;
}

void AHouseEncounter::ClearLayout()
{
	for (ACharacter* Zombie : Zombies)
		if (IsValid(Zombie) && Zombie->GetController()) Zombie->GetController()->Destroy();
	for (AActor* Actor : OwnedActors) if (IsValid(Actor)) Actor->Destroy();
	OwnedActors.Reset();
	Items.Reset();
	Zombies.Reset();
	House = nullptr;
	Placement = FHouseEncounterPlacement();
}

void AHouseEncounter::Cleanup()
{
	if (IsValid(Player))
		if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController())) Controller->CloseHouseGenerationStatus(this);
	ClearLayout();
	Catalog = nullptr;
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
