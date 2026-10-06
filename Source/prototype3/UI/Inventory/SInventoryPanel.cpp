#include "UI/Inventory/SInventoryPanel.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace
{
bool InRect(FVector2D P, double X, double Y, double W, double H)
{
	return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H;
}
}

void SInventoryPanel::Construct(const FArguments& Args)
{
	Inventory = Args._Inventory;
	ItemUse = Args._ItemUse;
	Player = Args._Player;
	bPocketsOnly = Args._PocketsOnly;
	Controls = Args._Controls ? Args._Controls : GetMutableDefault<UInventoryInputSettings>();
	bSaveControls = Args._SaveControls;
	OnClose = Args._OnClose;
	OnDropItem = Args._OnDropItem;
	SetCanTick(true);
	RefreshFloor();
}

bool SInventoryPanel::IsInterfaceReady() const
{
	return !bRequestedClose && (!ItemUse.IsValid() || bPocketsOnly || ItemUse->IsBackpackOpen());
}

FVector2D SInventoryPanel::FloorSize() const
{
	if (!bPocketsOnly && Inventory.IsValid())
		for (const auto& Pocket : Inventory->GetPockets())
			if (Pocket.Id == TEXT("Backpack")) return FVector2D(260, Pocket.Size.Y);
	return FVector2D(260,360);
}

FName SInventoryPanel::GetVisibleQuickPocket() const { return UPlayerItemUseComponent::QuickPocketId(QuickPocketIndex); }

void SInventoryPanel::CycleQuickPocket()
{
	if (bPocketsOnly || !IsInterfaceReady()) return;
	QuickPocketIndex = (QuickPocketIndex + 1) % UPlayerItemUseComponent::QuickPocketCount;
	FInventoryEntry Selected;
	if (!Active() && Inventory.IsValid() && Inventory->GetItem(SelectedId, Selected) && UPlayerItemUseComponent::IsQuickPocket(Selected.PocketId))
		SelectedId.Invalidate();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::SetPocketsOnly(bool bOnly)
{
	CancelInteraction(); SelectedId.Invalidate();
	bRequestedClose = false;
	bPocketsOnly = bOnly;
	QuickPocketIndex = 0;
	if (ItemUse.IsValid())
	{
		ItemUse->CloseBackpack();
		if (!bOnly) ItemUse->BeginOpenBackpack();
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SLeafWidget::Tick(Geometry, Time, Delta);
	if (!bPocketsOnly && ItemUse.IsValid() && !ItemUse->IsOpeningBackpack() && !ItemUse->IsBackpackOpen())
	{
		CancelInteraction();
		if (!bRequestedClose) { bRequestedClose = true; OnClose.ExecuteIfBound(); }
		return;
	}
	FloorRefreshRemaining -= Delta;
	if (FloorRefreshRemaining <= 0) { RefreshFloor(); FloorRefreshRemaining = .2f; }
	if (bFromFloor && !SourceAvailable())
	{
		CancelGesture();
		Status = TEXT("Floor item is no longer available nearby. Pickup cancelled.");
		if (HasMouseCapture()) FSlateApplication::Get().ReleaseAllPointerCapture();
	}
	if (ItemUse.IsValid())
	{
		if (bRequestedBackpack && ItemUse->IsBackpackOpen())
		{
			if (PendingTransfer.IsValid()) Status = ItemUse->Transfer(PendingTransfer, TEXT("Backpack")) ? TEXT("Transferred to backpack.") : ItemUse->Status;
			else Status = TEXT("Backpack open.");
			PendingTransfer.Invalidate(); bRequestedBackpack = false;
		}
	}
	if (Controls.IsValid() && Active() && (bTurnLeft || bTurnRight))
		Turn((static_cast<int32>(bTurnRight) - static_cast<int32>(bTurnLeft)) * Controls->TurnSpeed * Delta);
}

void SInventoryPanel::RefreshFloor()
{
	FloorItems = ADroppedItem::FindNearby(Player.Get());
	FloorItems.RemoveAll([this](const auto& Actor)
	{
		FInventoryItemProfile Profile;
		return !Inventory.IsValid() || !Actor.IsValid() || !Inventory->GetProfile(Actor->GetInventoryProfileId(), Profile)
			|| Profile.Definition != Actor->GetItem().Definition;
	});
	FloorScroll = FMath::Clamp(FloorScroll, 0, FMath::Max(0, FloorItems.Num() - FloorRows()));
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SInventoryPanel::BeginFloorDrag()
{
	const FVector2D Local = Cursor - FloorOrigin();
	if (!Player.IsValid() || !InRect(Local, 0, 0, 248, FloorRows() * 36)) return false;
	SelectedId.Invalidate();
	const int32 Index = FloorScroll + FMath::FloorToInt(Local.Y / 36);
	if (Index < FloorScroll + FloorRows() && FloorItems.IsValidIndex(Index) && FloorItems[Index].IsValid()
		&& FloorItems[Index]->CanInteract(Player.Get()))
	{
		CancelInteraction();
		FloorSource = FloorItems[Index];
		SelectedId.Invalidate();
		Pending.Item = FloorSource->GetItem();
		Pending.ProfileId = FloorSource->GetInventoryProfileId();
		GrabOffset = FVector2D::ZeroVector;
		PreviewAngle = 0;
		bFromFloor = bDragging = true;
		Status = TEXT("Place in a grid, or clear the storage grids to drop it.");
	}
	return true;
}

bool SInventoryPanel::SourceAvailable() const
{
	if (!IsInterfaceReady()) return false;
	if (bFromFloor) return FloorSource.IsValid() && FloorSource->CanInteract(Player.Get())
		&& FloorSource->GetItem().InstanceId == Pending.Item.InstanceId;
	if (bAdding) return true;
	FInventoryEntry Entry;
	return Inventory.IsValid() && Inventory->GetItem(SelectedId, Entry) && !Inventory->IsReserved(SelectedId)
		&& (!ItemUse.IsValid() || ItemUse->CanAccess(Entry.PocketId));
}

bool SInventoryPanel::IsWorldDrop() const
{
	if (!bDragging || !SourceAvailable() || !Inventory.IsValid() || !InventoryGeometry::IsFinite(PreviewCenter())) return false;
	FInventoryItemProfile Profile;
	if (!Inventory->GetProfile(Pending.ProfileId, Profile)) return false;
	// Clear every displayed storage container. Floor list and surrounding panel are drop space.
	const auto Parts = Profile.GetTransformedParts(PreviewCenter(), PreviewAngle);
	for (const auto& View : DisplayPockets())
	{
		const FVector2D Min = View.Origin - FVector2D(.001), Max = View.Origin + View.Pocket.Size + FVector2D(.001);
		FInventoryShapePart Rect;
		Rect.Vertices = {Min, FVector2D(Max.X,Min.Y), Max, FVector2D(Min.X,Max.Y)};
		for (const auto& Part : Parts) if (Part.Overlaps(Rect)) return false;
	}
	return true;
}

bool SInventoryPanel::HitItem(FVector2D Point, FInventoryEntry& OutEntry) const
{
	if (!Inventory.IsValid() || !IsInterfaceReady()) return false;
	const auto Pockets = DisplayPockets();
	const auto Entries = Inventory->GetEntries();
	for (const auto& View : Pockets)
	{
		const FVector2D Local = Point - View.Origin;
		if (!View.bAccessible || !InRect(Local, 0, 0, View.Pocket.Size.X, View.Pocket.Size.Y)) continue;
		for (int32 I = Entries.Num() - 1; I >= 0; --I)
		{
			FInventoryItemProfile Profile;
			if (Entries[I].PocketId == View.Pocket.Id && Inventory->GetProfile(Entries[I].ProfileId, Profile)
				&& Profile.Contains(Local, Entries[I].Position, Entries[I].AngleDegrees))
			{
				OutEntry = Entries[I];
				return true;
			}
		}
	}
	return false;
}

FString SInventoryPanel::Describe(EInventoryResult Result)
{
	switch (Result)
	{
	case EInventoryResult::Success: return TEXT("Valid placement");
	case EInventoryResult::Occupied: return TEXT("Overlaps another item");
	case EInventoryResult::OutOfBounds: return TEXT("Part of the item is outside the storage area");
	case EInventoryResult::TooHeavy: return TEXT("Above this pocket's per-item weight limit");
	case EInventoryResult::Incompatible: return TEXT("Firearms cannot enter quick storage");
	case EInventoryResult::InUse: return TEXT("Item in hands: stow it first");
	case EInventoryResult::InvalidRotation: return TEXT("This item does not allow that rotation");
	default: return TEXT("Cannot place this item");
	}
}

EInventoryResult SInventoryPanel::Preview(FName& PocketId, FVector2D& Center) const
{
	Center = PreviewCenter();
	if (!Inventory.IsValid()) return EInventoryResult::InvalidPocket;
	if (!SourceAvailable()) return EInventoryResult::InvalidItem;
	if (ItemUse.IsValid() && !bAdding && !bFromFloor)
	{
		FInventoryEntry Source;
		if (!Inventory->GetItem(SelectedId, Source) || !ItemUse->CanAccess(Source.PocketId)) return EInventoryResult::InvalidPocket;
	}
	for (const auto& View : DisplayPockets())
	{
		const FVector2D Local = Center - View.Origin;
		if (!View.bAccessible || !InRect(Local, 0, 0, View.Pocket.Size.X, View.Pocket.Size.Y)) continue;
		PocketId = View.Pocket.Id;
		Center = Local;
		return (bAdding || bFromFloor) ? Inventory->CheckPlacement(Pending.ProfileId, PocketId, Center, PreviewAngle)
			: Inventory->CheckMove(SelectedId, PocketId, Center, PreviewAngle);
	}
	return EInventoryResult::InvalidPocket;
}

void SInventoryPanel::CancelGesture()
{
	bDragging = bAdding = bTurnLeft = bTurnRight = false;
	bMouseRotating = bHasRotationDirection = false;
	Pending = FInventoryEntry();
	FloorSource.Reset(); bFromFloor = false;
	FInventoryEntry Selected;
	if (!bPocketsOnly && ItemUse.IsValid() && Inventory.IsValid() && Inventory->GetItem(SelectedId, Selected)
		&& UPlayerItemUseComponent::IsQuickPocket(Selected.PocketId) && Selected.PocketId != GetVisibleQuickPocket()) SelectedId.Invalidate();
	GrabOffset = FVector2D::ZeroVector;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::CommitGesture()
{
	if (!Active()) return;
	if (IsWorldDrop())
	{
		if (bFromFloor)
		{
			FString Error;
			Status = FloorSource->DropAtFeet(Player.Get(), Error) ? TEXT("Item dropped at your feet.") : Error;
			CancelGesture(); RefreshFloor();
		}
		else DropSelected();
		return;
	}
	FName Pocket;
	FVector2D Center;
	EInventoryResult Result = Preview(Pocket, Center);
	if (Result == EInventoryResult::Success)
	{
		if (bFromFloor)
		{
			Result = FloorSource->PickUp(Inventory.Get(), Player.Get(), Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else if (bAdding)
		{
			Result = Inventory->AddItem(Pending.Item, Pending.ProfileId, Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else Result = Inventory->MoveItem(SelectedId, Pocket, Center, PreviewAngle);
	}
	Status = Result == EInventoryResult::Success ? TEXT("Item placed.") : Describe(Result) + TEXT(". Placement cancelled.");
	CancelGesture();
	RefreshFloor();
}

void SInventoryPanel::Turn(double Delta)
{
	if (!Active()) return;
	PreviewAngle = InventoryGeometry::NormalizeAngle(PreviewAngle + Delta);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::BeginMouseRotation()
{
	if (!Active() || bMouseRotating) return;
	RotationCenter = Cursor - GrabOffset;
	bMouseRotating = true;
	bHasRotationDirection = false;
	UpdateCursor(Cursor);
}

void SInventoryPanel::EndMouseRotation()
{
	if (!bMouseRotating) return;
	// Re-anchor dragging at the current pointer without moving the item.
	GrabOffset = Cursor - RotationCenter;
	bMouseRotating = bHasRotationDirection = false;
}

void SInventoryPanel::UpdateCursor(FVector2D Position)
{
	Cursor = Position;
	if (bMouseRotating)
	{
		const FVector2D Direction = Cursor - RotationCenter;
		constexpr double PivotRadiusSquared = 9.0;
		if (Direction.SizeSquared() <= PivotRadiusSquared) bHasRotationDirection = false;
		else
		{
			if (bHasRotationDirection)
			{
				const FVector2D Travel = Direction - RotationDirection;
				const double Fraction = Travel.SizeSquared() > UE_DOUBLE_SMALL_NUMBER
					? FMath::Clamp(-FVector2D::DotProduct(RotationDirection, Travel) / Travel.SizeSquared(), 0.0, 1.0) : 0.0;
				// Crossing the pivot has no defined angle, even if no event lands on it.
				if ((RotationDirection + Travel * Fraction).SizeSquared() > PivotRadiusSquared)
				{
					const double Previous = FMath::Atan2(RotationDirection.Y, RotationDirection.X);
					const double Current = FMath::Atan2(Direction.Y, Direction.X);
					// The default speed preserves the original 1:1 cursor angle response.
					Turn(FMath::RadiansToDegrees(FMath::FindDeltaAngleRadians(Previous, Current)) * Controls->TurnSpeed / 120.0);
				}
			}
			RotationDirection = Direction;
			bHasRotationDirection = true;
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::DropSelected()
{
	if (bAdding || bFromFloor || !SelectedId.IsValid() || !Inventory.IsValid()) return;
	if (!SourceAvailable()) { Status = TEXT("Item unavailable. Stow it or open its storage first."); CancelGesture(); return; }
	FString Error;
	if (OnDropItem.IsBound() && OnDropItem.Execute(SelectedId, Error))
	{
		SelectedId.Invalidate();
		Status = TEXT("Item dropped at your feet.");
	}
	else Status = Error.IsEmpty() ? TEXT("Dropping is unavailable here. Item kept in inventory.") : Error;
	CancelGesture();
	RefreshFloor();
}

void SInventoryPanel::BeginBandage()
{
	CancelGesture();
	FInventoryItemProfile Profile;
	if (!Inventory.IsValid() || !Inventory->GetProfile(TEXT("Bandage_TestOnly"), Profile)) return;
	Pending.Item = FItemInstance::Create(Profile.Definition);
	Pending.ProfileId = Profile.Id;
	bAdding = Pending.Item.IsValid();
	PreviewAngle = 0;
	Status = TEXT("Place the bandage with your grab / place control.");
}

void SInventoryPanel::SavePreferences()
{
	if (bSaveControls && Controls.IsValid()) Controls->SaveConfig();
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SInventoryPanel::Reply()
{
	FReply Result = FReply::Handled().SetUserFocus(SharedThis(this));
	// Recapturing the same widget first sends OnMouseCaptureLost, cancelling the gesture.
	// Keep the existing capture while rotating or releasing another held control.
	if (Active() && !HasMouseCapture()) Result.CaptureMouse(SharedThis(this));
	else if (!Active() && HasMouseCapture()) Result.ReleaseMouseCapture();
	return Result;
}

FReply SInventoryPanel::Press(FKey Key)
{
	if (!Controls.IsValid()) return FReply::Handled();
	if (Key == EKeys::Escape || Key == Controls->GetKey(EInventoryControl::Toggle))
	{
		if (Key != EKeys::Escape && ItemUse.IsValid() && bPocketsOnly) { SetPocketsOnly(false); return Reply(); }
		CancelInteraction(); OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (ItemUse.IsValid() && Key == Controls->GetKey(EInventoryControl::ShowQuick))
	{
		if (bPocketsOnly) { CancelInteraction(); OnClose.ExecuteIfBound(); return FReply::Handled().ReleaseMouseCapture(); }
		CycleQuickPocket(); return Reply();
	}
	if (!IsInterfaceReady()) return Reply();
	if (Key == Controls->GetKey(EInventoryControl::Cancel))
	{
		CancelInteraction();
		Status = TEXT("Cancelled. Original position and angle restored.");
		return Reply();
	}
	if (Key == Controls->GetKey(EInventoryControl::ToggleGrabMode))
	{
		CancelGesture();
		Controls->bToggleGrab = !Controls->bToggleGrab;
		SavePreferences();
		Status = Controls->bToggleGrab ? TEXT("Click to grab; click again to place.") : TEXT("Hold to grab; release to place.");
		return Reply();
	}
	if (ItemUse.IsValid() && HandlePlayerControl(Key))
	{
		if (bCloseRequested) { bCloseRequested = false; return FReply::Handled().ReleaseMouseCapture(); }
		return Reply();
	}
	if (Key == Controls->GetKey(EInventoryControl::Grab))
	{
		if (bAdding || (bDragging && Controls->bToggleGrab)) CommitGesture();
		else if (!bDragging)
		{
			if (BeginFloorDrag()) return Reply();
			FInventoryEntry Entry;
			if (HitItem(Cursor, Entry))
			{
				PendingTransfer.Invalidate(); bRequestedBackpack = false;
				SelectedId = Entry.Item.InstanceId;
				if (Inventory->IsReserved(SelectedId))
				{
					Status = TEXT("Item in hands. Stow it before moving or dropping it.");
					return Reply();
				}
				Pending = Entry;
				PreviewAngle = Entry.AngleDegrees;
				for (const auto& View : DisplayPockets())
					if (View.Pocket.Id == Entry.PocketId) GrabOffset = Cursor - View.Origin - Entry.Position;
				bDragging = true;
			}
			else SelectedId.Invalidate();
		}
	}
	if (Key == Controls->GetKey(EInventoryControl::Drop)) { DropSelected(); return Reply(); }
	if (Active())
	{
		if (Key == Controls->GetKey(EInventoryControl::RotateWithMouse)) BeginMouseRotation();
		if (Key == Controls->GetKey(EInventoryControl::TurnLeft)) bTurnLeft = true;
		if (Key == Controls->GetKey(EInventoryControl::TurnRight)) bTurnRight = true;
	}
	else if (!ItemUse.IsValid() && Key == Controls->GetKey(EInventoryControl::Add)) BeginBandage();
	Invalidate(EInvalidateWidgetReason::Paint);
	return Reply();
}

FReply SInventoryPanel::Release(FKey Key)
{
	if (!Controls.IsValid()) return FReply::Handled();
	if (Key == Controls->GetKey(EInventoryControl::RotateWithMouse)) EndMouseRotation();
	if (Key == Controls->GetKey(EInventoryControl::TurnLeft)) bTurnLeft = false;
	if (Key == Controls->GetKey(EInventoryControl::TurnRight)) bTurnRight = false;
	if (Key == Controls->GetKey(EInventoryControl::Grab) && bDragging && !Controls->bToggleGrab) CommitGesture();
	return Reply();
}

FReply SInventoryPanel::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& Event)
{
	UpdateCursor(G.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	return Press(Event.GetEffectingButton());
}
FReply SInventoryPanel::OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& Event)
{
	return OnMouseButtonDown(G, Event);
}

FReply SInventoryPanel::OnMouseButtonUp(const FGeometry& G, const FPointerEvent& Event)
{
	UpdateCursor(G.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	return Release(Event.GetEffectingButton());
}

FReply SInventoryPanel::OnMouseMove(const FGeometry& G, const FPointerEvent& Event)
{
	UpdateCursor(G.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	return FReply::Handled();
}

FReply SInventoryPanel::OnMouseWheel(const FGeometry& G, const FPointerEvent& Event)
{
	UpdateCursor(G.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (!IsInterfaceReady()) return Reply();
	if (!Active() && Player.IsValid() && InRect(Cursor - FloorOrigin(), 0, 0, 260, FloorSize().Y))
	{
		FloorScroll = FMath::Clamp(FloorScroll - FMath::RoundToInt(Event.GetWheelDelta() * 3), 0, FMath::Max(0, FloorItems.Num() - FloorRows()));
		Invalidate(EInvalidateWidgetReason::Paint);
		return Reply();
	}
	if (Controls.IsValid() && !FMath::IsNearlyZero(Event.GetWheelDelta()))
	{
		Controls->TurnSpeed = FMath::Clamp(Controls->TurnSpeed + Event.GetWheelDelta() * 15.f, 15.f, 360.f);
		SavePreferences();
	}
	return Reply();
}

FReply SInventoryPanel::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	return Event.IsRepeat() ? FReply::Handled() : Press(Event.GetKey());
}
FReply SInventoryPanel::OnKeyUp(const FGeometry&, const FKeyEvent& Event) { return Release(Event.GetKey()); }
void SInventoryPanel::OnMouseCaptureLost(const FCaptureLostEvent&) { CancelGesture(); }
void SInventoryPanel::OnFocusLost(const FFocusEvent&) { CancelGesture(); }

int32 SInventoryPanel::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto Box = [&](FVector2D Pos, FVector2D Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer++, G.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Brush, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](FVector2D Pos, const FString& Value, int32 Size, FLinearColor Color = FLinearColor::White)
	{
		FSlateDrawElement::MakeText(Out, Layer++, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(Pos)),
			Value, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size), ESlateDrawEffect::None, Color);
	};
	auto Shape = [&](const FInventoryItemProfile& Profile, FVector2D Center, double Angle, FLinearColor Color)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (const auto& Part : Profile.GetTransformedParts(Center, Angle))
		{
			const int32 Base = Vertices.Num();
			for (FVector2D P : Part.Vertices)
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f::ZeroVector, Color.ToFColor(true)));
			for (int32 I = 1; I + 1 < Part.Vertices.Num(); ++I)
			{
				Indices.Add(static_cast<SlateIndex>(Base));
				Indices.Add(static_cast<SlateIndex>(Base + I));
				Indices.Add(static_cast<SlateIndex>(Base + I + 1));
			}
		}
		const auto Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		FSlateDrawElement::MakeCustomVerts(Out, Layer++, Handle, Vertices, Indices, nullptr, 0, 0);
	};
	const FLinearColor Muted(0.65f, 0.72f, 0.78f);
	if (!Inventory.IsValid() || !Controls.IsValid()) return Layer;
	Box(FVector2D::ZeroVector, PanelSize(), FLinearColor(0.022f, 0.03f, 0.043f));
	Text(FVector2D(40, 27), bPocketsOnly ? TEXT("POCKETS") : TEXT("INVENTORY"), 25);
	if (!IsInterfaceReady())
	{
		if (ItemUse.IsValid() && ItemUse->IsOpeningBackpack())
		{
			const FVector2D Bar = PanelSize() * .5 - FVector2D(200,6);
			Text(Bar - FVector2D(0,40), TEXT("Opening inventory..."), 18);
			Box(Bar, FVector2D(400,12), FLinearColor(.08f,.11f,.14f));
			Box(Bar, FVector2D(400 * ItemUse->GetProgress(),12), FLinearColor(.35f,.65f,.75f));
		}
		return Layer;
	}
	const auto Pockets = DisplayPockets();
	const auto Entries = Inventory->GetEntries();
	for (const auto& View : Pockets)
	{
		const bool bBag = View.Pocket.Id == TEXT("Backpack");
		const FString Label = !ItemUse.IsValid() ? TEXT("TEST STORAGE") : bBag ? TEXT("BACKPACK")
			: FString::Printf(TEXT("POCKET %d"), View.Pocket.Id == TEXT("Quick") ? 1 : View.Pocket.Id == TEXT("Quick2") ? 2 : 3);
		Text(View.Origin - FVector2D(0, 32), Label, 14, Muted);
		Box(View.Origin - FVector2D(2), View.Pocket.Size + FVector2D(4), FLinearColor(0.22f, 0.3f, 0.36f));
		Box(View.Origin, View.Pocket.Size, FLinearColor(0.055f, 0.075f, 0.095f));
		if (!View.bAccessible)
		{
			const FString State = ItemUse->IsOpeningBackpack()
				? FString::Printf(TEXT("Opening... %.0f%%"), ItemUse->GetProgress() * 100)
				: ItemUse->IsBackpackEquipped() ? TEXT("Closed") : TEXT("Unequipped");
			Text(View.Origin + FVector2D(16, 16), State, 13, Muted);
			continue;
		}
		for (const auto& Entry : Entries)
		{
			if (Entry.PocketId != View.Pocket.Id || (bDragging && Entry.Item.InstanceId == SelectedId)) continue;
			FInventoryItemProfile Profile;
			if (Inventory->GetProfile(Entry.ProfileId, Profile))
				Shape(Profile, View.Origin + Entry.Position, Entry.AngleDegrees,
					Inventory->IsReserved(Entry.Item.InstanceId) ? FLinearColor(0.15f,0.22f,0.25f) : Entry.Item.InstanceId == SelectedId ? FLinearColor(0.43f, 0.62f, 0.73f) : FLinearColor(0.3f, 0.4f, 0.47f));
		}
	}
	if (Player.IsValid())
	{
		const FVector2D Origin = FloorOrigin();
		const FVector2D Size = FloorSize();
		Text(Origin - FVector2D(0, 32), FString::Printf(TEXT("FLOOR (%d)"), FloorItems.Num()), 14, Muted);
		Box(Origin - FVector2D(2), Size + FVector2D(4), FLinearColor(.22f,.3f,.36f));
		Box(Origin, Size, FLinearColor(.055f,.075f,.095f));
		Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2D(248,Size.Y), FSlateLayoutTransform(Origin))));
		for (int32 Row = 0; Row < FloorRows() && FloorItems.IsValidIndex(FloorScroll + Row); ++Row)
		{
			const auto Actor = FloorItems[FloorScroll + Row];
			if (!Actor.IsValid() || !Actor->GetItem().IsValid()) continue;
			const FVector2D RowOrigin = Origin + FVector2D(0, Row * 36);
			if (Actor == FloorSource || InRect(Cursor - RowOrigin, 0, 0, 248, 36))
				Box(RowOrigin, FVector2D(248,36), FLinearColor(.12f,.19f,.23f));
			Text(RowOrigin + FVector2D(10,8), Actor->GetItem().Definition->DisplayName.ToString(), 12);
		}
		if (FloorItems.IsEmpty()) Text(Origin + FVector2D(10,12), TEXT("No nearby items"), 12, Muted);
		Out.PopClip();
		if (FloorItems.Num() > FloorRows())
		{
			const float Thumb = Size.Y * FloorRows() / FloorItems.Num();
			const float Offset = (Size.Y - Thumb) * FloorScroll / (FloorItems.Num() - FloorRows());
			Box(Origin + FVector2D(252,0), FVector2D(6,Size.Y), FLinearColor(.12f,.19f,.23f));
			Box(Origin + FVector2D(252,Offset), FVector2D(6,Thumb), Muted);
		}
	}
	if (ItemUse.IsValid())
	{
		auto Key = [this](EInventoryControl Action) { return Controls->GetKey(Action).GetDisplayName().ToString(); };
		Box(FVector2D(1120,0), FVector2D(420,940), FLinearColor(.022f,.03f,.043f,.95f));
		Text(FVector2D(1140,30), TEXT("CONTROLS"), 20);
		float Y = 86;
		auto Line = [&](const FString& Value) { Text(FVector2D(1140,Y),Value,12,Muted); Y += 32; };
		auto Control = [&](EInventoryControl Action, const TCHAR* Description) { Line(Key(Action) + TEXT(": ") + Description); };
		Control(EInventoryControl::Grab, Controls->bToggleGrab ? TEXT("click to grab / place") : TEXT("hold to drag; release to place"));
		Control(EInventoryControl::RotateWithMouse, TEXT("hold + move to rotate"));
		Control(EInventoryControl::TurnLeft, TEXT("turn left"));
		Control(EInventoryControl::TurnRight, TEXT("turn right"));
		Control(EInventoryControl::Cancel, TEXT("cancel placement"));
		Control(EInventoryControl::ToggleGrabMode, TEXT("switch hold / click mode"));
		Line(TEXT("Mouse wheel: rotation speed while dragging"));
		Line(TEXT("Mouse wheel over floor: scroll list"));
		Control(EInventoryControl::Drop, TEXT("drop selected item at your feet"));
		Control(EInventoryControl::Toggle, TEXT("open / close full inventory"));
		Control(EInventoryControl::ShowQuick, bPocketsOnly ? TEXT("close pockets") : TEXT("cycle pocket 1 / 2 / 3"));
		Control(EInventoryControl::OpenBackpack, TEXT("open full inventory"));
		Control(EInventoryControl::ToggleBackpack, TEXT("equip / unequip backpack"));
		Control(EInventoryControl::ToHands, TEXT("take selected item in hands"));
		Control(EInventoryControl::ToQuick, bPocketsOnly ? TEXT("transfer to a fitting pocket") : TEXT("transfer to the visible pocket"));
		Control(EInventoryControl::ToBackpack, TEXT("transfer to backpack"));
		Control(EInventoryControl::Stow, TEXT("stow held item"));
		Control(EInventoryControl::AssignShortcut1, TEXT("assign selected type to shortcut 1"));
		Control(EInventoryControl::AssignShortcut2, TEXT("assign selected type to shortcut 2"));
		Control(EInventoryControl::AssignShortcut3, TEXT("assign selected type to shortcut 3"));
		Line(TEXT("Escape: close interface"));
		Y += 12;
		Line(TEXT("Drag floor names into a storage grid."));
		Line(TEXT("Clear the storage containers to drop."));
		Line(TEXT("The floor list also accepts world drops."));
		Line(TEXT("Change bindings in Pause > Options."));
	}
	FInventoryEntry Selected;
	if (Inventory->GetItem(SelectedId, Selected))
		Text(FVector2D(40, PanelSize().Y - 80), Selected.Item.Definition->DisplayName.ToString(), 13);
	if (Active())
	{
		FName Pocket;
		FVector2D Center;
		const auto Result = Preview(Pocket, Center);
		const bool bDrop = IsWorldDrop();
		const bool bValid = bDrop || Result == EInventoryResult::Success;
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Pending.ProfileId, Profile))
		{
			// Show the actual rotated silhouette, including the portion outside the storage area.
			Shape(Profile, PreviewCenter(), PreviewAngle, bValid ? FLinearColor(0.09f, 0.68f, 0.35f) : FLinearColor(0.84f, 0.12f, 0.12f));
		}
		Text(FVector2D(40, PanelSize().Y - 50), (bDrop ? FString(TEXT("Drop at your feet")) : Describe(Result)) + FString::Printf(TEXT("  |  %.1f degrees"), PreviewAngle), 14,
			bValid ? FLinearColor(0.3f, 1, 0.6f) : FLinearColor(1, 0.4f, 0.3f));
	}
	else Text(FVector2D(40, PanelSize().Y - 50), Status, 12, Muted);
	Text(FVector2D(40, PanelSize().Y - 22), FString::Printf(TEXT("%d items"), Entries.Num()), 11, Muted);
	return Layer;
}

TArray<SInventoryPanel::FPocketView> SInventoryPanel::DisplayPockets() const
{
	if (!Inventory.IsValid()) return {};
	const auto Pockets = Inventory->GetPockets();
	if (!ItemUse.IsValid())
		return Pockets.IsEmpty() ? TArray<FPocketView>() : TArray<FPocketView>{ { Pockets[0], PocketOrigin(), true } };
	TArray<FPocketView> Views;
	for (const auto& Pocket : Pockets)
		if (!bPocketsOnly && Pocket.Id == TEXT("Backpack"))
		{
			Views.Add({ Pocket, PocketOrigin(), ItemUse->CanAccess(Pocket.Id) });
			break;
		}
	for (int32 Index = 0; Index < UPlayerItemUseComponent::QuickPocketCount; ++Index)
	{
		if (!bPocketsOnly && Index != QuickPocketIndex) continue;
		for (const auto& Pocket : Pockets)
			if (Pocket.Id == UPlayerItemUseComponent::QuickPocketId(Index))
			{
				const FVector2D Origin = bPocketsOnly ? FVector2D(40 + Index*240,150) : FVector2D(500,150);
				Views.Add({Pocket, Origin, true});
				break;
			}
	}
	return Views;
}

void SInventoryPanel::CancelInteraction()
{
	CancelGesture();
	PendingTransfer.Invalidate();
	bRequestedBackpack = false;
}

bool SInventoryPanel::HandlePlayerControl(FKey Key)
{
	auto Is = [&](EInventoryControl Action) { return Key == Controls->GetKey(Action); };
	if (Is(EInventoryControl::OpenBackpack))
	{
		if (bPocketsOnly) SetPocketsOnly(false);
		return true;
	}
	if (Is(EInventoryControl::ToggleBackpack))
	{
		CancelInteraction(); ItemUse->ToggleBackpackEquipment();
		SelectedId.Invalidate(); Status = ItemUse->Status;
		return true;
	}
	if (Is(EInventoryControl::ToHands))
	{
		const FGuid Item = SelectedId;
		CancelGesture();
		if (ItemUse->EquipToHands(Item))
		{
			CancelInteraction(); bCloseRequested = true; OnClose.ExecuteIfBound();
		}
		else Status = TEXT("Select an available item first.");
		return true;
	}
	if (Is(EInventoryControl::ToQuick))
	{
		const FGuid Item = SelectedId;
		CancelInteraction();
		Status = TEXT("Cannot transfer: check size, weight, firearm restriction and hands.");
		for (int32 Index=0; Index<UPlayerItemUseComponent::QuickPocketCount; ++Index)
		{
			if (!bPocketsOnly && Index != QuickPocketIndex) continue;
			if (ItemUse->Transfer(Item, UPlayerItemUseComponent::QuickPocketId(Index)))
			{ SelectedId = Item; Status = TEXT("Transferred to quick pocket."); break; }
		}
		return true;
	}
	if (Is(EInventoryControl::ToBackpack))
	{
		const FGuid Item = SelectedId;
		CancelInteraction();
		FInventoryEntry Entry;
		if (!Inventory->GetItem(Item, Entry) || Inventory->IsReserved(Item))
			Status = TEXT("Select an available item first; stow items in hands.");
		else if (ItemUse->BeginOpenBackpack())
		{
			if (bPocketsOnly) SetPocketsOnly(false);
			PendingTransfer = Item; bRequestedBackpack = true;
			Status = TEXT("Opening backpack to transfer...");
		}
		else Status = TEXT("Equip the backpack first.");
		return true;
	}
	if (Is(EInventoryControl::Stow))
	{
		CancelGesture(); ItemUse->Stow(); Status = TEXT("Item stowed in its reserved place.");
		return true;
	}
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		if (!Is(static_cast<EInventoryControl>(static_cast<int32>(EInventoryControl::AssignShortcut1) + Slot))) continue;
		CancelGesture();
		FInventoryEntry Entry;
		if (ItemUse->Shortcuts && Inventory->GetItem(SelectedId, Entry) && UPlayerItemUseComponent::IsQuickPocket(Entry.PocketId))
		{
			ItemUse->Shortcuts->ItemTypes[Slot] = Entry.Item.Definition->ItemId;
			ItemUse->SaveShortcuts(); Status = TEXT("Shortcut assigned to this item type.");
		}
		else Status = TEXT("Select an item in quick storage first.");
		return true;
	}
	return false;
}
