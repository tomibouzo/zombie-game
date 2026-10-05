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
			PendingTransfer.Invalidate(); bRequestedBackpack = false; DisplayPocket = TEXT("Backpack"); SelectedId.Invalidate();
		}
		if (DisplayPocket == TEXT("Backpack") && !ItemUse->IsBackpackOpen()) DisplayPocket = TEXT("Quick");
	}
	if (Controls.IsValid() && Active() && (bTurnLeft || bTurnRight))
		Turn((static_cast<int32>(bTurnRight) - static_cast<int32>(bTurnLeft)) * Controls->TurnSpeed * Delta);
}

bool SInventoryPanel::HitItem(FVector2D Point, FInventoryEntry& OutEntry) const
{
	if (!Inventory.IsValid()) return false;
	const FVector2D Local = Point - PocketOrigin();
	const auto Pockets = DisplayPockets();
	if (Pockets.IsEmpty() || !InRect(Local, 0, 0, Pockets[0].Size.X, Pockets[0].Size.Y)) return false;
	const auto Entries = Inventory->GetEntries();
	for (int32 I = Entries.Num() - 1; I >= 0; --I)
	{
		FInventoryItemProfile Profile;
		if (Entries[I].PocketId == Pockets[0].Id && Inventory->GetProfile(Entries[I].ProfileId, Profile)
			&& Profile.Contains(Local, Entries[I].Position, Entries[I].AngleDegrees))
		{
			OutEntry = Entries[I];
			return true;
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
	Center = bMouseRotating ? RotationCenter : Cursor - PocketOrigin() - GrabOffset;
	if (!Inventory.IsValid()) return EInventoryResult::InvalidPocket;
	const auto Pockets = DisplayPockets();
	if (Pockets.IsEmpty()) return EInventoryResult::InvalidPocket;
	PocketId = Pockets[0].Id;
	return bAdding ? Inventory->CheckPlacement(Pending.ProfileId, PocketId, Center, PreviewAngle)
		: Inventory->CheckMove(SelectedId, PocketId, Center, PreviewAngle);
}

void SInventoryPanel::CancelGesture()
{
	bDragging = bAdding = bTurnLeft = bTurnRight = false;
	bMouseRotating = bHasRotationDirection = false;
	Pending = FInventoryEntry();
	GrabOffset = FVector2D::ZeroVector;
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
	RotationCenter = Cursor - PocketOrigin() - GrabOffset;
	bMouseRotating = true;
	bHasRotationDirection = false;
	UpdateCursor(Cursor);
}

void SInventoryPanel::EndMouseRotation()
{
	if (!bMouseRotating) return;
	// Re-anchor dragging at the current pointer without moving the item.
	GrabOffset = Cursor - PocketOrigin() - RotationCenter;
	bMouseRotating = bHasRotationDirection = false;
}

void SInventoryPanel::UpdateCursor(FVector2D Position)
{
	Cursor = Position;
	if (bMouseRotating)
	{
		const FVector2D Direction = Cursor - PocketOrigin() - RotationCenter;
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

void SInventoryPanel::AssignKey(FKey Key)
{
	if (!Controls.IsValid() || Rebinding == INDEX_NONE) return;
	if (Rebinding == static_cast<int32>(EInventoryControl::Toggle) && ItemUse.IsValid() && ItemUse->Shortcuts->Keys.Contains(Key))
	{
		Status = TEXT("That key is assigned to a quick-item shortcut. Change the shortcut first.");
		return;
	}
	FString Error;
	if (Controls->TrySetKey(static_cast<EInventoryControl>(Rebinding), Key, Error))
	{
		Rebinding = INDEX_NONE;
		SavePreferences();
		Status = TEXT("Control updated.");
	}
	else Status = Error;
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SInventoryPanel::HandleButton()
{
	if (!Controls.IsValid()) return false;
	if (ItemUse.IsValid() && HandlePlayerButton()) return true;
	if (Rebinding != INDEX_NONE)
	{
		if (InRect(Cursor, 630, 150, 330, 28))
		{
			Rebinding = INDEX_NONE;
			Status = TEXT("Rebinding cancelled.");
			return true;
		}
		return false;
	}
	for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
		if (InRect(Cursor, 630, 190 + I * 38, 330, 36))
		{
			CancelGesture();
			Rebinding = I;
			Status = TEXT("Press a key or mouse button. Click 'Cancel rebinding' to exit.");
			return true;
		}
	if (InRect(Cursor, 630, 500, 330, 38))
	{
		Controls->bToggleGrab = !Controls->bToggleGrab;
		SavePreferences();
		return true;
	}
	if (InRect(Cursor, 630, 552, 44, 32) || InRect(Cursor, 916, 552, 44, 32))
	{
		Controls->TurnSpeed = FMath::Clamp(Controls->TurnSpeed + (Cursor.X < 700 ? -15.f : 15.f), 15.f, 360.f);
		SavePreferences();
		return true;
	}
	if (InRect(Cursor, 630, 598, 330, 34))
	{
		Controls->ResetDefaults();
		SavePreferences();
		Status = TEXT("Default controls restored.");
		return true;
	}
	if (InRect(Cursor, 630, 650, 155, 40)) { BeginBandage(); return true; }
	return false;
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
	if (QuickRebinding != INDEX_NONE && ItemUse.IsValid())
	{
		if (Key == EKeys::Escape) { QuickRebinding = INDEX_NONE; return Reply(); }
		if (ItemUse->Shortcuts->TryBind(QuickRebinding, Key, Controls->GetKey(EInventoryControl::Toggle)))
		{ QuickRebinding = INDEX_NONE; ItemUse->SaveShortcuts(); Status = TEXT("Shortcut updated."); }
		else Status = TEXT("Choose a free keyboard key. Movement, mouse and inventory-open controls are reserved.");
		return Reply();
	}
	if (Rebinding != INDEX_NONE) { AssignKey(Key); return Reply(); }
	if (Key == Controls->GetKey(EInventoryControl::Toggle))
	{
		CancelGesture(); OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (Key == Controls->GetKey(EInventoryControl::Cancel))
	{
		CancelGesture();
		Status = TEXT("Cancelled. Original position and angle restored.");
		return Reply();
	}
	if (Key == Controls->GetKey(EInventoryControl::Grab))
	{
		if (bAdding || (bDragging && Controls->bToggleGrab)) CommitGesture();
		else if (!bDragging)
		{
			FInventoryEntry Entry;
			if (HitItem(Cursor, Entry) && !Inventory->IsReserved(Entry.Item.InstanceId))
			{
				SelectedId = Entry.Item.InstanceId;
				Pending = Entry;
				PreviewAngle = Entry.AngleDegrees;
				GrabOffset = Cursor - PocketOrigin() - Entry.Position;
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
	else
	{
		if (Key == Controls->GetKey(EInventoryControl::Add)) BeginBandage();
	}
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
	if (!Active() && Event.GetEffectingButton() == EKeys::LeftMouseButton && InRect(Cursor, 880, 25, 80, 42))
	{
		CancelGesture(); Rebinding = INDEX_NONE; OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (!Active() && Event.GetEffectingButton() == EKeys::LeftMouseButton && HandleButton())
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		if (bCloseRequested) { bCloseRequested = false; return FReply::Handled().ReleaseMouseCapture(); }
		return Reply();
	}
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
	if (Rebinding != INDEX_NONE)
	{
		Status = TEXT("Wheel scrolling adjusts rotation speed. Press a key or mouse button to rebind.");
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
void SInventoryPanel::OnFocusLost(const FFocusEvent&) { CancelGesture(); Rebinding = QuickRebinding = INDEX_NONE; }

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
		for (const auto& Part : Profile.GetTransformedParts(PocketOrigin() + Center, Angle))
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
	const FLinearColor Muted(0.65f, 0.72f, 0.78f), Button(0.12f, 0.16f, 0.21f);
	Box(FVector2D::ZeroVector, FVector2D(1000, 800), FLinearColor(0.022f, 0.03f, 0.043f));
	Text(FVector2D(40, 27), TEXT("INVENTORY / free placement"), 25);
	if (Controls.IsValid())
	{
		auto KeyName = [&](EInventoryControl Action) { return Controls->GetKey(Action).GetDisplayName().ToString(); };
		Text(FVector2D(40, 76), FString::Printf(TEXT("%s %s to grab. Hold %s and move around the item's center to rotate."),
			Controls->bToggleGrab ? TEXT("Click") : TEXT("Hold"), *KeyName(EInventoryControl::Grab), *KeyName(EInventoryControl::RotateWithMouse)), 12, Muted);
		Text(FVector2D(40, 98), FString::Printf(TEXT("%s / %s: turn. Scroll: speed. %s: cancel. %s: drop selected item. Green fits; red cancels placement."),
			*KeyName(EInventoryControl::TurnLeft), *KeyName(EInventoryControl::TurnRight), *KeyName(EInventoryControl::Cancel), *KeyName(EInventoryControl::Drop)), 11, Muted);
	}
	Box(FVector2D(880, 25), FVector2D(80, 42), Button);
	Text(FVector2D(890, 36), TEXT("Close"), 13);
	if (!ItemUse.IsValid()) Text(FVector2D(40, 118), TEXT("TEST STORAGE"), 14, Muted);
	else
	{
		Box(FVector2D(40,118), FVector2D(110,27), Button);
		Text(FVector2D(47,122), TEXT("Quick storage"), 11);
		Box(FVector2D(160,118), FVector2D(120,27), Button);
		Text(FVector2D(167,122), TEXT("Open backpack"), 11);
		Box(FVector2D(300,118), FVector2D(290,27), Button);
		Text(FVector2D(307,122), ItemUse->IsBackpackEquipped() ? TEXT("Unequip backpack (keep contents)") : TEXT("Equip backpack"), 10);
	}
	if (!Inventory.IsValid() || !Controls.IsValid()) return Layer;
	const auto Pockets = DisplayPockets();
	if (Pockets.IsEmpty()) return Layer;
	Box(PocketOrigin() - FVector2D(2), Pockets[0].Size + FVector2D(4), FLinearColor(0.22f, 0.3f, 0.36f));
	Box(PocketOrigin(), Pockets[0].Size, FLinearColor(0.055f, 0.075f, 0.095f));
	const auto Entries = Inventory->GetEntries();
	for (const auto& Entry : Entries)
	{
		if (Entry.PocketId != Pockets[0].Id || (bDragging && Entry.Item.InstanceId == SelectedId)) continue;
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Entry.ProfileId, Profile))
			Shape(Profile, Entry.Position, Entry.AngleDegrees,
				Inventory->IsReserved(Entry.Item.InstanceId) ? FLinearColor(0.15f,0.22f,0.25f) : Entry.Item.InstanceId == SelectedId ? FLinearColor(0.43f, 0.62f, 0.73f) : FLinearColor(0.3f, 0.4f, 0.47f));
	}
	if (ItemUse.IsValid())
	{
		Text(FVector2D(40,516), DisplayPocket == TEXT("Quick") ? FString::Printf(TEXT("QUICK: max %.2f kg per object | no firearms"), ItemUse->QuickMaxItemMassKg) : TEXT("BACKPACK | smaller prototype storage"), 11, Muted);
		const TCHAR* Labels[] = { TEXT("To hands"), TEXT("To quick"), TEXT("To backpack"), TEXT("Stow") };
		for (int32 I=0; I<4; ++I) { Box(FVector2D(40+I*135,542), FVector2D(125,32), Button); Text(FVector2D(48+I*135,550), Labels[I], 11); }
		Text(FVector2D(40,582), TEXT("SHORTCUTS: key to rebind | Assign = selected item | double tap cycles"), 10, Muted);
		for (int32 I=0; I<3; ++I)
		{
			const float Y=607+I*36;
			Box(FVector2D(40,Y), FVector2D(95,30), Button);
			Text(FVector2D(47,Y+7), QuickRebinding==I ? TEXT("Key... Esc") : ItemUse->Shortcuts->Keys[I].GetDisplayName().ToString(), 11);
			Text(FVector2D(150,Y+7), ItemUse->Shortcuts->ItemTypes[I].ToString(), 11);
			Box(FVector2D(430,Y), FVector2D(160,30), Button); Text(FVector2D(444,Y+7), TEXT("Assign selected"), 11);
		}
		if (ItemUse->IsOpeningBackpack()) Text(FVector2D(40,710), FString::Printf(TEXT("Opening backpack... %.0f%%"), ItemUse->GetProgress()*100), 12);
	}
	Text(FVector2D(630, 150), Rebinding == INDEX_NONE ? TEXT("CONTROLS / click to rebind") : TEXT("Cancel rebinding"), 14, Muted);
	for (int32 I = 0; I < static_cast<int32>(EInventoryControl::Count); ++I)
	{
		const auto Action = static_cast<EInventoryControl>(I);
		const float Y = 190 + I * 38;
		Box(FVector2D(630, Y), FVector2D(330, 36), Rebinding == I ? FLinearColor(0.22f, 0.36f, 0.4f) : Button);
		Text(FVector2D(640, Y + 10), UInventoryInputSettings::Label(Action), 12);
		FString KeyLabel = Controls->GetKey(Action).GetDisplayName().ToString();
		if (Rebinding == I) KeyLabel = TEXT("Press a control...");
		Text(FVector2D(802, Y + 11), KeyLabel, 10, Muted);
	}
	Box(FVector2D(630, 500), FVector2D(330, 38), Button);
	Text(FVector2D(640, 511), Controls->bToggleGrab ? TEXT("Grab: click to pick up, click again to place") : TEXT("Grab: hold to move, release to place"), 11);
	Box(FVector2D(630, 552), FVector2D(44, 32), Button);
	Box(FVector2D(916, 552), FVector2D(44, 32), Button);
	Text(FVector2D(646, 556), TEXT("-"), 18);
	Text(FVector2D(931, 556), TEXT("+"), 18);
	Text(FVector2D(682, 560), FString::Printf(TEXT("Speed %.0f deg/s | mouse %.2fx"), Controls->TurnSpeed, Controls->TurnSpeed / 120.f), 10, Muted);
	Box(FVector2D(630, 598), FVector2D(330, 34), Button);
	Text(FVector2D(690, 607), TEXT("Reset controls"), 12);
	Box(FVector2D(630, 650), FVector2D(155, 40), FLinearColor(0.12f, 0.28f, 0.25f));
	Text(FVector2D(646, 663), TEXT("+ Add bandage"), 12);
	FInventoryEntry Selected;
	if (Inventory->GetItem(SelectedId, Selected))
		Text(FVector2D(630, 704), Selected.Item.Definition->DisplayName.ToString() + FString::Printf(TEXT("  /  %.1f degrees"), Active() ? PreviewAngle : Selected.AngleDegrees), 11, Muted);

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
			Shape(Profile, Center, PreviewAngle, Result == EInventoryResult::Success ? FLinearColor(0.09f, 0.68f, 0.35f) : FLinearColor(0.84f, 0.12f, 0.12f));
			Out.PopClip();
		}
		Text(FVector2D(40, 738), Describe(Result) + FString::Printf(TEXT("  |  %.1f degrees"), PreviewAngle), 14,
			Result == EInventoryResult::Success ? FLinearColor(0.3f, 1, 0.6f) : FLinearColor(1, 0.4f, 0.3f));
	}
	else Text(FVector2D(40, 738), Status, 12, Muted);
	Text(FVector2D(40, 775), FString::Printf(TEXT("%d items  |  Placeholder shapes  |  Items reset when Play ends"), Entries.Num()), 11, Muted);
	return Layer;
}

TArray<FInventoryPocket> SInventoryPanel::DisplayPockets() const
{
	if (!Inventory.IsValid()) return {};
	const auto Pockets = Inventory->GetPockets();
	if (!ItemUse.IsValid()) return Pockets;
	for (const auto& Pocket : Pockets) if (Pocket.Id == DisplayPocket) return { Pocket };
	return {};
}

bool SInventoryPanel::HandlePlayerButton()
{
	if (InRect(Cursor,40,118,110,27)) { CancelGesture(); DisplayPocket=TEXT("Quick"); SelectedId.Invalidate(); bRequestedBackpack=false; return true; }
	if (InRect(Cursor,160,118,120,27))
	{
		CancelGesture(); bRequestedBackpack=ItemUse->BeginOpenBackpack();
		Status = bRequestedBackpack ? TEXT("Opening backpack...") : TEXT("Equip the backpack first."); return true;
	}
	if (InRect(Cursor,300,118,290,27)) { ItemUse->ToggleBackpackEquipment(); DisplayPocket=TEXT("Quick"); SelectedId.Invalidate(); Status=ItemUse->Status; return true; }
	if (InRect(Cursor,40,542,125,32))
	{
		if (ItemUse->EquipToHands(SelectedId)) { bCloseRequested=true; OnClose.ExecuteIfBound(); }
		else Status=TEXT("Select an available item first."); return true;
	}
	if (InRect(Cursor,175,542,125,32))
	{
		if (ItemUse->Transfer(SelectedId,TEXT("Quick"))) { DisplayPocket=TEXT("Quick"); Status=TEXT("Transferred to quick storage."); }
		else Status=TEXT("Cannot transfer: check size, weight, firearm restriction and hands."); return true;
	}
	if (InRect(Cursor,310,542,125,32))
	{
		if (ItemUse->BeginOpenBackpack()) { PendingTransfer=SelectedId; bRequestedBackpack=true; Status=TEXT("Opening backpack to transfer..."); }
		else Status=TEXT("Equip the backpack first."); return true;
	}
	if (InRect(Cursor,445,542,125,32)) { ItemUse->Stow(); Status=TEXT("Item stowed in its reserved place."); return true; }
	for (int32 I=0; I<3; ++I)
	{
		const float Y=607+I*36;
		if (InRect(Cursor,40,Y,95,30)) { QuickRebinding=I; Rebinding=INDEX_NONE; Status=TEXT("Press a keyboard key. Escape cancels."); return true; }
		if (InRect(Cursor,430,Y,160,30))
		{
			FInventoryEntry Entry;
			if (Inventory->GetItem(SelectedId,Entry) && Entry.PocketId==TEXT("Quick"))
			{ ItemUse->Shortcuts->ItemTypes[I]=Entry.Item.Definition->ItemId; ItemUse->SaveShortcuts(); Status=TEXT("Shortcut assigned to this item type."); }
			else Status=TEXT("Select an item in quick storage first."); return true;
		}
	}
	return false;
}
