#include "Gameplay/Shelter/Build/ShelterBuildComponent.h"

#include "Core/Characters/prototype3Character.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Algo/StableSort.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Gameplay/Player/Vitals/PlayerVitalsComponent.h"
#include "Gameplay/Shelter/Build/ShelterBuildArea.h"
#include "Gameplay/Shelter/Build/ShelterPiece.h"
#include "UI/Inventory/InventoryInputSettings.h"

UShelterBuildComponent::UShelterBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

Aprototype3PlayerController* UShelterBuildComponent::Controller() const
{
	return Cast<Aprototype3PlayerController>(GetOwner());
}

void UShelterBuildComponent::SetCatalog(const TArray<UShelterPieceDefinition*>& Pieces)
{
	Catalog.Reset();
	for (UShelterPieceDefinition* Piece : Pieces)
		if (IsValid(Piece) && Piece->IsValidDefinition()
			&& !Catalog.ContainsByPredicate([&](const UShelterPieceDefinition* Other) { return Other->PieceId == Piece->PieceId; }))
			Catalog.Add(Piece);
	bCatalogLoaded = true;
	RebuildCategories();
}

void UShelterBuildComponent::LoadCatalog()
{
	if (bCatalogLoaded) return;
	TArray<UShelterPieceDefinition*> Found;
	if (UAssetManager* Assets = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> Ids;
		Assets->GetPrimaryAssetIdList(UShelterPieceDefinition::PieceAssetType, Ids);
		for (const FPrimaryAssetId& Id : Ids)
			if (auto* Piece = Cast<UShelterPieceDefinition>(Assets->GetPrimaryAssetPath(Id).TryLoad())) Found.Add(Piece);
	}
	SetCatalog(Found);
}

void UShelterBuildComponent::RebuildCategories()
{
	Algo::StableSort(Catalog, [](const TObjectPtr<UShelterPieceDefinition>& A, const TObjectPtr<UShelterPieceDefinition>& B)
	{
		return A->SortOrder != B->SortOrder ? A->SortOrder < B->SortOrder : A->PieceId.LexicalLess(B->PieceId);
	});
	// Categories appear in the order of their first (lowest sort order) piece.
	Categories.Reset();
	for (const UShelterPieceDefinition* Piece : Catalog)
		if (!Categories.ContainsByPredicate([&](const FText& Name) { return Name.EqualTo(Piece->Category); }))
			Categories.Add(Piece->Category);
	CategoryIndex = Categories.IsEmpty() ? 0 : FMath::Clamp(CategoryIndex, 0, Categories.Num() - 1);
	PieceIndex = 0;
}

TArray<UShelterPieceDefinition*> UShelterBuildComponent::GetPiecesInCategory(int32 Index) const
{
	TArray<UShelterPieceDefinition*> Pieces;
	if (!Categories.IsValidIndex(Index)) return Pieces;
	for (UShelterPieceDefinition* Piece : Catalog)
		if (Piece && Piece->Category.EqualTo(Categories[Index])) Pieces.Add(Piece);
	return Pieces;
}

UShelterPieceDefinition* UShelterBuildComponent::GetSelectedPiece() const
{
	const TArray<UShelterPieceDefinition*> Pieces = GetPiecesInCategory(CategoryIndex);
	return Pieces.IsValidIndex(PieceIndex) ? Pieces[PieceIndex] : nullptr;
}

void UShelterBuildComponent::WatchVitals()
{
	auto* Player = Controller();
	auto* Vitals = Player && Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UPlayerVitalsComponent>() : nullptr;
	if (WatchedVitals.Get() == Vitals) return;
	if (WatchedVitals.IsValid())
	{
		WatchedVitals->OnDamageApplied.RemoveDynamic(this, &UShelterBuildComponent::OwnerDamaged);
		WatchedVitals->OnDeath.RemoveDynamic(this, &UShelterBuildComponent::OwnerDied);
	}
	WatchedVitals = Vitals;
	if (Vitals)
	{
		Vitals->OnDamageApplied.AddDynamic(this, &UShelterBuildComponent::OwnerDamaged);
		Vitals->OnDeath.AddDynamic(this, &UShelterBuildComponent::OwnerDied);
	}
}

void UShelterBuildComponent::OwnerDamaged(float Amount, bool bInterruptActions)
{
	if (bInterruptActions && Amount > 0) Close();
}

void UShelterBuildComponent::OwnerDied() { Close(); }

bool UShelterBuildComponent::Open()
{
	auto* Player = Controller();
	auto* Character = Player ? Cast<Aprototype3Character>(Player->GetPawn()) : nullptr;
	if (IsActive() || !Player || !Player->IsLocalController() || !Character || !Character->IsAlive()
		|| Player->IsInventoryInterfaceOpen() || Player->IsPaused()) return false;
	AShelterBuildArea* Found = AShelterBuildArea::FindContaining(GetWorld(), Character->GetActorLocation());
	if (!Found) return false; // Outside a build area the key does nothing.
	LoadCatalog();
	if (Categories.IsEmpty()) return false;
	Area = Found;
	WatchVitals();
	// Cancels item use and a held attack without consuming the item, and forces fresh presses.
	Player->FlushKeysPreservingMovement();
	State = EShelterBuildState::Menu;
	Status.Reset();
	SetComponentTickEnabled(true);
	return true;
}

void UShelterBuildComponent::Close()
{
	if (!IsActive()) return;
	DestroyPreview();
	State = EShelterBuildState::Closed;
	bTargetValid = false;
	Status.Reset();
	Area.Reset();
	SetComponentTickEnabled(false);
	if (auto* Player = Controller()) Player->FlushKeysPreservingMovement();
}

void UShelterBuildComponent::EnterPlacing(int32 InCategory)
{
	if (!Categories.IsValidIndex(InCategory) || GetPiecesInCategory(InCategory).IsEmpty()) return;
	if (InCategory != CategoryIndex) PieceIndex = 0;
	CategoryIndex = InCategory;
	PieceIndex = FMath::Clamp(PieceIndex, 0, GetPiecesInCategory(CategoryIndex).Num() - 1);
	Turns = 0;
	State = EShelterBuildState::Placing;
	RefreshTarget();
}

void UShelterBuildComponent::ReturnToMenu()
{
	DestroyPreview();
	bTargetValid = false;
	Status.Reset();
	State = EShelterBuildState::Menu;
}

void UShelterBuildComponent::Cycle(int32 Step)
{
	if (State == EShelterBuildState::Menu)
	{
		if (!Categories.IsEmpty()) CategoryIndex = (CategoryIndex + Step + Categories.Num()) % Categories.Num();
		PieceIndex = 0;
	}
	else if (State == EShelterBuildState::Placing)
	{
		const int32 Count = GetPiecesInCategory(CategoryIndex).Num();
		if (Count > 0) PieceIndex = (PieceIndex + Step + Count) % Count;
		Turns = 0;
		RefreshTarget();
	}
}

void UShelterBuildComponent::DestroyPreview()
{
	if (IsValid(Preview)) Preview->Destroy();
	Preview = nullptr;
	PreviewPiece = nullptr;
}

void UShelterBuildComponent::RefreshTarget()
{
	bTargetValid = false;
	auto* Player = Controller();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	UShelterPieceDefinition* Piece = GetSelectedPiece();
	if (State != EShelterBuildState::Placing || !Area.IsValid() || !Pawn || !Piece) { DestroyPreview(); return; }
	if (PreviewPiece != Piece || !IsValid(Preview))
	{
		DestroyPreview();
		Preview = Area->SpawnPreview(Piece);
		PreviewPiece = Piece;
		bPreviewTintValid = false;
	}
	FVector Eye; FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	EShelterBuildResult Result = EShelterBuildResult::OutOfBounds;
	if (Area->FindSlotFromView(Eye, View.Vector(), Reach + 150.f, Piece->SlotKind, TargetSlot))
	{
		Result = Area->CanPlace(TargetSlot, Piece);
		const FVector SlotLocation = Area->SlotTransform(TargetSlot, Turns).GetLocation();
		if (Result == EShelterBuildResult::Success && FVector::Dist2D(SlotLocation, Pawn->GetActorLocation()) > Reach)
			Result = EShelterBuildResult::TooFar;
		if (IsValid(Preview)) Preview->SetActorTransform(Area->SlotTransform(TargetSlot, Turns));
	}
	bTargetValid = Result == EShelterBuildResult::Success;
	Status = FShelterBuildModel::Describe(Result);
	if (IsValid(Preview) && bPreviewTintValid != bTargetValid)
	{
		Preview->SetPreview(bTargetValid);
		bPreviewTintValid = bTargetValid;
	}
}

void UShelterBuildComponent::TryPlace()
{
	RefreshTarget();
	UShelterPieceDefinition* Piece = GetSelectedPiece();
	if (!bTargetValid || !Area.IsValid() || !Piece) return;
	const EShelterBuildResult Result = Area->PlacePiece(TargetSlot, Piece, Turns);
	RefreshTarget();
	if (Result == EShelterBuildResult::Success) Status = Piece->DisplayName.ToString() + TEXT(" placed");
}

void UShelterBuildComponent::TryRemove()
{
	auto* Player = Controller();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Area.IsValid() || !Pawn || !GetWorld()) return;
	FVector Eye; FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	// Prefer the piece under the crosshair; otherwise the piece in the aimed slot.
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShelterBuildRemove), false, Pawn);
	EShelterBuildResult Result = EShelterBuildResult::NotFound;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * (Reach + 150.f), ECC_Visibility, Query))
		if (const auto* Piece = Cast<AShelterPiece>(Hit.GetActor()); Piece && FVector::Dist2D(Piece->GetActorLocation(), Pawn->GetActorLocation()) <= Reach)
			Result = Area->RemovePiece(Piece);
	if (Result != EShelterBuildResult::Success && State == EShelterBuildState::Placing)
	{
		RefreshTarget();
		if (FVector::Dist2D(Area->SlotTransform(TargetSlot, 0).GetLocation(), Pawn->GetActorLocation()) <= Reach)
			Result = Area->RemoveAt(TargetSlot);
	}
	RefreshTarget();
	Status = Result == EShelterBuildResult::Success ? FString(TEXT("Piece removed")) : FShelterBuildModel::Describe(EShelterBuildResult::NotFound);
}

bool UShelterBuildComponent::HandleKey(const FKey& Key, EInputEvent Event)
{
	using A = EInventoryControl;
	const auto* Controls = GetDefault<UInventoryInputSettings>();
	const bool bPress = Event == IE_Pressed || Event == IE_DoubleClick;
	if (!IsActive())
	{
		if (!Controls->Matches(A::BuildMenu, Key)) return false;
		if (bPress) Open();
		return true;
	}
	// Opening the backpack or pockets leaves build mode and then opens normally.
	if (Controls->Matches(A::Toggle, Key) || Controls->Matches(A::ShowQuick, Key))
	{
		if (bPress) Close();
		return false;
	}
	if (Controls->Matches(A::BuildExit, Key)) { if (bPress) Close(); return true; }
	if (Controls->Matches(A::BuildMenu, Key))
	{
		if (bPress && State == EShelterBuildState::Placing) ReturnToMenu();
		return true;
	}
	if (Controls->Matches(A::BuildPrevious, Key)) { if (bPress) Cycle(-1); return true; }
	if (Controls->Matches(A::BuildNext, Key)) { if (bPress) Cycle(1); return true; }
	if (Controls->Matches(A::BuildRotate, Key))
	{
		if (bPress && State == EShelterBuildState::Placing) { ++Turns; RefreshTarget(); }
		return true;
	}
	if (Controls->Matches(A::Primary, Key))
	{
		if (bPress) { if (State == EShelterBuildState::Menu) EnterPlacing(CategoryIndex); else TryPlace(); }
		return true;
	}
	if (Controls->Matches(A::Secondary, Key))
	{
		if (bPress && State == EShelterBuildState::Placing) TryRemove();
		return true;
	}
	if (State == EShelterBuildState::Menu)
	{
		static const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Numbers); ++Index)
			if (Key == Numbers[Index] && Categories.IsValidIndex(Index)) { if (bPress) EnterPlacing(Index); return true; }
	}
	// Item actions are suspended while building; the held item stays in the hands.
	for (A Suspended : { A::PickUpWorld, A::DropHeld, A::Stow, A::Bandage })
		if (Controls->Matches(Suspended, Key)) return true;
	return false;
}

void UShelterBuildComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsActive()) return;
	auto* Player = Controller();
	auto* Character = Player ? Cast<Aprototype3Character>(Player->GetPawn()) : nullptr;
	if (!Character || !Character->IsAlive() || !Area.IsValid() || !Area->ContainsLocation(Character->GetActorLocation())
		|| Player->IsInventoryInterfaceOpen())
	{
		Close();
		return;
	}
	if (State == EShelterBuildState::Placing) RefreshTarget();
}

void UShelterBuildComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroyPreview();
	State = EShelterBuildState::Closed;
	if (WatchedVitals.IsValid())
	{
		WatchedVitals->OnDamageApplied.RemoveDynamic(this, &UShelterBuildComponent::OwnerDamaged);
		WatchedVitals->OnDeath.RemoveDynamic(this, &UShelterBuildComponent::OwnerDied);
	}
	Super::EndPlay(Reason);
}
