#include "Gameplay/Player/Interaction/PlayerInteractionComponent.h"
#include "Gameplay/Player/Interaction/Interactable.h"
#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UPlayerInteractionComponent::CanTarget() const
{
	const auto* Player = Cast<Aprototype3Character>(GetOwner());
	const auto* Controller = Player ? Cast<Aprototype3PlayerController>(Player->GetController()) : nullptr;
	const auto* Use = Player ? Player->FindComponentByClass<UPlayerItemUseComponent>() : nullptr;
	return Controller && Controller->IsLocalController() && !Controller->IsInteractionInputBlocked()
		&& Player->IsAlive() && (!Use || !Use->IsUsing());
}

AActor* UPlayerInteractionComponent::TraceTarget() const
{
	if (!CanTarget()) return nullptr;

	const auto* Pawn = CastChecked<APawn>(GetOwner());
	const auto* Controller = CastChecked<APlayerController>(Pawn->GetController());
	const auto* Camera = Controller->PlayerCameraManager.Get();
	if (!Camera) return nullptr;

	const FVector Start = Camera->GetCameraLocation();
	const FVector End = Start + Camera->GetCameraRotation().Vector() * UPlayerItemUseComponent::FloorPickupTraceDistance;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerInteraction), false, Pawn);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
	AActor* Target = bHit ? Hit.GetActor() : nullptr;

	// Floor items intentionally ignore Visibility, including when aiming the builder.
	// Keep the inventory trace and take its item only if it is in front of the visibility hit.
	if (const auto* Use = Pawn->FindComponentByClass<UPlayerItemUseComponent>())
	{
		float ItemDistance = 0.f;
		auto* Item = Use->GetLookedAtFloorItem(&ItemDistance);
		if (Item && (!bHit || ItemDistance < Hit.Distance)) Target = Item;
	}

	if (Target && !Cast<IInteractable>(Target)) Target = Target->GetOwner();
	const auto* Interactable = Cast<IInteractable>(Target);
	return Interactable && Interactable->CanInteract(Pawn) ? Target : nullptr;
}

void UPlayerInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	CurrentTarget = TraceTarget();
}

AActor* UPlayerInteractionComponent::GetCurrentTarget() const
{
	return CanTarget() ? CurrentTarget.Get() : nullptr;
}

FText UPlayerInteractionComponent::GetPrompt() const
{
	const auto* Interactable = Cast<IInteractable>(GetCurrentTarget());
	return Interactable && Interactable->CanInteract(GetOwner())
		? Interactable->GetInteractionPrompt(GetOwner()) : FText::GetEmpty();
}

void UPlayerInteractionComponent::TryInteract()
{
	CurrentTarget = TraceTarget();
	if (auto* Interactable = Cast<IInteractable>(CurrentTarget.Get())) Interactable->Interact(GetOwner());
}
