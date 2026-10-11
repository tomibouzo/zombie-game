#include "Gameplay/Player/Inventory/InventoryTestPile.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AInventoryTestPile::AInventoryTestPile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = .1f;
}

bool AInventoryTestPile::SpawnForPlayer(APawn* Player)
{
	if (bSpawnAttempted || !IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
	auto* Inventory = Player->FindComponentByClass<UInventoryComponent>();
	if (!Inventory || Inventory->GetPockets().IsEmpty()) return false;
	bSpawnAttempted = true;
	SetActorTickEnabled(false);
	Catalog = NewObject<UInventoryComponent>(this);
	if (!InventoryDemo::Populate(Catalog)) return false;
	TArray<FInventoryItemProfile> Profiles;
	TSet<FName> Types;
	for (const auto& Entry : Catalog->GetEntries())
	{
		const FName Type = Entry.Item.Definition->ItemId;
		if (!Type.ToString().StartsWith(TEXT("Demo_")) && !Types.Contains(Type))
		{
			FInventoryItemProfile Profile, Existing;
			if (Catalog->GetProfile(Entry.ProfileId, Profile))
			{
				if (!Inventory->GetProfile(Profile.Id, Existing)) Inventory->RegisterProfile(Profile);
				if (Inventory->GetProfile(Profile.Id, Existing) && Existing.Definition == Profile.Definition)
				{ Profiles.Add(Profile); Types.Add(Type); }
			}
		}
		FItemInstance Removed; Catalog->RemoveItem(Entry.Item.InstanceId, Removed);
	}
	if (Profiles.IsEmpty()) return false;
	FInventoryPocket Staging; Staging.Id = TEXT("FloorPileStaging");
	Catalog->AddPocket(Staging);
	FRandomStream Random(RandomSeed);
	const FVector Feet = Player->GetActorLocation() - FVector(0,0,Player->GetSimpleCollisionHalfHeight());
	const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Player->GetActorRightVector().GetSafeNormal2D();
	const FVector Center = Feet + Forward * 120 - Right * 70;
	const int32 Count = FMath::Clamp(ItemCount, 17, 128);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(InventoryTestPile), false, Player);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Include every existing item type, then fill the remainder randomly.
		const auto& Profile = Profiles[Index < Profiles.Num() ? Index : Random.RandRange(0, Profiles.Num()-1)];
		const float Angle = Random.FRandRange(0,2*PI), Radius = 70 * FMath::Sqrt(Random.FRand());
		const FVector Point = Center + Forward * (FMath::Cos(Angle)*Radius) + Right * (FMath::Sin(Angle)*Radius);
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, Point+FVector(0,0,120), Point-FVector(0,0,600),
			FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Floor.bStartPenetrating || Floor.ImpactNormal.Z < .7f) continue;
		const auto Item = FItemInstance::Create(Profile.Definition);
		if (Catalog->AddItem(Item, Profile.Id, Staging.Id, FVector2D(280), 0) != EInventoryResult::Success) continue;
		FString Error;
		auto* Dropped = ADroppedItem::DropFromInventory(Catalog, Item.InstanceId, Player, Error);
		if (Dropped)
		{
			Dropped->SetActorLocationAndRotation(Floor.ImpactPoint+FVector(0,0,45), FRotator(0,Random.FRandRange(0,360),0),
				false,nullptr,ETeleportType::TeleportPhysics);
			SpawnedItems.Add(Dropped);
		}
		else { FItemInstance Removed; Catalog->RemoveItem(Item.InstanceId, Removed); }
	}
	if (SpawnedItems.Num() != Count)
		UE_LOG(LogTemp, Warning, TEXT("Inventory test pile created %d of %d items; check nearby static floor and item assets."), SpawnedItems.Num(), Count);
	return SpawnedItems.Num() == Count;
}

void AInventoryTestPile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WaitSeconds += DeltaSeconds;
	auto* Controller = GetWorld()->GetFirstPlayerController();
	if (Controller && Controller->IsLocalPlayerController()) SpawnForPlayer(Controller->GetPawn());
	if (!bSpawnAttempted && WaitSeconds >= 10)
	{
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Warning, TEXT("Inventory test pile could not find an initialized local player within 10 seconds."));
	}
}
