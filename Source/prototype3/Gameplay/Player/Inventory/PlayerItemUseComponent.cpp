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
#include "UI/Inventory/InventoryInputSettings.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"

UPlayerItemUseComponent::UPlayerItemUseComponent() { PrimaryComponentTick.bCanEverTick = true; }

FName UPlayerItemUseComponent::QuickPocketId(int32 Index)
{
	static const FName Ids[] = { TEXT("Quick"), TEXT("Quick2"), TEXT("Quick3") };
	return Index >= 0 && Index < QuickPocketCount ? Ids[Index] : NAME_None;
}

bool UPlayerItemUseComponent::IsQuickPocket(FName Pocket)
{
	for (int32 Index = 0; Index < QuickPocketCount; ++Index)
		if (Pocket == QuickPocketId(Index)) return true;
	return false;
}

void UPlayerItemUseComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnablePrototype) { SetComponentTickEnabled(false); return; }
	Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
	Vitals = GetOwner()->FindComponentByClass<UPlayerVitalsComponent>();
	if (!Inventory || !Vitals) return;
	Vitals->OnHealthChanged.AddDynamic(this, &UPlayerItemUseComponent::HealthChanged);
	Vitals->OnDamageApplied.AddDynamic(this, &UPlayerItemUseComponent::DamageApplied);
	// Reuse the saved definitions and silhouettes supplied by the existing laboratory.
	UInventoryComponent* Catalog = NewObject<UInventoryComponent>(this);
	if (!InventoryDemo::Populate(Catalog)) { Status = TEXT("Could not load sample items."); return; }
	for (int32 Index = 0; Index < QuickPocketCount; ++Index)
	{
		FInventoryPocket Quick;
		Quick.Id = QuickPocketId(Index); Quick.Size = QuickSize;
		Quick.MaxItemMassKg = QuickMaxItemMassKg; Quick.bAllowFirearms = false;
		Inventory->AddPocket(Quick);
	}
	FInventoryPocket Bag;
	Bag.Id = TEXT("Backpack"); Bag.Size = BackpackSize;
	FInventoryPocket Staging;
	Staging.Id = TEXT("Setup");
	Inventory->AddPocket(Bag); Inventory->AddPocket(Staging);
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
		const FName Destination = bQuick ? QuickPocketId(0) : Bag.Id;
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

	Status = TEXT("Tap a quick-item key to take it in hands; hold to use. Release stops unfinished use.");
}

bool UPlayerItemUseComponent::CanAccess(FName Pocket) const
{
	return IsQuickPocket(Pocket) || (Pocket == TEXT("Backpack") && bBackpackEquipped && bBackpackOpen);
}

bool UPlayerItemUseComponent::EquipToHands(FGuid Id)
{
	FInventoryEntry Entry;
	if (!Inventory || !Vitals || !Vitals->IsAlive() || !Inventory->GetItem(Id, Entry) || !CanAccess(Entry.PocketId)) return false;
	if (HeldId == Id) return true;
	if (Inventory->IsReserved(Id)) return false;
	if (!Inventory->ReserveItem(Id)) return false;
	Stow();
	HeldId = Id;
	if (auto* Melee = GetOwner()->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	Status = GetHeldName() + TEXT(" in hands.");
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

const UHealingItemActionData* UPlayerItemUseComponent::HealingAction(bool bSecondary) const
{
	FInventoryEntry Entry;
	if (!Inventory || !Inventory->GetItem(HeldId, Entry)) return nullptr;
	return Cast<UHealingItemActionData>(bSecondary ? Entry.Item.Definition->SecondaryAction : Entry.Item.Definition->PrimaryAction);
}

bool UPlayerItemUseComponent::HandlePrimaryAction()
{
	return PressItemAction(false);
}

bool UPlayerItemUseComponent::PressItemAction(bool bSecondary)
{
	if (bOpening) return true;
	if (!HasHeldItem()) return false;
	if (bUsing) return true;
	FInventoryEntry Entry;
	if (!Inventory || !Inventory->GetItem(HeldId, Entry)) return true;
	const UItemActionData* Contract = bSecondary ? Entry.Item.Definition->SecondaryAction : Entry.Item.Definition->PrimaryAction;
	if (!Contract) return true; // A missing slot consumes its input without falling back to unarmed/another slot.
	const auto* Action = HealingAction(bSecondary);
	if (!Action) { Status = TEXT("This item's action is not implemented yet."); return true; }
	if (!Vitals || !Vitals->IsAlive() || Vitals->GetCurrentHealth() >= Vitals->GetMaxHealth())
	{ Status = TEXT("No healing needed. Item preserved."); return true; }
	if (!Action->IsValidActionData() || !FMath::IsFinite(Action->UseSeconds) || Action->UseSeconds <= 0) return true;
	if (auto* Character = Cast<Aprototype3Character>(GetOwner())) Character->PrepareForItemUse();
	if (auto* Melee = GetOwner()->FindComponentByClass<UPlayerMeleeComponent>()) Melee->StopAttacking();
	bActiveSecondary = bSecondary;
	Duration = Action->UseSeconds; Elapsed = 0; bUsing = true;
	Status = TEXT("Using bandage. Release to stop; an applicable conflicting action interrupts use.");
	return true;
}

void UPlayerItemUseComponent::ReleaseItemAction(bool bSecondary)
{
	if (bUsing && !bUsingFromQuickKey && bActiveSecondary == bSecondary) CancelUse();
}

void UPlayerItemUseComponent::CancelUse()
{
	if (bUsing) Status = TEXT("Use cancelled. Item preserved.");
	bUsing = false; if (!bOpening) Elapsed = 0;
	HeldQuickItem = INDEX_NONE;
	QuickHoldElapsed = 0;
	bQuickUseAttempted = bUsingFromQuickKey = false;
}

void UPlayerItemUseComponent::HealthChanged(float Current, float Maximum, float Percentage)
{
	if (Current <= 0) { Stow(); CloseBackpack(); }
	if (auto* Character = Cast<Aprototype3Character>(GetOwner())) Character->OnHealthUpdated.Broadcast(Percentage);
}

void UPlayerItemUseComponent::DamageApplied(float Amount, bool bInterruptActions)
{
	if (!bInterruptActions || Amount <= 0) return;
	const bool bInterrupted = bUsing || bOpening || bBackpackOpen;
	CancelUse(); CloseBackpack(); UpdateVisual();
	if (bInterrupted) Status = TEXT("Hurt: inventory and item use interrupted.");
}

void UPlayerItemUseComponent::PressQuickItem(int32 Slot)
{
	const FName Type = UItemUseSettings::ItemType(Slot);
	if (!Inventory || Type.IsNone() || bOpening || HeldQuickItem == Slot) return;
	for (const auto& Entry : Inventory->GetEntries())
		if (IsQuickPocket(Entry.PocketId) && Entry.Item.Definition->ItemId == Type && EquipToHands(Entry.Item.InstanceId))
		{
			CancelUse();
			HeldQuickItem = Slot;
			QuickHoldElapsed = 0;
			bQuickUseAttempted = bUsingFromQuickKey = false;
			return;
		}
	Status = UItemUseSettings::Label(Slot) + TEXT(" is not available in your pockets.");
}

void UPlayerItemUseComponent::ReleaseQuickItem(int32 Slot)
{
	if (HeldQuickItem == Slot) CancelQuickItemHold();
}

void UPlayerItemUseComponent::CancelQuickItemHold()
{
	if (bUsingFromQuickKey) CancelUse();
	HeldQuickItem = INDEX_NONE;
	QuickHoldElapsed = 0;
	bQuickUseAttempted = bUsingFromQuickKey = false;
}

float UPlayerItemUseComponent::BackpackHoldThreshold() const
{
	const auto* Character = Cast<Aprototype3Character>(GetOwner());
	return Character ? FMath::Max(0.f, Character->GetGaitHoldThreshold()) : .25f;
}

bool UPlayerItemUseComponent::BeginOpenBackpack(bool bSelectMode)
{
	if (!Backpack.IsValid() || !bBackpackEquipped || !Vitals || !Vitals->IsAlive()) return false;
	if (IsBackpackActive()) return true;
	CancelUse();
	if (auto* Character = Cast<Aprototype3Character>(GetOwner())) Character->PrepareForItemUse();
	BackpackMode = bSelectMode ? EBackpackMode::Selecting : EBackpackMode::Quick;
	bOpening = true;
	BackpackInputElapsed = Elapsed = 0;
	BackpackInputStartTime = FPlatformTime::Seconds();
	Duration = FMath::Max(.01f, QuickBackpackOpenSeconds);
	Status = TEXT("Opening backpack...");
	if (!bSelectMode) SelectQuickBackpack();
	return true;
}

void UPlayerItemUseComponent::StopInventoryMovement()
{
	if (auto* Character = Cast<Aprototype3Character>(GetOwner())) Character->StopInventoryMovement();
	else if (auto* OtherCharacter = Cast<ACharacter>(GetOwner())) OtherCharacter->GetCharacterMovement()->StopMovementImmediately();
}

void UPlayerItemUseComponent::SelectQuickBackpack()
{
	BackpackMode = EBackpackMode::Quick;
	Duration = FMath::Max(.01f, QuickBackpackOpenSeconds);
	Elapsed = BackpackInputElapsed;
	if (auto* Character = Cast<ACharacter>(GetOwner()))
	{
		bPreviouslyCrouched = Character->bIsCrouched || Character->GetCharacterMovement()->bWantsToCrouch;
		bRestoreBackpackStance = true;
		Character->Crouch();
	}
	StopInventoryMovement();
}

void UPlayerItemUseComponent::ReleaseBackpackInput()
{
	if (!bOpening) return; // Releasing after slow mode opens leaves it open.
	const double HeldSeconds = FMath::Max<double>(BackpackInputElapsed, FPlatformTime::Seconds() - BackpackInputStartTime);
	if (BackpackMode == EBackpackMode::Slow || (BackpackMode == EBackpackMode::Selecting && HeldSeconds >= BackpackHoldThreshold()))
	{ CloseBackpack(); return; }
	if (BackpackMode != EBackpackMode::Selecting) return;
	if (const auto* Character = Cast<Aprototype3Character>(GetOwner()); Character && !Character->GetMovementIntent().IsNearlyZero())
	{ CloseBackpack(); return; }
	SelectQuickBackpack();
}

void UPlayerItemUseComponent::CloseBackpack()
{
	CancelArrangement();
	bOpening = bBackpackOpen = false;
	BackpackMode = EBackpackMode::Closed;
	if (!bUsing) Elapsed = 0;
	if (bRestoreBackpackStance)
	{
		if (auto* Character = Cast<ACharacter>(GetOwner()))
		{
			if (bPreviouslyCrouched) Character->Crouch();
			else Character->UnCrouch(); // Character Movement still owns clearance.
		}
		bRestoreBackpackStance = false;
	}
}

bool UPlayerItemUseComponent::ValidateArrangement(const FInventoryArrangement& Request, FInventoryEntry& Source) const
{
	if (!Inventory || !Vitals || !Vitals->IsAlive()) return false;
	auto* Player = Cast<APawn>(GetOwner());
	if (Request.Kind == EInventoryArrangement::PickUp || Request.Kind == EInventoryArrangement::FloorDrop)
	{
		auto* Floor = Request.FloorItem.Get();
		if (!Floor || !Floor->CanInteract(Player) || Floor->GetItem().InstanceId != Request.ItemId) return false;
		Source.Item = Floor->GetItem(); Source.ProfileId = Floor->GetInventoryProfileId();
		if (Request.Kind == EInventoryArrangement::FloorDrop) return Floor->WouldMoveAtFeet(Player);
		FInventoryEntry Existing;
		return !Inventory->GetItem(Request.ItemId, Existing) && CanAccess(Request.Pocket)
			&& Inventory->CheckPlacement(Source.ProfileId, Request.Pocket, Request.Position, Request.Angle) == EInventoryResult::Success;
	}
	if (!Inventory->GetItem(Request.ItemId, Source) || (Inventory->IsReserved(Request.ItemId) && HeldId != Request.ItemId)) return false;
	// A held item's reserved home can be in the closed backpack; stowing remains available.
	if (Request.Kind == EInventoryArrangement::Stow) return HeldId == Request.ItemId;
	if (!CanAccess(Source.PocketId)) return false;
	switch (Request.Kind)
	{
	case EInventoryArrangement::Move:
		if (Source.PocketId == Request.Pocket && Source.Position.Equals(Request.Position, .001)
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(Source.AngleDegrees, Request.Angle)) < .001) return false;
		return CanAccess(Request.Pocket) && Inventory->CheckMove(Request.ItemId, Request.Pocket, Request.Position, Request.Angle, HeldId == Request.ItemId) == EInventoryResult::Success;
	case EInventoryArrangement::Take: return HeldId != Request.ItemId && !Inventory->IsReserved(Request.ItemId);
	case EInventoryArrangement::Drop: return Player && ADroppedItem::HasWorldRepresentation(Source.Item);
	default: return false;
	}
}

bool UPlayerItemUseComponent::CommitArrangement(const FInventoryArrangement& Request)
{
	FInventoryEntry Source;
	if (!ValidateArrangement(Request, Source)) { Status = TEXT("Destination or item changed. Arrangement cancelled."); return false; }
	auto* Player = Cast<APawn>(GetOwner());
	bool bSuccess = false;
	FString Error;
	switch (Request.Kind)
	{
	case EInventoryArrangement::Move:
		bSuccess = Inventory->MoveItem(Request.ItemId, Request.Pocket, Request.Position, Request.Angle, HeldId == Request.ItemId) == EInventoryResult::Success;
		if (bSuccess && HeldId == Request.ItemId) Stow();
		break;
	case EInventoryArrangement::PickUp:
		bSuccess = Request.FloorItem->PickUp(Inventory, Player, Request.Pocket, Request.Position, Request.Angle) == EInventoryResult::Success; break;
	case EInventoryArrangement::Drop:
		bSuccess = ADroppedItem::DropFromInventory(Inventory, Request.ItemId, Player, Error) != nullptr; break;
	case EInventoryArrangement::FloorDrop: bSuccess = Request.FloorItem->DropAtFeet(Player, Error); break;
	case EInventoryArrangement::Take: bSuccess = EquipToHands(Request.ItemId); break;
	case EInventoryArrangement::Stow: Stow(); bSuccess = true; break;
	}
	Status = bSuccess ? TEXT("Arrangement complete.") : Error.IsEmpty() ? TEXT("Arrangement cancelled. Original item preserved.") : Error;
	return bSuccess;
}

bool UPlayerItemUseComponent::RequestArrangement(const FInventoryArrangement& Request)
{
	if (bArranging || bOpening) return false;
	FInventoryEntry Source;
	if (!ValidateArrangement(Request, Source)) { Status = TEXT("Invalid or unchanged arrangement. Item kept in place."); return false; }
	if (BackpackMode != EBackpackMode::Slow || !bBackpackOpen) return CommitArrangement(Request);
	Arrangement = Request; ArrangementSource = Source; ArrangementHeldId = HeldId;
	ArrangementElapsed = 0; bArranging = true;
	StopInventoryMovement();
	Status = TEXT("Arranging item...");
	return true;
}

void UPlayerItemUseComponent::CancelArrangement()
{
	bArranging = false; ArrangementElapsed = 0;
	Arrangement = FInventoryArrangement(); ArrangementSource = FInventoryEntry(); ArrangementHeldId.Invalidate();
}

float UPlayerItemUseComponent::GetArrangementProgress() const
{
	return bArranging ? FMath::Clamp(ArrangementElapsed / FMath::Max(.01f, ArrangementSeconds), 0.f, 1.f) : 0;
}
void UPlayerItemUseComponent::ToggleBackpackEquipment()
{
	if (!Backpack.IsValid()) return;
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
	if (bArranging)
	{
		StopInventoryMovement();
		FInventoryEntry Current;
		const bool bSameSource = ValidateArrangement(Arrangement, Current)
			&& Current.Item.InstanceId == ArrangementSource.Item.InstanceId && Current.Item.Quantity == ArrangementSource.Item.Quantity
			&& Current.Item.Definition == ArrangementSource.Item.Definition && Current.ProfileId == ArrangementSource.ProfileId
			&& Current.PocketId == ArrangementSource.PocketId && Current.Position.Equals(ArrangementSource.Position, .001)
			&& Current.AngleDegrees == ArrangementSource.AngleDegrees && HeldId == ArrangementHeldId;
		if (!bBackpackOpen || !bSameSource) { CancelArrangement(); Status = TEXT("Arrangement cancelled. Source or destination changed."); return; }
		ArrangementElapsed += Seconds;
		if (ArrangementElapsed >= FMath::Max(.01f, ArrangementSeconds))
		{
			const auto Request = Arrangement;
			CancelArrangement();
			CommitArrangement(Request);
		}
		return;
	}
	if (HeldQuickItem != INDEX_NONE && !bQuickUseAttempted && !bUsing)
	{
		QuickHoldElapsed += Seconds;
		const float Threshold = FMath::Max(.1f, QuickUseHoldSeconds);
		if (QuickHoldElapsed >= Threshold)
		{
			bQuickUseAttempted = true;
			// These fixed consumables declare their use in the secondary action slot.
			PressItemAction(true);
			bUsingFromQuickKey = bUsing;
			Seconds = QuickHoldElapsed - Threshold;
		}
	}
	if (bOpening)
	{
		BackpackInputElapsed += Seconds;
		const bool bWalking = GetOwner() && GetOwner()->GetVelocity().SizeSquared2D() > 1.f;
		Elapsed += Seconds * ((BackpackMode == EBackpackMode::Selecting || BackpackMode == EBackpackMode::Slow) && bWalking ? .6f : 1.f);
		if (BackpackMode == EBackpackMode::Selecting && BackpackInputElapsed >= BackpackHoldThreshold())
		{
			BackpackMode = EBackpackMode::Slow;
			Duration = FMath::Max(.01f, SlowBackpackOpenSeconds);
		}
		if (BackpackMode == EBackpackMode::Quick) StopInventoryMovement();
		if (BackpackMode != EBackpackMode::Selecting && Elapsed >= Duration)
		{ bOpening = false; bBackpackOpen = true; Status = TEXT("Backpack open. Select an item to move or take in hands."); }
	}
	else if (bUsing)
	{
		const auto* Action = HealingAction(bActiveSecondary);
		if (!Action || !Vitals || !Vitals->IsAlive()) { CancelUse(); return; }
		Elapsed += Seconds;
		if (Elapsed >= Duration)
		{
			const float Amount = static_cast<float>(Action->HealAmount);
			if (Vitals->GetCurrentHealth() < Vitals->GetMaxHealth() && Inventory->ConsumeReservedItem(HeldId))
			{
				bUsing = false; bUsingFromQuickKey = false; HeldId.Invalidate(); Vitals->Heal(Amount);
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
	CloseBackpack();
	Stow();
	if (Vitals) Vitals->OnHealthChanged.RemoveDynamic(this, &UPlayerItemUseComponent::HealthChanged);
	if (Vitals) Vitals->OnDamageApplied.RemoveDynamic(this, &UPlayerItemUseComponent::DamageApplied);
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
