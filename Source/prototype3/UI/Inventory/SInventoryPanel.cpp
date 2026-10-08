#include "UI/Inventory/SInventoryPanel.h"
#include "Gameplay/Player/Inventory/InventoryComponent.h"
#include "Gameplay/Items/ItemDefinition.h"
#include "Gameplay/Items/World/DroppedItem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Core/PlayerControllers/prototype3PlayerController.h"
#include "Gameplay/Player/Inventory/PlayerItemUseComponent.h"
#include "Gameplay/Player/Inventory/ItemUseSettings.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformTime.h"

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
	if (!Active() && !bPendingDrag && Inventory.IsValid() && Inventory->GetItem(SelectedId, Selected) && UPlayerItemUseComponent::IsQuickPocket(Selected.PocketId))
		SelectedId.Invalidate();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::SetPocketsOnly(bool bOnly)
{
	if (!bOnly && ItemUse.IsValid() && !ItemUse->BeginOpenBackpack()) { Status = TEXT("Equip a backpack first."); return; }
	CancelInteraction(); SelectedId.Invalidate();
	bRequestedClose = false;
	bPocketsOnly = bOnly;
	QuickPocketIndex = 0;
	if (ItemUse.IsValid())
	{
		if (bOnly) ItemUse->CloseBackpack();
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
		if (ItemUse->IsArranging() || bWasArranging) Status = ItemUse->Status;
		bWasArranging = ItemUse->IsArranging();
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
		CancelGesture();
		FloorSource = FloorItems[Index];
		SelectedId.Invalidate();
		Pending.Item = FloorSource->GetItem();
		Pending.ProfileId = FloorSource->GetInventoryProfileId();
		GrabOffset = FVector2D::ZeroVector;
		PreviewAngle = 0;
		bFromFloor = true;
		bDragging = Controls->bToggleGrab;
		bPendingDrag = !bDragging;
		GrabStart = Cursor;
		bCanDoubleClick = false;
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
	return Inventory.IsValid() && Inventory->GetItem(SelectedId, Entry) && (!Inventory->IsReserved(SelectedId) || IsHeldSelection())
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
			: Inventory->CheckMove(SelectedId, PocketId, Center, PreviewAngle, IsHeldSelection());
	}
	return EInventoryResult::InvalidPocket;
}

void SInventoryPanel::CancelGesture()
{
	bDragging = bPendingDrag = bAdding = bTurnLeft = bTurnRight = false;
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
			if (ItemUse.IsValid())
			{
				FInventoryArrangement Request; Request.Kind = EInventoryArrangement::FloorDrop;
				Request.ItemId = Pending.Item.InstanceId; Request.FloorItem = FloorSource;
				ItemUse->RequestArrangement(Request); Status = ItemUse->Status;
			}
			else Status = FloorSource->DropAtFeet(Player.Get(), Error) ? TEXT("Item dropped at your feet.") : Error;
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
		if (ItemUse.IsValid() && !bAdding)
		{
			FInventoryArrangement Request;
			Request.Kind = bFromFloor ? EInventoryArrangement::PickUp : EInventoryArrangement::Move;
			Request.ItemId = Pending.Item.InstanceId; Request.FloorItem = FloorSource;
			Request.Pocket = Pocket; Request.Position = Center; Request.Angle = PreviewAngle;
			if (ItemUse->RequestArrangement(Request)) SelectedId = Request.ItemId;
			Status = ItemUse->Status;
			CancelGesture(); RefreshFloor(); return;
		}
		else if (bFromFloor)
		{
			Result = FloorSource->PickUp(Inventory.Get(), Player.Get(), Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else if (bAdding)
		{
			Result = Inventory->AddItem(Pending.Item, Pending.ProfileId, Pocket, Center, PreviewAngle);
			if (Result == EInventoryResult::Success) SelectedId = Pending.Item.InstanceId;
		}
		else
		{
			const bool bWasHeld = IsHeldSelection();
			Result = Inventory->MoveItem(SelectedId, Pocket, Center, PreviewAngle, bWasHeld);
			if (Result == EInventoryResult::Success && bWasHeld) ItemUse->Stow();
		}
	}
	Status = Result == EInventoryResult::Success ? TEXT("Item placed.") : Describe(Result) + TEXT(". Placement cancelled.");
	CancelGesture();
	RefreshFloor();
}

void SInventoryPanel::Turn(double Delta)
{
	if (!Active()) return;
	bCanDoubleClick = false;
	PreviewAngle = InventoryGeometry::NormalizeAngle(PreviewAngle + Delta);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SInventoryPanel::BeginMouseRotation()
{
	if (!Active() || bMouseRotating) return;
	bCanDoubleClick = false;
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
	if ((bPendingDrag || bDragging) && (Cursor - GrabStart).SizeSquared() > 25.0)
	{
		bCanDoubleClick = false;
		if (bPendingDrag) { bPendingDrag = false; bDragging = true; }
	}
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
	if (ItemUse.IsValid() && ItemUse->GetBackpackMode() == EBackpackMode::Slow)
	{
		FInventoryArrangement Request; Request.Kind = EInventoryArrangement::Drop; Request.ItemId = SelectedId;
		ItemUse->RequestArrangement(Request); Status = ItemUse->Status;
	}
	else if (OnDropItem.IsBound() && OnDropItem.Execute(SelectedId, Error))
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
	if ((Active() || bPendingDrag) && !HasMouseCapture()) Result.CaptureMouse(SharedThis(this));
	else if (!Active() && !bPendingDrag && HasMouseCapture()) Result.ReleaseMouseCapture();
	return Result;
}

bool SInventoryPanel::IsHeldSelection() const
{
	return ItemUse.IsValid() && SelectedId.IsValid() && ItemUse->GetHeldId() == SelectedId;
}
bool SInventoryPanel::IsActionDown(EInventoryControl Action) const
{
	return PressedKeys.Contains(Controls->GetKey(Action)) || PressedKeys.Contains(Controls->GetKey(Action, 1));
}
FReply SInventoryPanel::Press(FKey Key)
{
	if (!Controls.IsValid()) return FReply::Handled();
	auto Is = [&](EInventoryControl Action) { return Controls->Matches(Action, Key); };
	if (Is(EInventoryControl::Back) || Is(EInventoryControl::Toggle))
	{
		if (!Is(EInventoryControl::Back) && ItemUse.IsValid() && bPocketsOnly) { SetPocketsOnly(false); return Reply(); }
		CancelInteraction(); OnClose.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (ItemUse.IsValid() && ItemUse->IsArranging()) return Reply();
	if (ItemUse.IsValid())
	{
		if (bPocketsOnly && Is(EInventoryControl::ShowQuick))
		{ CancelInteraction(); OnClose.ExecuteIfBound(); return FReply::Handled().ReleaseMouseCapture(); }
		if (!bPocketsOnly && Is(EInventoryControl::CyclePocket)) { CycleQuickPocket(); return Reply(); }
	}
	if (!IsInterfaceReady()) return Reply();
	if (Is(EInventoryControl::Cancel)) { bCanDoubleClick = false; CancelInteraction(); Status = TEXT("Placement cancelled. Original state restored."); return Reply(); }
	if (ItemUse.IsValid() && HandlePlayerControl(Key)) return Reply();
	if (Is(EInventoryControl::Drop)) { bCanDoubleClick = false; DropSelected(); return Reply(); }
	if (Is(EInventoryControl::Grab))
	{
		const bool bAlreadyDown = IsActionDown(EInventoryControl::Grab);
		PressedKeys.Add(Key);
		if (bAlreadyDown) return Reply();
		FInventoryEntry Entry;
		const double Now = FPlatformTime::Seconds();
		const bool bHit = HitItem(Cursor, Entry);
		if (ItemUse.IsValid() && bCanDoubleClick && bHit && LastClickItem == Entry.Item.InstanceId
			&& LastClickKey == Key && Now - LastClickTime <= .3 && (Cursor - GrabStart).SizeSquared() <= 25.0)
		{
			const FGuid Id = Entry.Item.InstanceId;
			CancelGesture(); bCanDoubleClick = false; SelectedId = Id;
			FInventoryArrangement Request;
			Request.Kind = ItemUse->GetHeldId() == Id ? EInventoryArrangement::Stow : EInventoryArrangement::Take;
			Request.ItemId = Id; ItemUse->RequestArrangement(Request);
			Status = ItemUse->Status;
			return Reply();
		}
		if (bAdding || (bDragging && Controls->bToggleGrab)) { bCanDoubleClick = false; CommitGesture(); return Reply(); }
		if (!bDragging)
		{
			if (BeginFloorDrag()) return Reply();
			if (bHit)
			{
				SelectedId = Entry.Item.InstanceId;
				if (Inventory->IsReserved(SelectedId) && !IsHeldSelection()) return Reply();
				Pending = Entry; PreviewAngle = Entry.AngleDegrees;
				for (const auto& View : DisplayPockets())
					if (View.Pocket.Id == Entry.PocketId) GrabOffset = Cursor - View.Origin - Entry.Position;
				GrabStart = Cursor; LastClickTime = Now; LastClickKey = Key; LastClickItem = SelectedId;
				bCanDoubleClick = true;
				bDragging = Controls->bToggleGrab; bPendingDrag = !bDragging;
			}
			else { SelectedId.Invalidate(); bCanDoubleClick = false; }
		}
		return Reply();
	}
	PressedKeys.Add(Key);
	if (bPendingDrag && (Is(EInventoryControl::RotateWithMouse) || Is(EInventoryControl::TurnLeft) || Is(EInventoryControl::TurnRight)
		|| Is(EInventoryControl::FasterRotation) || Is(EInventoryControl::SlowerRotation)))
	{ bPendingDrag = false; bDragging = true; }
	if (Active())
	{
		if (Is(EInventoryControl::RotateWithMouse)) BeginMouseRotation();
		if (Is(EInventoryControl::TurnLeft)) { bTurnLeft = true; bCanDoubleClick = false; }
		if (Is(EInventoryControl::TurnRight)) { bTurnRight = true; bCanDoubleClick = false; }
		if (Is(EInventoryControl::FasterRotation)) { Controls->SetTurnSpeed(Controls->TurnSpeed + 15); SavePreferences(); }
		if (Is(EInventoryControl::SlowerRotation)) { Controls->SetTurnSpeed(Controls->TurnSpeed - 15); SavePreferences(); }
	}
	else if (Player.IsValid() && InRect(Cursor - FloorOrigin(), 0, 0, 260, FloorSize().Y))
	{
		const int32 Delta = Is(EInventoryControl::ScrollFloorUp) ? -3 : Is(EInventoryControl::ScrollFloorDown) ? 3 : 0;
		FloorScroll = FMath::Clamp(FloorScroll + Delta, 0, FMath::Max(0, FloorItems.Num() - FloorRows()));
	}
	if (!ItemUse.IsValid() && Key == Controls->GetKey(EInventoryControl::Add)) BeginBandage();
	return Reply();
}
FReply SInventoryPanel::Release(FKey Key)
{
	PressedKeys.Remove(Key);
	if (!Controls.IsValid()) return Reply();
	if (Controls->Matches(EInventoryControl::RotateWithMouse, Key) && !IsActionDown(EInventoryControl::RotateWithMouse)) EndMouseRotation();
	if (Controls->Matches(EInventoryControl::TurnLeft, Key)) bTurnLeft = IsActionDown(EInventoryControl::TurnLeft);
	if (Controls->Matches(EInventoryControl::TurnRight, Key)) bTurnRight = IsActionDown(EInventoryControl::TurnRight);
	if (Controls->Matches(EInventoryControl::Grab, Key) && !IsActionDown(EInventoryControl::Grab))
	{
		if (bPendingDrag) { bPendingDrag = false; if (bFromFloor) CancelGesture(); }
		else if (bDragging && !Controls->bToggleGrab) { bCanDoubleClick = false; CommitGesture(); }
	}
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
	if (!IsInterfaceReady() || FMath::IsNearlyZero(Event.GetWheelDelta())) return Reply();
	const FKey Key = Event.GetWheelDelta() > 0 ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
	const int32 Steps = FMath::Clamp(FMath::RoundToInt(FMath::Abs(Event.GetWheelDelta())), 1, 128);
	for (int32 Step = 0; Step < Steps; ++Step) { Press(Key); Release(Key); }
	return Reply();
}
FReply SInventoryPanel::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	return Event.IsRepeat() ? FReply::Handled() : Press(Event.GetKey());
}
FReply SInventoryPanel::OnKeyUp(const FGeometry&, const FKeyEvent& Event) { return Release(Event.GetKey()); }
void SInventoryPanel::OnMouseCaptureLost(const FCaptureLostEvent&)
{
	if (Active() || bPendingDrag) { bCanDoubleClick = false; PressedKeys.Empty(); }
	CancelGesture();
}
void SInventoryPanel::OnFocusLost(const FFocusEvent&)
{
	CancelInteraction();
	if (ItemUse.IsValid() && !bPocketsOnly) ItemUse->CloseBackpack();
	if (Player.IsValid()) if (auto* Controller = Cast<Aprototype3PlayerController>(Player->GetController()); Controller && Controller->IsInventoryInterfaceOpen())
		Controller->FlushPressedKeys();
}

int32 SInventoryPanel::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	// Tap/hold recognition is input bookkeeping, not a third loading screen.
	// Keep the focused input route alive without painting until a mode is chosen.
	if (!bPocketsOnly && ItemUse.IsValid() && (!ItemUse->IsBackpackActive()
		|| ItemUse->GetBackpackMode() == EBackpackMode::Selecting)) return Layer;
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
	if (!bPocketsOnly && ItemUse.IsValid() && ItemUse->IsBackpackOpen())
		Text(FVector2D(300,35), ItemUse->GetBackpackMode() == EBackpackMode::Slow ? TEXT("WALKING MODE") : TEXT("QUICK MODE"), 14, Muted);
	if (!IsInterfaceReady())
	{
		if (ItemUse.IsValid() && ItemUse->IsOpeningBackpack())
		{
			const FVector2D Bar = PanelSize() * .5 - FVector2D(200,6);
			const auto Mode = ItemUse->GetBackpackMode();
			Text(Bar - FVector2D(0,40), Mode == EBackpackMode::Slow ? TEXT("Opening: hold to continue...")
				: TEXT("Opening inventory..."), 18);
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
					(Inventory->IsReserved(Entry.Item.InstanceId) || (ItemUse.IsValid() && ItemUse->IsArranging() && ItemUse->GetArrangement().ItemId == Entry.Item.InstanceId))
					? FLinearColor(0.15f,0.22f,0.25f) : Entry.Item.InstanceId == SelectedId ? FLinearColor(0.43f, 0.62f, 0.73f) : FLinearColor(0.3f, 0.4f, 0.47f));
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
			const bool bPendingSource = ItemUse.IsValid() && ItemUse->IsArranging() && ItemUse->GetArrangement().FloorItem == Actor;
			if (bPendingSource) Box(RowOrigin, FVector2D(248,36), FLinearColor(.08f,.12f,.14f));
			Text(RowOrigin + FVector2D(10,8), Actor->GetItem().Definition->DisplayName.ToString(), 12, bPendingSource ? Muted : FLinearColor::White);
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
		auto Key = [this](EInventoryControl Action) { return Controls->KeyLabel(Action); };
		Box(FVector2D(1120,0), FVector2D(420,940), FLinearColor(.022f,.03f,.043f,.95f));
		Text(FVector2D(1140,30), TEXT("CONTROLS"), 20);
		float Y = 86;
		auto Line = [&](const FString& Value) { Text(FVector2D(1140,Y),Value,12,Muted); Y += 27; };
		auto Control = [&](EInventoryControl Action, const TCHAR* Description) { Line(Key(Action) + TEXT(": ") + Description); };
		Control(EInventoryControl::Grab, Controls->bToggleGrab ? TEXT("click to grab / place") : TEXT("hold to drag; release to place"));
		Control(EInventoryControl::RotateWithMouse, TEXT("hold + move to rotate"));
		Control(EInventoryControl::TurnLeft, TEXT("rotate item left"));
		Control(EInventoryControl::TurnRight, TEXT("rotate item right"));
		Control(EInventoryControl::Cancel, TEXT("cancel placement"));
		Control(EInventoryControl::FasterRotation, TEXT("increase item rotation speed"));
		Control(EInventoryControl::SlowerRotation, TEXT("decrease item rotation speed"));
		Control(EInventoryControl::ScrollFloorUp, TEXT("scroll floor items up"));
		Control(EInventoryControl::ScrollFloorDown, TEXT("scroll floor items down"));
		Control(EInventoryControl::Drop, TEXT("drop selected item at your feet"));
		Control(EInventoryControl::Toggle, bPocketsOnly ? TEXT("tap: quick / hold: walking backpack") : TEXT("close backpack"));
		Control(bPocketsOnly ? EInventoryControl::ShowQuick : EInventoryControl::CyclePocket, bPocketsOnly ? TEXT("close pockets") : TEXT("next pocket"));
		Line(TEXT("Double ") + Key(EInventoryControl::Grab) + TEXT(": take / stow item"));
		Control(EInventoryControl::Stow, TEXT("stow held item"));
		Control(EInventoryControl::Back, TEXT("close interface"));
		Y += 12;
		Line(TEXT("Drag floor names into a storage grid."));
		Line(TEXT("Clear the storage containers to drop."));
		Line(TEXT("The floor list also accepts world drops."));
		Line(TEXT("Keys / drag / speed: Options > Controls."));
		Y += 12;
		Line(TEXT("When inventory is closed:"));
		Control(EInventoryControl::Primary, TEXT("primary item action"));
		Control(EInventoryControl::Secondary, TEXT("secondary (hold to use bandage)"));
		Control(EInventoryControl::Bandage, TEXT("take bandage; hold to use"));
	}
	FInventoryEntry Selected;
	if (ItemUse.IsValid() && ItemUse->IsArranging())
	{
		const auto& Request = ItemUse->GetArrangement();
		const auto& Source = ItemUse->GetArrangementSource();
		FInventoryItemProfile Profile;
		if (Inventory->GetProfile(Source.ProfileId, Profile))
		{
			const FLinearColor Shade(.15f,.22f,.25f);
			if (Request.Kind == EInventoryArrangement::Move || Request.Kind == EInventoryArrangement::PickUp)
				for (const auto& View : Pockets) if (View.Pocket.Id == Request.Pocket)
					Shape(Profile, View.Origin + Request.Position, Request.Angle, Shade);
			if (Request.FloorItem.IsValid())
				Shape(Profile, FloorOrigin() + FVector2D(130, FloorSize().Y - 70), Source.AngleDegrees, Shade);
			if (Request.Kind == EInventoryArrangement::Drop || Request.Kind == EInventoryArrangement::FloorDrop)
			{
				Box(FloorOrigin() + FVector2D(0, FloorSize().Y - 130), FVector2D(248,130), FLinearColor(.05f,.07f,.09f,.95f));
				Shape(Profile, FloorOrigin() + FVector2D(130, FloorSize().Y - 70), Source.AngleDegrees, Shade);
			}
			if (Request.Kind == EInventoryArrangement::Take || Request.Kind == EInventoryArrangement::Stow || ItemUse->GetHeldId() == Request.ItemId)
			{
				Text(FVector2D(500,440), TEXT("HANDS"), 14, Muted);
				Shape(Profile, FVector2D(610,570), Source.AngleDegrees, Shade);
			}
		}
		Text(FVector2D(40, PanelSize().Y - 80), FString::Printf(TEXT("Arranging... %.0f%%"), ItemUse->GetArrangementProgress() * 100), 13, Muted);
	}
	if ((!ItemUse.IsValid() || !ItemUse->IsArranging()) && Inventory->GetItem(SelectedId, Selected))
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
	if (ItemUse.IsValid()) ItemUse->CancelArrangement();
	CancelGesture(); PressedKeys.Empty(); bCanDoubleClick = false;
}
bool SInventoryPanel::HandlePlayerControl(FKey Key)
{
	if (Controls->Matches(EInventoryControl::Stow, Key))
	{
		CancelInteraction();
		FInventoryArrangement Request; Request.Kind = EInventoryArrangement::Stow; Request.ItemId = ItemUse->GetHeldId();
		ItemUse->RequestArrangement(Request); Status = ItemUse->Status; return true;
	}
	return false;
}
