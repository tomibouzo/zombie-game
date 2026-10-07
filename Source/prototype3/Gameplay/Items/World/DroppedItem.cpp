#include "Gameplay/Items/World/DroppedItem.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Provisional centimetres, deliberately independent of the inventory's logical silhouettes.
struct FPlaceholder
{
	FName Id;
	FVector Size;
	bool bCylinder;
	FLinearColor Color;
	FRotator RestingRotation = FRotator::ZeroRotator;
	FVector RestingSize() const { return RestingRotation.RotateVector(Size).GetAbs(); }
};
const FPlaceholder Placeholders[] = {
	{TEXT("Bandage"), FVector(12, 8, 4), false, FLinearColor(0.85f, 0.65f, 0.3f)},
	{TEXT("CannedBeans"), FVector(8, 8, 11), true, FLinearColor(0.12f, 0.4f, 0.06f)},
	{TEXT("WaterBottle"), FVector(7, 7, 22), true, FLinearColor(0.03f, 0.3f, 0.8f)},
	{TEXT("Knife"), FVector(26, 4, 2), false, FLinearColor(0.35f, 0.4f, 0.5f)},
	{TEXT("Pistol"), FVector(22, 4, 14), false, FLinearColor(0.04f, 0.06f, 0.1f), FRotator(0, 0, 90)},
	{TEXT("Flashlight"), FVector(6, 6, 20), true, FLinearColor(0.9f, 0.22f, 0.02f), FRotator(0, 0, 90)},
	{TEXT("Jacket"), FVector(46, 35, 5), false, FLinearColor(0.2f, 0.35f, 0.08f)},
	{TEXT("SmallBackpack"), FVector(30, 20, 40), false, FLinearColor(0.5f, 0.12f, 0.05f)},
	{TEXT("ScrapMetal"), FVector(20, 14, 2), false, FLinearColor(0.2f, 0.23f, 0.28f)}
};
const FPlaceholder* FindPlaceholder(FName Id)
{
	for (const auto& Spec : Placeholders) if (Spec.Id == Id) return &Spec;
	return nullptr;
}
FTransform DropTransform(APawn* Player, const FPlaceholder& Spec)
{
	FVector Location = Player->GetActorLocation();
	Location.Z += -Player->GetSimpleCollisionHalfHeight() + Spec.RestingSize().Z * 0.5 + 3;
	return FTransform(FRotator(0, Player->GetActorRotation().Yaw, 0), Location);
}
}

ADroppedItem::ADroppedItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetBoxExtent(FVector(5));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetEnableGravity(true);
	Body->BodyInstance.bUseCCD = true;
	Body->SetLinearDamping(0.1f);
	Body->SetAngularDamping(0.8f);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ItemName"));
	Label->SetupAttachment(Body);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(5.f);
	Label->SetTextRenderColor(FColor(255, 235, 160));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PlaceholderMaterial = Material.Object;
}

bool ADroppedItem::Initialize(const FItemInstance& InItem, FName ProfileId)
{
	if (!InItem.IsValid() || !CubeMesh || !CylinderMesh || !PlaceholderMaterial) return false;
	const FPlaceholder* Spec = FindPlaceholder(InItem.Definition->ItemId);
	if (!Spec) return false;
	Item = InItem;
	InventoryProfileId = ProfileId;
	Body->SetBoxExtent(Spec->RestingSize() * 0.5);
	Label->SetText(InItem.Definition->DisplayName);
	Label->SetRelativeLocation(FVector(0, 0, Spec->RestingSize().Z * 0.5 + 7));
	auto* Material = UMaterialInstanceDynamic::Create(PlaceholderMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Spec->Color);
	auto Part = [&](const TCHAR* Name, FVector Size, FVector Position, bool bCylinder = false)
	{
		auto* Mesh = NewObject<UStaticMeshComponent>(this, FName(Name));
		AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(Body);
		Mesh->SetStaticMesh(bCylinder ? CylinderMesh : CubeMesh);
		Mesh->SetMaterial(0, Material);
		Mesh->SetRelativeLocation(Spec->RestingRotation.RotateVector(Position));
		Mesh->SetRelativeRotation(Spec->RestingRotation);
		Mesh->SetRelativeScale3D(Size / 100.0);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
	};
	const FName Id = Spec->Id;
	if (Id == TEXT("Knife"))
	{
		Part(TEXT("Blade"), FVector(17, 4, 1), FVector(4.5, 0, 0));
		Part(TEXT("Handle"), FVector(9, 3, 2), FVector(-8.5, 0, 0));
	}
	else if (Id == TEXT("Pistol"))
	{
		Part(TEXT("Slide"), FVector(22, 4, 4), FVector(0, 0, 5));
		Part(TEXT("Grip"), FVector(6, 4, 10), FVector(-8, 0, -2));
	}
	else if (Id == TEXT("Jacket"))
	{
		Part(TEXT("Torso"), FVector(26, 35, 5), FVector::ZeroVector);
		Part(TEXT("Sleeves"), FVector(46, 10, 5), FVector(0, -12.5, 0));
	}
	else if (Id == TEXT("SmallBackpack"))
	{
		Part(TEXT("Pack"), FVector(30, 16, 40), FVector(0, -2, 0));
		Part(TEXT("FrontPocket"), FVector(22, 4, 18), FVector(0, 8, -8));
	}
	else if (Id == TEXT("WaterBottle"))
	{
		Part(TEXT("Bottle"), FVector(7, 7, 18), FVector(0, 0, -2), true);
		Part(TEXT("Cap"), FVector(3, 3, 4), FVector(0, 0, 9), true);
	}
	else if (Id == TEXT("Flashlight"))
	{
		Part(TEXT("Handle"), FVector(4, 4, 15), FVector(0, 0, -2.5), true);
		Part(TEXT("Head"), FVector(6, 6, 5), FVector(0, 0, 7.5), true);
	}
	else Part(TEXT("Placeholder"), Spec->Size, FVector::ZeroVector, Spec->bCylinder);
	return true;
}

bool ADroppedItem::HasWorldRepresentation(const FItemInstance& Value)
{
	return Value.IsValid() && FindPlaceholder(Value.Definition->ItemId) != nullptr;
}

bool ADroppedItem::WouldMoveAtFeet(APawn* Player) const
{
	const FPlaceholder* Spec = Item.IsValid() ? FindPlaceholder(Item.Definition->ItemId) : nullptr;
	return CanInteract(Player) && Spec && Body->IsSimulatingPhysics()
		&& !GetActorTransform().Equals(DropTransform(Player, *Spec), .1f);
}

ADroppedItem* ADroppedItem::DropFromInventory(UInventoryComponent* Inventory, FGuid InstanceId, APawn* Player, FString& Error)
{
	Error.Empty();
	FInventoryEntry Entry;
	if (!IsValid(Inventory) || !IsValid(Player) || !Player->GetWorld()
		|| !Inventory->GetItem(InstanceId, Entry) || !Entry.Item.IsValid())
	{
		Error = TEXT("Cannot drop this item right now.");
		return nullptr;
	}
	auto* Use = Player->FindComponentByClass<UPlayerItemUseComponent>();
	const bool bHeldByPlayer = Use && Use->Inventory == Inventory && Use->GetHeldId() == InstanceId;
	if (Inventory->IsReserved(InstanceId) && !bHeldByPlayer)
	{
		Error = TEXT("Stow this item before dropping it.");
		return nullptr;
	}
	const FPlaceholder* Spec = FindPlaceholder(Entry.Item.Definition->ItemId);
	if (!Spec)
	{
		Error = TEXT("This test shape stays in the inventory.");
		return nullptr;
	}
	const FTransform Transform = DropTransform(Player, *Spec);
	auto* Dropped = Player->GetWorld()->SpawnActorDeferred<ADroppedItem>(StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Dropped || !Dropped->Initialize(Entry.Item, Entry.ProfileId))
	{
		if (Dropped) Dropped->Destroy();
		Error = TEXT("Could not create the world item. It remains in the inventory.");
		return nullptr;
	}
	Dropped->bTransferring = true;
	Dropped->FinishSpawning(Transform);
	if (!IsValid(Dropped))
	{
		Error = TEXT("Could not create the world item. It remains in the inventory.");
		return nullptr;
	}
	Dropped->Body->SetMassOverrideInKg(NAME_None, FMath::Max(0.01, Entry.Item.Definition->MassKg * Entry.Item.Quantity));
	Dropped->Body->SetSimulatePhysics(true);
	FItemInstance Removed;
	if (!IsValid(Dropped) || !Dropped->Body->IsSimulatingPhysics()
		|| Inventory->RemoveItem(InstanceId, Removed, bHeldByPlayer) != EInventoryResult::Success)
	{
		if (IsValid(Dropped)) Dropped->Destroy();
		Error = TEXT("Could not transfer the item. It remains in the inventory.");
		return nullptr;
	}
	Dropped->Item = Removed;
	if (bHeldByPlayer) Use->Stow();
	Dropped->bTransferring = false;
	return Dropped;
}

bool ADroppedItem::CanInteract(APawn* Player) const
{
	if (!IsValid(this) || IsActorBeingDestroyed() || bTransferring || !Item.IsValid()
		|| !IsValid(Player) || Player->GetWorld() != GetWorld() || !FMath::IsFinite(PickupRadius) || PickupRadius <= 0) return false;
	const FVector Feet = Player->GetActorLocation() - FVector(0, 0, Player->GetSimpleCollisionHalfHeight());
	return FVector::DistSquared(Feet, GetActorLocation()) <= FMath::Square(PickupRadius);
}

TArray<TWeakObjectPtr<ADroppedItem>> ADroppedItem::FindNearby(APawn* Player)
{
	TArray<TWeakObjectPtr<ADroppedItem>> Items;
	if (!IsValid(Player) || !Player->GetWorld()) return Items;
	for (TActorIterator<ADroppedItem> It(Player->GetWorld()); It; ++It)
		if (It->CanInteract(Player)) Items.Add(*It);
	// Stable rows while physics moves items. Duplicate names remain separate instances.
	Items.Sort([](const auto& A, const auto& B)
	{
		const FString NameA = A->GetItem().Definition->DisplayName.ToString();
		const FString NameB = B->GetItem().Definition->DisplayName.ToString();
		return NameA == NameB ? A->GetItem().InstanceId.ToString() < B->GetItem().InstanceId.ToString() : NameA < NameB;
	});
	return Items;
}

EInventoryResult ADroppedItem::PickUp(UInventoryComponent* Inventory, APawn* Player, FName Pocket, FVector2D Position, double Angle)
{
	if (!IsValid(Inventory) || !CanInteract(Player)) return EInventoryResult::InvalidItem;
	// AddItem broadcasts synchronously. Guard against another pickup during that callback.
	TGuardValue<bool> TransferGuard(bTransferring, true);
	const auto Result = Inventory->AddItem(Item, InventoryProfileId, Pocket, Position, Angle);
	if (Result != EInventoryResult::Success) return Result;
	Item = FItemInstance();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	Destroy();
	return EInventoryResult::Success;
}

bool ADroppedItem::DropAtFeet(APawn* Player, FString& Error)
{
	Error.Empty();
	if (!CanInteract(Player)) { Error = TEXT("Floor item is no longer nearby."); return false; }
	const FPlaceholder* Spec = FindPlaceholder(Item.Definition->ItemId);
	if (!Spec || !Body->IsSimulatingPhysics()) { Error = TEXT("Cannot move this floor item."); return false; }
	TGuardValue<bool> TransferGuard(bTransferring, true);
	const FTransform Transform = DropTransform(Player, *Spec);
	if (!SetActorLocationAndRotation(Transform.GetLocation(), Transform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics))
	{
		Error = TEXT("Could not move the floor item. It remains in place.");
		return false;
	}
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Body->WakeAllRigidBodies();
	return true;
}

void ADroppedItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetActorLocation().Z < CleanupZ)
	{
		Destroy();
		return;
	}
	// Keep temporary names readable even after a placeholder tumbles.
	if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
		if (const APlayerCameraManager* Camera = Controller->PlayerCameraManager)
			Label->SetWorldRotation((Camera->GetCameraLocation() - Label->GetComponentLocation()).Rotation());
}
