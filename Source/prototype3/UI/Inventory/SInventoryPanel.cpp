#include "UI/Inventory/SInventoryPanel.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
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
	Controls = Args._Controls ? Args._Controls : GetMutableDefault<UInventoryInputSettings>();
	bSaveControls = Args._SaveControls;
	OnClose = Args._OnClose;
	OnDropItem = Args._OnDropItem;
	SetCanTick(true);
}

void SInventoryPanel::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SLeafWidget::Tick(Geometry, Time, Delta);
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

bool SInventoryPanel::HitItem(FVector2D Point, FInventoryEntry& OutEntry) const
{
	if (!Inventory.IsValid()) return false;
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
	if (ItemUse.IsValid() && !bAdding)
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
		return bAdding ? Inventory->CheckPlacement(Pending.ProfileId, PocketId, Center, PreviewAngle)
			: Inventory->CheckMove(SelectedId, PocketId, Center, PreviewAngle);
	}
	return EInventoryResult::InvalidPocket;
}

void SInventoryPanel::CancelGesture()
{
	bDragging = bAdding = bTurnLeft = bTurnRight = false;
	bMouseRotating = bHasRotationDirection = false;
	Pending = FInventoryEntry();
	GrabOffset = FVector2D::ZeroVector;
	FInventoryEntry Selected;
	if (ItemUse.IsValid() && Inventory.IsValid() && Inventory->GetItem(SelectedId, Selected)
		&& UPlayerItemUseComponent::IsQuickPocket(Selected.PocketId) && Selected.PocketId != GetVisibleQuickPocket())
		SelectedId.Invalidate();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::CommitGesture()
{
	if (!Active()) return;
	FName Pocket;
	FVector2D Center;
	EInventoryResult Result = Preview(Pocket, Center);
	if (Result == EInventoryResult::Success)
	{
		if (bAdding)
		{
			Result = Inventory->AddItem(Pending.Item, Pending.ProfileId, Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else Result = Inventory->MoveItem(SelectedId, Pocket, Center, PreviewAngle);
	}
	Status = Result == EInventoryResult::Success ? TEXT("Item placed.") : Describe(Result) + TEXT(". Placement cancelled.");
	CancelGesture();
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
	if (bAdding || !SelectedId.IsValid() || !Inventory.IsValid()) return;
	FString Error;
	if (OnDropItem.IsBound() && OnDropItem.Execute(SelectedId, Error))
	{
		SelectedId.Invalidate();
		Status = TEXT("Item dropped at your feet.");
	}
	else Status = Error.IsEmpty() ? TEXT("Dropping is unavailable here. Item kept in inventory.") : Error;
	CancelGesture();
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
		CancelInteraction(); OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
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
	Box(FVector2D::ZeroVector, FVector2D(1000, 800), FLinearColor(0.022f, 0.03f, 0.043f));
	Text(FVector2D(40, 27), TEXT("INVENTORY"), 25);
	if (!Inventory.IsValid() || !Controls.IsValid()) return Layer;
	const auto Pockets = DisplayPockets();
	const auto Entries = Inventory->GetEntries();
	for (const auto& View : Pockets)
	{
		const bool bBag = View.Pocket.Id == TEXT("Backpack");
		const FString Label = !ItemUse.IsValid() ? TEXT("TEST STORAGE") : bBag ? TEXT("BACKPACK")
			: FString::Printf(TEXT("QUICK POCKET %d / %d"), QuickPocketIndex + 1, UPlayerItemUseComponent::QuickPocketCount);
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
	FInventoryEntry Selected;
	if (Inventory->GetItem(SelectedId, Selected))
		Text(FVector2D(40, 704), Selected.Item.Definition->DisplayName.ToString(), 13);
	if (Active())
	{
		FName Pocket;
		FVector2D Center;
		const auto Result = Preview(Pocket, Center);
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Pending.ProfileId, Profile))
		{
			// Show the actual rotated silhouette, including the portion outside the storage area.
			Out.PushClip(FSlateClippingZone(G));
			Shape(Profile, PreviewCenter(), PreviewAngle, Result == EInventoryResult::Success ? FLinearColor(0.09f, 0.68f, 0.35f) : FLinearColor(0.84f, 0.12f, 0.12f));
			Out.PopClip();
		}
		Text(FVector2D(40, 738), Describe(Result) + FString::Printf(TEXT("  |  %.1f degrees"), PreviewAngle), 14,
			Result == EInventoryResult::Success ? FLinearColor(0.3f, 1, 0.6f) : FLinearColor(1, 0.4f, 0.3f));
	}
	else Text(FVector2D(40, 738), Status, 12, Muted);
	Text(FVector2D(40, 775), FString::Printf(TEXT("%d items"), Entries.Num()), 11, Muted);
	return Layer;
}

FName SInventoryPanel::GetVisibleQuickPocket() const
{
	return UPlayerItemUseComponent::QuickPocketId(QuickPocketIndex);
}

TArray<SInventoryPanel::FPocketView> SInventoryPanel::DisplayPockets() const
{
	if (!Inventory.IsValid()) return {};
	const auto Pockets = Inventory->GetPockets();
	if (!ItemUse.IsValid())
		return Pockets.IsEmpty() ? TArray<FPocketView>() : TArray<FPocketView>{ { Pockets[0], PocketOrigin(), true } };
	TArray<FPocketView> Views;
	FVector2D QuickOrigin = PocketOrigin();
	for (const auto& Pocket : Pockets)
		if (Pocket.Id == TEXT("Backpack"))
		{
			Views.Add({ Pocket, PocketOrigin(), ItemUse->CanAccess(Pocket.Id) });
			QuickOrigin.X += Pocket.Size.X + 40;
			break;
		}
	for (const auto& Pocket : Pockets)
		if (Pocket.Id == GetVisibleQuickPocket()) { Views.Add({ Pocket, QuickOrigin, true }); break; }
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
	if (Is(EInventoryControl::ShowQuick))
	{
		QuickPocketIndex = (QuickPocketIndex + 1) % UPlayerItemUseComponent::QuickPocketCount;
		// A carried preview can move to another pocket; an idle hidden selection cannot.
		FInventoryEntry Selected;
		if (!Active() && Inventory->GetItem(SelectedId, Selected) && UPlayerItemUseComponent::IsQuickPocket(Selected.PocketId))
			SelectedId.Invalidate();
		Invalidate(EInvalidateWidgetReason::Paint);
		return true;
	}
	if (Is(EInventoryControl::OpenBackpack))
	{
		CancelInteraction(); bRequestedBackpack = ItemUse->BeginOpenBackpack();
		Status = bRequestedBackpack ? TEXT("Opening backpack...") : TEXT("Equip the backpack first.");
		return true;
	}
	if (Is(EInventoryControl::ToggleBackpack))
	{
		CancelInteraction(); ItemUse->ToggleBackpackEquipment();
		if (ItemUse->IsBackpackEquipped()) ItemUse->BeginOpenBackpack();
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
		if (ItemUse->Transfer(Item, GetVisibleQuickPocket())) { SelectedId = Item; Status = TEXT("Transferred to quick pocket."); }
		else Status = TEXT("Cannot transfer: check size, weight, firearm restriction and hands.");
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
