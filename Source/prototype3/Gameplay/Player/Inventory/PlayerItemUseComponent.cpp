#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Gameplay/Player/Inventory/InventoryTestSpikes.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Combat/Melee/PlayerMeleeComponent.h"
#include "Gameplay/Items/ItemActionData.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "UI/Inventory/InventoryDemoData.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Core/Characters/prototype3Character.h"
#include "GameFramework/PlayerController.h"

UPlayerItemUseComponent::UPlayerItemUseComponent() { PrimaryComponentTick.bCanEverTick = true; }

void UPlayerItemUseComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnablePrototype) { SetComponentTickEnabled(false); return; }
	Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
	Vitals = GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	if (!Inventory || !Vitals) return;
	Shortcuts = DuplicateObject<UItemUseSettings>(GetDefault<UItemUseSettings>(), this);
	Shortcuts->Normalize();
	PreviousHealth = Vitals->GetCurrentHealth();
	Vitals->OnHealthChanged.AddDynamic(this, &UPlayerItemUseComponent::HealthChanged);
	// Reuse the saved definitions and silhouettes supplied by the existing laboratory.
	UInventoryComponent* Catalog = NewObject<UInventoryComponent>(this);
	if (!InventoryDemo::Populate(Catalog)) { Status = TEXT("Could not load sample items."); return; }
	FInventoryPocket Quick;
	Quick.Id = TEXT("Quick"); Quick.Size = QuickSize; Quick.MaxItemMassKg = QuickMaxItemMassKg; Quick.bAllowFirearms = false;
	FInventoryPocket Bag;
	Bag.Id = TEXT("Backpack"); Bag.Size = BackpackSize;
	FInventoryPocket Staging;
	Staging.Id = TEXT("Setup");
	Inventory->AddPocket(Quick); Inventory->AddPocket(Bag); Inventory->AddPocket(Staging);
	int32 Bandages = 0;
	for (const auto& Entry : Catalog->GetEntries())
	{
		const FName Id = Entry.Item.Definition->ItemId;
		if (Id.ToString().StartsWith(TEXT("Demo_"))) continue;
		if (Id == TEXT("SmallBackpack")) { Backpack = Entry.Item; continue; }
		FInventoryItemProfile Profile, Existing;
		Catalog->GetProfile(Entry.ProfileId, Profile);
		if (!Inventory->GetProfile(Profile.Id, Existing)) Inventory->RegisterProfile(Profile);
		const bool bQuick = Id == TEXT("CannedBeans") || Id == TEXT("WaterBottle") || (Id == TEXT("Bandage") && Bandages++ == 0);
		if (Inventory->AddItem(Entry.Item, Profile.Id, Staging.Id, Entry.Position, Entry.AngleDegrees) != EInventoryResult::Success) continue;
		FVector2D Position;
		const FName Destination = bQuick ? Quick.Id : Bag.Id;
		if (Inventory->FindSpace(Entry.Item.InstanceId, Destination, Position)) Inventory->MoveItem(Entry.Item.InstanceId, Destination, Position, Entry.AngleDegrees);
		else UE_LOG(LogTemp, Error, TEXT("Sample %s could not fit in the player inventory."), *Id.ToString());
	}
	if (auto* Camera = GetOwner()->FindComponentByClass<UCameraComponent>())
	{
		HeldVisual = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("HeldItemPrototype"));
		// The current idle pose leaves both hands below the camera. Keep the
		// prototype prop visible until a dedicated holding/consuming pose exists.
		HeldVisual->SetupAttachment(Camera);
		HeldVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		HeldVisual->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		HeldVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldVisual->SetCastShadow(false); HeldVisual->SetOnlyOwnerSee(true);
		HeldVisual->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		HeldVisual->SetRelativeScale3D(FVector(0.08,0.08,0.06));
		HeldVisual->RegisterComponent();
		UpdateVisual();
	}

	Status = TEXT("Shortcut: item to hands. Left click: use. Marked spikes hurt, but cannot kill.");
}

bool UPlayerItemUseComponent::CanAccess(FName Pocket) const
{
	return Pocket == TEXT("Quick") || (Pocket == TEXT("Backpack") && bBackpackEquipped && bBackpackOpen);
}

bool UPlayerItemUseComponent::EquipToHands(FGuid Id)
{
	FInventoryEntry Entry;
	if (!Inventory || !Vitals || !Vitals->IsAlive() || !Inventory->GetItem(Id, Entry) || !CanAccess(Entry.PocketId)) return false;
	if (HeldId == Id) return true;
	if (Inventory->IsReserved(Id)) return false;
	Stow();
	if (!Inventory->ReserveItem(Id)) return false;
	HeldId = Id;
	if (auto* Melee = GetOwner()->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	Status = GetHeldName() + TEXT(" in hands. Left click to use; right click to stow.");
	UpdateVisual();
	return true;
}

void UPlayerItemUseComponent::Stow()
{
	CancelUse();
	if (Inventory) Inventory->ReleaseItem(HeldId);
	HeldId.Invalidate();
	UpdateVisual();
}

const UHealingItemActionData* UPlayerItemUseComponent::HealingAction() const
{
	FInventoryEntry Entry;
	if (!Inventory || !Inventory->GetItem(HeldId, Entry)) return nullptr;
	if (auto* Heal = Cast<UHealingItemActionData>(Entry.Item.Definition->PrimaryAction)) return Heal;
	return Cast<UHealingItemActionData>(Entry.Item.Definition->SecondaryAction);
}

bool UPlayerItemUseComponent::HandlePrimaryAction()
{
	if (bOpening) return true;
	if (!HasHeldItem()) return false;
	if (bUsing) return true;
	const auto* Action = HealingAction();
	if (!Action) { Status = TEXT("This sample's action is not implemented yet. Right click to stow."); return true; }
	if (!Vitals || !Vitals->IsAlive() || Vitals->GetCurrentHealth() >= Vitals->GetMaxHealth())
	{ Status = TEXT("No healing needed. Item preserved."); return true; }
	if (!Action->IsValidActionData() || !FMath::IsFinite(Action->UseSeconds) || Action->UseSeconds <= 0) return true;
	Duration = Action->UseSeconds; Elapsed = 0; bUsing = true;
	Status = TEXT("Using bandage. Walk only. Damage cancels use.");
	return true;
}

void UPlayerItemUseComponent::CancelUse()
{
	if (bUsing) Status = TEXT("Use cancelled. Item preserved.");
	bUsing = false; Elapsed = 0;
}

void UPlayerItemUseComponent::HealthChanged(float Current, float Maximum, float Percentage)
{
	if (Current < PreviousHealth && bUsing)
	{
		CancelUse();
		UpdateVisual();
		Status = TEXT("Hurt: healing cancelled. Left click to try again.");
	}
	PreviousHealth = Current;
	if (Current <= 0) { Stow(); CloseBackpack(); }
	if (auto* Character = Cast<Aprototype3Character>(GetOwner())) Character->OnHealthUpdated.Broadcast(Percentage);
}

void UPlayerItemUseComponent::HandleShortcut(int32 Slot, double Time)
{
	if (!Shortcuts || !Shortcuts->ItemTypes.IsValidIndex(Slot) || bUsing || bOpening) return;
	if (Slot == LastShortcut && Time >= LastShortcutTime && Time - LastShortcutTime <= DoubleTapSeconds)
	{
		TArray<FName> Types;
		for (const auto& Entry : Inventory->GetEntries())
			if (Entry.PocketId == TEXT("Quick") && (Entry.Item.Definition->PrimaryAction || Entry.Item.Definition->SecondaryAction)) Types.AddUnique(Entry.Item.Definition->ItemId);
		if (!Types.IsEmpty())
		{
			Shortcuts->ItemTypes[Slot] = Types[(Types.IndexOfByKey(Shortcuts->ItemTypes[Slot]) + 1) % Types.Num()];
			SaveShortcuts();
		}
		LastShortcut = INDEX_NONE;
	}
	else { LastShortcut = Slot; LastShortcutTime = Time; }
	for (const auto& Entry : Inventory->GetEntries())
		if (Entry.PocketId == TEXT("Quick") && Entry.Item.Definition->ItemId == Shortcuts->ItemTypes[Slot]) { EquipToHands(Entry.Item.InstanceId); return; }
}

void UPlayerItemUseComponent::SaveShortcuts()
{
	if (!bSavePreferences || !Shortcuts) return;
	auto* Saved = GetMutableDefault<UItemUseSettings>();
	Saved->Keys = Shortcuts->Keys; Saved->ItemTypes = Shortcuts->ItemTypes; Saved->SaveConfig();
}

bool UPlayerItemUseComponent::BeginOpenBackpack()
{
	if (!Backpack.IsValid() || !bBackpackEquipped || bUsing || !Vitals || !Vitals->IsAlive()) return false;
	if (!bBackpackOpen && !bOpening) { bOpening = true; Elapsed = 0; Duration = FMath::Max(0.01f, BackpackOpenSeconds); Status = TEXT("Opening backpack..."); }
	return true;
}
void UPlayerItemUseComponent::CloseBackpack() { bOpening = bBackpackOpen = false; if (!bUsing) Elapsed = 0; }
void UPlayerItemUseComponent::ToggleBackpackEquipment()
{
	if (!Backpack.IsValid() || bUsing) return;
	Stow(); CloseBackpack(); bBackpackEquipped = !bBackpackEquipped;
	Status = bBackpackEquipped ? TEXT("Backpack equipped.") : TEXT("Backpack unequipped. Contents preserved.");
}
bool UPlayerItemUseComponent::Transfer(FGuid Id, FName Pocket)
{
	FInventoryEntry Entry;
	if (!Inventory || !Inventory->GetItem(Id, Entry) || !CanAccess(Entry.PocketId) || !CanAccess(Pocket)) return false;
	FVector2D Position;
	if (!Inventory->FindSpace(Id, Pocket, Position)) { Status = TEXT("Cannot transfer: size, weight, item type, occupied space or item in hands."); return false; }
	return Inventory->MoveItem(Id, Pocket, Position, Entry.AngleDegrees) == EInventoryResult::Success;
}

void UPlayerItemUseComponent::Advance(float Seconds)
{
	if (!FMath::IsFinite(Seconds) || Seconds <= 0) return;
	if (bOpening) { Elapsed += Seconds; if (Elapsed >= Duration) { bOpening = false; bBackpackOpen = true; Status = TEXT("Backpack open. Select an item to move or take in hands."); } }
	else if (bUsing)
	{
		const auto* Action = HealingAction();
		if (!Action || !Vitals || !Vitals->IsAlive()) { CancelUse(); return; }
		Elapsed += Seconds;
		if (Elapsed >= Duration)
		{
			const float Amount = static_cast<float>(Action->HealAmount);
			if (Vitals->GetCurrentHealth() < Vitals->GetMaxHealth() && Inventory->ConsumeReservedItem(HeldId))
			{
				bUsing = false; HeldId.Invalidate(); Vitals->Heal(Amount);
				Status = TEXT("Bandage used. Health restored.");
			}
			else CancelUse();
			UpdateVisual();
		}
	}
}
float UPlayerItemUseComponent::GetProgress() const { return Duration > 0 ? FMath::Clamp(Elapsed / Duration, 0.f, 1.f) : 0; }
FString UPlayerItemUseComponent::GetHeldName() const
{
	FInventoryEntry Entry;
	return Inventory && Inventory->GetItem(HeldId, Entry) ? Entry.Item.Definition->DisplayName.ToString() : TEXT("Empty hands");
}
void UPlayerItemUseComponent::UpdateVisual()
{
	if (!HeldVisual) return;
	HeldVisual->SetVisibility(HasHeldItem());
	const float Motion = bUsing ? FMath::Sin(GetProgress()*PI*6) : 0;
	HeldVisual->SetRelativeLocation(FVector(35,12,-13 + Motion*1.5));
	HeldVisual->SetRelativeRotation(FRotator(75 + Motion*8,0,0));
}
void UPlayerItemUseComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(Delta, Type, Tick); TrySpawnSpikes(); Advance(Delta); UpdateVisual();
}
void UPlayerItemUseComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Stow();
	if (Vitals) Vitals->OnHealthChanged.RemoveDynamic(this, &UPlayerItemUseComponent::HealthChanged);
	if (Spikes) Spikes->Destroy();
	Super::EndPlay(Reason);
}

void UPlayerItemUseComponent::TrySpawnSpikes()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (bSpikesSpawnAttempted || !Player || !Player->IsLocalPlayerController()) return;
	bSpikesSpawnAttempted = true;
	if (bSpawnTestSpikes)
	{
		FVector Location = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector()*240 + GetOwner()->GetActorRightVector()*120;
		FHitResult Floor;
		FCollisionQueryParams Query; Query.AddIgnoredActor(GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Floor, Location, Location - FVector(0,0,500), ECC_Visibility, Query))
			Spikes = GetWorld()->SpawnActor<AInventoryTestSpikes>(Floor.ImpactPoint + FVector(0,0,3), FRotator(0,GetOwner()->GetActorRotation().Yaw+180,0));
	}
}
